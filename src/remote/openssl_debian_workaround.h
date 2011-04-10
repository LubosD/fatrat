/*
 Workaround for systems without SSLv2 support in OpenSSL (such as Debian)
 */

#ifndef OPENSSL_DEBIAN_WORKAROUND_H
#define OPENSSL_DEBIAN_WORKAROUND_H

#include <openssl/ssl.h>
#include <iostream>
#ifdef OPENSSL_NO_SSL2

inline const SSL_METHOD *SSLv2_method(void)
{
	std::cerr << "stub: SSLv2_method\n";
	return 0;
}

inline const SSL_METHOD *SSLv2_server_method(void)
{
	std::cerr << "stub: SSLv2_server_method\n";
	return 0;
}

inline const SSL_METHOD *SSLv2_client_method(void)
{
	std::cerr << "stub: SSLv2_client_method\n";
	return 0;
}

#endif

#endif // OPENSSL_DEBIAN_WORKAROUND_H
