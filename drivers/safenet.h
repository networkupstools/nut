/*
 * safenet.h - defines/macros for the safenet driver
 *
 * Copyright (C) 2003  Arjen de Korte <arjen@de-korte.org>
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

/*
 * The following looks a bit ugly, but allows for simple logical operations
 */ 
#define FALSE   0
#define TRUE    !0

typedef struct {
	char	onbattery;
	char	dunno_02;
	char	batterylow;
	char	overload;
	char	dunno_05;
	char	silenced;
	char	batteryfail;
	char	systemfail;
	char	systemtest;
	char	dunno_10;
} safenet_s;

typedef union {
	safenet_s	status;
	char		reply[10];
} safenet_u;
