/*  nuttimetest.c - test custom NUT routines for time comparison and manipulation
 *
 *  Copyright (C)
 *      2023            Jim Klimov <jimklimov+nut@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include "config.h"
#include "common.h"
#include "nut_stdint.h"
#include "nut_float.h"

#include <stdio.h>
#include <stdlib.h>

static int check_difftime(void)
{
	time_t tv1, tv2;
	double d1, d2;
	int res = 0;

	printf("=== %s(time_t):\t", __func__);

	/* Often time_t is a long int, maybe signed */
	tv1 = 5;
	printf(" tv1=%" PRIiMAX, (intmax_t)tv1);

	tv2 = 8;
	printf(" tv2=%" PRIiMAX, (intmax_t)tv2);

	/* like in difftime(): (double)seconds elapsed time between older tv2 and newer tv1 */
	d1 = difftime(tv1, tv2);
	if (!d_equal(d1, -3))
		res++;
	printf(" => diff1(tv1, tv2)=%f (%s)", d1, d_equal(d1, -3) ? "OK" : "FAIL");

	d2 = difftime(tv2, tv1);
	if (!d_equal(d2, 3))
		res++;
	printf(" => diff2(tv2, tv1)=%f (%s)", d2, d_equal(d2, 3) ? "OK" : "FAIL");

	if (!d_equal(d1, -d2)) {
		printf(" => diff1 != -diff2 (FAIL)\n");
		res++;
	} else {
		printf(" => diff1 == -diff2 (OK)\n");
	}

	return res;
}

static int check_difftimeval(void)
{
	struct timeval tv1, tv2;
	double d1, d2;
	int res = 0;

	printf("=== %s(struct timeval):\t", __func__);

	tv1.tv_sec = 5;
	tv1.tv_usec = 900000;
	printf(" tv1=%" PRIiMAX ".%06" PRIiMAX, (intmax_t)tv1.tv_sec, (intmax_t)tv1.tv_usec);

	tv2.tv_sec = 8;
	tv2.tv_usec = 230000;
	printf(" tv2=%" PRIiMAX ".%06" PRIiMAX, (intmax_t)tv2.tv_sec, (intmax_t)tv2.tv_usec);

	/* like in difftime(): (double)seconds elapsed time between older tv2 and newer tv1 */
	d1 = difftimeval(tv1, tv2);
	if (!d_equal(d1, -2.33))
		res++;
	printf(" => diff1(tv1, tv2)=%f (%s)", d1, d_equal(d1, -2.33) ? "OK" : "FAIL");

	d2 = difftimeval(tv2, tv1);
	if (!d_equal(d2, 2.33))
		res++;
	printf(" => diff2(tv2, tv1)=%f (%s)", d2, d_equal(d2, 2.33) ? "OK" : "FAIL");

	if (!d_equal(d1, -d2)) {
		printf(" => diff1 != -diff2 (FAIL)\n");
		res++;
	} else {
		printf(" => diff1 == -diff2 (OK)\n");
	}

	return res;
}

static int check_difftimespec(void)
{
	int res = 0;
#if defined(HAVE_CLOCK_GETTIME) && defined(HAVE_CLOCK_MONOTONIC) && HAVE_CLOCK_GETTIME && HAVE_CLOCK_MONOTONIC
	struct timespec tv1, tv2;
	double d1, d2;

	printf("=== %s(struct timespec):\t", __func__);

	tv1.tv_sec = 5;
	tv1.tv_nsec = 900000000;
	printf(" tv1=%" PRIiMAX ".%09" PRIiMAX, (intmax_t)tv1.tv_sec, (intmax_t)tv1.tv_nsec);

	tv2.tv_sec = 8;
	tv2.tv_nsec =    230000;
	printf(" tv2=%" PRIiMAX ".%09" PRIiMAX, (intmax_t)tv2.tv_sec, (intmax_t)tv2.tv_nsec);

	/* like in difftime(): (double)seconds elapsed time between older tv2 and newer tv1 */
	d1 = difftimespec(tv1, tv2);
	if (!d_equal(d1, -2.10023))
		res++;
	printf(" => diff1(tv1, tv2)=%f (%s)", d1, d_equal(d1, -2.10023) ? "OK" : "FAIL");

	d2 = difftimespec(tv2, tv1);
	if (!d_equal(d2, 2.10023))
		res++;
	printf(" => diff2(tv2, tv1)=%f (%s)", d2, d_equal(d2, 2.10023) ? "OK" : "FAIL");

	if (!d_equal(d1, -d2)) {
		printf(" => diff1 != -diff2 (FAIL)\n");
		res++;
	} else {
		printf(" => diff1 == -diff2 (OK)\n");
	}
#else
	printf("=== %s(struct timespec):\tSKIP: NOT IMPLEMENTED for this build (not HAVE_CLOCK_GETTIME or not HAVE_CLOCK_MONOTONIC)\n", __func__);
#endif

	return res;
}

int main(void)
{
	int ret = 0;

	ret += check_difftime();
	ret += check_difftimeval();
	ret += check_difftimespec();

	return (ret != 0);
}
