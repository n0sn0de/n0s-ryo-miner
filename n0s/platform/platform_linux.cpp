/**
 * platform_linux.cpp — Linux implementations of platform abstraction
 */

#ifndef _WIN32

#include "platform.hpp"

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <pthread.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

namespace n0s
{
namespace platform
{

// ─── Filesystem Paths ────────────────────────────────────────────────────────

std::string getHomePath()
{
	const char* home = getenv("HOME");
	if(home != nullptr && home[0] != '\0')
		return home;

	struct passwd* pw = getpwuid(getuid());
	if(pw != nullptr && pw->pw_dir != nullptr)
		return pw->pw_dir;

	return ".";
}

static std::string ensureDir(const std::string& path)
{
	mkdir(path.c_str(), 0755); // ignore failure (already exists is fine)
	return path;
}

std::string getConfigDir()
{
	const char* xdg = getenv("XDG_CONFIG_HOME");
	if(xdg != nullptr && xdg[0] != '\0')
		return ensureDir(std::string(xdg) + "/n0s");
	return ensureDir(getHomePath() + "/.config/n0s");
}

std::string getCacheDir()
{
	const char* xdg = getenv("XDG_CACHE_HOME");
	if(xdg != nullptr && xdg[0] != '\0')
		return ensureDir(std::string(xdg) + "/n0s");
	return ensureDir(getHomePath() + "/.cache/n0s");
}

// ─── Console ─────────────────────────────────────────────────────────────────

int getKey()
{
	struct termios oldattr, newattr;
	int ch;
	if(tcgetattr(STDIN_FILENO, &oldattr) != 0)
		return -1;
	newattr = oldattr;
	newattr.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &newattr);
	ch = getchar();
	tcsetattr(STDIN_FILENO, TCSANOW, &oldattr);
	return ch;
}

void enableConsoleColors()
{
	// No-op on Linux — ANSI colors are natively supported
}

void formatLocalTime(char* buf, size_t bufLen, const char* fmt, int64_t t)
{
	time_t tt = static_cast<time_t>(t);
	struct tm stime;
	localtime_r(&tt, &stime);
	strftime(buf, bufLen, fmt, &stime);
}

// ─── Signals ─────────────────────────────────────────────────────────────────

void disableSigpipe()
{
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_IGN;
	sa.sa_flags = 0;
	sigaction(SIGPIPE, &sa, nullptr);
}

void installShutdownHandler(void (*handler)(int))
{
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = handler;
	sa.sa_flags = 0;
	sigaction(SIGINT, &sa, nullptr);
	sigaction(SIGTERM, &sa, nullptr);
}

// ─── Process / Browser ───────────────────────────────────────────────────────

void openBrowser(const char* url)
{
	pid_t pid = fork();
	if(pid == 0)
	{
		// Child: redirect stdout/stderr to /dev/null
		int devnull = open("/dev/null", O_WRONLY);
		if(devnull >= 0)
		{
			dup2(devnull, STDOUT_FILENO);
			dup2(devnull, STDERR_FILENO);
			close(devnull);
		}
		execlp("xdg-open", "xdg-open", url, nullptr);
		_exit(127); // exec failed
	}
	// Parent: fire and forget (no waitpid needed for detached xdg-open)
}

int spawnProcess(const char* path, const char* const argv[])
{
	pid_t pid = fork();
	if(pid < 0)
		return -1;
	if(pid == 0)
	{
		// Child
		execvp(path, const_cast<char* const*>(argv));
		_exit(127); // exec failed
	}
	return static_cast<int>(pid);
}

// ─── Threading ───────────────────────────────────────────────────────────────

void setThreadName(const char* name)
{
	// pthread_setname_np is limited to 15 chars + NUL on Linux
	char buf[16];
	strncpy(buf, name, 15);
	buf[15] = '\0';
	pthread_setname_np(pthread_self(), buf);
}

// ─── Sockets ─────────────────────────────────────────────────────────────────

void sockInit()
{
	// No-op on Linux/POSIX
}

void sockCleanup()
{
	// No-op on Linux/POSIX
}

// ─── Platform Detection ──────────────────────────────────────────────────────

const char* platformName()
{
	return "linux";
}

} // namespace platform
} // namespace n0s

#endif // !_WIN32
