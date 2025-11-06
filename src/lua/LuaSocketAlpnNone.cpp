#include "LuaSocketTcpHttp.h"
#include <curl/curl.h>
#include <iostream>

namespace LuaSocket
{
	CURLcode SetAlpn(CURL *, void *, void *)
	{
		std::cerr << "ALPN configuration glue code for current TLS backend not compiled in" << std::endl;
		return CURLE_SSL_CONNECT_ERROR;
	}
}
