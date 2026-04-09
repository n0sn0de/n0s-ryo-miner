/**
 * platform_windows.cpp — Windows implementations of platform abstraction
 *
 * Stub file for Pillar 3 (Windows Support).
 * Compiles only on Windows (_WIN32 defined).
 * Each function will be implemented during the Windows support phase.
 */

#ifdef _WIN32

#include "platform.hpp"

// winsock2.h MUST come before windows.h to avoid redefinition errors
#include <winsock2.h>
#include <ws2tcpip.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <cstdio>
#include <ctime>
#include <cstring>

// MinGW may not have _kbhit/_getch in <conio.h> — check
#if defined(_MSC_VER)
#include <conio.h>
#else
// MinGW: use Windows console input APIs directly
#endif

#if defined(_MSC_VER)
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "shell32.lib")
#endif

namespace n0s
{
namespace platform
{

namespace
{

ConsoleCapabilities g_console_caps;
bool g_console_caps_init = false;

bool env_present(const char* name)
{
	char buf[8];
	DWORD len = GetEnvironmentVariableA(name, buf, sizeof(buf));
	return len > 0;
}

bool env_equals(const char* name, const char* expected)
{
	char buf[64];
	DWORD len = GetEnvironmentVariableA(name, buf, sizeof(buf));
	if(len == 0 || len >= sizeof(buf))
		return false;
	buf[len] = '\0';
	return _stricmp(buf, expected) == 0;
}

bool env_is_truthy(const char* name)
{
	char buf[16];
	DWORD len = GetEnvironmentVariableA(name, buf, sizeof(buf));
	if(len == 0 || len >= sizeof(buf))
		return false;
	buf[len] = '\0';
	return buf[0] != '\0' && strcmp(buf, "0") != 0;
}

bool terminal_host_looks_unicode_safe()
{
	return env_present("WT_SESSION") ||
		env_equals("TERM_PROGRAM", "Windows_Terminal") ||
		env_present("ANSICON") ||
		env_equals("ConEmuANSI", "ON") ||
		env_present("TERM");
}

} // namespace

// ─── Filesystem Paths ────────────────────────────────────────────────────────

std::string getHomePath()
{
	// %USERPROFILE% (e.g. C:\Users\miner)
	char buf[MAX_PATH];
	DWORD len = GetEnvironmentVariableA("USERPROFILE", buf, sizeof(buf));
	if(len > 0 && len < sizeof(buf))
		return std::string(buf, len);
	return "C:\\";
}

static std::string ensureDir(const std::string& path)
{
	CreateDirectoryA(path.c_str(), nullptr); // ignore if exists
	return path;
}

std::string getConfigDir()
{
	// %APPDATA%\n0s (e.g. C:\Users\miner\AppData\Roaming\n0s)
	char buf[MAX_PATH];
	if(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, buf) == S_OK)
		return ensureDir(std::string(buf) + "\\n0s");
	return ensureDir(getHomePath() + "\\.n0s");
}

std::string getCacheDir()
{
	// %LOCALAPPDATA%\n0s (e.g. C:\Users\miner\AppData\Local\n0s)
	char buf[MAX_PATH];
	if(SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, buf) == S_OK)
		return ensureDir(std::string(buf) + "\\n0s");
	return ensureDir(getHomePath() + "\\.n0s\\cache");
}

// ─── Console ─────────────────────────────────────────────────────────────────

int getKey()
{
#if defined(_MSC_VER)
	if(_kbhit())
		return _getch();
#else
	// MinGW: use Windows Console API
	HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
	DWORD avail = 0;
	INPUT_RECORD ir;
	if(GetNumberOfConsoleInputEvents(hStdin, &avail) && avail > 0)
	{
		DWORD read = 0;
		if(PeekConsoleInputA(hStdin, &ir, 1, &read) && read > 0 &&
		   ir.EventType == KEY_EVENT && ir.Event.KeyEvent.bKeyDown)
		{
			ReadConsoleInputA(hStdin, &ir, 1, &read);
			return ir.Event.KeyEvent.uChar.AsciiChar;
		}
	}
#endif
	return -1;
}

void enableConsoleColors()
{
	(void)initConsole();
}

ConsoleCapabilities initConsole()
{
	if(g_console_caps_init)
		return g_console_caps;

	g_console_caps_init = true;
	g_console_caps = {};

	const bool no_color = env_is_truthy("NO_COLOR");
	const bool force_color = env_is_truthy("CLICOLOR_FORCE") || env_is_truthy("FORCE_COLOR") || env_is_truthy("N0S_FORCE_COLOR");
	const bool force_ascii = env_is_truthy("N0S_FORCE_ASCII");

	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	if(hOut != INVALID_HANDLE_VALUE)
	{
		DWORD mode = 0;
		if(GetConsoleMode(hOut, &mode))
		{
			g_console_caps.isTTY = true;
			if(SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
				g_console_caps.color = !no_color;
		}
	}

	const char* term = getenv("TERM");
	if(!g_console_caps.color && !no_color && (force_color || (term != nullptr && term[0] != '\0' && strcmp(term, "dumb") != 0)))
		g_console_caps.color = true;

	if(!force_ascii)
	{
		SetConsoleOutputCP(CP_UTF8);
		SetConsoleCP(CP_UTF8);
		g_console_caps.unicode = terminal_host_looks_unicode_safe();
	}

	return g_console_caps;
}

ConsoleCapabilities getConsoleCapabilities()
{
	return g_console_caps_init ? g_console_caps : initConsole();
}

void formatLocalTime(char* buf, size_t bufLen, const char* fmt, int64_t t)
{
	time_t tt = static_cast<time_t>(t);
	struct tm stime;
	localtime_s(&stime, &tt); // Windows: args reversed vs POSIX
	strftime(buf, bufLen, fmt, &stime);
}

// ─── Signals ─────────────────────────────────────────────────────────────────

void disableSigpipe()
{
	// No SIGPIPE on Windows — TCP send failures return SOCKET_ERROR
}

static void (*s_shutdownHandler)(int) = nullptr;

static BOOL WINAPI consoleCtrlHandler(DWORD ctrlType)
{
	if(s_shutdownHandler != nullptr &&
		(ctrlType == CTRL_C_EVENT || ctrlType == CTRL_BREAK_EVENT || ctrlType == CTRL_CLOSE_EVENT))
	{
		s_shutdownHandler(static_cast<int>(ctrlType));
		return TRUE;
	}
	return FALSE;
}

void installShutdownHandler(void (*handler)(int))
{
	s_shutdownHandler = handler;
	SetConsoleCtrlHandler(consoleCtrlHandler, TRUE);
}

// ─── Process / Browser ───────────────────────────────────────────────────────

void openBrowser(const char* url)
{
	ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOWNORMAL);
}

int spawnProcess(const char* path, const char* const argv[])
{
	// Build command line from argv
	std::string cmdLine;
	for(int i = 0; argv[i] != nullptr; i++)
	{
		if(i > 0) cmdLine += ' ';
		// Quote args containing spaces
		std::string arg(argv[i]);
		if(arg.find(' ') != std::string::npos)
			cmdLine += '"' + arg + '"';
		else
			cmdLine += arg;
	}

	STARTUPINFOA si = {};
	si.cb = sizeof(si);
	PROCESS_INFORMATION pi = {};

	if(!CreateProcessA(path, const_cast<char*>(cmdLine.c_str()),
		nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
	{
		return -1;
	}

	DWORD pid = pi.dwProcessId;
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	return static_cast<int>(pid);
}

// ─── Threading ───────────────────────────────────────────────────────────────

void setThreadName(const char* name)
{
	// SetThreadDescription requires Win10 1607+ — may not be in older MinGW headers
	// Use runtime dynamic loading to be safe
	typedef HRESULT(WINAPI* SetThreadDescriptionFn)(HANDLE, PCWSTR);
	static auto fn = reinterpret_cast<SetThreadDescriptionFn>(
		GetProcAddress(GetModuleHandleA("kernel32.dll"), "SetThreadDescription"));
	if(!fn) return;

	int wLen = MultiByteToWideChar(CP_UTF8, 0, name, -1, nullptr, 0);
	if(wLen <= 0) return;
	std::wstring wname(wLen, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, name, -1, &wname[0], wLen);
	fn(GetCurrentThread(), wname.c_str());
}

// ─── Sockets ─────────────────────────────────────────────────────────────────

void sockInit()
{
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
}

void sockCleanup()
{
	WSACleanup();
}

// ─── Platform Detection ──────────────────────────────────────────────────────

const char* platformName()
{
	return "windows";
}

} // namespace platform
} // namespace n0s

#endif // _WIN32
