/**
 * platform.hpp — Cross-platform abstraction layer
 *
 * Thin wrappers for OS-specific functionality:
 *   - Filesystem paths (home, config, cache dirs)
 *   - Console I/O (terminal key input, color enablement)
 *   - Signal handling
 *   - Socket primitives
 *   - Browser launch
 *   - Thread naming
 *   - Process control
 *
 * Linux:  platform_linux.cpp
 * Windows: platform_windows.cpp (Pillar 3)
 *
 * Design: each function is a thin veneer — no abstractions for abstractions' sake.
 * Compile-time selection via CMake (one .cpp per platform in the build).
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace n0s
{
namespace platform
{

struct ConsoleCapabilities
{
	bool isTTY = false;
	bool color = false;
	bool unicode = false;
};

// ─── Filesystem Paths ────────────────────────────────────────────────────────

/// User's home directory.
/// Linux: $HOME or getpwuid(getuid())->pw_dir
/// Windows: %USERPROFILE%
std::string getHomePath();

/// Configuration directory (created if missing).
/// Linux: $XDG_CONFIG_HOME/n0s/ or ~/.config/n0s/
/// Windows: %APPDATA%\n0s\ 
std::string getConfigDir();

/// Cache directory (created if missing).
/// Linux: $XDG_CACHE_HOME/n0s/ or ~/.cache/n0s/
/// Windows: %LOCALAPPDATA%\n0s\ 
std::string getCacheDir();

// ─── Console ─────────────────────────────────────────────────────────────────

/// Read a single keypress without echo or line buffering.
/// Returns the key code, or -1 on error.
int getKey();

/// Enable virtual terminal color sequences (needed on Windows 10+).
/// No-op on Linux (ANSI is native).
void enableConsoleColors();

/// Detect and initialize console capabilities.
/// Enables ANSI colors / UTF-8 where supported and returns the final capability set.
ConsoleCapabilities initConsole();

/// Returns the cached console capability set.
/// Calls initConsole() on first use.
ConsoleCapabilities getConsoleCapabilities();

/// Get formatted local time string.
/// Uses localtime_r (POSIX) or localtime_s (Windows).
/// @param buf    output buffer
/// @param bufLen buffer size
/// @param fmt    strftime format (e.g. "[%F %T] : ")
/// @param t      time_t value
void formatLocalTime(char* buf, size_t bufLen, const char* fmt, int64_t t);

// ─── Signals ─────────────────────────────────────────────────────────────────

/// Ignore SIGPIPE (POSIX) or no-op (Windows).
void disableSigpipe();

/// Install shutdown handler for SIGINT/SIGTERM (POSIX) or
/// SetConsoleCtrlHandler (Windows).
/// @param handler  callback invoked on shutdown signal
void installShutdownHandler(void (*handler)(int));

// ─── Process / Browser ───────────────────────────────────────────────────────

/// Open a URL in the system default browser (non-blocking).
/// Linux: fork + execlp("xdg-open", ...)
/// Windows: ShellExecuteW(..., L"open", ...)
void openBrowser(const char* url);

/// Fork + exec a child process (used by autotune subprocess runner).
/// Linux: fork() + execvp()
/// Windows: CreateProcess()
/// @return child PID (>0), 0 in child (Linux), or -1 on error
int spawnProcess(const char* path, const char* const argv[]);

// ─── Threading ───────────────────────────────────────────────────────────────

/// Set the current thread's name (for debugger / top / htop).
/// Linux: pthread_setname_np()
/// Windows: SetThreadDescription() (Win10 1607+)
void setThreadName(const char* name);

// ─── Sockets ─────────────────────────────────────────────────────────────────

/// One-time socket subsystem initialization.
/// Linux: no-op
/// Windows: WSAStartup()
void sockInit();

/// Clean up socket subsystem.
/// Linux: no-op
/// Windows: WSACleanup()
void sockCleanup();

// ─── Platform Detection ──────────────────────────────────────────────────────

/// Returns "linux", "windows", or "unknown".
const char* platformName();

/// Returns true if running on Windows.
constexpr bool isWindows()
{
#ifdef _WIN32
	return true;
#else
	return false;
#endif
}

} // namespace platform
} // namespace n0s
