#pragma once

#include "socks.hpp"
#include <atomic>

class jpsock;

class base_socket
{
  public:
	virtual ~base_socket() = default;
	[[nodiscard]] virtual bool set_hostname(const char* sAddr) = 0;
	[[nodiscard]] virtual bool connect() = 0;
	[[nodiscard]] virtual int recv(char* buf, unsigned int len) = 0;
	[[nodiscard]] virtual bool send(const char* buf) = 0;
	virtual void close(bool free) = 0;

  protected:
	std::atomic<bool> sock_closed;
};

class plain_socket : public base_socket
{
  public:
	plain_socket(jpsock* err_callback);

	[[nodiscard]] bool set_hostname(const char* sAddr) override;
	[[nodiscard]] bool connect() override;
	[[nodiscard]] int recv(char* buf, unsigned int len) override;
	[[nodiscard]] bool send(const char* buf) override;
	void close(bool free) override;

  private:
	jpsock* pCallback;
	addrinfo* pSockAddr;
	addrinfo* pAddrRoot;
	SOCKET hSocket;
};

typedef struct ssl_ctx_st SSL_CTX;
typedef struct bio_st BIO;
typedef struct ssl_st SSL;

class tls_socket : public base_socket
{
  public:
	tls_socket(jpsock* err_callback);

	[[nodiscard]] bool set_hostname(const char* sAddr) override;
	[[nodiscard]] bool connect() override;
	[[nodiscard]] int recv(char* buf, unsigned int len) override;
	[[nodiscard]] bool send(const char* buf) override;
	void close(bool free) override;

  private:
	void init_ctx();
	void print_error();

	jpsock* pCallback;

	SSL_CTX* ctx = nullptr;
	BIO* bio = nullptr;
	SSL* ssl = nullptr;
};
