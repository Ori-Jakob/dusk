#pragma once

namespace dusk {
void InstallDebugCrashDiagnostics() noexcept;
void LogDebugStackTrace(const char* reason, unsigned framesToSkip = 0) noexcept;
} // namespace dusk
