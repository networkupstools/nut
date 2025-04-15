/* belkin.h - serial commands for Belkin smart protocol units

   Copyright (C) 2000 Marcus Müller <marcus@ebootis.de>

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

#ifndef WIN32
#include <sys/ioctl.h>
#endif	/* !WIN32 */

#include "serial.h"
#include "timehead.h"

#define STATUS		'P'
#define CONTROL		'S'

#define MANUFACTURER	"MNU"
#define MODEL		"MOD"
#define VERSION_CMD	"VER"
#define RATING		"RAT"
#define STAT_INPUT	"STI"
#define STAT_OUTPUT	"STO"
#define STAT_BATTERY	"STB"
#define STAT_STATUS	"STA"
#define TEST_RESULT	"TSR"
#define POWER_ON	"RON"
#define POWER_OFF	"ROF"
#define POWER_SDTYPE	"SDT"	/* shutdown type? */
#define POWER_CYCLE	"SDA"	/* shutdown, then restore */
#define BUZZER "BUZ"
#define BUZZER_ON "1"
#define BUZZER_OFF0 "0"	/* Switching the buzzer off can be either 0 or 2 */
#define BUZZER_OFF2 "2"	/* Seems to be used for F6c1400 */
#define TEST "TST"
#define TEST_10SEC "3"
#define TEST_DEEP "4"
#define TEST_CANCEL "0"

/* The UPS Status "low battery" comes up 10s before the UPS actually stops.
   Therefore a shutdown is done at this battery % */

#define LOW_BAT		20

/* the maximum allowed number of missed replies */
#define MAXTRIES	3

/* dangerous instant commands must be reconfirmed within a 12 second window */
#define CONFIRM_DANGEROUS_COMMANDS 1
#define MINCMDTIME	3
#define MAXCMDTIME	15
