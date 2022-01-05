/*  adele_cbi.h - Driver for generic UPS connected via modbus RIO
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

#ifndef ADELE_CBI_H
#define ADELE_CBI_H

/* UPS device details */
#define DEVICE_MFR  "ADELE"
#define DEVICE_MODEL "CB/CBI"

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
    CHRG = 0,       /* Charging status */
    BATV,           /* Battery voltage */
    BCEF = 6,       /* Battery charge efficiency factor (CEF) */
    BSOH,           /* Battery state-of-health */
    BSOC = 9,       /* Battery state-of-charge */
    BTMP = 11,      /* Battery temperature in Kelvin units */
    PMNG = 15,      /* Power management */
};
typedef enum devstate devstate_t;

/* BIT MASKS and VALUES */
#define CHRG_NONE 0
#define CHRG_RECV 1
#define CHRG_BULK 2
#define CHRG_ABSR 3
#define CHRG_FLOAT 4
#define PMNG_BKUP 0
#define PMNG_CHRG 1
#define PMNG_BOOST 2
#define PMNG_NCHRG 3

/* UPS state signal attributes */
struct regattr {
    int num;
	int saddr;          /* register start address */
    int xaddr;          /* register hex address */
    float scale;        /* scale */
	regtype_t type;     /* register type */
};
typedef struct regattr regattr_t;

/* ADELE CBI registers */
regattr_t regs[] = {
         {40005, 0, 0, 1, HOLDING},    /* Charging status */
         {40008, 0, 0, 1, HOLDING},    /* Battery voltage */
         {40014, 0, 0, 1, HOLDING},    /* Battery charge current */
         {40016, 0, 0, .1, HOLDING},   /* Battery capacity consumed */
         {40017, 0, 0, 1, HOLDING},    /* Battery discharge current */
         {40018, 0, 0, .1, HOLDING},   /* Effective battery capacity */
         {40019, 0, 0, 1, HOLDING},    /* Battery charge efficiency factor (CEF) */
         {40021, 0, 0, 1, HOLDING},    /* Battery state-of-health */
         {40022, 0, 0, 1, HOLDING},    /* Time remaining to 100% discharge */
         {40023, 0, 0, .1, HOLDING},   /* Battery state-of-charge */
         {40024, 0, 0, 1, HOLDING},   /* Battery type currently selected */
         {40026, 0, 0, 1, HOLDING},   /* Battery temperature in Kelvin units */
         {40028, 0, 0, .1, HOLDING},  /* Battery net internal resistance */
         {40089, 0, 0, 1, HOLDING},   /* Number of battery cells */
         {40100, 0, 0, 1, HOLDING},   /* SoC/SoH test possible */
         {40006, 0, 0, 1, HOLDING},   /* Power management
                                                                        * 0:Backup 1:Charging 2:boost 3:Not charging
                                                                        */
         {40007, 0, 0, 1, HOLDING},   /* Nominal output voltage */
         {40009, 0, 0, 1, HOLDING},   /* Parameter map version ID */
         {40010, 0, 0, 1, HOLDING},   /* Software ID */
         {40027, 0, 0, 1, HOLDING},   /* Configuration mode */
         {40029, 0, 0, 1, HOLDING},   /* On-board temperature */
         {40067, 0, 0, 1, HOLDING},   /* Product name */
         {40039, 0, 0, 1, HOLDING},   /* Device variant */
         {40103, 0, 0, 1, HOLDING},   /* Firmware ID */
         {40030, 0, 0, 1, HOLDING},   /* AC input voltage */
         {40011, 0, 0, 1, HOLDING},   /* Output load voltage */
         {40020, 0, 0, 1, HOLDING},   /* Output load current */
         {40048, 0, 0, 1, HOLDING},   /* Number of charge cycles completed */
         {40049, 0, 0, 1, HOLDING},   /* Charge cycles not completed */
         {40050, 0, 0, .1, HOLDING},  /* Ah charged */
         {40051, 0, 0, 1, HOLDING},   /* Total run time */
         {40052, 0, 0, 1, HOLDING},   /* Number of low battery voltage events */
         {40053, 0, 0, 1, HOLDING},   /* Number of high battery voltage events */
         {40058, 0, 0, 1, HOLDING},   /* Number power boost events */
         {40059, 0, 0, 1, HOLDING},   /* Highest battery voltage */
         {40062, 0, 0, 1, HOLDING},   /* Lowest battery voltage */
         {40061, 0, 0, .1, HOLDING},  /* Maximum depth of discharge */
         {40064, 0, 0, .1, HOLDING},  /* Average depth of discharge */
         {40056, 0, 0, 1, HOLDING},   /* Number of overtemperature inside events */
         {40054, 0, 0, 1, HOLDING},   /* Number of low AC input voltage events at mains input */
         {40055, 0, 0, 1, HOLDING},   /* Number of High AC input voltage events at mains input */
         {40057, 0, 0, 1, HOLDING},   /* Number of mains-backup transitions */
         {40060, 0, 0, 1, HOLDING},   /* Highest output load voltage */
         {40063, 0, 0, 1, HOLDING},   /* Lowest output load voltage */
         {40069, 0, 0, 1, HOLDING},   /* Reset internal battery model */
         {40098, 0, 0, 1, HOLDING},   /* SoC/SoH test period */
         {40099, 0, 0, 1, HOLDING},   /* Manual SoC/SoH test request */
         {40101, 0, 0, .1, HOLDING},  /* Nominal battery internal resistance */
         {40102, 0, 0, .1, HOLDING},  /* Nominal battery cables resistance */
         {40105, 0, 0, .1, HOLDING},  /* Battery capacity C20 */
         {40106, 0, 0, .1, HOLDING},  /* Battery Capacity C10 */
         {40108, 0, 0, .1, HOLDING},  /* Battery Capacity C5 */
         {40109, 0, 0, .1, HOLDING},  /* Battery Capacity C2 */
         {40112, 0, 0, .1, HOLDING},  /* Battery Capacity C1 */
         {40113, 0, 0, .1, HOLDING},  /* Low state-of-charge */
         {40120, 0, 0, 1, HOLDING},   /* Zero-SoC reference */
         {40071, 0, 0, 1, HOLDING},   /* Deep discharge battery prevention */
         {40072, 0, 0, 1, HOLDING},   /* Maximum charge current */
         {40073, 0, 0, 1, HOLDING},   /* Bulk voltage */
         {40074, 0, 0, 1, HOLDING},   /* Max bulk timer */
         {40075, 0, 0, 1, HOLDING},   /* Min bulk timer */
         {40077, 0, 0, 1, HOLDING},   /* Absorption voltage */
         {40078, 0, 0, 1, HOLDING},   /* Max absorption timer */
         {40079, 0, 0, 1, HOLDING},   /* Min absorption timer */
         {40080, 0, 0, 1, HOLDING},   /* Return Amperes to float */
         {40081, 0, 0, 1, HOLDING},   /* Return amps timer */
         {40082, 0, 0, 1, HOLDING},   /* Float voltage */
         {40083, 0, 0, 1, HOLDING},   /* Force boost charge */
         {40084, 0, 0, 1, HOLDING},   /* Return to bulk voltage from float */
         {40085, 0, 0, 1, HOLDING},   /* Return to bulk delay */
         {40087, 0, 0, 1, HOLDING},   /* Switchoff voltage without mains */
         {40090, 0, 0, 1, HOLDING},   /* Temperature compensation coefficient */
         {40092, 0, 0, 1, HOLDING},   /* Lifetest enable */
         {40093, 0, 0, 1, HOLDING},   /* Max alarm temp */
         {40094, 0, 0, 1, HOLDING},   /* Min alarm temp */
         {40097, 0, 0, 1, HOLDING},   /* Low battery threshold */
         {40034, 0, 0, 1, HOLDING},   /* Load output off duration after PC shutdown */
         {40065, 0, 0, 1, HOLDING},   /* History clear all */
         {40066, 0, 0, 1, HOLDING},   /* Factory settings */
         {40088, 0, 0, 1, HOLDING},   /* Backup Inhibit 0 = Backup allowed 1 = Backup not allowed 0 */
         {40104, 0, 0, 1, HOLDING},   /* Time buffering */
         {40111, 0, 0, 1, HOLDING},   /* PC power supply removal delay */
         {40036, 0, 0, 1, HOLDING},   /* Low AC input voltage alarm threshold */
         {40037, 0, 0, 1, HOLDING},   /* High AC input voltage alarm threshold */
         {40107, 0, 0, 1, HOLDING},   /* Device switchoff delay */
         {40001, 0, 0, 1, HOLDING},   /* Address of slave unit */
         {40002, 0, 0, 1, HOLDING},   /* Baud rate for serial communication */
         {40003, 0, 0, 1, HOLDING},   /* Parity bit for serial communication */
         {40032, 0, 0, 1, HOLDING},   /* Battery status alarm */
         {40033, 0, 0, 1, HOLDING},   /* Battery State of Charge and State of Health */
         {40035, 0, 0, 1, HOLDING},   /* Battery voltage alarm */
         {40044, 0, 0, 1, HOLDING},   /* Battery temperature sensor failure */
         {40043, 0, 0, 1, HOLDING},   /* Device failure */
         {40047, 0, 0, 1, HOLDING},   /* On board temperature alarm */
         {40045, 0, 0, 1, HOLDING},   /* AC input voltage alarm */
         {40046, 0, 0, 1, HOLDING},   /* Input mains on / backup */
         {40038, 0, 0, 1, HOLDING}    /* Load alarm */
};

#define NUMOF_REGS 14
#define NOTUSED -1

#endif /* ADELE_CBI_H */
