/*
 * nut_stdint.h - Network UPS Tools sets of integer types having specified widths 
 * 
 * Copyright (C) 2011	Arjen de Korte <adkorte-guest@alioth.debian.org>
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

#ifndef NUT_STDINT_H_SEEN
#define NUT_STDINT_H_SEEN 1

#include "config.h"

#if defined HAVE_INTTYPES_H
#  include <inttypes.h>
#endif

#if defined HAVE_STDINT_H
#  include <stdint.h>
#endif

#endif	/* NUT_STDINT_H_SEEN */
