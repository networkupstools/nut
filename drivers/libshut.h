/*!
 * @file libshut.h
 * @brief HID Library - Generic serial SHUT backend for Generic HID Access (using MGE HIDParser)
 *        SHUT stands for Serial HID UPS Transfer, and was created by MGE UPS SYSTEMS
 *
 * @author Copyright (C) 2006 - 2007
 *	Arnaud Quette <aquette.dev@gmail.com>
 *
 * This program is sponsored by MGE UPS SYSTEMS - opensource.mgeups.com
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * -------------------------------------------------------------------------- */

#ifndef LIBSHUT_H
#define LIBSHUT_H

#include "libhid.h"

extern communication_subdriver_t shut_subdriver;

/*!
 * Notification levels
 * These are however not processed currently
 */
#define OFF_NOTIFICATION	1	/* notification off */
#define LIGHT_NOTIFICATION	2	/* light notification */
#define COMPLETE_NOTIFICATION	3	/* complete notification for UPSs which do */
					/* not support disabling it like some early */
					/* Ellipse models */
#define DEFAULT_NOTIFICATION COMPLETE_NOTIFICATION

#endif /* LIBSHUT_H */

