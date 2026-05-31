#pragma once

/**
 * socks.hpp — Cross-platform socket primitives
 *
 * Provides SOCKET type, INVALID_SOCKET, SOCKET_ERROR constants,
 * and sock_close/sock_strerror wrappers for both POSIX and Winsock.
 */

#ifdef _WIN32
// ─── Windows (Winsock2) ──────────────────────────────────────────────────────

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

// SOCKET, INVALID_SOCKET, SOCKET_ERROR already defined by winsock2.h

inline void sock_close(SOCKET s)
{
	shutdown(s, SD_BOTH);
	closesocket(s);
}

inline const char* sock_strerror(char* buf, size_t len)
{
	int err = WSAGetLastError();
	FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr, err, 0, buf, static_cast<DWORD>(len), nullptr);
	return buf;
}

inline const char* sock_gai_strerror(int err, char* buf, size_t len)
{
	// gai_strerror on Windows is not thread-safe, use FormatMessage instead
	FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr, static_cast<DWORD>(err), 0, buf, static_cast<DWORD>(len), nullptr);
	return buf;
}

#else
// ─── POSIX ───────────────────────────────────────────────────────────────────

#include <arpa/inet.h>
#include <cerrno>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>

typedef int SOCKET;

#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)

inline void sock_close(SOCKET s)
{
	shutdown(s, SHUT_RDWR);
	close(s);
}

inline const char* sock_strerror(char* buf, size_t len)
{
	buf[0] = '\0';
	return strerror_r(errno, buf, len);
}

inline const char* sock_gai_strerror(int err, char* buf, [[maybe_unused]] size_t len)
{
	buf[0] = '\0';
	return gai_strerror(err);
}

#endif // _WIN32
