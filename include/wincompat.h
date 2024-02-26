#ifndef NUT_WINCOMPAT_H
#define NUT_WINCOMPAT_H 1

/*
   Copyright (C) 2001 Andrew Delpha (delpha@computer.org)

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, WRITE to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


/*
   This header is provided to map Windows system calls to be compatible
   with the NUT code.
*/

#include "common.h"
#include <limits.h>

/* This value is defined in the error.h file of the libusb-win32 sources
 * FIXME: Should only be relevant for builds WITH_LIBUSB_0_1 - #ifdef it so?
 * Conflicts with e.g. /msys64/mingw64/include/errno.h which defines it as 138
 */
#ifndef ETIMEDOUT
# define ETIMEDOUT 116
#endif

#define random() rand()
typedef const char * uid_t;
struct passwd {
	const char		*pw_name;
	int			pw_uid;	/* Fake value, alwaus set to 0 */
};

uid_t getuid(void);
struct passwd *getpwuid(uid_t uid);
char *getpass( const char *prompt);

#define system(a) win_system(a)
#define sleep(n) Sleep(1000 * n)
char * strtok_r(char *str, const char *delim, char **saveptr);
char * filter_path(const char * source);

/* Network compatibility */

/* This is conditional because read/write are generic in unix, and this will make them network specific */
#ifdef W32_NETWORK_CALL_OVERRIDE
#define close(h) sktclose(h)
#define read(h,b,s) sktread(h,b,s)
#define write(h,b,s) sktwrite( h ,(char *) b , s )
#define connect(h,n,l) sktconnect(h,n,l)
#endif

#ifndef strcasecmp
#define strcasecmp(a,b) _stricmp(a,b)
#endif
#ifndef snprintf
#define snprintf _snprintf
#endif
#ifndef vsnprintf
#define vsnprintf _vsnprintf
#endif

int sktconnect(int fh, struct sockaddr * name, int len);
int sktread(int fh, char *buf, int size);
int sktwrite(int fh, char *buf, int size);
int sktclose(int fh);

#if ! HAVE_INET_NTOP
/* NOTE: This fallback can get used on systems that do have inet_ntop()
 * but their antivirus precluded the configure-script test program linking.
 * A symptom of that would be:
 *   warning: 'inet_ntop' redeclared without dllimport attribute:
 *      previous dllimport ignored [-Wattributes]
 * ...and adding the attributes would cause a conflict of private fallback
 * and system DLL symbols. So the lesser of two evils, we use fallback.
 * People who see the warning should however fix their build environment
 * and use the native OS-provided implementation ;)
 */
# if (0)
/* Some old winapi? or just sloppy original commits?
 * Why is this here at all if there's something per ws2tcpip.h -
 * maybe should be configure-detected?
 */
const char* inet_ntop(int af, const void* src, char* dst, int cnt);
# else
const char* inet_ntop(int af, const void* src, char* dst, size_t /* socklen_t */ cnt);
# endif
#endif

#if ! HAVE_INET_PTON
int inet_pton(int af, const char *src, void *dst);
#endif

/* from the MSDN getaddrinfo documentation : */
#define EAI_AGAIN	WSATRY_AGAIN
#define EAI_BADFLAGS	WSAEINVAL
#define EAI_FAIL	WSANO_RECOVERY
#define EAI_FAMILY	WSAEAFNOSUPPORT
#define EAI_MEMORY	WSA_NOT_ENOUGH_MEMORY
#define EAI_NONAME	WSAHOST_NOT_FOUND
#define EAI_SERVICE	WSATYPE_NOT_FOUND
#define EAI_SOCKTYPE	WSAESOCKTNOSUPPORT
/* not from MS docs : */
#define EAI_SYSTEM	WSANO_RECOVERY
#ifndef EAFNOSUPPORT
# define EAFNOSUPPORT	WSAEAFNOSUPPORT
#endif

/* "system" function */
int win_system(const char * command);

/* syslog compatibility */

void syslog(int priority, const char *fmt, ...);

extern const char * EventLogName;

/* Signal emulation via named pipe */
typedef struct pipe_conn_s {
	HANDLE		handle;
	OVERLAPPED	overlapped;
	char		buf[LARGEBUF];
	struct pipe_conn_s	*prev;
	struct pipe_conn_s	*next;
} pipe_conn_t;

extern pipe_conn_t *pipe_connhead;
extern OVERLAPPED pipe_connection_overlapped;
void pipe_create(const char * pipe_name);
void pipe_connect();
void pipe_disconnect(pipe_conn_t *conn);
int pipe_ready(pipe_conn_t *conn);
int send_to_named_pipe(const char * pipe_name, const char * data);

#define COMMAND_FSD "COMMAND_FSD"
#define COMMAND_STOP "COMMAND_STOP"
#define COMMAND_RELOAD "COMMAND_RELOAD"

/* serial function compatibility */

int w32_setcomm ( serial_handler_t * h, int * flags );
int w32_getcomm ( serial_handler_t * h, int * flags );
int tcsendbreak (serial_handler_t * sh, int duration);

typedef unsigned char   cc_t;
typedef unsigned int    speed_t;
typedef unsigned int    tcflag_t;

#define NCCS 19
struct termios {
	tcflag_t c_iflag;	/* input mode flags */
	tcflag_t c_oflag;	/* output mode flags */
	tcflag_t c_cflag;	/* control mode flags */
	tcflag_t c_lflag;	/* local mode flags */
	cc_t c_line;		/* line discipline */
	cc_t c_cc[NCCS]; 	/* control characters */
	speed_t c_ispeed;	/* input speed */
	speed_t c_ospeed;	/* output speed */
};

serial_handler_t * w32_serial_open (const char *name, int flags);
int w32_serial_close (serial_handler_t * sh);
int w32_serial_write (serial_handler_t * sh, const void *ptr, size_t len);
int w32_serial_read (serial_handler_t * sh, void *ptr, size_t ulen, DWORD timeout);
int tcgetattr (serial_handler_t * sh, struct termios *t);
int tcsetattr (serial_handler_t * sh, int action, const struct termios *t);
int tcflush (serial_handler_t * sh, int queue);
#define HAVE_CFSETISPEED
void cfsetispeed(struct termios * t, speed_t speed);
void cfsetospeed(struct termios * t, speed_t speed);
speed_t cfgetispeed(const struct termios *t);
speed_t cfgetospeed(const struct termios *t);

#define _POSIX_VDISABLE '\0'

/* c_cc characters */
#define VINTR 0
#define VQUIT 1
#define VERASE 2
#define VKILL 3
#define VEOF 4
#define VTIME 5
#define VMIN 6
#define VSWTC 7
#define VSTART 8
#define VSTOP 9
#define VSUSP 10
#define VEOL 11
#define VREPRINT 12
#define VDISCARD 13
#define VWERASE 14
#define VLNEXT 15
#define VEOL2 16

/* c_iflag bits */
#define IGNBRK  0000001
#define BRKINT  0000002
#define IGNPAR  0000004
#define PARMRK  0000010
#define INPCK   0000020
#define ISTRIP  0000040
#define INLCR   0000100
#define IGNCR   0000200
#define ICRNL   0000400
#define IUCLC   0001000
#define IXON    0002000
#define IXANY   0004000
#define IXOFF   0010000
#define IMAXBEL 0020000
#define IUTF8   0040000

/* c_oflag bits */
#define OPOST   0000001
#define OLCUC   0000002
#define ONLCR   0000004
#define OCRNL   0000010
#define ONOCR   0000020
#define ONLRET  0000040
#define OFILL   0000100
#define OFDEL   0000200
#define NLDLY   0000400
#define   NL0   0000000
#define   NL1   0000400
#define CRDLY   0003000
#define   CR0   0000000
#define   CR1   0001000
#define   CR2   0002000
#define   CR3   0003000
#define TABDLY  0014000
#define   TAB0  0000000
#define   TAB1  0004000
#define   TAB2  0010000
#define   TAB3  0014000
#define   XTABS 0014000
#define BSDLY   0020000
#define   BS0   0000000
#define   BS1   0020000
#define VTDLY   0040000
#define   VT0   0000000
#define   VT1   0040000
#define FFDLY   0100000
#define   FF0   0000000
#define   FF1   0100000

/* c_cflag bit meaning */
#define CBAUD   0010017
#define  B0     0000000         /* hang up */
#define  B50    0000001
#define  B75    0000002
#define  B110   0000003
#define  B134   0000004
#define  B150   0000005
#define  B200   0000006
#define  B300   0000007
#define  B600   0000010
#define  B1200  0000011
#define  B1800  0000012
#define  B2400  0000013
#define  B4800  0000014
#define  B9600  0000015
#define  B19200 0000016
#define  B38400 0000017
#define EXTA B19200
#define EXTB B38400
#define CSIZE   0000060
#define   CS5   0000000
#define   CS6   0000020
#define   CS7   0000040
#define   CS8   0000060
#define CSTOPB  0000100
#define CREAD   0000200
#define PARENB  0000400
#define PARODD  0001000
#define HUPCL   0002000
#define CLOCAL  0004000
#define CBAUDEX 0010000
#define    BOTHER 0010000
#define    B57600 0010001
#define   B115200 0010002
#define   B230400 0010003
#define   B460800 0010004
#define   B500000 0010005
#define   B576000 0010006
#define   B921600 0010007
#define  B1000000 0010010
#define  B1152000 0010011
#define  B1500000 0010012
#define  B2000000 0010013
#define  B2500000 0010014
#define  B3000000 0010015
#define  B3500000 0010016
#define  B4000000 0010017
#define CIBAUD    002003600000	/* input baud rate */
#define CMSPAR    010000000000	/* mark or space (stick) parity */
#define CRTSCTS   020000000000	/* flow control */

#define IBSHIFT   16		/* Shift from CBAUD to CIBAUD */

/* c_lflag bits */
#define ISIG    0000001
#define ICANON  0000002
#define XCASE   0000004
#define ECHO    0000010
#define ECHOE   0000020
#define ECHOK   0000040
#define ECHONL  0000100
#define NOFLSH  0000200
#define TOSTOP  0000400
#define ECHOCTL 0001000
#define ECHOPRT 0002000
#define ECHOKE  0004000
#define FLUSHO  0010000
#define PENDIN  0040000
#define IEXTEN  0100000

/* tcflow() and TCXONC use these */
#define TCOOFF          0
#define TCOON           1
#define TCIOFF          2
#define TCION           3

/* tcflush() and TCFLSH use these */
#define TCIFLUSH        0
#define TCOFLUSH        1
#define TCIOFLUSH       2

/* tcsetattr uses these */
#define TCSANOW         0
#define TCSADRAIN       1
#define TCSAFLUSH       2

#define TIOCM_DTR	0x0001
#define TIOCM_RTS	0x0002
#define TIOCM_ST	0x0004
#define TIOCM_CTS	MS_CTS_ON /* 0x0010*/
#define TIOCM_DSR	MS_DSR_ON /* 0x0020*/
#define TIOCM_RNG	MS_RING_ON /*0x0040*/
#define TIOCM_RI	TIOCM_RNG /* at least that's the definition in Linux */
#define TIOCM_CD	MS_RLSD_ON /*0x0080*/

#if !defined(PATH_MAX) && defined(MAX_PATH)
/* PATH_MAX is the POSIX equivalent for Microsoft's MAX_PATH
 * both should be defined in (mingw) limits.h
 */
# define PATH_MAX MAX_PATH
#endif

#endif /* NUT_WINCOMPAT_H */
