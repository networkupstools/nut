/*
 * blazer_soi.c: support for Megatec/Q1 serial protocol over IP (yport)
 *
 * A document describing the protocol implemented by this driver can be
 * found online at "http://www.networkupstools.org/protocols/megatec.html".
 *
 * Copyright (C) 2014 - Alessandro Mauro <alez@maetech.it>
 *
 * Based on blazer_ser by Arjen de Korte <adkorte-guest@alioth.debian.org>
 * Copyright (C) 2008
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "main.h"
#include "blazer.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#define DRIVER_NAME	"Megatec/Q1 protocol serial driver over IP"
#define DRIVER_VERSION	"1.00"

static long ans_timeout =0;

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Alessandro Mauro <alez@maetech.it>",
	DRV_BETA,
	{ NULL }
};

//#define SER_WAIT_SEC	1 /* 3 seconds for Best UPS */

static int	dumpdone = 0, online = 1, outlet = 1;
static int	offdelay = 120, ondelay = 30;

static PCONF_CTX_t	sock_ctx;

static time_t	last_poll = 0, last_heard = 0,
		last_ping = 0, last_connfail = 0;

static int blazer_soi_connect(int port)
{
	int	ret, fd;

/* risoluzione dell'hostname */
	struct sockaddr_in sa;
	sa.sin_family=AF_INET;
	sa.sin_port=htons(port);
	struct hostent *h;
	h=gethostbyname(device_path);
	if (h==0) {
		upslog_with_errno(LOG_ERR, "Failed to get host address for UPS [%s]", device_path);
		return -1;
	}

	bcopy(h->h_addr,&sa.sin_addr,h->h_length);

	fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP); //tcp socket

	if (fd < 0) {
		upslog_with_errno(LOG_ERR, "Can't create socket for UPS [%s] at address [%s]", device_path);
		return -1;
	}

/* set socket timeout */
//	struct timeval tv;
//        tv.tv_usec = 1000;
//        if(setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0){
//        if(setsockopt(fd, IPPROTO_TCP, SO_RCVTIMEO, &tv,sizeof(tv)) < 0){
//            upslog_with_errno(LOG_ERR, "Error setting the socket timeout for UPS [%s]",device_path);
//        }

	ret = connect(fd, (struct sockaddr *) &sa, sizeof(sa));

	if (ret < 0) {
		time_t	now;

		close(fd);

		/* rate-limit complaints - don't spam the syslog */
		time(&now);

		if (difftime(now, last_connfail) < 60) {
			return -1;
		}

		last_connfail = now;

		upslog_with_errno(LOG_ERR, "Can't connect to UPS [%s]", device_path);
		return -1;
	}

	ret = fcntl(fd, F_GETFL, 0);

	if (ret < 0) {
		upslog_with_errno(LOG_ERR, "fcntl get on UPS [%s] failed", device_path);
		close(fd);
		return -1;
	}

	ret = fcntl(fd, F_SETFL, ret | O_NDELAY);

	if (ret < 0) {
		upslog_with_errno(LOG_ERR, "fcntl set O_NDELAY on UPS [%s] failed", device_path);
		close(fd);
		return -1;
	}

	pconf_init(&sock_ctx, NULL);

	/* set ups.status to "WAIT" while waiting for the driver response to dumpcmd */
	/* im not waiting for response, so maybe better change ?? */
	dstate_setinfo("ups.status", "WAIT");

	upslogx(LOG_INFO, "Connected to UPS at address [%s]", device_path);
	return fd;
}

static int soi_sendline(const char *buf)
{
	int	ret;

	if (upsfd < 0) {
		return -1;	/* failed */

	}

	ret = write(upsfd, buf, strlen(buf));

	if (ret == (int)strlen(buf)) {

		return 0;

	}

	upslog_with_errno(LOG_NOTICE, "Send to UPS [%s] failed", device_path);

	return -1;	/* failed */

}


static int soi_readline(char *buf, size_t buflen)
{
	int	i=0, ret;

	if (upsfd < 0) {

		return -1;	/* failed */
	}

memset(buf,'\0',buflen);

time_t now, start;
time(&start);

do {
	time(&now);
	ret=recv(upsfd, buf+i, sizeof(buf),0);
	if (ret>0){
		i+=ret;
	}
	if (difftime(now,start)>((double)ans_timeout/1000.0)) {
		upslogx(LOG_NOTICE, "Timeout: no answer");
		return -1;
	}
} while (buf[i-1]!='\r');
	return i;
}



/*
 * Generic command processing function. Send a command and read a reply.
 * Returns < 0 on error, 0 on timeout and the number of bytes read on
 * success.
 */
int blazer_command(const char *cmd, char *buf, size_t buflen)
{
#ifndef TESTING
	int	ret;

	ret= soi_sendline(cmd);


	if (ret < 0) {
		upsdebugx(3, "send: %s", ret ? strerror(errno) : "timeout");
		return ret;
	}

	upsdebugx(3, "send: '%.*s'", (int)strcspn(cmd, "\r"), cmd);
	
	ret = soi_readline(buf, buflen);

	if (ret < 0) {
		upsdebugx(3, "read: %s", ret ? strerror(errno) : "timeout");
		return ret;
	}

	upsdebugx(3, "read: '%.*s'", (int)strcspn(buf, "\r"), buf);
	return ret;
#else
	const struct {
		const char	*cmd;
		const char	*answer;
	} testing[] = {
		{ "Q1\r", "(215.0 195.0 230.0 014 49.0 2.27 30.0 00101000\r" },
		{ "F\r",  "#230.0 000 024.0 50.0\r" },
		{ "I\r",  "#NOT_A_LIVE_UPS  TESTING    TESTING   \r" },
		{ NULL }
	};

	int	i;

	memset(buf, 0, buflen);

	for (i = 0; cmd && testing[i].cmd; i++) {

		if (strcasecmp(cmd, testing[i].cmd)) {
			continue;
		}

		return snprintf(buf, buflen, "%s", testing[i].answer);
	}

	return snprintf(buf, buflen, "%s", testing[i].cmd);
#endif
}


void upsdrv_help(void)
{
	printf("******\n");
	printf("Blazer_SOI - a Serial Over IP version of Blazer\n");
	printf("by Alessandro Mauro <alez@maetech.it>\n");
	printf("To know: 'port' is the ip or hostname of the remote serial gateway;\n");
    printf(" 'tcpport' is the port to connect, default 7970;\n");
	printf(" 'timeout' is the amount of milliseconds to wait for the remote ups to reply, min 100 default 1000.\n");
	printf("For more info read blazer_ser manual ('man 8 blazer_ser')\n");
}


void upsdrv_makevartable(void)
{
	addvar(VAR_VALUE, "tcpport","Set TCP port for remote serial (YPort)");
	addvar(VAR_VALUE, "timeout","Set timeout (ms) for remote ups reply");
	blazer_makevartable();
}


void upsdrv_initups(void)
{
#ifndef TESTING

	const char	*val;
	int port=0;
	

	val = getval("tcpport");
	if (val) port=strtol(val, NULL, 10);

	printf("port = %d \n",port);
	
	if (port<=0) port=7970;
	upsfd = blazer_soi_connect(port);
	val = getval("timeout");
	if (val) ans_timeout=strtol(val,NULL,10);
	else ans_timeout=0;
	printf("t.o = %d \n",ans_timeout);
	if (ans_timeout < 100) ans_timeout=1000;

	printf("timeout = %d \n",ans_timeout);
	

#endif
	blazer_initups();
}


void upsdrv_initinfo(void)
{
	blazer_initinfo();
}


void upsdrv_cleanup(void)
{
#ifndef TESTING
	close(upsfd);
#endif
}
