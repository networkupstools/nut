/* ssl.c - Interface to OpenSSL for upsd

   Copyright (C)
	2002	Russell Kroll <rkroll@exploits.org>
	2008	Arjen de Korte <adkorte-guest@alioth.debian.org>

   based on the original implementation:

   Copyright (C) 2002  Technorama Ltd. <oss-list-ups@technorama.net>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include <sys/types.h>
#ifndef WIN32
#include <netinet/in.h>
#include <sys/socket.h>
#else
#include <winsock2.h>
#endif

#include "upsd.h"
#include "neterr.h"
#include "ssl.h"

char	*certfile = NULL;

static int	ssl_initialized = 0;

#ifndef HAVE_SSL

/* stubs for non-ssl compiles */
void net_starttls(ctype_t *client, int numarg, const char **arg)
{
	send_err(client, NUT_ERR_FEATURE_NOT_SUPPORTED);
	return;
}

int ssl_write(ctype_t *client, const char *buf, size_t buflen)
{
	upslogx(LOG_ERR, "ssl_write called but SSL wasn't compiled in");
	return -1;
}

int ssl_read(ctype_t *client, char *buf, size_t buflen)
{
	upslogx(LOG_ERR, "ssl_read called but SSL wasn't compiled in");
	return -1;
}

void ssl_init(void)
{
	ssl_initialized = 0;	/* keep gcc quiet */
}

void ssl_finish(ctype_t *client)
{
	if (client->ssl) {
		upslogx(LOG_ERR, "ssl_finish found active SSL connection but SSL wasn't compiled in");
	}
}

#else

static SSL_CTX	*ssl_ctx = NULL;

static void ssl_debug(void)
{
	int	e;
	char	errmsg[SMALLBUF];

	while ((e = ERR_get_error()) != 0) {
		ERR_error_string_n(e, errmsg, sizeof(errmsg));
		upsdebugx(1, "ssl_debug: %s", errmsg);
	}
}

void net_starttls(ctype_t *client, int numarg, const char **arg)
{
	if (client->ssl) {
		send_err(client, NUT_ERR_ALREADY_SSL_MODE);
		return;
	}

	if ((!ssl_ctx) || (!certfile) || (!ssl_initialized)) {
		send_err(client, NUT_ERR_FEATURE_NOT_CONFIGURED);
		return;
	}

	if (!sendback(client, "OK STARTTLS\n")) {
		return;
	}

	client->ssl = SSL_new(ssl_ctx);

	if (!client->ssl) {
		upslog_with_errno(LOG_ERR, "SSL_new failed\n");
		ssl_debug();
		return;
	}

	if (SSL_set_fd(client->ssl, client->sock_fd) != 1) {
		upslog_with_errno(LOG_ERR, "SSL_set_fd failed\n");
		ssl_debug();
	}
}

void ssl_init(void)
{
	if (!certfile) {
		return;
	}

	check_perms(certfile);

	SSL_library_init();
	SSL_load_error_strings();
	OpenSSL_add_ssl_algorithms();

	if ((ssl_ctx = SSL_CTX_new(TLSv1_server_method())) == NULL) {
		fatal_with_errno(EXIT_FAILURE, "SSL_CTX_new");
	}

	if (SSL_CTX_use_RSAPrivateKey_file(ssl_ctx, certfile, SSL_FILETYPE_PEM) != 1) {
		ssl_debug();
		upslogx(LOG_ERR, "SSL_CTX_use_RSAPrivateKey_file(%s) failed", certfile);
		return;
	}

	if (SSL_CTX_use_certificate_chain_file(ssl_ctx, certfile) != 1) {
		upslogx(LOG_ERR, "SSL_CTX_use_certificate_chain_file(%s) failed", certfile);
		return;
	}

	if (SSL_CTX_check_private_key(ssl_ctx) != 1) {
		upslogx(LOG_ERR, "SSL_CTX_check_private_key(%s) failed", certfile);
		return;
	}

	SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_NONE, NULL);

	if (SSL_CTX_set_cipher_list(ssl_ctx, "HIGH:@STRENGTH") != 1) {
		fatalx(EXIT_FAILURE, "SSL_CTX_set_cipher_list");
	}

	ssl_initialized = 1;
}

static int ssl_error(SSL *ssl, int ret)
{
	int	e;

	e = SSL_get_error(ssl, ret);

	switch (e)
	{
	case SSL_ERROR_WANT_READ:
		upsdebugx(1, "ssl_error() ret=%d SSL_ERROR_WANT_READ", ret);
		break;

	case SSL_ERROR_WANT_WRITE:
		upsdebugx(1, "ssl_error() ret=%d SSL_ERROR_WANT_WRITE", ret);
		break;

	case SSL_ERROR_SYSCALL:
		if (ret == 0 && ERR_peek_error() == 0) {
			upsdebugx(1, "ssl_error() EOF from client");
		} else {
			upsdebugx(1, "ssl_error() ret=%d SSL_ERROR_SYSCALL", ret);
		}
		break;

	default:
		upsdebugx(1, "ssl_error() ret=%d SSL_ERROR %d", ret, e);
		ssl_debug();
	}

	return -1;
}
	
static int ssl_accept(ctype_t *client)
{
	int	ret;

	ret = SSL_accept(client->ssl);

	switch (ret)
	{
	case 1:
		client->ssl_connected = 1;
		upsdebugx(3, "SSL connected");
		return 0;
		
	case 0:
	case -1:
		return ssl_error(client->ssl, ret);
	}
	
	upslog_with_errno(LOG_ERR, "Unknown return value from SSL_accept");
	return -1;
}

int ssl_read(ctype_t *client, char *buf, size_t buflen)
{
	int	ret;

	if (!client->ssl_connected) {
		if (ssl_accept(client) != 0)
			return -1;
	}

	ret = SSL_read(client->ssl, buf, buflen);

	if (ret < 1) {
		ssl_error(client->ssl, ret);
		return -1;
	}

	return ret;
}

int ssl_write(ctype_t *client, const char *buf, size_t buflen)
{
	int	ret;

	ret = SSL_write(client->ssl, buf, buflen);

	upsdebugx(5, "ssl_write ret=%d", ret);

	return ret;
}

void ssl_finish(ctype_t *client)
{
	if (client->ssl) {
		SSL_free(client->ssl);
	}
}

#endif	/* HAVE_SSL */
