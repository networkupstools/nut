#ifndef SERIAL_H_SEEN
#define SERIAL_H_SEEN 1

#include "attribute.h"

#include "config.h"

#include "common.h" /* for TYPE_FD */
#ifndef WIN32
# if defined(HAVE_SYS_TERMIOS_H)
#  include <sys/termios.h>      /* for speed_t */
# else
#  include <termios.h>
# endif /* HAVE_SYS_TERMIOS_H */
#else /* WIN32 */
# include "wincompat.h"
#endif /* WIN32 */

#include <unistd.h>             /* for usleep() and useconds_t, latter also might be via <sys/types.h> */
#include <sys/types.h>
#include <sys/select.h>         /* for suseconds_t */

/* limit the amount of spew that goes in the syslog when we lose the UPS */
#define SER_ERR_LIMIT 10	/* start limiting after 10 in a row  */
#define SER_ERR_RATE 100	/* then only print every 100th error */

/* porting stuff for WIN32 */
#ifdef WIN32
/* TODO : support "open" flags */
# define O_NONBLOCK 0
# define O_NOCTTY 0
#endif

TYPE_FD ser_open_nf(const char *port);
TYPE_FD ser_open(const char *port);

int ser_set_speed(TYPE_FD fd, const char *port, speed_t speed);
int ser_set_speed_nf(TYPE_FD fd, const char *port, speed_t speed);

/* set the state of modem control lines */
int ser_set_dtr(TYPE_FD fd, int state);
int ser_set_rts(TYPE_FD fd, int state);

/* get the status of modem control lines */
int ser_get_dsr(TYPE_FD fd);
int ser_get_cts(TYPE_FD fd);
int ser_get_dcd(TYPE_FD fd);

int ser_flush_io(TYPE_FD fd);

int ser_close(TYPE_FD fd, const char *port);

ssize_t ser_send_char(TYPE_FD fd, unsigned char ch);

/* send the results of the format string with d_usec delay after each char */
ssize_t ser_send_pace(TYPE_FD fd, useconds_t d_usec, const char *fmt, ...)
	__attribute__ ((__format__ (__printf__, 3, 4)));

/* send the results of the format string with no delay */
ssize_t ser_send(TYPE_FD fd, const char *fmt, ...)
	__attribute__ ((__format__ (__printf__, 2, 3)));

/* send buflen bytes from buf with no delay */
ssize_t ser_send_buf(TYPE_FD fd, const void *buf, size_t buflen);

/* send buflen bytes from buf with d_usec delay after each char */
ssize_t ser_send_buf_pace(TYPE_FD fd, useconds_t d_usec, const void *buf,
	size_t buflen);

ssize_t ser_get_char(TYPE_FD fd, void *ch, time_t d_sec, useconds_t d_usec);

ssize_t ser_get_buf(TYPE_FD fd, void *buf, size_t buflen, time_t d_sec, useconds_t d_usec);

/* keep reading until buflen bytes are received or a timeout occurs */
ssize_t ser_get_buf_len(TYPE_FD fd, void *buf, size_t buflen, time_t d_sec, useconds_t d_usec);

/* reads a line up to <endchar>, discarding anything else that may follow,
   with callouts to the handler if anything matches the alertset */
ssize_t ser_get_line_alert(TYPE_FD fd, void *buf, size_t buflen, char endchar,
	const char *ignset, const char *alertset, void handler (char ch),
	time_t d_sec, useconds_t d_usec);

/* as above, only with no alertset handling (just a wrapper) */
ssize_t ser_get_line(TYPE_FD fd, void *buf, size_t buflen, char endchar,
	const char *ignset, time_t d_sec, useconds_t d_usec);

ssize_t ser_flush_in(TYPE_FD fd, const char *ignset, int verbose);

/* unified failure reporting: call these often */
void ser_comm_fail(const char *fmt, ...)
	__attribute__ ((__format__ (__printf__, 1, 2)));
void ser_comm_good(void);

#ifdef WIN32
#define open(a,b)	w32_serial_open(a,b)
#define close(a)	w32_serial_close(a)
#define read(a,b,c)	w32_serial_read(a,b,c,INFINITE)
#define write(a,b,c)	w32_serial_write(a,b,c)
#endif


#endif	/* SERIAL_H_SEEN */
