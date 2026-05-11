/**
 * m_Do_controller_pad.cpp
 * JUTGamePad Wrapper and Conversion
 */

#include "m_Do/m_Do_controller_pad.h"
#include "JSystem/JAWExtSystem/JAWExtSystem.h"
#include "SSystem/SComponent/c_lib.h"
#include "d/actor/d_a_player.h"
#include "d/d_com_inf_game.h"
#include "f_ap/f_ap_game.h"
#include "m_Do/m_Do_Reset.h"
#include "m_Do/m_Do_main.h"
#include "tracy/Tracy.hpp"

JUTGamePad* mDoCPd_c::m_gamePad[4];

interface_of_controller_pad mDoCPd_c::m_cpadInfo[4];
interface_of_controller_pad mDoCPd_c::m_debugCpadInfo[4];

namespace {

constexpr u32 kBufferedButtonMask =
    PAD_BUTTON_LEFT | PAD_BUTTON_RIGHT | PAD_BUTTON_DOWN | PAD_BUTTON_UP | PAD_TRIGGER_Z |
    PAD_TRIGGER_R | PAD_TRIGGER_L | PAD_BUTTON_A | PAD_BUTTON_B | PAD_BUTTON_X | PAD_BUTTON_Y |
    PAD_BUTTON_START;

bool sInputBufferWasBlocked = false;
bool sDelayEventEntryForInputBuffer = false;
u32 sBufferedButtons[4] = {};
bool sBufferedLTrigger[4] = {};
bool sBufferedRTrigger[4] = {};

bool is_event_blocking_gameplay_input() {
    return !dComIfGp_getEvent()->isOrderOK();
}

bool is_gameplay_input_blocked() {
    daPy_py_c* player = dComIfGp_getLinkPlayer();
    if (player == NULL) {
        return is_event_blocking_gameplay_input() || sInputBufferWasBlocked;
    }
    return is_event_blocking_gameplay_input() || player->mDemo.getDemoType() != 0;
}

void reset_input_buffer() {
    sInputBufferWasBlocked = false;
    for (int i = 0; i < 4; i++) {
        sBufferedButtons[i] = 0;
        sBufferedLTrigger[i] = false;
        sBufferedRTrigger[i] = false;
    }
}

bool apply_input_buffer() {
    bool appliedInput = false;
    for (u32 i = 0; i < 4; i++) {
        interface_of_controller_pad& cpadInfo = mDoCPd_c::m_cpadInfo[i];
        const u32 bufferedButtons = cpadInfo.mButtonFlags & sBufferedButtons[i] & kBufferedButtonMask;
        cpadInfo.mPressedButtonFlags |= bufferedButtons;
        appliedInput = appliedInput || bufferedButtons != 0;

        if (sBufferedLTrigger[i] && cpadInfo.mTriggerLeft > fapGmHIO_getLROnValue()) {
            cpadInfo.mTrigLockL = true;
            appliedInput = true;
        }
        if (sBufferedRTrigger[i] && cpadInfo.mTriggerRight > fapGmHIO_getLROnValue()) {
            cpadInfo.mTrigLockR = true;
            appliedInput = true;
        }
    }
    return appliedInput;
}

}  // namespace

void mDoCPd_c::create() {
    #if PLATFORM_GCN || PLATFORM_SHIELD
    m_gamePad[0] = JKR_NEW JUTGamePad(JUTGamePad::EPort1);
    #endif

    if (DEBUG || mDoMain::developmentMode != 0) {
        #if PLATFORM_WII
        m_gamePad[0] = JKR_NEW JUTGamePad(JUTGamePad::EPort1);
        #endif

        m_gamePad[1] = JKR_NEW JUTGamePad(JUTGamePad::EPort2);
        m_gamePad[2] = JKR_NEW JUTGamePad(JUTGamePad::EPort3);
        m_gamePad[3] = JKR_NEW JUTGamePad(JUTGamePad::EPort4);
    } else {
        #if PLATFORM_WII
        m_gamePad[0] = NULL;
        #endif

        m_gamePad[1] = NULL;
        m_gamePad[2] = NULL;
        m_gamePad[3] = NULL;
    }

    #if PLATFORM_GCN || PLATFORM_SHIELD
    if (!mDoRst::isReset()) {
        JUTGamePad::clearResetOccurred();
        JUTGamePad::setResetCallback(mDoRst_resetCallBack, NULL);
    }
    #endif
    JUTGamePad::setAnalogMode(3);

    interface_of_controller_pad* cpad = &m_cpadInfo[0];
    for (int i = 0; i < 4; i++) {
        cpad->mHoldLockL = cpad->mTrigLockL = false;
        cpad->mHoldLockR = cpad->mTrigLockR = false;
        cpad++;
    }

    reset_input_buffer();
}

void mDoCPd_c::read() {
    ZoneScoped;
    sDelayEventEntryForInputBuffer = false;
    JUTGamePad::read();

    if (!mDoRst::isReset() && mDoRst::is3ButtonReset()) {
        if (!JUTGamePad::getGamePad(mDoRst::get3ButtonResetPort())->isPushing3ButtonReset()) {
            mDoRst::off3ButtonReset();
        }
    }

#if DEBUG
    if (m_gamePad[3]) {
        JAWExtSystem::padProc(*m_gamePad[3]);
    }
#endif

    JUTGamePad** pad = m_gamePad;
    interface_of_controller_pad* interface = m_cpadInfo;
#if DEBUG
    interface_of_controller_pad* interface2 = m_debugCpadInfo;
    if (dComIfG_isDebugMode()) {
        interface_of_controller_pad* tmp = interface;
        interface = interface2;
        interface2 = tmp;
    }
#endif

    for (u32 i = 0; i < 4; i++) {
        if (*pad == NULL) {
            cLib_memSet(interface, 0, sizeof(interface_of_controller_pad));
        } else {
            convert(interface, *pad);
            LRlockCheck(interface);
        }
#if DEBUG
        cLib_memSet(interface2, 0, sizeof(interface_of_controller_pad));
#endif
        pad++;
        interface++;
#if DEBUG
        interface2++;
#endif
    }

    if (!dusk::getSettings().game.enableInputBuffering) {
        reset_input_buffer();
        return;
    }

    const bool inputBlocked = is_gameplay_input_blocked();
    if (inputBlocked) {
        for (u32 i = 0; i < 4; i++) {
            sBufferedButtons[i] = m_cpadInfo[i].mButtonFlags & kBufferedButtonMask;
            sBufferedLTrigger[i] = m_cpadInfo[i].mTriggerLeft > fapGmHIO_getLROnValue();
            sBufferedRTrigger[i] = m_cpadInfo[i].mTriggerRight > fapGmHIO_getLROnValue();
        }
        sInputBufferWasBlocked = true;
        return;
    }

    if (sInputBufferWasBlocked) {
        sDelayEventEntryForInputBuffer = apply_input_buffer();
    }
    reset_input_buffer();
}

bool mDoCPd_c::shouldDelayEventEntryForInputBuffer() {
    return sDelayEventEntryForInputBuffer;
}

void mDoCPd_c::convert(interface_of_controller_pad* pInterface, JUTGamePad* pPad) {
    pInterface->mButtonFlags = pPad->getButton();
    pInterface->mPressedButtonFlags = pPad->getTrigger();
    pInterface->mMainStickPosX = pPad->getMainStickX();
    pInterface->mMainStickPosY = pPad->getMainStickY();
    pInterface->mMainStickValue = pPad->getMainStickValue();
    pInterface->mMainStickAngle = pPad->getMainStickAngle();
    pInterface->mCStickPosX = pPad->getSubStickX();
    pInterface->mCStickPosY = pPad->getSubStickY();
    pInterface->mCStickValue = pPad->getSubStickValue();
    pInterface->mCStickAngle = pPad->getSubStickAngle();

    mDoCPd_ANALOG_CONV(pPad->getAnalogA(), pInterface->mAnalogA);
    mDoCPd_ANALOG_CONV(pPad->getAnalogB(), pInterface->mAnalogB);
    mDoCPd_TRIGGER_CONV(pPad->getAnalogL(), pInterface->mTriggerLeft);
    mDoCPd_TRIGGER_CONV(pPad->getAnalogR(), pInterface->mTriggerRight);

    pInterface->mGamepadErrorFlags = pPad->getErrorStatus();
}

void mDoCPd_c::LRlockCheck(interface_of_controller_pad* interface) {
    f32 trigger = interface->mTriggerLeft;
    interface->mTrigLockL = false;
    interface->mTrigLockR = false;

    if (trigger > fapGmHIO_getLROnValue()) {
        if (interface->mHoldLockL != true) {
            interface->mTrigLockL = true;
        }
        interface->mHoldLockL = true;
    } else if (trigger < fapGmHIO_getLROffValue()) {
        interface->mHoldLockL = false;
    }

    trigger = interface->mTriggerRight;
    if (trigger > fapGmHIO_getLROnValue()) {
        if (interface->mHoldLockR != true) {
            interface->mTrigLockR = true;
        }
        interface->mHoldLockR = true;
    } else if (trigger < fapGmHIO_getLROffValue()) {
        interface->mHoldLockR = false;
    }
}

void mDoCPd_c::recalibrate(void) {
    JUTGamePad::clearForReset();
    JUTGamePad::CRumble::setEnabled(PAD_CHAN3_BIT | PAD_CHAN2_BIT | PAD_CHAN1_BIT | PAD_CHAN0_BIT);
}
