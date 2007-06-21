/* powerpanel.h	Model specific data/definitions for CyberPower text/binary
			protocol UPSes 

   Copyright (C) 2007  Arjen de Korte <arjen@de-korte.org>
                       Doug Reynolds <mav@wastegate.net>

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

#define ENDCHAR		'\r'
#define IGNCHAR		""
#define MAXTRIES	3
#define UPSDELAY	50000

#define SER_WAIT_SEC	0
#define SER_WAIT_USEC	250000

#define DRV_VERSION "0.23"

/*
 * Handlers for the CyberPower binary protocol
 */
static int	initups_bin();
static void	initinfo_bin();
static void	updateinfo_bin();
static void	shutdown_bin();

/*
 * Handlers for the CyberPower text protocol
 */
static int	initups_txt();
static void	initinfo_txt();
static void	updateinfo_txt();
static void	shutdown_txt();

static const struct {
	char	*version;
	int	(*initups)(void);
	void	(*initinfo)(void);
	void	(*updateinfo)(void);
	void	(*shutdown)(void);
} powpan_protocol[] = {
	{ "binary", initups_bin, initinfo_bin, updateinfo_bin, shutdown_bin },
	{ "text", initups_txt, initinfo_txt, updateinfo_txt, shutdown_txt },
	{ NULL, NULL, NULL, NULL }
};

static const struct {
	char	*cmd;
	char	*command;
	int	len;
} powpan_cmdtab_bin[] = {
	{ "test.failure.start", "T\230\r", 3 },		/* 20 seconds test */
	{ "test.failure.stop", "CT\r", 3 },
	{ "beeper.toggle", "B\r", 2 },
	{ "shutdown.reboot", "S\0\0R\0\1W\r", 8},
	{ "shutdown.return", "S\0\0W\r", 5 },
	{ "shutdown.stop", "C\r", 2 },
/*
	{ "shutdown.stayoff", "S\0\0\W\r", 5 },
 */
	{ NULL, NULL, 0 }
};

static const struct {
	char	*cmd;
	char	*command;
} powpan_cmdtab_txt[] = {
	{ "test.failure.start", "T\r" },
	{ "test.failure.stop", "CT\r" },
	{ "beeper.on", "C7:1\r" },
	{ "beeper.off", "C7:0\r" },
	{ "shutdown.reboot", "S01R0001\r" },
	{ "shutdown.return", "Z02\r" },
	{ "shutdown.stop", "C\r" },
	{ "shutdown.stayoff", "S01\r" },
	{ NULL, NULL }
};

typedef struct {
	char	*val;
	char	command;
} powpan_valtab_t;

static const powpan_valtab_t	tran_high[] = {
	{ "138", -9 }, { "139", -8 }, { "140", -7 }, { "141", -6 }, { "142", -5 },
	{ "143", -4 }, { "144", -3 }, { "145", -2 }, { "146", -1 }, { "147", 0 },
	{ NULL, 0 }
};

static const powpan_valtab_t	tran_low[] = {
	{ "88", 0 }, { "89", 1 }, { "90", 2 }, { "91", 3 }, { "92", 4 },
	{ "93", 5 }, { "94", 6 }, { "95", 7 }, { "96", 8 }, { "97", 9 },
	{ NULL, 0 }
};

static const powpan_valtab_t	batt_low[] = {
	{ "25", -6 }, { "30", -5 }, { "35", -3 }, { "40", -1 },
	{ "45", 0 }, { "50", 2 }, { "55", 4 }, { "60", 6 },
	{ NULL, 0 }
};

static const powpan_valtab_t	out_volt[] = {
	{ "110", -10 }, { "120", 0 }, { "130", 10 },
	{ NULL, 0 }
};

static const powpan_valtab_t 	on_or_off[] = {
	{ "enabled", 2 }, { "disabled", 0 },
	{ NULL, 0 }
};

static const powpan_valtab_t	null_val[] = {
	{ NULL, 0 }
};

static const struct {
	char	*var;
	char	*get;
	char	*set;
	const powpan_valtab_t	*map;
} powpan_vartab_bin[] = {
	{ "input.transfer.high", "R\002\r", "Q\002%c\r", tran_high },
	{ "input.transfer.low", "R\004\r", "Q\004%c\r", tran_low },
	{ "battery.charge.low", "R\010\r", "Q\010%c\r", batt_low },
	{ "output.voltage.nominal", "R\030\r", "Q\030%c\r", out_volt },
	{ "ups.coldstart", "R\017\r", "Q\017%c\r", on_or_off },
	{ "unknown.variable.0x3d", "R\075\r", "Q\075%c\r", null_val },
	{ "unknown.variable.0x29", "R\051\r", "Q\051%c\r", null_val },
	{ "unknown.variable.0x2b", "R\053\r", "Q\053%c\r", null_val },
	{ NULL, NULL, NULL, NULL }
};

static const struct {
	char	*var;
	char	*get;
	char	*set;
} powpan_vartab_txt[] = {
	{ "input.transfer.high", "P6\r", "C2:%03d\r" },
	{ "input.transfer.low", "P7\r", "C3:%03d\r" },
	{ "battery.charge.low", "P8\r", "C4:%02d\r" },
	{ NULL, NULL, NULL }
};
