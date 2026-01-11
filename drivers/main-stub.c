/* main-stub.c - Network UPS Tools driver core stub

   This code is linked into each binary to ensure that even if the
   NUT drivers are built as dynamically-linked programs, the shared
   library can find needed methods and data provided by each specific
   driver program source file(s) and declared by main.h.

   Copyright (C)
   2026 -     	Jim Klimov <jimklimov+nut@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include "common.h"
#include "main.h"

#if (defined ENABLE_SHARED_PRIVATE_LIBS) && ENABLE_SHARED_PRIVATE_LIBS
/* Provided by main.c in shared-mode builds */
int drv_main(int argc, char **argv);

/* This source file is used in some unit tests to mock realistic driver
 * behavior - using a production driver skeleton, but their own main().
 */
# ifndef DRIVERS_MAIN_WITHOUT_MAIN
int main(int argc, char **argv)
{
	/* shared build, symbols should be visible to us right away,
	 * but not to (library-stored copy of) main.c */
	default_register_upsdrv_callbacks();
	return drv_main(argc, argv);
}
# endif /* DRIVERS_MAIN_WITHOUT_MAIN */
#else	/* !ENABLE_SHARED_PRIVATE_LIBS */

/* Just avoid warning/error: ISO C forbids an empty translation unit [-Werror=pedantic] */
int main (int argc, char ** argv);

#endif	/* !ENABLE_SHARED_PRIVATE_LIBS */
