/*  generic_modbus.h - Driver for generic UPS connected via modbus RIO
 *
 *  Copyright (C)
 *    2021 Dimitris Economou <dimitris.s.economou@gmail.com>
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

#ifndef NUT_GENERIC_MODBUS_H
#define NUT_GENERIC_MODBUS_H

/* UPS device details */
#define DEVICE_MFR  "UNKNOWN"
#define DEVICE_MODEL "unknown"

/* serial access parameters */
#define BAUD_RATE 9600
#define PARITY 'N'
#define DATA_BIT 8
#define STOP_BIT 1

/*
 * modbus response and byte timeouts
 * us: 1 - 999999
 */
#define MODRESP_TIMEOUT_s 0
#define MODRESP_TIMEOUT_us 200000
#define MODBYTE_TIMEOUT_s 0
#define MODBYTE_TIMEOUT_us 50000


/* modbus access parameters */
#define MODBUS_SLAVE_ID 5

/* shutdown repeat on error */
#define FSD_REPEAT_CNT 3

/* shutdown repeat interval in ms */
#define FSD_REPEAT_INTRV 1500

/* definition of register type */
enum regtype {
	COIL = 0,
	INPUT_B,
	INPUT_R,
	HOLDING
};
typedef enum regtype regtype_t;

/* UPS device state enum */
enum devstate {
	OL_T = 0,
	OB_T,
	LB_T,
	HB_T,
	RB_T,
	CHRG_T,
	DISCHRG_T,
	BYPASS_T,
	CAL_T,
	OFF_T,
	OVER_T,
	TRIM_T,
	BOOST_T,
	FSD_T
};
typedef enum devstate devstate_t;

/* UPS state signal attributes */
struct sigattr {
	int addr;           /* register address */
	regtype_t type;     /* register type */
	int noro;           /* 1: normally open contact 0: normally closed contact. */
                        /* noro is used to indicate the logical ON or OFF in regard
                           of the contact state. if noro is set to 1 then ON corresponds
                           to an open contact */
};
typedef struct sigattr sigattr_t;

#define NUMOF_SIG_STATES 14
#define NOTUSED -1

/* define the duration of the shutdown pulse */
#define SHTDOWN_PULSE_DURATION NOTUSED

/*
 * associate PULS signals to NUT device states
 *
 * Ready contact        <--> 1:HB, 0:CHRG
 * Buffering contact    <--> 1:OB, 0:OL
 * Battery-low          <--> 1:LB
 * Replace Battery      <--> 1:RB
 * Inhibit buffering    <--> 1:FSD
 */


#endif /* NUT_GENERIC_MODBUS_H */
