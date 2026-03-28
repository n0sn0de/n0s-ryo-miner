#pragma once

#include <stdlib.h>
#include <microhttpd.h>

// MHD_Result was added in libmicrohttpd 0.9.71.
// Older versions (Ubuntu 18.04/20.04) use int for callback return types.
#if !defined(MHD_VERSION) || MHD_VERSION < 0x00097100
typedef int MHD_Result;
#endif

struct MHD_Daemon;
struct MHD_Connection;

class httpd
{
  public:
	static httpd* inst()
	{
		if(oInst == nullptr)
			oInst = new httpd;
		return oInst;
	};

	bool start_daemon();

  private:
	httpd();
	static httpd* oInst;

	static MHD_Result req_handler(void* cls,
		MHD_Connection* connection,
		const char* url,
		const char* method,
		const char* version,
		const char* upload_data,
		size_t* upload_data_size,
		void** ptr);

	MHD_Daemon* d;
};
