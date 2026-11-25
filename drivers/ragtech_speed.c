/* ragtech_speed.c - arbitrary serial line speed helper for the ragtech driver
 *
 * Why a file of its own: the Linux way to ask for a speed that has no Bxxx
 * constant is "struct termios2" with BOTHER and the TCGETS2/TCSETS2 ioctls,
 * and those come from <asm/termbits.h>, which redefines "struct termios" and
 * so cannot be included together with the <termios.h> that the rest of the
 * driver (and NUT's serial.h) use. Keeping the Linux code in a translation
 * unit that never sees <termios.h> avoids the clash without any tricks.
 *
 * Three variants, picked at compile time:
 *
 *  - Linux with TCSETS2/BOTHER: the kernel termios2 interface;
 *  - systems where speed_t is the plain number (the BSDs, macOS): the
 *    number goes straight into cfsetispeed()/cfsetospeed();
 *  - anything else (Windows, Linux without termios2): not supported, the
 *    caller is told so and falls back to a standard speed.
 *
 * Copyright (C) 2026  juslex
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
 */

#include "config.h"

#include "ragtech_speed.h"

#if defined(__linux__)
/* No <termios.h> anywhere in this file on Linux, see above. */
# include <sys/ioctl.h>
# include <asm/ioctls.h>
# include <asm/termbits.h>
# if defined(TCGETS2) && defined(TCSETS2) && defined(BOTHER)
#  define RAGTECH_SPEED_TERMIOS2 1
# endif
#elif !defined(_WIN32) && !defined(WIN32)
# include <termios.h>
# if defined(B2400) && (B2400 == 2400)
#  define RAGTECH_SPEED_NUMERIC 1
# endif
#endif

#if defined(RAGTECH_SPEED_TERMIOS2)

static struct termios2 saved_t2;
static int saved_ok = 0;

int ragtech_set_baud(int fd, unsigned int baud)
{
	struct termios2 prev, t;

	if (ioctl(fd, TCGETS2, &prev) != 0)
		return -1;
	t = prev;

	t.c_cflag &= (tcflag_t)~(tcflag_t)CBAUD;
# ifdef CIBAUD
	t.c_cflag &= (tcflag_t)~(tcflag_t)CIBAUD;	/* input speed follows output speed */
# endif
	t.c_cflag |= BOTHER;
	t.c_ispeed = baud;
	t.c_ospeed = baud;

	if (ioctl(fd, TCSETS2, &t) != 0)
		return -1;

	/* Ask what the port really has now: the tty layer may have rounded the
	 * speed, or the port driver may have refused it without an error. */
	if (ioctl(fd, TCGETS2, &t) != 0 || (t.c_cflag & CBAUD) != BOTHER
	    || t.c_ospeed != baud) {
		(void)ioctl(fd, TCSETS2, &prev);
		return -1;
	}

	if (!saved_ok) {
		saved_t2 = prev;
		saved_ok = 1;
	}
	return 0;
}

void ragtech_restore_baud(int fd)
{
	if (saved_ok) {
		(void)ioctl(fd, TCSETS2, &saved_t2);
		saved_ok = 0;
	}
}

#elif defined(RAGTECH_SPEED_NUMERIC)

static struct termios saved_tio;
static int saved_ok = 0;

int ragtech_set_baud(int fd, unsigned int baud)
{
	struct termios prev, t;

	if (tcgetattr(fd, &prev) != 0)
		return -1;
	t = prev;

	if (cfsetispeed(&t, (speed_t)baud) != 0
	    || cfsetospeed(&t, (speed_t)baud) != 0
	    || tcsetattr(fd, TCSANOW, &t) != 0)
		return -1;

	/* tcsetattr() reports success if any part of the change took effect,
	 * so read the speed back. */
	if (tcgetattr(fd, &t) != 0 || cfgetospeed(&t) != (speed_t)baud
	    || cfgetispeed(&t) != (speed_t)baud) {
		(void)tcsetattr(fd, TCSANOW, &prev);
		return -1;
	}

	if (!saved_ok) {
		saved_tio = prev;
		saved_ok = 1;
	}
	return 0;
}

void ragtech_restore_baud(int fd)
{
	if (saved_ok) {
		(void)tcsetattr(fd, TCSANOW, &saved_tio);
		saved_ok = 0;
	}
}

#else	/* no way to ask for an arbitrary speed on this platform */

int ragtech_set_baud(int fd, unsigned int baud)
{
	(void)fd;
	(void)baud;
	return -1;
}

void ragtech_restore_baud(int fd)
{
	(void)fd;
}

#endif
