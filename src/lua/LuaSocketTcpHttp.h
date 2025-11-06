#pragma once
#include "client/http/requestmanager/CurlError.h"
#include "common/String.h"
#include <curl/curl.h>
#include <optional>
#include <vector>

namespace LuaSocket
{
	enum Status
	{
		StatusReady,
		StatusConnecting,
		StatusConnected,
		StatusDead,
	};

	struct TCPSocket
	{
		CURL *easy;
		CURLM *multi;
		char errorBuf[CURL_ERROR_SIZE];
		Status status;
		bool timeoutIndefinite;
		bool blocking;
		double timeout;
		std::vector<char> recvBuf;
		size_t stashedLen;
		bool readClosed;
		bool writeClosed;
		struct AlpnProtos
		{
			std::vector<ByteString> protos;
			std::optional<std::vector<const char *>> protoPtrs;
		};
		std::optional<AlpnProtos> alpnProtos;
	};

	CURLcode SetAlpn(CURL *easy, void *sslCtx, void *userdata);
}
