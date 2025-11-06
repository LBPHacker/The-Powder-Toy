#include "LuaSocketTcpHttp.h"
#include <mbedtls/ssl.h>
#include <iostream>

namespace LuaSocket
{
	CURLcode SetAlpn(CURL *easy, void *sslCtx, void *userdata)
	{
		auto die = [](const char *why) {
			std::cerr << "SetAlpn: " << why << std::endl;
			return CURLE_SSL_CONNECT_ERROR;
		};
		char *privatePtr;
		if (curl_easy_getinfo(easy, CURLINFO_PRIVATE, &privatePtr) != CURLE_OK) return die("failed to query private data");
		if (static_cast<void *>(privatePtr) != userdata)
		{
			// Libcurl might call this function with easy handles other than the one we're trying to
			// configure ALPN for (think proxying); only configure ALPN for the one we're actually interested in.
			return CURLE_OK;
		}
		struct curl_tlssessioninfo *session;
		if (curl_easy_getinfo(easy, CURLINFO_TLS_SSL_PTR, &session) != CURLE_OK) return die("failed to query private data");
		if (session->backend != CURLSSLBACKEND_MBEDTLS) return die("dare not interact with TLS object due to kind mismatch");
		auto *mbedtlsCtx = reinterpret_cast<const mbedtls_ssl_context *>(session->internals);
		auto *mbedtlsConfig = static_cast<mbedtls_ssl_config *>(sslCtx);
		if (!(mbedtlsCtx && mbedtlsCtx->private_conf == mbedtlsConfig)) return die("dare not interact with TLS object due to config pointer mismatch");
		auto *tcps = static_cast<TCPSocket *>(userdata);
		if (tcps->alpnProtos)
		{
			if (!tcps->alpnProtos->protoPtrs)
			{
				tcps->alpnProtos->protoPtrs = std::vector<const char *>();
				for (auto &proto : tcps->alpnProtos->protos)
				{
					tcps->alpnProtos->protoPtrs->push_back(proto.c_str());
				}
				tcps->alpnProtos->protoPtrs->push_back(nullptr);
			}
			if (mbedtls_ssl_conf_alpn_protocols(mbedtlsConfig, tcps->alpnProtos->protoPtrs->data()))
			{
				return die("mbedtls_ssl_conf_alpn_protocols failed");
			}
		}
		return CURLE_OK;
	}
}
