/* ragtech_speed.h - arbitrary serial line speed helper for the ragtech driver
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

#ifndef RAGTECH_SPEED_H_SEEN
#define RAGTECH_SPEED_H_SEEN 1

/* Set a non-standard line speed (such as 2560 baud, which has no Bxxx
 * constant) on the serial device `fd`, keeping every other termios setting.
 *
 * Returns 0 if the port now reports the requested speed, -1 if it could not
 * be applied (platform without support, ioctl/tcsetattr failure, or the
 * driver of the port quietly picked another speed). On failure the speed of
 * the port is left as it was.
 *
 * The state before the first successful call is remembered, to be given back
 * by ragtech_restore_baud(). */
int ragtech_set_baud(int fd, unsigned int baud);

/* Undo ragtech_set_baud(): put back the exact termios state the port had
 * before it. Does nothing if ragtech_set_baud() did not succeed. */
void ragtech_restore_baud(int fd);

#endif	/* RAGTECH_SPEED_H_SEEN */
