/* genericups.h - contact closure UPS line status definitions

   Copyright (C) 1999, 2000  Russell Kroll <rkroll@exploits.org>

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

#ifndef NUT_GENERICUPS_H_SEEN
#define NUT_GENERICUPS_H_SEEN 1

static struct {
	const	char	*mfr;			/* value for INFO_MFR	*/
	const	char	*model;			/* value for INFO_MODEL	*/
	const	char	*desc;			/* used in -h listing	*/
	int	line_norm;
	int	line_ol, val_ol;
	int	line_bl, val_bl;
	int	line_sd;
}	upstab[] =
{
	/* Type 0 */
	{ "UPSONIC",
	  "LAN Saver 600",
	  "UPSONIC LAN Saver 600",
	  TIOCM_DTR | TIOCM_RTS,	/* cable power: DTR + RTS	*/
	  TIOCM_CTS, 0,			/* online: CTS off		*/
	  TIOCM_CD, TIOCM_CD,		/* low battery: CD on		*/
	  TIOCM_RTS			/* shutdown: RTS		*/
	},

	/* Type 1 */
	{ "APC",
	  "Back-UPS",
	  "APC Back-UPS (940-0095A/C cable)",
	  TIOCM_DTR,			/* cable power: DTR		*/
	  TIOCM_RNG, 0,			/* online: RNG off		*/
	  TIOCM_CD, TIOCM_CD,		/* low battery: CD on		*/
	  TIOCM_RTS 			/* shutdown: RTS		*/
	},

	/* Type 2 */
	{ "APC",
	  "Back-UPS",
	  "APC Back-UPS (940-0020B/C cable)",
	  TIOCM_RTS,			/* cable power: RTS		*/
	  TIOCM_CTS, 0,			/* online: CTS off		*/
	  TIOCM_CD, TIOCM_CD,		/* low battery: CD on		*/
	  TIOCM_DTR|TIOCM_RTS		/* shutdown: DTR + RTS		*/
	},

	/* Type 3 */
	{ "PowerTech",
	  "Comp1000",
	  "PowerTech Comp1000 with DTR as cable power",
	  TIOCM_DTR,			/* cable power: DTR		*/
	  TIOCM_CTS, 0,			/* online: CTS off		*/
	  TIOCM_CD, TIOCM_CD,		/* low battery: CD on		*/
	  TIOCM_DTR | TIOCM_RTS		/* shutdown: DTR + RTS		*/
	},

	/* Type 4 */
	{ "Generic",
	  "Generic RUPS model",
	  "Generic RUPS model",
	  TIOCM_RTS,			/* cable power: RTS		*/
	  TIOCM_CTS, TIOCM_CTS,		/* online: CTS on		*/
	  TIOCM_CD, 0,			/* low battery: CD off		*/
	  0				/* shutdown: none		*/
	},

	/* Type 5 */
	{ "TrippLite",
	  "Internet Office Series",
	  "Tripp Lite UPS with Lan2.2 interface (black 73-0844 cable)",
	  TIOCM_DTR,			/* cable power: DTR		*/
	  TIOCM_CTS, TIOCM_CTS,		/* online: CTS on		*/
	  TIOCM_CD, 0,			/* low battery: CD off		*/
	  TIOCM_DTR | TIOCM_RTS		/* shutdown: DTR + RTS		*/
	},

	/* Type 6 */
	{ "Best",
	  "Patriot",
	  "Best Patriot (INT51 cable)",
	  TIOCM_DTR,			/* cable power: DTR		*/
	  TIOCM_CTS, TIOCM_CTS,		/* online: CTS on		*/
	  TIOCM_CD, 0,			/* low battery: CD off		*/
	  TIOCM_RTS			/* shutdown: set RTS		*/
	},

	/* Type 7 */
	{ "CyberPower",
	  "Power99",
	  "CyberPower Power99",
	  TIOCM_RTS,			/* cable power: RTS		*/
	  TIOCM_CTS, TIOCM_CTS,		/* online: CTS on		*/
	  TIOCM_CD, 0,			/* low battery: CD off		*/
	  TIOCM_DTR			/* shutdown: set DTR		*/
	},

	/* Type 8 */
	{ "Nitram",
          "Elite UPS",
          "Nitram Elite 500",
          TIOCM_DTR,			/* cable power: DTR		*/
          TIOCM_CTS, TIOCM_CTS,		/* online: CTS on		*/
          TIOCM_CD, 0,			/* low battery: CD off		*/
          -1				/* shutdown: unknown		*/
        },

	/* Type 9 */
	{ "APC",
	  "Back-UPS",
	  "APC Back-UPS (940-0023A cable)",
	  0,				/* cable power: none		*/
	  TIOCM_CD, 0,			/* online: CD off		*/
	  TIOCM_CTS, TIOCM_CTS,		/* low battery: CTS on		*/
	  TIOCM_RTS			/* shutdown: RTS		*/
	},

	/* Type 10 (duplicate from 7) */
	{ "Victron",
	  "Lite",
	  "Victron Lite (crack cable)",
	  TIOCM_RTS,                    /* cable power: RTS             */
	  TIOCM_CTS, TIOCM_CTS,         /* online: CTS on               */
	  TIOCM_CD, 0,                  /* low battery: CD off          */
	  TIOCM_DTR                     /* shutdown: DTR                */
	},

	/* Type 11 */
	{ "Powerware",
	  "3115",
	  "Powerware 3115",
	  TIOCM_DTR,			/* cable power: DTR		*/
	  TIOCM_CTS, 0,			/* online: CTS off		*/
	  TIOCM_CD, 0,			/* low battery: CD off		*/
	  TIOCM_ST			/* shutdown: ST			*/
	},

	/* Type 12 */
	{ "APC",
	  "Back-UPS Office",
	  "APC Back-UPS Office (940-0119A cable)",
	  TIOCM_RTS,			/* cable power: RTS		*/
	  TIOCM_CTS, 0,			/* online: CTS off		*/
	  TIOCM_CD, TIOCM_CD,		/* low battery: CD on		*/
	  TIOCM_DTR			/* shutdown: raise DTR		*/
	},

	/* Type 13 */
	{ "RPT",
          "Repoteck",
	  "Repoteck RPT-800A, RPT-162A",
	  TIOCM_DTR | TIOCM_RTS,	/* cable power: DTR + RTS	*/
	  TIOCM_CD, TIOCM_CD,		/* On-line : DCD on		*/
	  TIOCM_CTS, 0,			/* Battery low: CTS off		*/
	  TIOCM_ST			/* shutdown: TX BREA		*/
	},

	/* Type 14 */
	{ "Online",
	   "P250, P500, P750, P1250",
	   "Online P-series",
	   TIOCM_DTR,			/* cable power: DTR		*/
	   TIOCM_CD, TIOCM_CD,		/* online: CD on		*/
	   TIOCM_CTS, 0,		/* low battery: CTS off		*/
	   TIOCM_RTS			/* shutdown: raise RTS		*/
	},

	/* Type 15 */
	{ "Powerware",
	  "5119",
	  "Powerware 5119",
	  TIOCM_DTR,			/* cable power: DTR		*/
	  TIOCM_CTS, TIOCM_CTS,		/* online: CTS on		*/
	  TIOCM_CD, 0,			/* low battery: CD off		*/
	  TIOCM_ST			/* shutdown: ST (break)		*/
	},

	/* Type 16 */
	{ "Nitram",
	  "Elite UPS",
	  "Nitram Elite 2002",
	  TIOCM_DTR | TIOCM_RTS,	/* cable power: DTR + RTS	*/
	  TIOCM_CTS, TIOCM_CTS,		/* online: CTS on		*/
	  TIOCM_CD, 0,			/* low battery: CD off		*/
	  -1				/* shutdown: unknown		*/
	},

	/* Type 17 (duplicate from 8) */
	{ "PowerKinetics",
	  "9001",
	  "PowerKinetics 9001",
	  TIOCM_DTR,			/* cable power: DTR		*/
	  TIOCM_CTS, TIOCM_CTS,		/* online: CTS on		*/
	  TIOCM_CD, 0,			/* low battery: CD off		*/
	  -1				/* shutdown: unknown		*/
	},

	/* Type 18 */
	{ "TrippLite",
	  "Omni 450LAN",
	  "TrippLite UPS with Martin's cabling",
	  TIOCM_DTR,			/* cable power: DTR		*/
	  TIOCM_CTS, TIOCM_CTS,		/* online: CTS on		*/
	  TIOCM_CD, TIOCM_CD,		/* low battery: CAR on		*/
	  -1				/* shutdown: none		*/
	},

	/* Type 19 (duplicate from 6) */
	{ "Fideltronik",
	  "Ares Series",
	  "Fideltronik Ares Series",
	  TIOCM_DTR,			/* cable power: DTR		*/
	  TIOCM_CTS, TIOCM_CTS,		/* online: CTS on		*/
	  TIOCM_CD, 0,			/* low battery: DCD off		*/
	  TIOCM_RTS			/* shutdown: set RTS		*/
	},

	/* Type 20 */
	/* docs/cables/powerware.txt */
	{ "Powerware",
	  "5119 RM",
	  "Powerware 5119 RM",
	  TIOCM_DTR,			/* cable power: DTR		*/
	  TIOCM_CTS, 0,			/* online: CTS off		*/
	  TIOCM_CD, TIOCM_CD,		/* low battery: CD on		*/
	  TIOCM_ST			/* shutdown: ST (break)		*/
	},

	/* Type 21 */
	/* http://lists.exploits.org/upsdev/Oct2004/00004.html */
	{ "Generic",
	  "Generic RUPS 2000",
	  "Generic RUPS 2000 (Megatec M2501 cable)",
	  TIOCM_RTS,                    /* cable power: RTS		*/
	  TIOCM_CTS, TIOCM_CTS,         /* online: CTS on		*/
	  TIOCM_CD, 0,                  /* low battery: CD off		*/
	  TIOCM_RTS | TIOCM_DTR         /* shutdown: RTS+DTR		*/
	},

	/* Type 22 (duplicate from 7)*/
	{ "Gamatronic Electronic Industries",
	  "Generic Alarm UPS",
	  "Gamatronic UPSs with alarm interface",
	  TIOCM_RTS,			/* cable power: RTS		*/
	  TIOCM_CTS, TIOCM_CTS,		/* online: CTS on		*/
	  TIOCM_CD, 0,			/* low battery: CD off		*/
	  TIOCM_DTR			/* shutdown: DTR		*/
	},

	/* add any new entries directly above this line */

	{ NULL,
	  NULL,
	  NULL,
	  0,
	  0, 0,
	  0, 0,
	  0
	}
};

#endif	/* NUT_GENERICUPS_H_SEEN */
