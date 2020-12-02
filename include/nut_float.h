/*
 * nut_float.h - Network UPS Tools include files for floating point routines
 * and ways to compare if non-integers are close enough to assume equal
 *
 * Copyright (C) 2020	Jim Klimov <jimklimov@gmail.com>
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

#ifndef NUT_FLOAT_H_SEEN
#define NUT_FLOAT_H_SEEN 1

#include "config.h"

#if defined HAVE_FLOAT_H
#  include <float.h>
#endif

#if defined HAVE_MATH_H
#  include <math.h>
#endif

/* Modern compilers frown upon direct comparisons of floating point numbers
 * since their imprecise internal representations can misfire, and really we
 * care if they are indiscernably close. To aviod warnings like -Wfloat-equal
 * we prefer to compare with methods defined below. Note: despite the "floating"
 * name, fabs() seems to be defined over doubles or wider types, e.g.:
 *     double fabs(double x);
 *     float fabsf(float x);
 *     long double fabsl(long double x);
 * To be on the safer side, we compare the difference to be smaller or equal to
 * the Epsilon for the respective numeric type, in order to be equivalent to a
 * zero for our needs. Various libs define it as "the next representable number
 * after 1.0 which is not equal to 1.0" for the discrete maths involved; no talk
 * about exactly comparing it to zero or whether it is the smallest representable
 * non-zero value...
 */
#define f_equal(x, y) ( fabsf((float)(x) - (float)(y)) <= FLT_EPSILON )
#define d_equal(x, y) ( fabs((double)(x) - (double)(y)) <= DBL_EPSILON )
#define ld_equal(x, y) ( fabsl((long double)(x) - (long double)(y)) <= LDBL_EPSILON )

#endif	/* NUT_FLOAT_H_SEEN */
