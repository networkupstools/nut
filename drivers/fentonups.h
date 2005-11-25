/* fentonups.h - model capability table

   Copyright (C) 1999  Russell Kroll <rkroll@exploits.org>
                 2005  Michel Bouissou <michel@bouissou.net>

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

struct {
	const	char	*mtext;
	const	char	*desc;
	float	lowvolt;
	float	voltrange;
	float	chrglow;
	float	chrgrange;
	int	lowxfer;
	int	lownorm;
	int	highnorm;
	int	highxfer;
	int	has_temp;
}	modeltab[] =
{
	/* USA models */
	{ "L280A",  "PowerPal 280",  9.6,  2.4, 12.1, 1.7, 84, 98, 126, 142, 0 },
	{ "L425A",  "PowerPal 425",  9.6,  2.4, 12.1, 1.7, 84, 98, 126, 142, 0 },	
	{ "L660A",  "PowerPal 660",  19.6, 4.4, 24.2, 3.3, 84, 98, 126, 142, 0 },
	{ "L1000A", "PowerPal 1000", 19.6, 4.4, 24.2, 3.3, 84, 98, 126, 142, 0 },
	{ "L1400A", "PowerPal 1400", 29.4, 6.6, 36.3, 4.7, 84, 98, 126, 142, 0 },

	/* European models */
	{ "L280E",  "PowerPal 280",   9.6, 2.4, 12.1, 1.7, 168, 196, 252, 284, 0 },
	{ "L425E",  "PowerPal 425",   9.6, 2.4, 12.1, 1.7, 168, 196, 252, 284, 0 },	
	{ "L660E",  "PowerPal 660",  19.6, 4.4, 24.2, 3.3, 168, 196, 252, 284, 0 },
	{ "L1000E", "PowerPal 1000", 19.6, 4.4, 24.2, 3.3, 168, 196, 252, 284, 0 },
	{ "L1400E", "PowerPal 1400", 29.4, 6.6, 36.3, 4.7, 168, 196, 252, 284, 0 },

	{ "M1000", "PowerPure 1000", 25.0,  3.4, 25.2,  3.2, 80, 80, 138, 138, 1 },
	{ "M2000", "PowerPure 2000",    0,    0,    0,    0, 80, 80, 138, 138, 1 },
	{ "M3000", "PowerPure 3000",    0,    0,    0,    0, 80, 80, 138, 138, 1 }, 

	{ "H4000", "PowerOn 4000",  154.0, 14.0, 154.0, 14.0, 88, 88, 132, 132, 1 },
	{ "H6000", "PowerOn 6000",  154.0, 14.0, 154.0, 14.0, 88, 88, 132, 132, 1 },
	{ "H8000", "PowerOn 8000",  154.0, 14.0, 154.0, 14.0, 88, 88, 132, 132, 1 },
	{ "H010K", "PowerOn 10000", 154.0, 14.0, 154.0, 14.0, 88, 88, 132, 132, 1 },

	/* non-Fenton, yet compatible (Megatec protocol) models */

	{ "UPS-PRO", "PowerGuard PG-600", 0, 0, 0, 0, 170, 200, 250, 270, 1 },

	{ "SMK800A", "PowerCom SMK-800A", 1.9, 0.5, 1.9, 0.5, 165, 200, 240, 275, 1 },

	{ "ULT-1000", "PowerCom ULT-1000", 1.91, 0.42, 1.91, 0.42, 165, 200, 240, 275, 1 },

	{ "Alpha500iC", "Alpha 500 iC", 10.7, 1.4, 13.0, 0.8, 172, 196, 252, 288, 0 },

	/* From Yuri V. Kurenkov (http://www.inelt.ru) */
	{ "M1000LT", "INELT Monolith 1000LT", 1.91, 0.42, 1.91, 0.5, 160, 160, 270, 270, 1 },
	
	{ NULL,    NULL,		  0, 0, 0, 0,   0,   0,   0,   0, 0 }
};

/* devices which don't implement the I string the same way */

struct {
	const	char	*id;
	const	char	*mfr;
	const	char	*model;
	float	lowvolt;
	float	voltrange;
	float	chrglow;
	float	chrgrange;
	int	lowxfer;
	int	lownorm;
	int	highnorm;
	int	highxfer;
	int	has_temp;
}	mtab2[] =
{
	{ "WELI 500 1.0", "Giant Power", "MT650", 10.6, 3.7, 12.1, 2.2, 170, 180, 270, 280, 0 },
	{ "SMART-UPS       1800VA     T18Q16AG", "Effekta", "MT 2000 RM",
		50.0, 19.5, 50.0, 19.5, 171, 200, 260, 278, 1 },
	
	/* Sysgration model data from Simon J. Rowe */
	
	{ "                Pro 650    4.01", "Sysgration", "UPGUARDS Pro650",
		9.6, 2.4, 9.6, 2.4, 168, 196, 252, 284, 1 },

	/* SuperPower model data from Denis Zaika */

	{ "----            ----       VS00024Q", "SuperPower", "HP360", 9.6, 
		3.9, 9.6, 3.9, 140, 190, 240, 280, 1 },
	{ " -------------   ------     VS000391", "SuperPower", "Hope-550", 
		9.6, 3.9, 9.6, 3.9, 170, 190, 240, 280, 0 },

	/* Unitek data from Antoine Cuvellard */
	        
	{ "UNITEK          Alph1000iS A0", "Unitek", "Alpha 1000is",
		9.6, 2.4, 9.6, 2.4, 158, 172, 288, 290, 0 },

	{ NULL,    NULL,		NULL,  0, 0,   0,   0,   0,   0, 0 }
};
