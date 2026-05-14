#include "dusk/debug_stacktrace.h"

#include "dusk/logging.h"

#if TARGET_PC && defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>

#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

#include <algorithm>
#include <array>
#include <mutex>

namespace {
std::once_flag sSymbolsInitialized;

void InitializeSymbols() noexcept {
    const HANDLE process = GetCurrentProcess();
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
    SymInitialize(process, nullptr, TRUE);
}

const char* Basename(const char* path) noexcept {
    if (path == nullptr) {
        return "";
    }

    const char* base = path;
    for (const char* p = path; *p != '\0'; ++p) {
        if (*p == '\\' || *p == '/') {
            base = p + 1;
        }
    }
    return base;
}

#if defined(_MSC_VER) && defined(_DEBUG)
int CrtReportHook(int reportType, char* message, int* returnValue) {
    (void)returnValue;

    const char* type = "CRT";
    switch (reportType) {
    case _CRT_WARN:
        type = "CRT warning";
        break;
    case _CRT_ERROR:
        type = "CRT error";
        break;
    case _CRT_ASSERT:
        type = "CRT assert";
        break;
    }

    DuskLog.error("{}: {}", type, message != nullptr ? message : "");
    dusk::LogDebugStackTrace(type, 2);
    return FALSE;
}
#endif
} // namespace

void dusk::InstallDebugCrashDiagnostics() noexcept {
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetReportHook(CrtReportHook);
#endif
}

void dusk::LogDebugStackTrace(const char* reason, unsigned framesToSkip) noexcept {
    std::call_once(sSymbolsInitialized, InitializeSymbols);

    std::array<void*, 64> frames{};
    const USHORT count = CaptureStackBackTrace(framesToSkip + 1, static_cast<DWORD>(frames.size()), frames.data(), nullptr);
    DuskLog.error("Debug stack trace: {}", reason != nullptr ? reason : "unknown");

    const HANDLE process = GetCurrentProcess();
    for (USHORT i = 0; i < count; ++i) {
        const DWORD64 address = reinterpret_cast<DWORD64>(frames[i]);

        std::array<char, sizeof(SYMBOL_INFO) + MAX_SYM_NAME> symbolStorage{};
        auto* symbol = reinterpret_cast<SYMBOL_INFO*>(symbolStorage.data());
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;

        DWORD64 displacement = 0;
        const bool hasSymbol = SymFromAddr(process, address, &displacement, symbol) == TRUE;

        IMAGEHLP_LINE64 line{};
        line.SizeOfStruct = sizeof(line);
        DWORD lineDisplacement = 0;
        const bool hasLine = SymGetLineFromAddr64(process, address, &lineDisplacement, &line) == TRUE;

        MEMORY_BASIC_INFORMATION memoryInfo{};
        const bool hasMemory = VirtualQuery(frames[i], &memoryInfo, sizeof(memoryInfo)) != 0;
        const auto moduleBase = reinterpret_cast<DWORD64>(hasMemory ? memoryInfo.AllocationBase : nullptr);
        const DWORD64 moduleOffset = moduleBase != 0 ? address - moduleBase : 0;

        char modulePath[MAX_PATH]{};
        if (hasMemory) {
            GetModuleFileNameA(reinterpret_cast<HMODULE>(memoryInfo.AllocationBase), modulePath, sizeof(modulePath));
        }

        if (hasSymbol && hasLine) {
            DuskLog.error("  #{:02} {}+0x{:x} {}+0x{:x} ({}:{})", i, Basename(modulePath), moduleOffset, symbol->Name,
                          displacement, line.FileName, line.LineNumber);
        } else if (hasSymbol) {
            DuskLog.error("  #{:02} {}+0x{:x} {}+0x{:x}", i, Basename(modulePath), moduleOffset, symbol->Name,
                          displacement);
        } else {
            DuskLog.error("  #{:02} {}+0x{:x} {}", i, Basename(modulePath), moduleOffset,
                          reinterpret_cast<const void*>(address));
        }
    }
}
#else
void dusk::InstallDebugCrashDiagnostics() noexcept {}
void dusk::LogDebugStackTrace(const char*, unsigned) noexcept {}
#endif
