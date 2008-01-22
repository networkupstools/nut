/*
 * powercom.h - defines for the newpowercom.c driver
 *
 * $Id$
 *
 * Copyrights:
 * (C) 2002 Simon Rozman <simon@rozman.net>
 * (C) 1999  Peter Bieringer <pb@bieringer.de>
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
 *
 */

/* C-libary includes */
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include "serial.h"
#include <limits.h>

/* nut includes */
#include "timehead.h"

/* general constants */
enum general {
	MAX_NUM_OF_BYTES_FROM_UPS = 16
};

/* values for sending to UPS */
enum commands {
	SEND_DATA    = '\x01',
	BATTERY_TEST = '\x03',
	WAKEUP_TIME  = '\x04',
	RESTART	     = '\xb9',
	SHUTDOWN     = '\xba',
	COUNTER      = '\xbc'
};

/* location of data in received string */
enum data {
	UPS_LOAD         = 0U,
	BATTERY_CHARGE   = 1U,
	INPUT_VOLTAGE    = 2U,
	OUTPUT_VOLTAGE   = 3U,
	INPUT_FREQUENCY  = 4U,
	UPSVERSION       = 5U,
	OUTPUT_FREQUENCY = 6U,
	STATUS_A         = 9U,
	STATUS_B         = 10U,
	MODELNAME        = 11U,
	MODELNUMBER      = 12U
};

/* status bits */
enum status {
	SUMMARY       = 0U,
	MAINS_FAILURE = 1U,
	ONLINE        = 1U,
	FAULT         = 1U,
	LOW_BAT       = 2U,
	BAD_BAT       = 2U,
	TEST          = 4U,
	AVR_ON        = 8U,
	AVR_MODE      = 16U,
	SD_COUNTER    = 16U,
	OVERLOAD      = 32U,
	SHED_COUNTER  = 32U,
	DIS_NOLOAD    = 64U,
	SD_DISPLAY    = 128U,
	OFF           = 128U
};

unsigned int voltages[]={100,110,115,120,0,0,0,200,220,230,240};
unsigned int BNTmodels[]={0,400,500,600,800,801,1000,1200,1500,2000};
unsigned int KINmodels[]={0,425,500,525,625,800,1000,1200,1500,1600,2200,2200,2500,3000,5000};

/* supported types */
struct type {
	const char    *name;
	unsigned char num_of_bytes_from_ups;
	
	struct method_of_flow_control {
	    char *name;
	    void (*setup_flow_control)(void);
	} flowControl;
	
	struct validation_byte {
	    unsigned int index_of_byte, required_value;
	    /* An example might explain the intention better then prose.
	     * Suppose we want to validate the data with:
	     *     powercom_raw_data[5] == 0x80
	     * then we will set index_of_byte to 5U and required_value to
	     * 0x80U: { 5U, 0x80U }.
	     */
	} validation[3];
	/* The validation array is of length 3 because 3 is longest 
	 * validation sequence for any type.
	 */
	
	/* Some UPSs must have a minutes and a seconds arguments for 
	 * the COUNTER commands while others are known to work with the
	 * seconds argument alone.
	 */
	struct deley_for_power_kill {
	    unsigned int  delay[2];   /* { minutes, seconds } */
	    unsigned char minutesShouldBeUsed;
	    /* 'n' in case the minutes value, which is deley[0], should
		 * be skipped and not sent to the UPS. 
		 */
	} shutdown_arguments;
	
	/* parameters to calculate input and output freq., one pair for
	 * each type:
	 *  Each pair defines parameters for 1/(A*x+B) to calculate freq.
	 *  from raw data
	 */
	float         freq[2];
	
	/* parameters to calculate load %, two pairs for each type:
	 *  First pair defines the parameters for A*x+B to calculate load
	 *  from raw data when offline and the second pair is used when
	 *  online
	 */
	float         loadpct[4];
	
	/* parameters to calculate battery %, five parameters for each type:
	 *  First three params defines the parameters for A*x+B*y+C to calculate
	 *  battery % (x is raw data, y is load %) when offline.
	 *  Fourth and fifth parameters are used to calculate D*y+E when online.
	 */
	float         battpct[5];

	/* parameters to calculate utility and output voltage, two pairs for
	 * each type:
	 *  First pair defines the parameters for A*x+B to calculate utility
	 *  from raw data when line voltage is >=220 and the second pair
	 *  is used otherwise.
	 */
	float         voltage[4];
};
