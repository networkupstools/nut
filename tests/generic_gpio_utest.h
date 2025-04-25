/*  generic_gpio_utest.h - unit tests for gpiod based NUT driver definitions
 *  for GPIO attached UPS devices
 *
 *  Copyright (C)
 *      2023            Modris Berzonis <modrisb@apollo.lv>
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

#ifndef GENERIC_GPIO_UTEST_H
#define GENERIC_GPIO_UTEST_H

#include "generic_gpio_common.h"
#include "generic_gpio_libgpiod.h"

void setNextLinesReadToFail(void);
void getWithoutUnderscores(char *var);
int get_test_status(struct gpioups_t *result, int on_fail_path);

#endif /* GENERIC_GPIO_UTEST_H */
