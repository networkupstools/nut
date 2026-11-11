/*
 * nhs_ser.c - NUT support for NHS Nobreaks, senoidal line
 *
 *
 * Copyright (C) 2024   Lucas Willian Bocchi <lucas@lucas.inf.br>
 *     Initial Release (as nhs-nut.c)
 * Copyright (C) 2024 - 2026 Jim Klimov <jimklimov+nut@gmail.com>
 *     Codebase adjusted to NUT standards
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
 */

#include "config.h"
#include "main.h"
#include "common.h"
#include "nut_stdint.h"
#include "nut_float.h"
#include "serial.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
#include <termios.h>
#include <ctype.h>
#include <math.h>

#define DRIVER_NAME     "NHS Nobreak Drivers"
#define DRIVER_VERSION  "0.07"
#define MANUFACTURER    "NHS Sistemas Eletronicos LTDA"

#define DEFAULTBAUD     2400
#define DEFAULTPORT     "/dev/ttyACM0"
#define DATAPACKETSIZE  100     /* NOTE: Practical anticipated max is 50 */

/*
 * Values held by the UNKNOWN device, which answers for whatever a model does
 * not know (see upsinfo and ups_unknown_device). They
 * are named here only so that the help texts can quote them; the code never
 * uses them directly as a fallback.
 */
#define DEFAULTPF       0.9     /* power factor */
#define DEFAULTPERC     2.0     /* margin for the input voltage thresholds, % */
#define DEFAULTBATV     12.0    /* nominal voltage of each battery, V */
#define DEFAULTBATSER   1       /* batteries wired in series (0: in parallel) */
#define DEFAULTREAD16   0       /* two-byte fields read as "first.second" (1: as 16 bits) */
#define DEFAULTOVERLOAD 105     /* load, in %, above which the UPS is overloaded */

/*
 * Resting voltage of a fully charged lead-acid battery, as a ratio of its
 * nominal voltage: 6 cells at 2.1V make 12.6V for a 12V battery. It is the
 * yardstick used to tell how many batteries a voltage reading covers, and
 * the point that counts as 100% of charge; see the battery charge
 * calculation in interpret_pkt_data().
 */
#define BATTERY_FULL_RATIO      1.05

/*
 * Voltage at which a lead-acid battery is taken as empty, as a ratio of its
 * nominal voltage: 6 cells at 1.75V make 10.5V for a 12V battery. That is
 * the usual end-of-discharge voltage of this kind of battery, and my own
 * unit shut down at 10.4V. It is the point that counts as 0% of charge; the
 * measurement behind it is written out at the battery charge calculation in
 * interpret_pkt_data().
 */
#define BATTERY_EMPTY_RATIO     0.875

/*
 * Constants of the battery runtime estimate. The reasoning behind the whole
 * calculation, and how these three values were arrived at, is written out
 * where battery.runtime is computed in interpret_pkt_data().
 */

/*
 * Discharge time, in hours, at which the capacity printed on a lead-acid
 * battery is valid: "58Ah" means 2.9A for 20 hours (the C20 rating), not 58A
 * for one hour.
 */
#define RUNTIME_RATING_HOURS            20.0

/*
 * Peukert exponent: how fast the usable capacity shrinks as the discharge
 * current grows. 1.0 would mean no shrinking at all. 1.25 is the value that
 * best reproduces the runtimes NHS declares in its product sheets.
 */
#define RUNTIME_PEUKERT_EXPONENT        1.25

/*
 * Inverter efficiency, in percent, assumed by the runtime estimate when the
 * model has no known rating. 80 is the most common figure in the NHS sheets.
 * It is deliberately not the eff_inverter of the UNKNOWN device: that field
 * is published as ups.efficiency, and there an assumption would pass for the
 * manufacturer's rating.
 */
#define RUNTIME_ASSUMED_INVERTER_EFF    80.0

/*
 * Defaults shared by every model (see upsinfo below). They used to appear
 * in the model data as the bare numbers 3 and 1.
 */
#define DEFAULT_PROTOCOL_VERSION    3   /* NHS serial protocol generation */

/*
 * Whether the "bypass active" status bit raises the [ON BYPASS] alarm when
 * ups.conf does not say otherwise (the bypassasalarm setting).
 *
 * This default used to be 1, and that was my mistake. When I first wrote the
 * driver I took "BIT6 - BYPASS ATIVO", in the STATUS byte (byte [17]) of the
 * "PACOTE DE DADOS" table of the manufacturer's "Protocolo NHS 3.0" document,
 * for a fault flag. That is what bypass means on a double-conversion UPS,
 * where it tells that the load is no longer protected by the inverter.
 *
 * Reading the documentation again, nothing in it supports that:
 *
 *  - The protocol document only names the bit. It sits among the bits that
 *    describe the operating state ("MODO BATERIA ATIVO", "ENTRADA DE REDE EM
 *    220V", "SAIDA DO NOBREAK EM 220V", "CARREGADOR ATIVO"), while the fault
 *    conditions have bits of their own ("BATERIA BAIXA", "FALHA DE REDE",
 *    "FALHA RAPIDA DE REDE").
 *
 *  - The "Manual Software de Monitoracao NHS 3.0" (version 3.0.30, section
 *    3.3.1) describes "Bypass ativado" and "Bypass desativado" as entering
 *    and leaving an operating mode ("modo bypass"), and words the real
 *    failures in the same list differently ("Falha na rede", "Falha de
 *    comunicacao").
 *
 * What settled it was the event log kept by that same monitoring software:
 * the SQLite file nhsupsserver.dat, table "evento", columns "bypass_ativo"
 * and "modo_inversor". I recorded it from my own unit between 2024-11-21 and
 * 2024-11-24. All 2860 events logged on mains have bypass_ativo = 1 and all
 * 1389 events logged on inverter have bypass_ativo = 0, without a single
 * exception either way. On that unit the bit is simply the opposite of
 * inverter mode: "bypass active" means the load is being fed from mains,
 * which is the normal state of a line-interactive UPS and not a fault. With
 * the default at 1, a healthy UPS showed "ALARM OL" all the time; the same
 * was reported for a PDV Senoidal 1500VA in issue #3510.
 *
 * Not every model behaves like that. A Laser Senoidal 2600VA GII reported in
 * the same issue stays "OL" with no alarm, so it keeps the bit clear on
 * mains, and on such a unit the bit may well mark a real bypass. That is why
 * the setting stays. The default is now 0, so the alarm is raised only for
 * who has checked how the bit behaves on their unit (the debug_pkt_data flag
 * shows it) and set bypassasalarm = 1.
 */
#define DEFAULT_BYPASS_AS_ALARM     0

/*
 * Serial settings used when ups.conf does not give them: 2400 8N1 without
 * flow control. The maximum values bound user-provided delays.
 *
 * Flow control was hard-coded to RTS/CTS at first and then became the
 * serial_flow_control setting, still with "hardware" as its default. The
 * default is now "none", for these reasons:
 *
 *  - The manufacturer's "Protocolo NHS 3.0" document gives, on its first
 *    page ("Configuracao para terminal de comunicacao"), the settings to
 *    talk to the UPS: baud rate 2400, data bits 8, parity none, stop bits 1
 *    and "Handshaking: None". Reading it again, no flow control is what the
 *    documentation itself gives as the standard way; RTS/CTS was my choice,
 *    not something the manufacturer asks for.
 *
 *  - With CRTSCTS set, the kernel holds transmission back for as long as
 *    CTS is low. On the USB models the port is a CDC-ACM bridge, where CTS
 *    only mirrors a notification that the UPS firmware is not bound to
 *    send, and a 3-wire serial cable has no CTS line at all. If CTS stays
 *    low, the request that starts the communication is never transmitted,
 *    no packet ever comes back and upsc only shows "Data stale". That is
 *    the failure reported in issue #3510.
 *
 *  - In that issue the units are being run with serial_flow_control = none.
 *    For a PDV Senoidal 1500VA it was the setting that made the UPS answer
 *    at all, and a Mini Gamer 600VA tested with all four modes worked the
 *    same with "none" as with the others.
 *
 * None of this ever showed up in my own tests: every unit I have, and those
 * of my customers, has always worked with hardware flow control. It may be
 * down to the cables I use, all bought from the same supplier. But most
 * people are running the mode that the documentation gives as standard, so
 * that is the one the driver now uses by default. Setting
 * serial_flow_control = hardware brings the previous behavior back.
 */
#define DEFAULT_SERIAL_DATA_BITS        8
#define DEFAULT_SERIAL_PARITY           "none"
#define DEFAULT_SERIAL_STOP_BITS        1
#define DEFAULT_SERIAL_FLOW_CONTROL     "none"
#define DEFAULT_SERIAL_READ_TIMEOUT_MS  100
#define DEFAULT_SERIAL_SEND_PACE_US     0
#define MAX_SERIAL_READ_TIMEOUT_MS      60000
#define MAX_SERIAL_SEND_PACE_US         999999

/* comms revival attempts before declaring them stale */
#define MAXTRIES    3

/* driver description structure */
upsdrv_info_t upsdrv_info =
{
	DRIVER_NAME,
	DRIVER_VERSION,
	"Lucas Willian Bocchi <lucas@lucas.inf.br>",
	DRV_BETA,
	{ NULL }
};


/* Struct to represent serial conventions in termios.h */
typedef struct {
	speed_t     rate;          /* Constant in termios.h */
	int         speed;         /* Numeric speed, used in NUT */
	const char  *description;  /* Description */
} baud_rate_t;

static baud_rate_t baud_rates[] = {
	{ B50,           50,  "50 bps" },
	{ B75,           75,  "75 bps" },
	{ B110,         110,  "110 bps" },
	{ B134,         134,  "134.5 bps" },
	{ B150,         150,  "150 bps" },
	{ B200,         200,  "200 bps" },
	{ B300,         300,  "300 bps" },
	{ B600,         600,  "600 bps" },
	{ B1200,       1200,  "1200 bps" },
	{ B2400,       2400,  "2400 bps" },
	{ B4800,       4800,  "4800 bps" },
	{ B9600,       9600,  "9600 bps" },
	{ B19200,     19200,  "19200 bps" },
	{ B38400,     38400,  "38400 bps" },
	{ B57600,     57600,  "57600 bps" },
	{ B115200,   115200,  "115200 bps" },
	{ B230400,   230400,  "230400 bps" },
	{ B460800,   460800,  "460800 bps" },
	{ B921600,   921600,  "921600 bps" },
	{ B1500000, 1500000,  "1.5 Mbps" },
	{ B2000000, 2000000,  "2 Mbps" },
/* NOTE: Per https://github.com/networkupstools/nut/issues/3163
 * not all platforms offer all baud rates, so we wrap some into
 * conditional uses.
 */
#ifdef B2500000
	{ B2500000, 2500000,  "2.5 Mbps" },
#endif
#ifdef B3000000
	{ B3000000, 3000000,  "3 Mbps" },
#endif
#ifdef B3500000
	{ B3500000, 3500000,  "3.5 Mbps" },
#endif
#ifdef B4000000
	{ B4000000, 4000000,  "4 Mbps" },
#endif
};
#define NUM_BAUD_RATES (sizeof(baud_rates) / sizeof(baud_rates[0]))

/* Struct that contains nobreak info */
typedef struct {
	unsigned int   header;
	unsigned int   size;
	char           type;
	unsigned int   model;
	unsigned int   hardwareversion;
	unsigned int   softwareversion;
	unsigned int   configuration;
	unsigned int   configuration_array[5];
	bool           c_oem_mode;
	bool           c_buzzer_disable;
	bool           c_potmin_disable;
	bool           c_rearm_enable;
	bool           c_bootloader_enable;
	unsigned int   numbatteries;
	unsigned int   undervoltagein120V;
	unsigned int   overvoltagein120V;
	unsigned int   undervoltagein220V;
	unsigned int   overvoltagein220V;
	unsigned int   tensionout120V;
	unsigned int   tensionout220V;
	unsigned int   statusval;
	unsigned int   status[6];
	bool           s_220V_in;
	bool           s_220V_out;
	bool           s_sealed_battery;
	bool           s_show_out_tension;
	bool           s_show_temperature;
	bool           s_show_charger_current;
	unsigned int   chargercurrent;
	unsigned char  checksum;
	unsigned char  checksum_calc;
	bool           checksum_ok;
	char           serial[17];
	unsigned int   year;
	unsigned int   month;
	unsigned int   wday;
	unsigned int   hour;
	unsigned int   minute;
	unsigned int   second;
	unsigned int   alarmyear;
	unsigned int   alarmmonth;
	unsigned int   alarmwday;
	unsigned int   alarmday;
	unsigned int   alarmhour;
	unsigned int   alarmminute;
	unsigned int   alarmsecond;
	unsigned int   end_marker;
} pkt_hwinfo;

/* Struct that contains the data packet info */
typedef struct {
	unsigned int   header;
	unsigned int   length;
	char           packet_type;
	unsigned int   vacinrms_high;
	unsigned int   vacinrms_low;
	float          vacinrms;
	unsigned int   vdcmed_high;
	unsigned int   vdcmed_low;
	float          vdcmed;
	float          vdcmed_real;
	unsigned int   potrms;
	unsigned int   vacinrmsmin_high;
	unsigned int   vacinrmsmin_low;
	float          vacinrmsmin;
	unsigned int   vacinrmsmax_high;
	unsigned int   vacinrmsmax_low;
	float          vacinrmsmax;
	unsigned int   vacoutrms_high;
	unsigned int   vacoutrms_low;
	float          vacoutrms;
	unsigned int   tempmed_high;
	unsigned int   tempmed_low;
	float          tempmed;
	float          tempmed_real;
	unsigned int   icarregrms;
	unsigned int   icarregrms_real;
	unsigned int   statusval;
	unsigned int   status[8];
	bool           s_battery_mode;
	bool           s_battery_low;
	bool           s_network_failure;
	bool           s_fast_network_failure;
	bool           s_220_in;
	bool           s_220_out;
	bool           s_bypass_on;
	bool           s_charger_on;
	unsigned char  checksum;
	unsigned char  checksum_calc;
	bool           checksum_ok;
	unsigned int   end_marker;
} pkt_data;

/*
 * What the driver knows about one UPS model. getupsinfo() has one case per
 * protocol model code, and ups_unknown_device, the UNKNOWN device, holds the
 * value to use whenever a model, listed or not, does not know something.
 * getupsinfo() resolves every field in the same order:
 *
 *   1. the ups.conf setting, when there is one;
 *   2. the case of the model reported by the UPS;
 *   3. the UNKNOWN device.
 *
 * No other function supplies a default of its own: the get_*() helpers only
 * return what getupsinfo() resolved. In the fields that hold ratings (VA and
 * everything from watts on), 0 means "not known". A 0 that survives the three
 * steps is not an error: what depends on it is just not published.
 *
 * upscode        Model code. The UPS reports it in byte [3] of the HWINFO
 *                answer and it selects the case in getupsinfo(). It
 *                identifies the hardware, so it has no ups.conf setting.
 *                (unsigned int)-1 marks the UNKNOWN device.
 *
 * upsdesc        Model name, published as device.model and ups.model.
 *                ups.conf: overridemodel. UNKNOWN device: "NHS UNKNOWN".
 *
 * VA             Nominal apparent power, in VA. Published as
 *                ups.power.nominal and used to turn the load percentage
 *                sent by the UPS into VA and W. ups.conf: va. UNKNOWN
 *                device: 0, so the power and runtime variables are not
 *                published until "va" is set.
 *
 * pversion       Generation of the NHS serial protocol spoken by the model.
 *                Informational for now: it does not change framing or
 *                commands. ups.conf: protocolversion.
 *                UNKNOWN device: DEFAULT_PROTOCOL_VERSION.
 *
 * bypassasalarm  Whether the "bypass active" status bit raises the
 *                [ON BYPASS] alarm: 1 raises it, 0 keeps the bit only in the
 *                packet diagnostics. On most units seen so far that bit is
 *                set during normal mains operation, so the alarm is off
 *                unless asked for. ups.conf: bypassasalarm.
 *                UNKNOWN device: DEFAULT_BYPASS_AS_ALARM; the evidence
 *                behind it is written out at that define.
 *
 * watts          Nominal real power, in W ("Pot. continua" in the product
 *                sheet). This is what the case of a model sets. Once
 *                resolved it is always VA * pf. UNKNOWN device: 0.
 *
 * pf             Power factor: nominal real power divided by nominal
 *                apparent power. Used for ups.realpower,
 *                ups.realpower.nominal and the runtime estimate. A model gets
 *                it from its own W and VA. ups.conf: pf.
 *                UNKNOWN device: DEFAULTPF.
 *
 * eff_line       Efficiency, in percent, at full load while on mains with
 *                the input and the output at the same nominal voltage
 *                (120V/120V or 220V/220V); "Rend. pl. carga rede" in the
 *                product sheet. Published as ups.efficiency while on mains.
 *                ups.conf: efficiency_line. UNKNOWN device: 0.
 *
 * eff_line_conv  Same as eff_line, for a unit converting between 120V and
 *                220V (input and output at different nominal voltages),
 *                which costs a few points in the transformer. 0 means the
 *                sheet gives a single mains figure, and eff_line is then
 *                used for both cases. ups.conf: efficiency_line_conv.
 *                UNKNOWN device: 0.
 *
 * eff_inverter   Efficiency, in percent, at full load while on battery
 *                ("Rend. pl. carga inversor" in the product sheet).
 *                Published as ups.efficiency while on battery.
 *                ups.conf: efficiency_inverter. UNKNOWN device: 0.
 *
 * battery_ah     Capacity, in Ah, of each battery fitted at the factory;
 *                "2 x 58Ah/12V" in the sheet is stored as 58. Most models
 *                wire their batteries as one series string, and there this
 *                is also the capacity of the whole bank. A few use parallel
 *                strings (the sheets show, for example, 4 x 58Ah at 24V),
 *                and there the bank holds this capacity times the number
 *                of strings. Published as battery.capacity and used for the
 *                battery.runtime estimate. ups.conf: ah. UNKNOWN device: 0,
 *                so neither is published until "ah" is set.
 *
 * battery_v      Nominal voltage, in V, of ONE battery, never of the bank.
 *                Every battery in the NHS product sheets is a 12V unit, so
 *                no model sets it and it should hardly ever be changed.
 *                It exists for units whose batteries were replaced by
 *                another kind, and it is a float because of them: it used
 *                to be an integer, which could not express, for example,
 *                the 12.8V of a lithium iron phosphate battery. The
 *                driver multiplies it by the number of batteries that the
 *                voltage reading covers to get battery.voltage.nominal and
 *                battery.charge; it also enters the runtime estimate.
 *                ups.conf: vbat. UNKNOWN device: DEFAULTBATV.
 *
 * bat_series     How the batteries are wired to each other. It is a plain
 *                number, like bypassasalarm: 1 means in series (the same
 *                current runs through all of them), 0 means in parallel
 *                (each one adds its own current). How many there are is
 *                another matter and comes from the UPS or from
 *                "numbatteries". In the product sheet the batteries are in
 *                series when their number times 12V gives the voltage of
 *                the bank ("Tensao operacao"), as in "2 x 58Ah/12V" at
 *                24V. Used for battery.current.total, the current of the
 *                whole bank: in series it is the current through one
 *                battery, in parallel that current times the number of
 *                batteries. ups.conf: battery_series, for a unit whose
 *                batteries were wired differently from the factory set.
 *                UNKNOWN device: DEFAULTBATSER, in series, which is how
 *                116 of the 118 products whose sheet gives the bank
 *                voltage are built.
 *
 * r16_vin        How one two-byte field of the data packet is read. There
 * r16_vin_min    is a flag for each field: input voltage, its minimum and
 * r16_vin_max    its maximum, output voltage, temperature and battery
 * r16_vout       voltage.
 * r16_temp       0 reads the bytes as "first.second", an integer part and
 * r16_vbat       a fraction. That is what the manufacturer's protocol
 *                document describes ("PARTE ALTA", "PARTE BAIXA -
 *                FRACIONARIO") and what the driver has always done; the
 *                battery is also divided by 10 when the second byte is 0.
 *                1 reads them as one 16-bit number, first byte + 256 *
 *                second byte, times 0.1 for the battery. That is what the
 *                manufacturer's own monitoring software does: I found it
 *                in its Linux server, nhsupsserver 3.1.36, in the routines
 *                PacoteDados20::get_evento and Pacote::Reader::read_int.
 *                Both readings give the same value while the second byte
 *                is 0, that is, up to 255 (25.5V for the battery). Above
 *                that they differ: 260V is 4.1 for the first and 260 for
 *                the second. ups.conf: read16_vin, read16_vin_min,
 *                read16_vin_max, read16_vout, read16_temp and read16_vbat.
 *                UNKNOWN device: DEFAULTREAD16, the driver's own reading.
 *
 * overload_perc  Load, in percent of the nominal power, above which the UPS
 *                is reported as overloaded (OVER in ups.status). 0 turns
 *                the check off. ups.conf: overload_perc.
 *                UNKNOWN device: DEFAULTOVERLOAD, the limit at which the
 *                manufacturer's monitoring software flags "Potencia
 *                excessiva" (same server, PacoteDados20::get_evento).
 *
 * vin_perc       Margin, in percent, applied to the input voltage transfer
 *                points to derive the input.voltage.*.warning and
 *                input.voltage.*.critical thresholds. No model sets it.
 *                ups.conf: vin_low_warn_perc, vin_low_crit_perc,
 *                vin_high_warn_perc and vin_high_crit_perc, one for each
 *                threshold. UNKNOWN device: DEFAULTPERC.
 */
typedef struct {
	unsigned int  upscode;        /* model code reported by the UPS */
	char          upsdesc[100];   /* model name */
	unsigned int  VA;             /* nominal apparent power, VA */
	unsigned int  pversion;       /* NHS protocol generation */
	unsigned int  bypassasalarm;  /* 1: bypass bit raises an alarm */
	unsigned int  watts;          /* nominal real power, W */
	float         pf;             /* power factor, watts / VA */
	float         eff_line;       /* % on mains, same in/out voltage */
	float         eff_line_conv;  /* % on mains, 120V<->220V conversion */
	float         eff_inverter;   /* % on battery */
	unsigned int  battery_ah;     /* Ah of each battery */
	float         battery_v;      /* nominal V of each battery */
	unsigned int  bat_series;     /* 1: batteries in series, 0: in parallel */
	unsigned int  r16_vin;        /* 1: input V read as 16 bits */
	unsigned int  r16_vin_min;    /* 1: minimum input V read as 16 bits */
	unsigned int  r16_vin_max;    /* 1: maximum input V read as 16 bits */
	unsigned int  r16_vout;       /* 1: output V read as 16 bits */
	unsigned int  r16_temp;       /* 1: temperature read as 16 bits */
	unsigned int  r16_vbat;       /* 1: battery V read as 16 bits */
	unsigned int  overload_perc;  /* load % above which OVER is set */
	float         vin_perc;       /* % margin for the input thresholds */
} upsinfo;


static const unsigned int  string_initialization_long[9] = {0xFF, 0x09, 0x53, 0x83, 0x00, 0x00, 0x00, 0xDF, 0xFE};
static const unsigned int  string_initialization_short[9] = {0xFF, 0x09, 0x53, 0x03, 0x00, 0x00, 0x00, 0x5F, 0xFE};
static const unsigned int  string_initialization_comptmode[5] = {0xFF, 0x05, 0x01, 0x06, 0xFE};

static int  debug_pkt_data = 0, debug_pkt_hwinfo = 0, debug_pkt_raw = 0;

static TYPE_FD_SER    serial_fd = ERROR_FD_SER;
static unsigned char  chr;
static size_t         datapacket_index = 0;
static unsigned int   datapacketsize = 0;
static bool           datapacketstart = false;
static time_t         lastdp = 0;
static unsigned int   checktime = 2000000;      /* 2 seconds */
static unsigned int   max_checktime = 6000000;  /* max wait time: 6 seconds */
static unsigned int   send_extended = 0;
static int            bwritten = 0;
static unsigned char  datapacket[DATAPACKETSIZE];
static char           porta[NUT_PATH_MAX + 1] = DEFAULTPORT;

/* Validated serial settings used for initial connection and reconnection. */
static int           baudrate = DEFAULTBAUD;
static unsigned int  serial_data_bits = DEFAULT_SERIAL_DATA_BITS;
static char          serial_parity[8] = DEFAULT_SERIAL_PARITY;
static unsigned int  serial_stop_bits = DEFAULT_SERIAL_STOP_BITS;
static char          serial_flow_control[16] = DEFAULT_SERIAL_FLOW_CONTROL;
static unsigned int  serial_read_timeout_ms = DEFAULT_SERIAL_READ_TIMEOUT_MS;
static unsigned int  serial_send_pace_us = DEFAULT_SERIAL_SEND_PACE_US;
static float         minpower = 0;
static float         maxpower = 0;
static unsigned int  minpowerperc = 0;
static unsigned int  maxpowerperc = 0;
/* False until the first power reading: only then are the minimums meaningful. */
static bool          minpower_seen = false;
/*
 * True from the moment a different UPS answers on the port until
 * interpret_pkt_data() has dropped the ratings it kept from the previous
 * one. See forget_previous_ups().
 */
static bool          ups_replaced = false;

static pkt_hwinfo lastpkthwinfo = {
	0xFF,                /* header */
	0,                   /* size */
	'S',                 /* type */
	0,                   /* model */
	0,                   /* hardwareversion */
	0,                   /* softwareversion */
	0,                   /* configuration */
	{0, 0, 0, 0, 0},     /* configuration_array */
	false,               /* c_oem_mode */
	false,               /* c_buzzer_disable */
	false,               /* c_potmin_disable */
	false,               /* c_rearm_enable */
	false,               /* c_bootloader_enable */
	0,                   /* numbatteries */
	0,                   /* undervoltagein120V */
	0,                   /* overvoltagein120V */
	0,                   /* undervoltagein220V */
	0,                   /* overvoltagein220V */
	0,                   /* tensionout120V */
	0,                   /* tensionout220V */
	0,                   /* statusval */
	{0, 0, 0, 0, 0, 0},  /* status */
	false,               /* s_220V_in */
	false,               /* s_220V_out */
	false,               /* s_sealed_battery */
	false,               /* s_show_out_tension */
	false,               /* s_show_temperature */
	false,               /* s_show_charger_current */
	0,                   /* chargercurrent */
	0,                   /* checksum */
	0,                   /* checksum_calc */
	false,               /* checksum_ok */
	"----------------",  /* serial */
	0,                   /* year */
	0,                   /* month */
	0,                   /* wday */
	0,                   /* hour */
	0,                   /* minute */
	0,                   /* second */
	0,                   /* alarmyear */
	0,                   /* alarmmonth */
	0,                   /* alarmwday */
	0,                   /* alarmday */
	0,                   /* alarmhour */
	0,                   /* alarmminute */
	0,                   /* alarmsecond */
	0xFE                 /* end_marker */
};

static pkt_data lastpktdata = {
	0xFF,                      /* header */
	0x21,                      /* length */
	'D',                       /* packet_type */
	0,                         /* vacinrms_high */
	0,                         /* vacinrms_low */
	0,                         /* vacinrms */
	0,                         /* vdcmed_high */
	0,                         /* vdcmed_low */
	0,                         /* vdcmed */
	0,                         /* vdcmed_real */
	0,                         /* potrms */
	0,                         /* vacinrmsmin_high */
	0,                         /* vacinrmsmin_low */
	0,                         /* vacinrmsmin */
	0,                         /* vacinrmsmax_high */
	0,                         /* vacinrmsmax_low */
	0,                         /* vacinrmsmax */
	0,                         /* vacoutrms_high */
	0,                         /* vacoutrms_low */
	0,                         /* vacoutrms */
	0,                         /* tempmed_high */
	0,                         /* tempmed_low */
	0,                         /* tempmed */
	0,                         /* tempmed_real */
	0,                         /* icarregrms */
	0,                         /* icarregrms_real */
	0,                         /* statusval */
	{0, 0, 0, 0, 0, 0, 0, 0},  /* status */
	false,                     /* s_battery_mode */
	false,                     /* s_battery_low */
	false,                     /* s_network_failure */
	false,                     /* s_fast_network_failure */
	false,                     /* s_220_in */
	false,                     /* s_220_out */
	false,                     /* s_bypass_on */
	false,                     /* s_charger_on */
	0,                         /* checksum */
	0,                         /* checksum_calc */
	false,                     /* checksum_ok */
	0xFE                       /* end_marker */
};

/* internal methods */
static int get_bit_in_position(void *ptr, size_t size, size_t bit_position, int invertorder);
static float createfloat(int integer, int decimal);
static float read_pair(int first, int second, unsigned int as16);
static float battery_volts(int first, int second, unsigned int as16);
#if 0
static char * strtolow(char* s);
#endif

static unsigned char calculate_checksum(unsigned char *pacote, int inicio, int fim);

static void parse_serial_options(void);
static void close_serial_port(void);
static TYPE_FD_SER openfd(const char *portarg, int requested_baudrate);
static TYPE_FD_SER reconnect_ups_if_needed(void);
#if 0
static int write_serial(int fd, const char * dados, int size);
#endif
static int write_serial_int(TYPE_FD_SER fd, const unsigned int *data, size_t size);
static bool send_initialization_packet(const unsigned int *data, size_t size, TYPE_FD_SER fd);

static void print_pkt_hwinfo(pkt_hwinfo data);
static void print_pkt_data(pkt_data data);
static void pdatapacket(unsigned char *datapkt, size_t size);
static pkt_data mount_datapacket(unsigned char *datapkt, size_t size, double tempodecorrido);
static pkt_hwinfo mount_hwinfo(unsigned char *datapkt, size_t size);
static upsinfo getupsinfo(unsigned int upscode);

static float get_float_option(const char *name);
static unsigned int get_flag_option(const char *name, unsigned int current);
static unsigned int get_va(int equipment);
static float get_vbat(int equipment);
static float get_pf(int equipment);
static unsigned int get_ah(int equipment);
static float get_vin_perc(char *var, int equipment);
static unsigned int get_numbat(void);
static void parse_battery_options(void);
static void parse_model_options(void);
static bool same_ups(const pkt_hwinfo *a, const pkt_hwinfo *b);
static void forget_previous_ups(void);


/* method implementations */

static int get_bit_in_position(void *ptr, size_t size, size_t bit_position, int invertorder) {
	unsigned char  *byte_ptr = (unsigned char *)ptr;
	int            retval = -2;
	size_t         byte_index = bit_position / 8;
	size_t         bit_index = bit_position % 8;

	if (bit_position >= size * 8)
		return -3;  /* Invalid Position */

	if (invertorder == 0)
		retval = (byte_ptr[byte_index] >> (7 - bit_index)) & 1 ? 1 : 0;
	else
		retval = (byte_ptr[byte_index] >> (7 - bit_index)) & 1 ? 0 : 1;
	return retval;
}

static void print_pkt_hwinfo(pkt_hwinfo data) {
	int  i = 0, retorno;

	if (!debug_pkt_hwinfo)
		return;

	upsdebugx(1, "%s: logging packet details at debug verbosity 5 or more", __func__);

	upsdebugx(5, "Header: %u", data.header);
	upsdebugx(5, "Size: %u", data.size);
	upsdebugx(5, "Type: %c", data.type);
	upsdebugx(5, "Model: %u", data.model);
	upsdebugx(5, "Hardware Version: %u", data.hardwareversion);
	upsdebugx(5, "Software Version: %u", data.softwareversion);
	upsdebugx(5, "Configuration: %u", data.configuration);

	upsdebugx(5, "Configuration Array: ");
	upsdebugx(5, "-----");
	for (i = 0; i < 5; i++) {
		retorno = get_bit_in_position(&data.configuration, sizeof(data.configuration), i, 0);
		upsdebugx(5, "Binary value is %d", retorno);
		upsdebugx(5, "%u ", data.configuration_array[i]);
	}  /* end for */
	upsdebugx(5, "-----");

	upsdebugx(5, "OEM Mode: %s", data.c_oem_mode ? "true" : "false");
	upsdebugx(5, "Buzzer Disable: %s", data.c_buzzer_disable ? "true" : "false");
	upsdebugx(5, "Potmin Disable: %s", data.c_potmin_disable ? "true" : "false");
	upsdebugx(5, "Rearm Enable: %s", data.c_rearm_enable ? "true" : "false");
	upsdebugx(5, "Bootloader Enable: %s", data.c_bootloader_enable ? "true" : "false");
	upsdebugx(5, "Number of Batteries: %u", data.numbatteries);
	upsdebugx(5, "Undervoltage In 120V: %u", data.undervoltagein120V);
	upsdebugx(5, "Overvoltage In 120V: %u", data.overvoltagein120V);
	upsdebugx(5, "Undervoltage In 220V: %u", data.undervoltagein220V);
	upsdebugx(5, "Overvoltage In 220V: %u", data.overvoltagein220V);
	upsdebugx(5, "Tension Out 120V: %u", data.tensionout120V);
	upsdebugx(5, "Tension Out 220V: %u", data.tensionout220V);
	upsdebugx(5, "Status Value: %u", data.statusval);

	upsdebugx(5, "Status: ");
	upsdebugx(5, "-----");
	for (i = 0; i < 6; i++) {
		upsdebugx(5, "Binary value is %d", get_bit_in_position(&data.statusval, sizeof(data.statusval), i, 0));
		upsdebugx(5, "status %d --> %u ", i, data.status[i]);
	}  /* end for */
	upsdebugx(5, "-----");

	upsdebugx(5, "220V In: %s", data.s_220V_in ? "true" : "false");
	upsdebugx(5, "220V Out: %s", data.s_220V_out ? "true" : "false");
	upsdebugx(5, "Sealed Battery: %s", data.s_sealed_battery ? "true" : "false");
	upsdebugx(5, "Show Out Tension: %s", data.s_show_out_tension ? "true" : "false");
	upsdebugx(5, "Show Temperature: %s", data.s_show_temperature ? "true" : "false");
	upsdebugx(5, "Show Charger Current: %s", data.s_show_charger_current ? "true" : "false");
	upsdebugx(5, "Charger Current: %u", data.chargercurrent);
	upsdebugx(5, "Checksum: %u", data.checksum);
	upsdebugx(5, "Checksum Calc: %u", data.checksum_calc);
	upsdebugx(5, "Checksum OK: %s", data.checksum_ok ? "true" : "false");
	upsdebugx(5, "Serial: %s", data.serial);
	upsdebugx(5, "Year: %u", data.year);
	upsdebugx(5, "Month: %u", data.month);
	upsdebugx(5, "Weekday: %u", data.wday);
	upsdebugx(5, "Hour: %u", data.hour);
	upsdebugx(5, "Minute: %u", data.minute);
	upsdebugx(5, "Second: %u", data.second);
	upsdebugx(5, "Alarm Year: %u", data.alarmyear);
	upsdebugx(5, "Alarm Month: %u", data.alarmmonth);
	upsdebugx(5, "Alarm Weekday: %u", data.alarmwday);
	upsdebugx(5, "Alarm Day: %u", data.alarmday);
	upsdebugx(5, "Alarm Hour: %u", data.alarmhour);
	upsdebugx(5, "Alarm Minute: %u", data.alarmminute);
	upsdebugx(5, "Alarm Second: %u", data.alarmsecond);
	upsdebugx(5, "End Marker: %u", data.end_marker);
}

static void print_pkt_data(pkt_data data) {
	int  i = 0;

	if (!debug_pkt_data)
		return;

	upsdebugx(1, "%s: logging packet details at debug verbosity 5 or more", __func__);

	upsdebugx(5, "Header: %u", data.header);
	upsdebugx(5, "Length: %u", data.length);
	upsdebugx(5, "Packet Type: %c", data.packet_type);
	upsdebugx(5, "Vacin RMS High: %u", data.vacinrms_high);
	upsdebugx(5, "Vacin RMS Low: %u", data.vacinrms_low);
	upsdebugx(5, "Vacin RMS: %0.2f", data.vacinrms);
	upsdebugx(5, "VDC Med High: %u", data.vdcmed_high);
	upsdebugx(5, "VDC Med Low: %u", data.vdcmed_low);
	upsdebugx(5, "VDC Med: %0.2f", data.vdcmed);
	upsdebugx(5, "VDC Med Real: %0.2f", data.vdcmed_real);
	upsdebugx(5, "Pot RMS: %u", data.potrms);
	upsdebugx(5, "Vacin RMS Min High: %u", data.vacinrmsmin_high);
	upsdebugx(5, "Vacin RMS Min Low: %u", data.vacinrmsmin_low);
	upsdebugx(5, "Vacin RMS Min: %0.2f", data.vacinrmsmin);
	upsdebugx(5, "Vacin RMS Max High: %u", data.vacinrmsmax_high);
	upsdebugx(5, "Vacin RMS Max Low: %u", data.vacinrmsmax_low);
	upsdebugx(5, "Vacin RMS Max: %0.2f", data.vacinrmsmax);
	upsdebugx(5, "Vac Out RMS High: %u", data.vacoutrms_high);
	upsdebugx(5, "Vac Out RMS Low: %u", data.vacoutrms_low);
	upsdebugx(5, "Vac Out RMS: %0.2f", data.vacoutrms);
	upsdebugx(5, "Temp Med High: %u", data.tempmed_high);
	upsdebugx(5, "Temp Med Low: %u", data.tempmed_low);
	upsdebugx(5, "Temp Med: %0.2f", data.tempmed);
	upsdebugx(5, "Temp Med Real: %0.2f", data.tempmed_real);
	upsdebugx(5, "Icar Reg RMS: %u", data.icarregrms);
	upsdebugx(5, "Icar Reg RMS Real: %u", data.icarregrms_real);
	upsdebugx(5, "Status Value: %u", data.statusval);

	upsdebugx(5, "Status: ");
	upsdebugx(5, "-----");
	for (i = 0; i < 8; i++) {
		upsdebugx(5, "Binary value is %d", get_bit_in_position(&data.statusval, sizeof(data.statusval), i, 0));
		upsdebugx(5, "status %d --> %u ", i, data.status[i]);
	}  /* end for */
	upsdebugx(5, "-----");

	upsdebugx(5, "Battery Mode: %s", data.s_battery_mode ? "true" : "false");
	upsdebugx(5, "Battery Low: %s", data.s_battery_low ? "true" : "false");
	upsdebugx(5, "Network Failure: %s", data.s_network_failure ? "true" : "false");
	upsdebugx(5, "Fast Network Failure: %s", data.s_fast_network_failure ? "true" : "false");
	upsdebugx(5, "220 In: %s", data.s_220_in ? "true" : "false");
	upsdebugx(5, "220 Out: %s", data.s_220_out ? "true" : "false");
	upsdebugx(5, "Bypass On: %s", data.s_bypass_on ? "true" : "false");
	upsdebugx(5, "Charger On: %s", data.s_charger_on ? "true" : "false");
	upsdebugx(5, "Checksum: %u", data.checksum);
	upsdebugx(5, "Checksum Calc: %u", data.checksum_calc);
	upsdebugx(5, "Checksum OK: %s", data.checksum_ok ? "true" : "false");
	upsdebugx(5, "End Marker: %u", data.end_marker);
}

/*
 * Load the seven supported serial options from ups.conf. Defaults are reset
 * first so each setting has one source of truth, and every supplied value is
 * fully validated before the serial port is touched.
 */
static void parse_serial_options(void)
{
	const char  *value;
	char        *endptr;
	long        number;
	size_t      i;
	int         supported;

	baudrate = DEFAULTBAUD;
	serial_data_bits = DEFAULT_SERIAL_DATA_BITS;
	snprintf(serial_parity, sizeof(serial_parity), "%s", DEFAULT_SERIAL_PARITY);
	serial_stop_bits = DEFAULT_SERIAL_STOP_BITS;
	snprintf(serial_flow_control, sizeof(serial_flow_control), "%s", DEFAULT_SERIAL_FLOW_CONTROL);
	serial_read_timeout_ms = DEFAULT_SERIAL_READ_TIMEOUT_MS;
	serial_send_pace_us = DEFAULT_SERIAL_SEND_PACE_US;

	/*
	 * The existing table is authoritative: platform-dependent entries guarded
	 * in the original source are accepted only when present in this build.
	 */
	value = getval("baud");
	if (value) {
		errno = 0;
		endptr = NULL;
		number = strtol(value, &endptr, 10);
		if (errno != 0 || endptr == value || *endptr != '\0' || number <= 0)
			fatalx(EXIT_FAILURE, "Invalid baud value '%s': expected a supported baud rate", value);

		supported = 0;
		for (i = 0; i < NUM_BAUD_RATES && !supported; i++)
			if (number == baud_rates[i].speed)
				supported = 1;
		if (!supported)
			fatalx(EXIT_FAILURE, "Invalid baud value '%s': rate is not available in this build", value);
		baudrate = (int)number;
	}  /* end if */

	value = getval("serial_data_bits");
	if (value) {
		/* Reject partial numbers and anything outside the termios CS5..CS8 set. */
		errno = 0;
		endptr = NULL;
		number = strtol(value, &endptr, 10);
		if (errno != 0 || endptr == value || *endptr != '\0' || (number != 5 && number != 6 && number != 7 && number != 8))
			fatalx(EXIT_FAILURE, "Invalid serial_data_bits value '%s': expected 5, 6, 7 or 8", value);
		serial_data_bits = (unsigned int)number;
	}  /* end if */

	value = getval("serial_parity");
	if (value) {
		if (strcasecmp(value, "none") != 0 && strcasecmp(value, "even") != 0 && strcasecmp(value, "odd") != 0)
			fatalx(EXIT_FAILURE, "Invalid serial_parity value '%s': expected none, even or odd", value);
		snprintf(serial_parity, sizeof(serial_parity), "%s", value);
	}  /* end if */

	value = getval("serial_stop_bits");
	if (value) {
		errno = 0;
		endptr = NULL;
		number = strtol(value, &endptr, 10);
		if (errno != 0 || endptr == value || *endptr != '\0' || (number != 1 && number != 2))
			fatalx(EXIT_FAILURE, "Invalid serial_stop_bits value '%s': expected 1 or 2", value);
		serial_stop_bits = (unsigned int)number;
	}  /* end if */

	value = getval("serial_flow_control");
	if (value) {
		if (strcasecmp(value, "none") != 0 && strcasecmp(value, "hardware") != 0 && strcasecmp(value, "software") != 0 && strcasecmp(value, "both") != 0)
			fatalx(EXIT_FAILURE, "Invalid serial_flow_control value '%s': expected none, hardware, software or both", value);
		snprintf(serial_flow_control, sizeof(serial_flow_control), "%s", value);
	}  /* end if */

	value = getval("serial_read_timeout_ms");
	if (value) {
		/* A zero timeout requests a non-blocking poll from ser_get_char. */
		errno = 0;
		endptr = NULL;
		number = strtol(value, &endptr, 10);
		if (errno != 0 || endptr == value || *endptr != '\0' || number < 0 || number > MAX_SERIAL_READ_TIMEOUT_MS)
			fatalx(EXIT_FAILURE, "Invalid serial_read_timeout_ms value '%s': expected 0 to %d", value, MAX_SERIAL_READ_TIMEOUT_MS);
		serial_read_timeout_ms = (unsigned int)number;
	}  /* end if */

	value = getval("serial_send_pace_us");
	if (value) {
		/* Zero selects an unpaced send; positive values delay each byte. */
		errno = 0;
		endptr = NULL;
		number = strtol(value, &endptr, 10);
		if (errno != 0 || endptr == value || *endptr != '\0' || number < 0 || number > MAX_SERIAL_SEND_PACE_US)
			fatalx(EXIT_FAILURE, "Invalid serial_send_pace_us value '%s': expected 0 to %d", value, MAX_SERIAL_SEND_PACE_US);
		serial_send_pace_us = (unsigned int)number;
	}  /* end if */
}

static void close_serial_port(void)
{
	/* Use NUT's portable descriptor checks and always invalidate after closing. */
	if (VALID_FD_SER(serial_fd)) {
		if (ser_close(serial_fd, porta) != 0)
			upsdebug_with_errno(1, "%s: Error closing serial port %s", __func__, porta);
		serial_fd = ERROR_FD_SER;
	}  /* end if */
}

static TYPE_FD_SER openfd(const char *portarg, int requested_baudrate)
{
	TYPE_FD_SER     fd;
	struct termios  tty;
	speed_t         rate = 0;
	size_t          i;

	/* Translate the numeric ups.conf value to the matching termios constant. */
	for (i = 0; i < NUM_BAUD_RATES && rate == 0; i++) {
		if (baud_rates[i].speed == requested_baudrate) {
			rate = baud_rates[i].rate;
			upsdebugx(1, "%s: Selected baud rate %d -- %s", __func__, baud_rates[i].speed, baud_rates[i].description);
		}  /* end if */
	}  /* end for */

	if (rate == 0) {
		upslogx(LOG_ERR, "%s: Unsupported baud rate %d", __func__, requested_baudrate);
		return ERROR_FD_SER;
	}  /* end if */

	/*
	 * Establish NUT's standard raw serial baseline first, then customize only
	 * data bits, parity, stop bits and flow control below.
	 */
	fd = ser_open_nf(portarg);
	if (INVALID_FD_SER(fd)) {
		upsdebug_with_errno(1, "%s: Error opening %s", __func__, portarg);
		return ERROR_FD_SER;
	}  /* end if */

	if (ser_set_speed_nf(fd, portarg, rate) != 0) {
		upsdebug_with_errno(1, "%s: Error setting baud rate on %s", __func__, portarg);
		ser_close(fd, portarg);
		return ERROR_FD_SER;
	}  /* end if */

	if (tcgetattr(fd, &tty) != 0) {
		upsdebug_with_errno(1, "%s: Error reading serial settings from %s", __func__, portarg);
		ser_close(fd, portarg);
		return ERROR_FD_SER;
	}  /* end if */

	/* Replace the baseline character size with the validated selection. */
	tty.c_cflag &= ~CSIZE;
	switch (serial_data_bits) {
		case 5:
			tty.c_cflag |= CS5;
			break;
		case 6:
			tty.c_cflag |= CS6;
			break;
		case 7:
			tty.c_cflag |= CS7;
			break;
		default:
			tty.c_cflag |= CS8;
			break;
	}  /* end switch */

	/*
	 * Configure parity as a complete unit. Parity errors are ignored for
	 * "none", but checked for the even and odd modes.
	 */
	tty.c_cflag &= ~(PARENB | PARODD);
	tty.c_iflag &= ~INPCK;
	if (strcasecmp(serial_parity, "none") == 0)
		tty.c_iflag |= IGNPAR;
	else {
		if (strcasecmp(serial_parity, "even") == 0) {
			tty.c_iflag &= ~IGNPAR;
			tty.c_cflag |= PARENB;
			tty.c_iflag |= INPCK;
		}  /* end if */
		else {
			tty.c_iflag &= ~IGNPAR;
			tty.c_cflag |= PARENB | PARODD;
			tty.c_iflag |= INPCK;
		}  /* end else */
	}  /* end else */

	/* CSTOPB clear means one stop bit; set means two stop bits. */
	if (serial_stop_bits == 2)
		tty.c_cflag |= CSTOPB;
	else
		tty.c_cflag &= ~CSTOPB;

	/*
	 * Disable every flow-control mechanism before enabling the selected mode.
	 * In particular, "none" (the default, see DEFAULT_SERIAL_FLOW_CONTROL)
	 * leaves RTS/CTS off for CDC-ACM and 3-wire links.
	 * Both mechanisms are disabled here; the configured mode is enabled below.
	 */
	tty.c_cflag &= ~CRTSCTS;
	tty.c_iflag &= ~(IXON | IXOFF | IXANY);

	if (strcasecmp(serial_flow_control, "none") == 0) {
		/* Hardware and software flow control remain disabled. */
	}  /* end if */
	else {
		if (strcasecmp(serial_flow_control, "hardware") == 0)
			tty.c_cflag |= CRTSCTS;
		else {
			if (strcasecmp(serial_flow_control, "software") == 0)
				tty.c_iflag |= IXON | IXOFF;
			else {
				if (strcasecmp(serial_flow_control, "both") == 0) {
					tty.c_cflag |= CRTSCTS;
					tty.c_iflag |= IXON | IXOFF;
				}  /* end if */
			}  /* end else */
		}  /* end else */
	}  /* end else */

	/* Apply the four conventional settings together. */
	if (tcsetattr(fd, TCSANOW, &tty) != 0) {
		upsdebug_with_errno(1, "%s: Error applying serial settings to %s", __func__, portarg);
		ser_close(fd, portarg);
		return ERROR_FD_SER;
	}  /* end if */

	/* Discard bytes queued under any previous port configuration. */
	if (ser_flush_io(fd) != 0) {
		upsdebug_with_errno(1, "%s: Error flushing serial port %s", __func__, portarg);
		ser_close(fd, portarg);
		return ERROR_FD_SER;
	}  /* end if */

	upsdebugx(1, "%s: Serial settings: %d baud, %u data bits, %s parity, %u stop bit(s), %s flow control, read timeout %u ms, send pace %u us", __func__, requested_baudrate, serial_data_bits, serial_parity, serial_stop_bits, serial_flow_control, serial_read_timeout_ms, serial_send_pace_us);

	return fd;
}

static unsigned char calculate_checksum(unsigned char *pacote, int inicio, int fim) {
	int  soma = 0, i = 0;

	for (i = inicio; i <= fim; i++)
		soma += pacote[i];
	return (soma & 0xFF);
}

static void pdatapacket(unsigned char *datapkt, size_t size) {
	size_t  i = 0;

	if (!debug_pkt_raw)
		return;

	if (datapkt != NULL) {
		/* FIXME: convert to upsdebug_hex()? */
		upsdebugx(1, "%s: logging received data packet bytes at debug verbosity 5 or more", __func__);

		for (i = 0; i < size; i++)
			upsdebugx(5, "\tPosition %" PRIuSIZE " -- 0x%02X -- Decimal %d -- Char %c", i, datapkt[i], datapkt[i], datapkt[i]);
	}  /* end if */
}

static float createfloat(int integer, int decimal) {
	char  flt[1024];
	snprintf(flt, sizeof(flt), "%d.%d", integer, decimal);
	return atof(flt);
}

/*
 * One two-byte field of the data packet, read in one of the two ways
 * described at r16_* in upsinfo: as "first.second" (as16 = 0, the driver's
 * own reading) or as the 16-bit number first + 256 * second (as16 = 1, the
 * reading of the manufacturer's software).
 */
static float read_pair(int first, int second, unsigned int as16) {
	if (as16)
		return (float)(first + 256 * second);
	return createfloat(first, second);
}

/*
 * Battery voltage, in V, from its two bytes. The UPS sends it in tenths of
 * a volt. In the driver's own reading that is told from the second byte
 * being 0; in the 16-bit reading it always holds.
 */
static float battery_volts(int first, int second, unsigned int as16) {
	float  raw = read_pair(first, second, as16);

	if (as16 || second == 0)
		return raw / 10;
	return raw;
}

static pkt_data mount_datapacket(unsigned char *datapkt, size_t size, double tempodecorrido)  {
	size_t         i = 0;
	unsigned char  checksum = 0x00;
	upsinfo        ups;
	pkt_data pktdata = {
		0xFF,                      /* header */
		0x21,                      /* length */
		'D',                       /* packet_type */
		0,                         /* vacinrms_high */
		0,                         /* vacinrms_low */
		0,                         /* vacinrms */
		0,                         /* vdcmed_high */
		0,                         /* vdcmed_low */
		0,                         /* vdcmed */
		0,                         /* vdcmed_real */
		0,                         /* potrms */
		0,                         /* vacinrmsmin_high */
		0,                         /* vacinrmsmin_low */
		0,                         /* vacinrmsmin */
		0,                         /* vacinrmsmax_high */
		0,                         /* vacinrmsmax_low */
		0,                         /* vacinrmsmax */
		0,                         /* vacoutrms_high */
		0,                         /* vacoutrms_low */
		0,                         /* vacoutrms */
		0,                         /* tempmed_high */
		0,                         /* tempmed_low */
		0,                         /* tempmed */
		0,                         /* tempmed_real */
		0,                         /* icarregrms */
		0,                         /* icarregrms_real */
		0,                         /* statusval */
		{0, 0, 0, 0, 0, 0, 0, 0},  /* status */
		false,                     /* s_battery_mode */
		false,                     /* s_battery_low */
		false,                     /* s_network_failure */
		false,                     /* s_fast_network_failure */
		false,                     /* s_220_in */
		false,                     /* s_220_out */
		false,                     /* s_bypass_on */
		false,                     /* s_charger_on */
		0,                         /* checksum */
		0,                         /* checksum_calc */
		false,                     /* checksum_ok */
		0xFE                       /* end_marker */
	};

	NUT_UNUSED_VARIABLE(tempodecorrido);

	/* How each two-byte field is to be read: the r16_* flags of upsinfo. */
	ups = getupsinfo(lastpkthwinfo.model);

	pktdata.length = (int)datapkt[1];
	pktdata.packet_type = datapkt[2];
	pktdata.vacinrms_high = (int)datapkt[3];
	pktdata.vacinrms_low = (int)datapkt[4];
	pktdata.vacinrms = read_pair(pktdata.vacinrms_high, pktdata.vacinrms_low, ups.r16_vin);
	/*
	 * Battery voltage, bytes [5] and [6].
	 *
	 * The table "PACOTE DE DADOS" of the manufacturer's "Protocolo NHS 3.0"
	 * document describes them as "TENSAO DA BATERIA - PARTE ALTA, 1 PESO
	 * POR VOLT" and "PARTE BAIXA, FRACIONARIO", the same words it uses for
	 * the input and the output voltage. For the battery that does not
	 * hold: byte [5] carries tenths of a volt, so 136 means 13.6V. That is
	 * why vdcmed_real, the value the rest of the driver uses, is vdcmed
	 * divided by 10 when byte [6] is 0.
	 *
	 * I wrote this rule in my first Python decoder and brought it to the
	 * driver without a word of explanation, and without one it reads as a
	 * mistake: a battery at exactly 13.0V would seem to come out as 1.3V.
	 * It is not a mistake. Where the rule comes from:
	 *
	 *  - Real packets. The data packets of my own unit that I kept in my
	 *    emulator (emulador-nhs.py, under "Transmite dados reais") have
	 *    0x88 0x00 in these two bytes, that is 136 and 0, with the
	 *    batteries at 13.6V.
	 *
	 *  - The manufacturer's monitoring software. On 2024-11-22 I fed it
	 *    from that emulator with the same number in byte [5] and in the
	 *    high byte of the output voltage (byte [12]), counting up from 0
	 *    to 34, for hardware versions 0 to 3 and from 1 to 5 batteries.
	 *    Its event log (the SQLite file nhsupsserver.dat, table "evento")
	 *    has "tensao_saida" equal to that number and "tensao_bateria"
	 *    equal to one tenth of it in every one of the 80 readings.
	 *
	 *  - The manufacturer's own example. The sample data packet printed
	 *    in the protocol document has 7F 00 here: 127, for 12.7V.
	 *
	 * When byte [6] is not 0 this reading takes the value as it comes,
	 * byte [5] as volts and byte [6] as the fraction. The manufacturer's
	 * software does not: it reads the two bytes as one 16-bit number in
	 * tenths of a volt (see r16_* in upsinfo). The two agree up to 25.5V.
	 * The driver's own reading stays the default; read16_vbat = 1 in
	 * ups.conf selects the other one, which is the one to try on a unit
	 * that reports more than 25.5V in this field.
	 */
	pktdata.vdcmed_high = (int)datapkt[5];
	pktdata.vdcmed_low = (int)datapkt[6];
	pktdata.vdcmed = read_pair(pktdata.vdcmed_high, pktdata.vdcmed_low, ups.r16_vbat);
	pktdata.vdcmed_real = battery_volts(pktdata.vdcmed_high, pktdata.vdcmed_low, ups.r16_vbat);
	pktdata.potrms = (int)datapkt[7];
	pktdata.vacinrmsmin_high = (int)datapkt[8];
	pktdata.vacinrmsmin_low = (int)datapkt[9];
	pktdata.vacinrmsmin = read_pair(pktdata.vacinrmsmin_high, pktdata.vacinrmsmin_low, ups.r16_vin_min);
	pktdata.vacinrmsmax_high = (int)datapkt[10];
	pktdata.vacinrmsmax_low = (int)datapkt[11];
	pktdata.vacinrmsmax = read_pair(pktdata.vacinrmsmax_high, pktdata.vacinrmsmax_low, ups.r16_vin_max);
	pktdata.vacoutrms_high = (int)datapkt[12];
	pktdata.vacoutrms_low = (int)datapkt[13];
	pktdata.vacoutrms = read_pair(pktdata.vacoutrms_high, pktdata.vacoutrms_low, ups.r16_vout);
	pktdata.tempmed_high = (int)datapkt[14];
	pktdata.tempmed_low = (int)datapkt[15];
	pktdata.tempmed = read_pair(pktdata.tempmed_high, pktdata.tempmed_low, ups.r16_temp);
	pktdata.tempmed_real = pktdata.tempmed;
	pktdata.icarregrms = (int)datapkt[16];
	/* 25 units = 750mA, then 1 unit = 30mA */
	pktdata.icarregrms_real = pktdata.icarregrms * 30;
	pktdata.statusval = datapkt[17];
	for (i = 0; i < 8; i++)
		pktdata.status[i] = get_bit_in_position(&datapkt[17], sizeof(datapkt[17]), i, 0);

	/* I don't know WHY, but bit order is INVERTED here.
	 * Discovered on clyra's github python implementation
	 */
	/* TODO: check if ANY OTHER VARIABLES (like hardware array bits)
	 *  have same problem. I won't have so much equipment to test
	 *  these, then we need help to test more
	 */
	pktdata.s_battery_mode = (bool)pktdata.status[7];
	pktdata.s_battery_low = (bool)pktdata.status[6];
	pktdata.s_network_failure = (bool)pktdata.status[5];
	pktdata.s_fast_network_failure = (bool)pktdata.status[4];
	pktdata.s_220_in = (bool)pktdata.status[3];
	pktdata.s_220_out = (bool)pktdata.status[2];
	pktdata.s_bypass_on = (bool)pktdata.status[1];
	pktdata.s_charger_on = (bool)pktdata.status[0];
	/* Position 18 means status, but I won't discover what's it */
	pktdata.checksum = datapkt[19];
	checksum = calculate_checksum(datapkt, 1, 18);
	pktdata.checksum_calc = checksum;
	if (pktdata.checksum == checksum)
		pktdata.checksum_ok = true;
	/*
	 * This function only decodes the packet. The values that used to be
	 * derived here (the voltage of the whole bank, measured and nominal)
	 * are worked out in interpret_pkt_data(), which is where the ratings
	 * and the ups.conf settings they depend on are known; the reasons are
	 * written there, at "experimental.nhs.data.battery_tension".
	 */

	if (debug_pkt_data) {
		pdatapacket(datapkt, size);
		print_pkt_data(pktdata);
	}  /* end if */

	return pktdata;
}

static pkt_hwinfo mount_hwinfo(unsigned char *datapkt, size_t size) {
	size_t         i = 0;
	unsigned char  checksum = 0x00;
	pkt_hwinfo pkthwinfo = {
		0xFF,                /* header */
		0,                   /* size */
		'S',                 /* type */
		0,                   /* model */
		0,                   /* hardwareversion */
		0,                   /* softwareversion */
		0,                   /* configuration */
		{0, 0, 0, 0, 0},     /* configuration_array */
		false,               /* c_oem_mode */
		false,               /* c_buzzer_disable */
		false,               /* c_potmin_disable */
		false,               /* c_rearm_enable */
		false,               /* c_bootloader_enable */
		0,                   /* numbatteries */
		0,                   /* undervoltagein120V */
		0,                   /* overvoltagein120V */
		0,                   /* undervoltagein220V */
		0,                   /* overvoltagein220V */
		0,                   /* tensionout120V */
		0,                   /* tensionout220V */
		0,                   /* statusval */
		{0, 0, 0, 0, 0, 0},  /* status */
		false,               /* s_220V_in */
		false,               /* s_220V_out */
		false,               /* s_sealed_battery */
		false,               /* s_show_out_tension */
		false,               /* s_show_temperature */
		false,               /* s_show_charger_current */
		0,                   /* chargercurrent */
		0,                   /* checksum */
		0,                   /* checksum_calc */
		false,               /* checksum_ok */
		"----------------",  /* serial */
		0,                   /* year */
		0,                   /* month */
		0,                   /* wday */
		0,                   /* hour */
		0,                   /* minute */
		0,                   /* second */
		0,                   /* alarmyear */
		0,                   /* alarmmonth */
		0,                   /* alarmwday */
		0,                   /* alarmday */
		0,                   /* alarmhour */
		0,                   /* alarmminute */
		0,                   /* alarmsecond */
		0xFE                 /* end_marker */
	};

	pkthwinfo.size = (int)datapkt[1];
	pkthwinfo.type = datapkt[2];
	pkthwinfo.model = (int)datapkt[3];
	pkthwinfo.hardwareversion = (int)datapkt[4];
	pkthwinfo.softwareversion = (int)datapkt[5];
	pkthwinfo.configuration = datapkt[6];
	for (i = 0; i < 5; i++)
		pkthwinfo.configuration_array[i] = get_bit_in_position(&datapkt[6], sizeof(datapkt[6]), i, 0);
	/* TODO: check if ANY OTHER VARIABLES (like hardware array bits)
	 * have same problem. I won't have so much equipment to test
	 * these, then we need help to test more */
	pkthwinfo.c_oem_mode = (bool)pkthwinfo.configuration_array[0];
	pkthwinfo.c_buzzer_disable = (bool)pkthwinfo.configuration_array[1];
	pkthwinfo.c_potmin_disable = (bool)pkthwinfo.configuration_array[2];
	pkthwinfo.c_rearm_enable = (bool)pkthwinfo.configuration_array[3];
	pkthwinfo.c_bootloader_enable = (bool)pkthwinfo.configuration_array[4];
	pkthwinfo.numbatteries = (int)datapkt[7];
	pkthwinfo.undervoltagein120V = (int)datapkt[8];
	pkthwinfo.overvoltagein120V = (int)datapkt[9];
	pkthwinfo.undervoltagein220V = (int)datapkt[10];
	pkthwinfo.overvoltagein220V = (int)datapkt[11];
	pkthwinfo.tensionout120V = (int)datapkt[12];
	pkthwinfo.tensionout220V = (int)datapkt[13];
	pkthwinfo.statusval = datapkt[14];
	for (i = 0; i < 6; i++)
		pkthwinfo.status[i] = get_bit_in_position(&datapkt[14], sizeof(datapkt[14]), i, 0);
	/* TODO: check if ANY OTHER VARIABLES (like hardware array bits)
	 * have same problem. I won't have so much equipment to test
	 * these, then we need help to test more */
	pkthwinfo.s_220V_in = (bool)pkthwinfo.status[0];
	pkthwinfo.s_220V_out = (bool)pkthwinfo.status[1];
	pkthwinfo.s_sealed_battery = (bool)pkthwinfo.status[2];
	pkthwinfo.s_show_out_tension = (bool)pkthwinfo.status[3];
	pkthwinfo.s_show_temperature = (bool)pkthwinfo.status[4];
	pkthwinfo.s_show_charger_current = (bool)pkthwinfo.status[5];
	pkthwinfo.chargercurrent = (int)datapkt[15];
	if (pkthwinfo.size > 18) {
		for (i = 0; i < 16; i++)
			pkthwinfo.serial[i] = datapkt[16 + i];
		pkthwinfo.year = datapkt[32];
		pkthwinfo.month = datapkt[33];
		pkthwinfo.wday = datapkt[34];
		pkthwinfo.hour = datapkt[35];
		pkthwinfo.minute = datapkt[36];
		pkthwinfo.second = datapkt[37];
		pkthwinfo.alarmyear = datapkt[38];
		pkthwinfo.alarmmonth = datapkt[39];
		pkthwinfo.alarmday = datapkt[40];
		pkthwinfo.alarmhour = datapkt[41];
		pkthwinfo.alarmminute = datapkt[42];
		pkthwinfo.alarmsecond = datapkt[43];
		pkthwinfo.checksum = datapkt[48];
		checksum = calculate_checksum(datapkt, 1, 47);
	}  /* end if */
	else {
		pkthwinfo.checksum = datapkt[16];
		checksum = calculate_checksum(datapkt, 1, 15);
	}  /* end else */
	pkthwinfo.checksum_calc = checksum;
	if (pkthwinfo.checksum == checksum)
		pkthwinfo.checksum_ok = true;

	if (debug_pkt_hwinfo) {
		pdatapacket(datapkt, size);
		print_pkt_hwinfo(pkthwinfo);
	}  /* end if */

	return pkthwinfo;
}

#if 0
static int write_serial(int fd, const char *dados, size_t size) {
	ssize_t  bytes_written;

	if (fd > 0) {
		bytes_written = write(fd, dados, size);
		if (bytes_written < 0)
			return -1;
		if (tcdrain(fd) != 0)
			return -2;
		return size;
	}  /* end if */
	else
		return fd;
}
#endif

static int write_serial_int(TYPE_FD_SER fd, const unsigned int *data, size_t size)
{
	uint8_t  *message;
	ssize_t  sent;
	size_t   i;

	if (INVALID_FD_SER(fd))
		return -1;

	/*
	 * NHS commands are declared as unsigned int arrays in the original
	 * protocol code. Convert each element to the byte buffer expected by the
	 * NUT serial API without changing command contents or framing.
	 */
	message = (uint8_t *)xcalloc(size, sizeof(*message));
	for (i = 0; i < size; i++)
		message[i] = (uint8_t)data[i];

	/* Avoid per-byte delays unless serial_send_pace_us explicitly requests one. */
	if (serial_send_pace_us == 0)
		sent = ser_send_buf(fd, message, size);
	else
		sent = ser_send_buf_pace(fd, (useconds_t)serial_send_pace_us, message, size);

	free(message);

	/* A partial transmission is a communication failure, not a successful send. */
	if (sent < 0 || (size_t)sent != size)
		return -1;

	return (int)sent;
}

#if 0
static char * strtolow(char* s) {
	char  *p;
	for (p = s; *p; p++)
		*p = tolower(*p);
	return s;
}
#endif

/*
 * The UNKNOWN device.
 *
 * I created this entry so that a model the driver does not know would still
 * go through the same fields as any other model, with values that are safe
 * to use: it is what the "default" of the switch in getupsinfo() answers
 * with. Over time the code drifted away from that idea: get_pf(), get_vbat()
 * and get_vin_perc() each returned a default of their own, and get_va()
 * ended the driver when the nominal power was missing instead of using what
 * is here. The decision is:
 *
 *  - This is the only place that holds the driver's defaults. To change what
 *    happens when a value is not known, change it here and nowhere else.
 *
 *  - It answers for a model code that has no case in getupsinfo(), and also
 *    for any field that the case of a listed model does not set.
 *
 *  - getupsinfo() is the only function that reads it. A get_*() helper that
 *    finds no value in ups.conf nor in the case of the model returns the
 *    value held here, never one of its own.
 *
 *  - The ratings that cannot be guessed (VA, W, efficiency and battery
 *    capacity) are 0 here on purpose. Guessing a nominal power would publish
 *    invented power and runtime figures as if they were real. With 0 the
 *    driver keeps running and those variables are simply not published
 *    until ups.conf provides the value.
 *
 *  - Nothing else in the driver special-cases an unknown model, logs about
 *    it or stops because a rating is missing.
 */
static const upsinfo  ups_unknown_device = {
	.upscode       = (unsigned int)-1,
	.upsdesc       = "NHS UNKNOWN",
	.VA            = 0,
	.pversion      = DEFAULT_PROTOCOL_VERSION,
	.bypassasalarm = DEFAULT_BYPASS_AS_ALARM,
	.watts         = 0,
	.pf            = (float)DEFAULTPF,
	.eff_line      = 0,
	.eff_line_conv = 0,
	.eff_inverter  = 0,
	.battery_ah    = 0,
	.battery_v     = (float)DEFAULTBATV,
	.bat_series    = DEFAULTBATSER,
	.r16_vin       = DEFAULTREAD16,
	.r16_vin_min   = DEFAULTREAD16,
	.r16_vin_max   = DEFAULTREAD16,
	.r16_vout      = DEFAULTREAD16,
	.r16_temp      = DEFAULTREAD16,
	.r16_vbat      = DEFAULTREAD16,
	.overload_perc = DEFAULTOVERLOAD,
	.vin_perc      = (float)DEFAULTPERC
};

/*
 * Model data: one case per model code reported by the UPS, and "default" for
 * a code that has no case.
 *
 * This is the switch I first wrote the driver with. For a while it was
 * replaced by a table indexed by the model code and filled through a macro.
 * That was shorter, but each row was a list of bare numbers whose meaning
 * had to be looked up in a legend. I brought the switch back for two
 * reasons:
 *
 *  - Maintenance of the model data. Adding a model or correcting one is
 *    writing or editing its own case, without touching a macro, the size of
 *    a table or the position of a column.
 *
 *  - Readability for whoever is maintaining the driver now. In a case every
 *    value is assigned to a named field, so a model can be read, checked
 *    against its product sheet and corrected on its own.
 *
 * A case sets only what is known about its model. Everything it does not set
 * keeps the value of the UNKNOWN device, because the answer starts as a copy
 * of it; "default" therefore has nothing to assign.
 *
 * upscode, upsdesc and VA are the data the driver always had. The lines
 * under a "product sheet" comment come from the NHS product sheets
 * (nhs.com.br): watts, the three efficiencies, battery_ah and bat_series;
 * upsinfo describes each field. A model code is linked to a product by
 * name: same VA and same family words. A value is set only when every
 * matching product agrees on it. Old generations without a sheet of their
 * own use the current product of the same family and VA. The comment gives
 * the product code the values were taken from ("+N" when N more products
 * with the same values matched).
 */
static upsinfo getupsinfo(unsigned int upscode) {
	upsinfo  data = ups_unknown_device;
	char     *overridemodel = getval("overridemodel");
	char     *protocolversion = getval("protocolversion");
	char     *bypassasalarm = getval("bypassasalarm");
	char     *va = getval("va");
	char     *pf = getval("pf");
	char     *ah = getval("ah");
	char     *vbat = getval("vbat");
	char     *series = getval("battery_series");
	char     *overload = getval("overload_perc");
	float    value = 0;

	/*
	 * The three steps described at upsinfo are applied from the weakest to the
	 * strongest, so that a stronger source overwrites a weaker one.
	 *
	 * Step 3 was the initialization above: start from the UNKNOWN device.
	 * Step 2 is this switch: the case of the model sets what is known for it.
	 */
	switch (upscode) {
		case 1:
			data.upscode = 1;
			strncpy(data.upsdesc, "NHS COMPACT PLUS", sizeof(data.upsdesc));
			data.VA = 1000;
			/* product sheet 90.B1.A01000 */
			data.watts = 450;
			data.eff_line = 95;
			data.eff_line_conv = 90;
			data.eff_inverter = 80;
			data.battery_ah = 7;
			data.bat_series = 1;
			break;

		case 2:
			data.upscode = 2;
			strncpy(data.upsdesc, "NHS COMPACT PLUS SENOIDAL", sizeof(data.upsdesc));
			data.VA = 1000;
			/* product sheet 91.A0.010300 +2 */
			data.eff_line = 95;
			data.eff_line_conv = 90;
			data.eff_inverter = 80;
			data.bat_series = 1;
			break;

		case 3:
			data.upscode = 3;
			strncpy(data.upsdesc, "NHS COMPACT PLUS RACK", sizeof(data.upsdesc));
			data.VA = 1000;
			break;

		case 4:
			data.upscode = 4;
			strncpy(data.upsdesc, "NHS PREMIUM PDV", sizeof(data.upsdesc));
			data.VA = 1500;
			break;

		case 5:
			data.upscode = 5;
			strncpy(data.upsdesc, "NHS PREMIUM PDV SENOIDAL", sizeof(data.upsdesc));
			data.VA = 1500;
			/* product sheet 91.B0.015700 +3 */
			data.eff_line = 95;
			data.eff_line_conv = 90;
			data.bat_series = 1;
			break;

		case 6:
			data.upscode = 6;
			strncpy(data.upsdesc, "NHS PREMIUM 1500VA", sizeof(data.upsdesc));
			data.VA = 1500;
			/* product sheet 90.D1.015000 +4 */
			data.watts = 750;
			data.eff_line = 95;
			data.eff_line_conv = 90;
			data.eff_inverter = 80;
			data.battery_ah = 58;
			data.bat_series = 1;
			break;

		case 7:
			data.upscode = 7;
			strncpy(data.upsdesc, "NHS PREMIUM 2200VA", sizeof(data.upsdesc));
			data.VA = 2200;
			break;

		case 8:
			data.upscode = 8;
			strncpy(data.upsdesc, "NHS PREMIUM SENOIDAL", sizeof(data.upsdesc));
			data.VA = 1500;
			break;

		case 9:
			data.upscode = 9;
			strncpy(data.upsdesc, "NHS LASER 2600VA", sizeof(data.upsdesc));
			data.VA = 2600;
			break;

		case 10:
			data.upscode = 10;
			strncpy(data.upsdesc, "NHS LASER 3300VA", sizeof(data.upsdesc));
			data.VA = 3300;
			break;

		case 11:
			data.upscode = 11;
			strncpy(data.upsdesc, "NHS LASER 2600VA ISOLADOR", sizeof(data.upsdesc));
			data.VA = 2600;
			break;

		case 12:
			data.upscode = 12;
			strncpy(data.upsdesc, "NHS LASER SENOIDAL", sizeof(data.upsdesc));
			data.VA = 2600;
			/* product sheet 91.C0.026300 */
			data.watts = 1560;
			data.eff_line = 95;
			data.eff_line_conv = 90;
			data.eff_inverter = 75;
			data.battery_ah = 58;
			data.bat_series = 1;
			break;

		case 13:
			data.upscode = 13;
			strncpy(data.upsdesc, "NHS LASER ON-LINE", sizeof(data.upsdesc));
			data.VA = 2600;
			break;

		case 15:
			data.upscode = 15;
			strncpy(data.upsdesc, "NHS COMPACT PLUS 2003", sizeof(data.upsdesc));
			data.VA = 1000;
			/* product sheet 90.B1.A01000 */
			data.watts = 450;
			data.eff_line = 95;
			data.eff_line_conv = 90;
			data.eff_inverter = 80;
			data.battery_ah = 7;
			data.bat_series = 1;
			break;

		case 16:
			data.upscode = 16;
			strncpy(data.upsdesc, "COMPACT PLUS SENOIDAL 2003", sizeof(data.upsdesc));
			data.VA = 1000;
			/* product sheet 91.A0.010300 +2 */
			data.eff_line = 95;
			data.eff_line_conv = 90;
			data.eff_inverter = 80;
			data.bat_series = 1;
			break;

		case 17:
			data.upscode = 17;
			strncpy(data.upsdesc, "COMPACT PLUS RACK 2003", sizeof(data.upsdesc));
			data.VA = 1000;
			break;

		case 18:
			data.upscode = 18;
			strncpy(data.upsdesc, "PREMIUM PDV 2003", sizeof(data.upsdesc));
			data.VA = 1500;
			break;

		case 19:
			data.upscode = 19;
			strncpy(data.upsdesc, "PREMIUM PDV SENOIDAL 2003", sizeof(data.upsdesc));
			data.VA = 1500;
			/* product sheet 91.B0.015700 +3 */
			data.eff_line = 95;
			data.eff_line_conv = 90;
			data.bat_series = 1;
			break;

		case 20:
			data.upscode = 20;
			strncpy(data.upsdesc, "PREMIUM 1500VA 2003", sizeof(data.upsdesc));
			data.VA = 1500;
			/* product sheet 90.D1.015000 +4 */
			data.watts = 750;
			data.eff_line = 95;
			data.eff_line_conv = 90;
			data.eff_inverter = 80;
			data.battery_ah = 58;
			data.bat_series = 1;
			break;

		case 21:
			data.upscode = 21;
			strncpy(data.upsdesc, "PREMIUM 2200VA 2003", sizeof(data.upsdesc));
			data.VA = 2200;
			break;

		case 22:
			data.upscode = 22;
			strncpy(data.upsdesc, "PREMIUM SENOIDAL 2003", sizeof(data.upsdesc));
			data.VA = 1500;
			break;

		case 23:
			data.upscode = 23;
			strncpy(data.upsdesc, "LASER 2600VA 2003", sizeof(data.upsdesc));
			data.VA = 2600;
			break;

		case 24:
			data.upscode = 24;
			strncpy(data.upsdesc, "LASER 3300VA 2003", sizeof(data.upsdesc));
			data.VA = 3300;
			break;

		case 25:
			data.upscode = 25;
			strncpy(data.upsdesc, "LASER 2600VA ISOLADOR 2003", sizeof(data.upsdesc));
			data.VA = 2600;
			break;

		case 26:
			data.upscode = 26;
			strncpy(data.upsdesc, "LASER SENOIDAL 2003", sizeof(data.upsdesc));
			data.VA = 2600;
			/* product sheet 91.C0.026300 */
			data.watts = 1560;
			data.eff_line = 95;
			data.eff_line_conv = 90;
			data.eff_inverter = 75;
			data.battery_ah = 58;
			data.bat_series = 1;
			break;

		case 27:
			data.upscode = 27;
			strncpy(data.upsdesc, "PDV ONLINE 2003", sizeof(data.upsdesc));
			data.VA = 1500;
			break;

		case 28:
			data.upscode = 28;
			strncpy(data.upsdesc, "LASER ONLINE 2003", sizeof(data.upsdesc));
			data.VA = 3300;
			break;

		case 29:
			data.upscode = 29;
			strncpy(data.upsdesc, "EXPERT ONLINE 2003", sizeof(data.upsdesc));
			data.VA = 5000;
			break;

		case 30:
			data.upscode = 30;
			strncpy(data.upsdesc, "MINI 2", sizeof(data.upsdesc));
			data.VA = 500;
			break;

		case 31:
			data.upscode = 31;
			strncpy(data.upsdesc, "COMPACT PLUS 2", sizeof(data.upsdesc));
			data.VA = 1000;
			/* product sheet 90.B1.A01000 */
			data.watts = 450;
			data.eff_line = 95;
			data.eff_line_conv = 90;
			data.eff_inverter = 80;
			data.battery_ah = 7;
			data.bat_series = 1;
			break;

		case 32:
			data.upscode = 32;
			strncpy(data.upsdesc, "LASER ON-LINE", sizeof(data.upsdesc));
			data.VA = 2600;
			break;

		case 33:
			data.upscode = 33;
			strncpy(data.upsdesc, "PDV SENOIDAL 1500VA", sizeof(data.upsdesc));
			data.VA = 1500;
			break;

		case 34:
			data.upscode = 34;
			strncpy(data.upsdesc, "PDV SENOIDAL 1000VA", sizeof(data.upsdesc));
			data.VA = 1000;
			break;

		case 36:
			data.upscode = 36;
			strncpy(data.upsdesc, "LASER ONLINE 3750VA", sizeof(data.upsdesc));
			data.VA = 3750;
			break;

		case 37:
			data.upscode = 37;
			strncpy(data.upsdesc, "LASER ONLINE 5000VA", sizeof(data.upsdesc));
			data.VA = 5000;
			break;

		case 38:
			data.upscode = 38;
			strncpy(data.upsdesc, "PREMIUM SENOIDAL 2000VA", sizeof(data.upsdesc));
			data.VA = 2000;
			/* product sheet 91.B0.020300 */
			data.watts = 1800;
			data.eff_line = 95;
			data.eff_line_conv = 90;
			data.eff_inverter = 80;
			data.battery_ah = 7;
			data.bat_series = 1;
			break;

		case 39:
			data.upscode = 39;
			strncpy(data.upsdesc, "LASER SENOIDAL 3500VA", sizeof(data.upsdesc));
			data.VA = 3500;
			/* product sheet 91.D1.035100 */
			data.watts = 3150;
			data.eff_line = 95;
			data.eff_line_conv = 90;
			data.eff_inverter = 80;
			data.battery_ah = 7;
			data.bat_series = 1;
			break;

		case 40:
			data.upscode = 40;
			strncpy(data.upsdesc, "PREMIUM PDV 1200VA", sizeof(data.upsdesc));
			data.VA = 1200;
			/* product sheet 90.C1.012200 +1 */
			data.watts = 600;
			data.eff_line = 95;
			data.eff_line_conv = 90;
			data.eff_inverter = 80;
			data.battery_ah = 45;
			data.bat_series = 1;
			break;

		case 41:
			data.upscode = 41;
			strncpy(data.upsdesc, "PREMIUM 1500VA", sizeof(data.upsdesc));
			data.VA = 1500;
			/* product sheet 90.D1.015000 +4 */
			data.watts = 750;
			data.eff_line = 95;
			data.eff_line_conv = 90;
			data.eff_inverter = 80;
			data.battery_ah = 58;
			data.bat_series = 1;
			break;

		case 42:
			data.upscode = 42;
			strncpy(data.upsdesc, "PREMIUM 2200VA", sizeof(data.upsdesc));
			data.VA = 2200;
			break;

		case 43:
			data.upscode = 43;
			strncpy(data.upsdesc, "LASER 2600VA", sizeof(data.upsdesc));
			data.VA = 2600;
			break;

		case 44:
			data.upscode = 44;
			strncpy(data.upsdesc, "LASER 3300VA", sizeof(data.upsdesc));
			data.VA = 3300;
			break;

		case 45:
			data.upscode = 45;
			strncpy(data.upsdesc, "COMPACT PLUS SENOIDAL 700VA", sizeof(data.upsdesc));
			data.VA = 700;
			/* product sheet 91.A0.A00700 */
			data.watts = 700;
			data.eff_line = 95;
			data.eff_line_conv = 90;
			data.eff_inverter = 80;
			data.battery_ah = 7;
			data.bat_series = 1;
			break;

		case 46:
			data.upscode = 46;
			strncpy(data.upsdesc, "PREMIUM ONLINE 2000VA", sizeof(data.upsdesc));
			data.VA = 2000;
			break;

		case 47:
			data.upscode = 47;
			strncpy(data.upsdesc, "EXPERT ONLINE 10000VA", sizeof(data.upsdesc));
			data.VA = 10000;
			break;

		case 48:
			data.upscode = 48;
			strncpy(data.upsdesc, "LASER SENOIDAL 4200VA", sizeof(data.upsdesc));
			data.VA = 4200;
			/* product sheet 91.D1.042100 +1 */
			data.watts = 3780;
			data.eff_line = 95;
			data.eff_line_conv = 90;
			data.eff_inverter = 80;
			data.battery_ah = 9;
			data.bat_series = 1;
			break;

		case 49:
			data.upscode = 49;
			strncpy(data.upsdesc, "NHS COMPACT PLUS EXTENDIDO 1500VA", sizeof(data.upsdesc));
			data.VA = 1500;
			break;

		case 50:
			data.upscode = 50;
			strncpy(data.upsdesc, "LASER ONLINE 6000VA", sizeof(data.upsdesc));
			data.VA = 6000;
			/* product sheet 92.B3.060200 */
			data.watts = 6000;
			data.eff_line = 94;
			data.eff_inverter = 94;
			data.battery_ah = 9;
			data.bat_series = 1;
			break;

		case 51:
			data.upscode = 51;
			strncpy(data.upsdesc, "LASER EXT 3300VA", sizeof(data.upsdesc));
			data.VA = 3300;
			break;

		case 52:
			data.upscode = 52;
			strncpy(data.upsdesc, "NHS COMPACT PLUS 1200VA", sizeof(data.upsdesc));
			data.VA = 1200;
			/* product sheet 90.C1.A01200 +1 */
			data.watts = 550;
			data.eff_line = 95;
			data.eff_line_conv = 90;
			data.eff_inverter = 80;
			data.battery_ah = 7;
			data.bat_series = 1;
			break;

		case 53:
			data.upscode = 53;
			strncpy(data.upsdesc, "LASER SENOIDAL 3000VA GII", sizeof(data.upsdesc));
			data.VA = 3000;
			break;

		case 54:
			data.upscode = 54;
			strncpy(data.upsdesc, "LASER SENOIDAL 3500VA GII", sizeof(data.upsdesc));
			data.VA = 3500;
			/* product sheet 91.D1.035100 */
			data.watts = 3150;
			data.eff_line = 95;
			data.eff_line_conv = 90;
			data.eff_inverter = 80;
			data.battery_ah = 7;
			data.bat_series = 1;
			break;

		case 55:
			data.upscode = 55;
			strncpy(data.upsdesc, "LASER SENOIDAL 4200VA GII", sizeof(data.upsdesc));
			data.VA = 4200;
			/* product sheet 91.D1.042100 +1 */
			data.watts = 3780;
			data.eff_line = 95;
			data.eff_line_conv = 90;
			data.eff_inverter = 80;
			data.battery_ah = 9;
			data.bat_series = 1;
			break;

		case 56:
			data.upscode = 56;
			strncpy(data.upsdesc, "LASER ONLINE 3000VA", sizeof(data.upsdesc));
			data.VA = 3000;
			break;

		case 57:
			data.upscode = 57;
			strncpy(data.upsdesc, "LASER ONLINE 3750VA", sizeof(data.upsdesc));
			data.VA = 3750;
			break;

		case 58:
			data.upscode = 58;
			strncpy(data.upsdesc, "LASER ONLINE 5000VA", sizeof(data.upsdesc));
			data.VA = 5000;
			break;

		case 59:
			data.upscode = 59;
			strncpy(data.upsdesc, "LASER ONLINE 6000VA", sizeof(data.upsdesc));
			data.VA = 6000;
			/* product sheet 92.B3.060200 */
			data.watts = 6000;
			data.eff_line = 94;
			data.eff_inverter = 94;
			data.battery_ah = 9;
			data.bat_series = 1;
			break;

		case 60:
			data.upscode = 60;
			strncpy(data.upsdesc, "PREMIUM ONLINE 2000VA", sizeof(data.upsdesc));
			data.VA = 2000;
			break;

		case 61:
			data.upscode = 61;
			strncpy(data.upsdesc, "PREMIUM ONLINE 1500VA", sizeof(data.upsdesc));
			data.VA = 1500;
			/* product sheet 92.A1.015000 +3 */
			data.watts = 1050;
			data.eff_line = 90;
			data.eff_inverter = 94;
			data.battery_ah = 9;
			data.bat_series = 1;
			break;

		case 62:
			data.upscode = 62;
			strncpy(data.upsdesc, "PREMIUM ONLINE 1200VA", sizeof(data.upsdesc));
			data.VA = 1200;
			break;

		case 63:
			data.upscode = 63;
			strncpy(data.upsdesc, "COMPACT PLUS II MAX 1400VA", sizeof(data.upsdesc));
			data.VA = 1400;
			break;

		case 64:
			data.upscode = 64;
			strncpy(data.upsdesc, "PREMIUM PDV MAX 2200VA", sizeof(data.upsdesc));
			data.VA = 2200;
			/* product sheet 90.D0.022400 */
			data.watts = 1200;
			data.eff_line = 95;
			data.eff_line_conv = 90;
			data.eff_inverter = 80;
			break;

		case 65:
			data.upscode = 65;
			strncpy(data.upsdesc, "PREMIUM PDV 3000VA", sizeof(data.upsdesc));
			data.VA = 3000;
			break;

		case 66:
			data.upscode = 66;
			strncpy(data.upsdesc, "PREMIUM SENOIDAL 2200VA GII", sizeof(data.upsdesc));
			data.VA = 2200;
			/* product sheet 91.C0.022300 */
			data.watts = 1980;
			data.eff_line = 95;
			data.eff_line_conv = 90;
			data.eff_inverter = 80;
			data.battery_ah = 9;
			data.bat_series = 1;
			break;

		case 67:
			data.upscode = 67;
			strncpy(data.upsdesc, "LASER PRIME SENOIDAL 3200VA GII", sizeof(data.upsdesc));
			data.VA = 3200;
			break;

		case 68:
			data.upscode = 68;
			strncpy(data.upsdesc, "PREMIUM RACK ONLINE 3000VA", sizeof(data.upsdesc));
			data.VA = 3000;
			/* product sheet 92.A1.030200 +1 */
			data.watts = 2700;
			data.eff_line = 94;
			data.eff_inverter = 94;
			data.battery_ah = 9;
			data.bat_series = 1;
			break;

		case 69:
			data.upscode = 69;
			strncpy(data.upsdesc, "PREMIUM ONLINE 3000VA", sizeof(data.upsdesc));
			data.VA = 3000;
			/* product sheet 92.A1.030000 +3 */
			data.watts = 2700;
			data.eff_line = 94;
			data.eff_inverter = 94;
			data.battery_ah = 5;
			data.bat_series = 1;
			break;

		case 70:
			data.upscode = 70;
			strncpy(data.upsdesc, "LASER ONLINE 4000VA", sizeof(data.upsdesc));
			data.VA = 4000;
			break;

		case 71:
			data.upscode = 71;
			strncpy(data.upsdesc, "LASER ONLINE 7500VA", sizeof(data.upsdesc));
			data.VA = 7500;
			break;

		case 72:
			data.upscode = 72;
			strncpy(data.upsdesc, "LASER ONLINE BIFASICO 5000VA", sizeof(data.upsdesc));
			data.VA = 5000;
			break;

		case 73:
			data.upscode = 73;
			strncpy(data.upsdesc, "LASER ONLINE BIFASICO 6000VA", sizeof(data.upsdesc));
			data.VA = 6000;
			break;

		case 74:
			data.upscode = 74;
			strncpy(data.upsdesc, "LASER ONLINE BIFASICO 7500VA", sizeof(data.upsdesc));
			data.VA = 7500;
			break;

		case 75:
			data.upscode = 75;
			strncpy(data.upsdesc, "NHS MINI ST", sizeof(data.upsdesc));
			data.VA = 500;
			break;

		case 76:
			data.upscode = 76;
			strncpy(data.upsdesc, "NHS MINI 120", sizeof(data.upsdesc));
			data.VA = 120;
			break;

		case 77:
			data.upscode = 77;
			strncpy(data.upsdesc, "NHS MINI BIVOLT", sizeof(data.upsdesc));
			data.VA = 500;
			break;

		case 78:
			data.upscode = 78;
			strncpy(data.upsdesc, "PDV 600", sizeof(data.upsdesc));
			data.VA = 600;
			break;

		case 79:
			data.upscode = 79;
			strncpy(data.upsdesc, "NHS MINI MAX", sizeof(data.upsdesc));
			data.VA = 500;
			break;

		case 80:
			data.upscode = 80;
			strncpy(data.upsdesc, "NHS MINI EXT", sizeof(data.upsdesc));
			data.VA = 500;
			break;

		case 81:
			data.upscode = 81;
			strncpy(data.upsdesc, "NHS AUTONOMY PDV 4T", sizeof(data.upsdesc));
			data.VA = 4000;
			break;

		case 82:
			data.upscode = 82;
			strncpy(data.upsdesc, "NHS AUTONOMY PDV 8T", sizeof(data.upsdesc));
			data.VA = 8000;
			break;

		case 83:
			data.upscode = 83;
			strncpy(data.upsdesc, "NHS COMPACT PLUS RACK 1200VA", sizeof(data.upsdesc));
			data.VA = 1200;
			/* product sheet 90.C1.012400 */
			data.watts = 500;
			data.eff_line = 95;
			data.eff_line_conv = 90;
			data.eff_inverter = 80;
			data.battery_ah = 7;
			data.bat_series = 1;
			break;

		case 84:
			data.upscode = 84;
			strncpy(data.upsdesc, "PDV SENOIDAL ISOLADOR 1500VA", sizeof(data.upsdesc));
			data.VA = 1500;
			break;

		case 85:
			data.upscode = 85;
			strncpy(data.upsdesc, "NHS PDV RACK 1500VA", sizeof(data.upsdesc));
			data.VA = 1500;
			break;

		case 86:
			data.upscode = 86;
			strncpy(data.upsdesc, "NHS PDV 1400VA S GII", sizeof(data.upsdesc));
			data.VA = 1400;
			break;

		case 87:
			data.upscode = 87;
			strncpy(data.upsdesc, "PDV SENOIDAL ISOLADOR 1500VA", sizeof(data.upsdesc));
			data.VA = 1500;
			break;

		case 88:
			data.upscode = 88;
			strncpy(data.upsdesc, "LASER PRIME SENOIDAL ISOLADOR 2000VA", sizeof(data.upsdesc));
			data.VA = 2000;
			break;

		case 89:
			data.upscode = 89;
			strncpy(data.upsdesc, "PREMIUM SENOIDAL 2400VA GII", sizeof(data.upsdesc));
			data.VA = 2400;
			/* product sheet 91.C0.024200 */
			data.watts = 2160;
			data.eff_line = 95;
			data.eff_line_conv = 90;
			data.eff_inverter = 80;
			data.battery_ah = 9;
			data.bat_series = 1;
			break;

		case 90:
			data.upscode = 90;
			strncpy(data.upsdesc, "NHS PDV 1400VA S 8T GII", sizeof(data.upsdesc));
			data.VA = 1400;
			break;

		case 91:
			data.upscode = 91;
			strncpy(data.upsdesc, "PREMIUM ONLINE 2000VA", sizeof(data.upsdesc));
			data.VA = 2000;
			break;

		case 92:
			data.upscode = 92;
			strncpy(data.upsdesc, "LASER PRIME ONLINE 2200VA", sizeof(data.upsdesc));
			data.VA = 2200;
			break;

		case 93:
			data.upscode = 93;
			strncpy(data.upsdesc, "PREMIUM RACK ONLINE 2200VA", sizeof(data.upsdesc));
			data.VA = 2200;
			break;

		case 94:
			data.upscode = 94;
			strncpy(data.upsdesc, "PREMIUM SENOIDAL 2400VA GII", sizeof(data.upsdesc));
			data.VA = 2400;
			/* product sheet 91.C0.024200 */
			data.watts = 2160;
			data.eff_line = 95;
			data.eff_line_conv = 90;
			data.eff_inverter = 80;
			data.battery_ah = 9;
			data.bat_series = 1;
			break;

		case 95:
			data.upscode = 95;
			strncpy(data.upsdesc, "LASER ONLINE 10000VA", sizeof(data.upsdesc));
			data.VA = 10000;
			break;

		case 96:
			data.upscode = 96;
			strncpy(data.upsdesc, "LASER ONLINE BIFASICO 10000VA", sizeof(data.upsdesc));
			data.VA = 10000;
			break;

		case 97:
			data.upscode = 97;
			strncpy(data.upsdesc, "LASER SENOIDAL 3300VA GII", sizeof(data.upsdesc));
			data.VA = 3300;
			/* product sheet 91.D1.033100 */
			data.watts = 1980;
			data.eff_line = 95;
			data.eff_line_conv = 90;
			data.eff_inverter = 75;
			data.battery_ah = 36;
			data.bat_series = 1;
			break;

		case 98:
			data.upscode = 98;
			strncpy(data.upsdesc, "LASER SENOIDAL 2600VA GII", sizeof(data.upsdesc));
			data.VA = 2600;
			/* product sheet 91.C0.026300 */
			data.watts = 1560;
			data.eff_line = 95;
			data.eff_line_conv = 90;
			data.eff_inverter = 75;
			data.battery_ah = 58;
			data.bat_series = 1;
			break;

		case 99:
			data.upscode = 99;
			strncpy(data.upsdesc, "PREMIUM SENOIDAL 3000VA GII", sizeof(data.upsdesc));
			data.VA = 3000;
			break;

		case 100:
			data.upscode = 100;
			strncpy(data.upsdesc, "PREMIUM SENOIDAL 2200VA GII", sizeof(data.upsdesc));
			data.VA = 2200;
			/* product sheet 91.C0.022300 */
			data.watts = 1980;
			data.eff_line = 95;
			data.eff_line_conv = 90;
			data.eff_inverter = 80;
			data.battery_ah = 9;
			data.bat_series = 1;
			break;

		case 101:
			data.upscode = 101;
			strncpy(data.upsdesc, "LASER ONLINE BIFASICO 4000VA", sizeof(data.upsdesc));
			data.VA = 4000;
			break;

		case 102:
			data.upscode = 102;
			strncpy(data.upsdesc, "LASER ONLINE 12000VA", sizeof(data.upsdesc));
			data.VA = 12000;
			break;

		case 103:
			data.upscode = 103;
			strncpy(data.upsdesc, "LASER ONLINE 8000VA", sizeof(data.upsdesc));
			data.VA = 8000;
			/* product sheet 92.C3.080100 */
			data.watts = 6400;
			data.eff_line = 94;
			data.eff_inverter = 94;
			data.battery_ah = 9;
			data.bat_series = 1;
			break;

		case 104:
			data.upscode = 104;
			strncpy(data.upsdesc, "PDV SENOIDAL ISOLADOR 1000VA", sizeof(data.upsdesc));
			data.VA = 1000;
			break;

		case 105:
			data.upscode = 105;
			strncpy(data.upsdesc, "MINI SENOIDAL 500VA", sizeof(data.upsdesc));
			data.VA = 500;
			break;

		case 106:
			data.upscode = 106;
			strncpy(data.upsdesc, "LASER SENOIDAL 5000VA GII", sizeof(data.upsdesc));
			data.VA = 5000;
			/* product sheet 91.D1.050100 */
			data.watts = 4500;
			data.eff_line = 95;
			data.eff_line_conv = 90;
			data.eff_inverter = 80;
			data.battery_ah = 9;
			data.bat_series = 1;
			break;

		case 107:
			data.upscode = 107;
			strncpy(data.upsdesc, "COMPACT PLUS SENOIDAL 1000VA", sizeof(data.upsdesc));
			data.VA = 1000;
			/* product sheet 91.A0.010300 +2 */
			data.eff_line = 95;
			data.eff_line_conv = 90;
			data.eff_inverter = 80;
			data.bat_series = 1;
			break;

		case 108:
			data.upscode = 108;
			strncpy(data.upsdesc, "QUAD_COM 80A", sizeof(data.upsdesc));
			data.VA = 0;
			break;

		case 109:
			data.upscode = 109;
			strncpy(data.upsdesc, "LASER ONLINE 5000VA", sizeof(data.upsdesc));
			data.VA = 5000;
			break;

		case 113:
			data.upscode = 113;
			strncpy(data.upsdesc, "PDV SENOIDAL ISOLADOR 700VA", sizeof(data.upsdesc));
			data.VA = 700;
			break;

		default:
			/* No case for this code: the answer stays the UNKNOWN device. */
			break;
	}  /* end switch */

	/*
	 * The power factor of a model is its own W divided by its own VA, when
	 * the case gave both. Otherwise it stays the one of the UNKNOWN device.
	 */
	if (data.watts > 0 && data.VA > 0)
		data.pf = (float)data.watts / (float)data.VA;

	/*
	 * Step 1, applied last so that it wins: the ups.conf settings. Formats and
	 * ranges were checked at startup by parse_battery_options() and
	 * parse_model_options(). The protocol version is kept as model data for
	 * now and does not select framing or initialization commands.
	 */
	if (overridemodel && overridemodel[0] != '\0') {
		data.upscode = upscode;
		snprintf(data.upsdesc, sizeof(data.upsdesc), "%s", overridemodel);
	} /* end if */
	if (protocolversion)
		data.pversion = (unsigned int)atoi(protocolversion);
	if (bypassasalarm)
		data.bypassasalarm = (unsigned int)atoi(bypassasalarm);
	/* "va" used to be read only for models without a VA of their own. */
	if (va && atoi(va) > 0)
		data.VA = (unsigned int)atoi(va);
	if (pf && atof(pf) > 0)
		data.pf = (float)atof(pf);
	if (ah && atoi(ah) > 0)
		data.battery_ah = (unsigned int)atoi(ah);
	if (vbat && atof(vbat) > 0)
		data.battery_v = (float)atof(vbat);
	/* 0 is a value here (in parallel), so it is taken as given */
	if (series)
		data.bat_series = (unsigned int)atoi(series);
	/* Reading mode of each two-byte field of the data packet, 0 or 1 */
	data.r16_vin = get_flag_option("read16_vin", data.r16_vin);
	data.r16_vin_min = get_flag_option("read16_vin_min", data.r16_vin_min);
	data.r16_vin_max = get_flag_option("read16_vin_max", data.r16_vin_max);
	data.r16_vout = get_flag_option("read16_vout", data.r16_vout);
	data.r16_temp = get_flag_option("read16_temp", data.r16_temp);
	data.r16_vbat = get_flag_option("read16_vbat", data.r16_vbat);
	/* 0 is a value here too: it turns the overload check off */
	if (overload)
		data.overload_perc = (unsigned int)atoi(overload);
	value = get_float_option("efficiency_line");
	if (value > 0)
		data.eff_line = value;
	value = get_float_option("efficiency_line_conv");
	if (value > 0)
		data.eff_line_conv = value;
	value = get_float_option("efficiency_inverter");
	if (value > 0)
		data.eff_inverter = value;

	/* The nominal W always follows the resolved VA and power factor. */
	data.watts = (unsigned int)lrint((double)data.VA * (double)data.pf);

	return data;
}

/*
 * A 0/1 setting of ups.conf, or the value given when ups.conf does not have
 * it. The format was checked at startup by parse_model_options().
 */
static unsigned int get_flag_option(const char *name, unsigned int current) {
	const char  *value = getval(name);

	return value ? (unsigned int)atoi(value) : current;
}

/* Numeric ups.conf setting, or 0 when it is absent. */
static float get_float_option(const char *name) {
	char  *value = getval(name);
	if (value)
		return (float)atof(value);
	else
		return 0;
}

/*
 * The get_*() helpers below only hand out what getupsinfo() resolved for the
 * model: ups.conf, then the case of the model, then the UNKNOWN device. None
 * of them has a default or a failure path of its own.
 *
 * get_va() used to be the exception: when neither ups.conf nor the model had
 * the nominal power, it ended the driver with a fatal error asking for "va".
 * The error was raised from inside the polling loop, after the first HWINFO
 * packet, so the driver started, looked healthy and then died; through
 * upsdrvctl only "Data stale" was visible, and the message itself was seen
 * only in the foreground with -D. It also went around the UNKNOWN device,
 * which exists to answer exactly that case. Now it returns the 0 held there
 * and the caller leaves out the variables that depend on the nominal power.
 */

/* Nominal apparent power, in VA. */
static unsigned int get_va(int equipment) {
	upsinfo  ups;

	ups = getupsinfo((unsigned int)equipment);
	return ups.VA;
}

/* Power factor. */
static float get_pf(int equipment) {
	upsinfo  ups;

	ups = getupsinfo((unsigned int)equipment);
	return ups.pf;
}

/* Capacity of each battery, in Ah. */
static unsigned int get_ah(int equipment) {
	upsinfo  ups;

	ups = getupsinfo((unsigned int)equipment);
	return ups.battery_ah;
}

/* Nominal voltage of each battery, in V. */
static float get_vbat(int equipment) {
	upsinfo  ups;

	ups = getupsinfo((unsigned int)equipment);
	return ups.battery_v;
}

/*
 * Margin, in percent, for one of the input voltage thresholds. Each threshold
 * has a ups.conf setting of its own, named by var; without it, the margin is
 * the one resolved for the model.
 */
static float get_vin_perc(char *var, int equipment) {
	upsinfo  ups;
	char     *perc = getval(var);

	if (perc)
		return (float)atof(perc);
	ups = getupsinfo((unsigned int)equipment);
	return ups.vin_perc;
}

/*
 * Send one hardware-discovery request only through a valid serial descriptor.
 * If the descriptor is already invalid, try the driver's bounded reopen path.
 * A failed or partial write closes and invalidates the descriptor so the next
 * update cycle knows that it must reopen the port before communicating again.
 */
static bool send_initialization_packet(const unsigned int *data, size_t size, TYPE_FD_SER fd) {
	int  written;

	if (INVALID_FD_SER(fd)) {
		upslogx(LOG_WARNING, "%s: serial port %s is not open before the initialization request; trying to reopen it", __func__, porta);
		fd = reconnect_ups_if_needed();
		if (INVALID_FD_SER(fd)) {
			upslogx(LOG_WARNING, "%s: unable to send the initialization request because serial port %s could not be reopened", __func__, porta);
			dstate_datastale();
			return false;
		}  /* end if */
	}  /* end if */

	upsdebugx(3, "%s: sending initialization request (%" PRIuSIZE " bytes) on %s", __func__, size, porta);
	errno = 0;
	written = write_serial_int(fd, data, size);
	if (written < 0 || (size_t)written != size) {
		if (errno != 0)
			upslog_with_errno(LOG_WARNING, "%s: failed to send the initialization request on %s", __func__, porta);
		else
			upslogx(LOG_WARNING, "%s: failed to send the complete initialization request on %s; wrote %d of %" PRIuSIZE " bytes", __func__, porta, written, size);

		/* Mark the connection unusable. upsdrv_updateinfo() will invoke the
		 * normal reopen procedure before its next read or retry.
		 */
		close_serial_port();
		dstate_datastale();
		return false;
	}  /* end if */

	upsdebugx(3, "%s: initialization request sent successfully on %s", __func__, porta);
	return true;
}

void upsdrv_initinfo(void) {
	/* From docs/new-drivers.txt:
	 * Try to detect what kind of UPS is out there,
	 * if any, assuming that's possible for your hardware.
	 * If there is a way to detect that hardware and it
	 * doesn't appear to be connected, display an error
	 * and exit. This is the last time your driver is
	 * allowed to bail out.
	 * This is usually a good place to create variables
	 * like `ups.mfr`, `ups.model`, `ups.serial`, register
	 * instant commands, and other "one time only" items.
	 */

	upsdebugx(3, "%s: starting...", __func__);

	/* Reject malformed battery and model settings here, where bailing out is
	 * still allowed, rather than from inside the polling loop. */
	parse_battery_options();
	parse_model_options();

	/* TODO: Any instant commands? */
	if (!send_initialization_packet(string_initialization_long, 9, serial_fd))
		return;
	usleep(250000);
	if (!send_initialization_packet(string_initialization_short, 9, serial_fd))
		return;
	usleep(250000);
	if (!send_initialization_packet(string_initialization_comptmode, 5, serial_fd))
		return;

	upsdebugx(3, "%s: initialization commands sent", __func__);

	upsdebugx(3, "%s: finished", __func__);
}

/*
 * Battery count override from ups.conf.
 *
 * The option is registered in upsdrv_makevartable() as "numbatteries", but
 * this lookup used to ask getval() for "numbat". getval() only resolves names
 * registered with addvar(), so the override was silently ignored and the
 * count always came from the HWINFO packet; writing "numbat" in ups.conf did
 * not help either, since main.c rejects it as an unknown variable. The lookup
 * now uses the registered name, so the documented option takes effect.
 */
static unsigned int get_numbat(void) {
	char          *nb = getval("numbatteries");
	unsigned int  retval = 0;

	if (nb)
		retval = atoi(nb);
	return retval;
}

/*
 * Validate the battery parameters supplied in ups.conf at startup, where the
 * message is logged, instead of failing later inside the polling loop.
 *
 * "ah" used to be mandatory and was first read after the first DATA packet,
 * so a missing value killed the driver while polling and upsdrvctl only showed
 * "Data stale". It is now optional: the model data may supply the capacity,
 * and when neither does the driver keeps running without capacity and runtime
 * data (see interpret_pkt_data()). The model is not known yet at this point,
 * so only the format can be checked here.
 *
 * "numbatteries" is optional too. Both must be positive integers when present,
 * so a typo does not quietly turn into zero.
 *
 * "battery_series" says how the batteries are wired and takes only the
 * numbers 0 (in parallel) and 1 (in series). Anything else is rejected,
 * because atoi() would read a word such as "parallel" as 0 and a typo as
 * whatever digits it starts with.
 */
static void parse_battery_options(void)
{
	const char  *value;
	char        *endptr;
	long        number;

	value = getval("ah");
	if (value) {
		errno = 0;
		endptr = NULL;
		number = strtol(value, &endptr, 10);
		if (errno != 0 || endptr == value || *endptr != '\0' || number <= 0)
			fatalx(EXIT_FAILURE, "Invalid ah value '%s': expected a positive integer number of Ampere-hours", value);
	}  /* end if */

	value = getval("numbatteries");
	if (value) {
		errno = 0;
		endptr = NULL;
		number = strtol(value, &endptr, 10);
		if (errno != 0 || endptr == value || *endptr != '\0' || number <= 0)
			fatalx(EXIT_FAILURE, "Invalid numbatteries value '%s': expected a positive integer", value);
	}  /* end if */

	value = getval("battery_series");
	if (value && strcmp(value, "0") != 0 && strcmp(value, "1") != 0)
		fatalx(EXIT_FAILURE, "Invalid battery_series value '%s': expected 0 (batteries in parallel) or 1 (batteries in series)", value);
}

/*
 * Validate the model ratings a user may declare in ups.conf to replace the
 * values of the model data. Each one must be a plain number inside its
 * range, so a typo (for example "0,6" instead of "0.6") is reported at
 * startup instead of silently becoming zero while polling.
 */
static void parse_model_options(void)
{
	static const struct {
		const char  *name;      /* ups.conf setting */
		double      min;        /* lowest accepted value */
		double      max;        /* highest accepted value */
		const char  *expected;  /* shown when the value is rejected */
	} checks[] = {
		{ "va",                   1,    1000000, "the nominal power in VA" },
		{ "pf",                   0.01, 1,       "a power factor from 0.01 to 1" },
		{ "vbat",                 1,    1000,    "the nominal voltage of one battery in V" },
		{ "efficiency_line",      1,    100,     "a percentage from 1 to 100" },
		{ "efficiency_line_conv", 1,    100,     "a percentage from 1 to 100" },
		{ "efficiency_inverter",  1,    100,     "a percentage from 1 to 100" },
		{ "overload_perc",        0,    255,     "a load percentage from 0 to 255 (0 turns the check off)" },
	};
	/* Settings that take only 0 or 1: the reading mode of each two-byte field. */
	static const char  *flags[] = {
		"read16_vin",
		"read16_vin_min",
		"read16_vin_max",
		"read16_vout",
		"read16_temp",
		"read16_vbat",
	};
	const char  *value;
	char        *endptr;
	double      number;
	size_t      i;

	for (i = 0; i < sizeof(checks) / sizeof(checks[0]); i++) {
		value = getval(checks[i].name);
		if (!value)
			continue;
		errno = 0;
		endptr = NULL;
		number = strtod(value, &endptr);
		if (errno != 0 || endptr == value || *endptr != '\0' || number < checks[i].min || number > checks[i].max)
			fatalx(EXIT_FAILURE, "Invalid %s value '%s': expected %s", checks[i].name, value, checks[i].expected);
	}  /* end for */

	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		value = getval(flags[i]);
		if (value && strcmp(value, "0") != 0 && strcmp(value, "1") != 0)
			fatalx(EXIT_FAILURE, "Invalid %s value '%s': expected 0 (read as \"first.second\") or 1 (read as a 16-bit number)", flags[i], value);
	}  /* end for */
}

/*
 * Return serial_fd after preserving the original bounded retry behavior.
 * Reopened ports pass through openfd, so the same validated settings are
 * restored after a communication failure.
 */
static TYPE_FD_SER reconnect_ups_if_needed(void) {
	/* retries to open port until we declare "data stale" loudly */
	static unsigned int  retries = 0;
	bool                 retry_limit_reached = false;

	/* If comms failed earlier, try to resuscitate */
	if (INVALID_FD_SER(serial_fd)) {
		upsdebugx(1, "%s: Serial port '%s' communications problem", __func__, porta);

		/* Uh oh, got to reconnect! */
		reconnect_trying(RECONNECT_TRYING);

		/* Close any surviving handle and mark it invalid before reopening. */
		close_serial_port();

		while (INVALID_FD_SER(serial_fd) && !retry_limit_reached) {
			upsdebugx(1, "%s: Trying to reopen serial...", __func__);
			serial_fd = openfd(porta, baudrate);
			retries++;
			if (retries >= MAXTRIES) {
				upsdebugx(1, "%s: serial port reopen failed", __func__);
				retry_limit_reached = true;
			}  /* end if */
			else
				usleep(checktime);
		}  /* end while */

		if (VALID_FD_SER(serial_fd)) {
			if (retries > MAXTRIES && may_log_reconnect_trying(1))
				upslogx(LOG_NOTICE, "Communications with UPS re-established");

			/* A reopened serial port may now be connected to a different UPS.
			 * Invalidate the cached discovery result and restart the initialization
			 * sequence so model-specific data is obtained without restarting NUT.
			 */
			lastpkthwinfo.checksum_ok = false;
			send_extended = 0;
			checktime = 2000000;
			retries = 0;
			reconnect_trying(RECONNECT_SUCCESS);
		}  /* end if */
		else {
			if (retries == MAXTRIES && may_log_reconnect_trying(1))
				upslogx(LOG_WARNING, "Communications with UPS lost: port reopen failed!");
			dstate_datastale();
		}  /* end else */
	}  /* end if */

	return serial_fd;
}

/*
 * Tell whether two HWINFO answers come from the same UPS.
 *
 * The protocol has no field that identifies a unit beyond doubt, so this
 * compares everything in the answer that describes the equipment itself and
 * not its settings or its state: the model code, the hardware version, the
 * firmware version and the number of batteries. The serial number is
 * compared as well, but only when both answers are the extended one
 * (50 bytes), because the standard answer (18 bytes) does not carry it and
 * the same unit may answer with either.
 *
 * Limit: two units of the same model, hardware, firmware and number of
 * batteries that report no serial number, or the same placeholder, answer
 * exactly alike and cannot be told apart. After swapping such units the
 * driver has to be restarted by hand.
 */
static bool same_ups(const pkt_hwinfo *a, const pkt_hwinfo *b) {
	if (a->model != b->model)
		return false;
	if (a->hardwareversion != b->hardwareversion)
		return false;
	if (a->softwareversion != b->softwareversion)
		return false;
	if (a->numbatteries != b->numbatteries)
		return false;
	if (a->size > 18 && b->size > 18 && strncmp(a->serial, b->serial, sizeof(a->serial)) != 0)
		return false;
	return true;
}

/*
 * Drop everything the driver learned or accumulated from the UPS it was
 * talking to, because another one is now answering on the port.
 *
 * The rule is: data from one UPS is never valid for another. The new unit
 * may be of another model, and even one of the same model has other
 * batteries, whose charge and condition nothing is known about. Carrying
 * figures over would mean publishing power, battery current and runtime of
 * one equipment worked out with the ratings of another, and statistics that
 * mix the two.
 *
 * That is what happened until now. When I made the driver ask for the
 * hardware information again after a reconnection (my commit "Refresh NHS
 * hardware discovery during driver lifecycle", 2026-08-30) the purpose was
 * that another UPS could be connected to the same port without restarting
 * NUT, and I left it half done: the model name and the hardware data
 * followed the new unit, while the ratings that interpret_pkt_data() keeps
 * (nominal power, power factor, battery capacity and voltage, number of
 * batteries) and the power statistics stayed those of the old one.
 *
 * So when same_ups() says that the unit is another, the driver starts over
 * for it as it does after a restart:
 *
 *  - the ratings kept by interpret_pkt_data() are cleared there (the
 *    ups_replaced flag) and resolved again for the new model;
 *
 *  - the minimum and maximum power statistics start again;
 *
 *  - the variables that are published only when a rating is known are
 *    removed, so that a figure of the old unit cannot outlive it when the
 *    new one has no value for it. Every other variable is written again by
 *    the next packet.
 *
 * None of this happens when the same unit is merely found again after a
 * reconnection: its data stay valid, also while the new answer is awaited.
 */
static void forget_previous_ups(void) {
	static const char  *rating_vars[] = {
		"ups.power",
		"ups.realpower",
		"ups.power.nominal",
		"ups.realpower.nominal",
		"output.power",
		"output.realpower",
		"output.power.percent",
		"output.power.maximum",
		"output.power.minimum",
		"output.power.maximum.percent",
		"output.power.minimum.percent",
		"battery.capacity",
		"battery.capacity.nominal",
		NULL
	};
	size_t  i = 0;

	ups_replaced = true;

	minpower = 0;
	maxpower = 0;
	minpowerperc = 0;
	maxpowerperc = 0;
	minpower_seen = false;

	for (i = 0; rating_vars[i] != NULL; i++)
		dstate_delinfo(rating_vars[i]);
}

static void interpret_pkt_hwinfo(void) {
	/* TOTHINK: Consider passing in the packet struct as parameter? */
	upsinfo       ups;
	char          hw_scratch_buf[1024];  /* General-purpose string buffer */
	unsigned int  i = 0;

	/* @freechurros identified in issue #3592 that this function used to
	 * inspect lastpktdata here. Validate the HWINFO packet being interpreted,
	 * otherwise a valid HWINFO received before the first DATA packet is lost.
	 */
	if (!lastpkthwinfo.checksum_ok) {
		upslogx(LOG_WARNING, "%s: bad lastpkthwinfo.checksum", __func__);
		return;
	}  /* end if */

	if (lastpkthwinfo.size < 1) {
		upslogx(LOG_WARNING, "%s: Pkt HWINFO is not OK. See if will be requested next time!", __func__);
		return;
	}  /* end if */

	/* checksum is OK, then use it to set values */
	upsdebugx(4, "Pkt HWINFO is OK. Model code is %u, hwversion is %u and swversion is %u", lastpkthwinfo.model, lastpkthwinfo.hardwareversion, lastpkthwinfo.softwareversion);

	/* We need to set data on NUT with data
	 * that I believe that I can calculate.
	 * Now setting data on NUT
	 */
	ups = getupsinfo(lastpkthwinfo.model);
	upsdebugx(4, "UPS Struct data: Code %u Model %s VA %u Protocol version %u Bypass as alarm %u", ups.upscode, ups.upsdesc, ups.VA, ups.pversion, ups.bypassasalarm);
	/* Ratings as resolved by getupsinfo(); 0 means not known. */
	upsdebugx(4, "UPS ratings: %u W, power factor %.2f, efficiency %.1f%% (mains), %.1f%% (mains, converting), %.1f%% (battery), batteries of %u Ah and %.2f V", ups.watts, (double)ups.pf, (double)ups.eff_line, (double)ups.eff_line_conv, (double)ups.eff_inverter, ups.battery_ah, (double)ups.battery_v);
	dstate_setinfo("device.model", "%s", ups.upsdesc);
	dstate_setinfo("device.mfr", "%s", MANUFACTURER);
	dstate_setinfo("device.serial", "%s", lastpkthwinfo.serial);
	dstate_setinfo("device.type", "%s", "ups");

	dstate_setinfo("ups.model", "%s", ups.upsdesc);
	dstate_setinfo("ups.mfr", "%s", MANUFACTURER);
	dstate_setinfo("ups.serial", "%s", lastpkthwinfo.serial);
	dstate_setinfo("ups.firmware", "%u", lastpkthwinfo.softwareversion);

	/* Setting hardware version here.
	 * Did not find another place to do this.
	 * Feel free to correct it.
	 * FIXME: move to upsdrv_initinfo() or so
	 */
	dstate_setinfo("ups.firmware.aux", "%u", lastpkthwinfo.hardwareversion);

	if (debug_pkt_hwinfo) {
		/* Now, creating a structure called NHS.HW, for latest HW
		 * info packet contents and raw data points, including those
		 * that were sorted above into NUT standard variables -
		 * for debug.
		 */
		dstate_setinfo("experimental.nhs.hw.header", "%u", lastpkthwinfo.header);
		dstate_setinfo("experimental.nhs.hw.size", "%u", lastpkthwinfo.size);
		dstate_setinfo("experimental.nhs.hw.type", "%c", lastpkthwinfo.type);
		dstate_setinfo("experimental.nhs.hw.model", "%u", lastpkthwinfo.model);
		dstate_setinfo("experimental.nhs.hw.hardwareversion", "%u", lastpkthwinfo.hardwareversion);
		dstate_setinfo("experimental.nhs.hw.softwareversion", "%u", lastpkthwinfo.softwareversion);
		dstate_setinfo("experimental.nhs.hw.configuration", "%u", lastpkthwinfo.configuration);
		for (i = 0; i < 5; i++) {
			/* Reusing variable */
			snprintf(hw_scratch_buf, sizeof(hw_scratch_buf), "experimental.nhs.hw.configuration_array_p%u", i);
			dstate_setinfo(hw_scratch_buf, "%u", lastpkthwinfo.configuration_array[i]);
		}  /* end for */
		dstate_setinfo("experimental.nhs.hw.c_oem_mode", "%s", lastpkthwinfo.c_oem_mode ? "true" : "false");
		dstate_setinfo("experimental.nhs.hw.c_buzzer_disable", "%s", lastpkthwinfo.c_buzzer_disable ? "true" : "false");
		dstate_setinfo("experimental.nhs.hw.c_potmin_disable", "%s", lastpkthwinfo.c_potmin_disable ? "true" : "false");
		dstate_setinfo("experimental.nhs.hw.c_rearm_enable", "%s", lastpkthwinfo.c_rearm_enable ? "true" : "false");
		dstate_setinfo("experimental.nhs.hw.c_bootloader_enable", "%s", lastpkthwinfo.c_bootloader_enable ? "true" : "false");
		dstate_setinfo("experimental.nhs.hw.numbatteries", "%u", lastpkthwinfo.numbatteries);
		dstate_setinfo("experimental.nhs.hw.undervoltagein120V", "%u", lastpkthwinfo.undervoltagein120V);
		dstate_setinfo("experimental.nhs.hw.overvoltagein120V", "%u", lastpkthwinfo.overvoltagein120V);
		dstate_setinfo("experimental.nhs.hw.undervoltagein220V", "%u", lastpkthwinfo.undervoltagein220V);
		dstate_setinfo("experimental.nhs.hw.overvoltagein220V", "%u", lastpkthwinfo.overvoltagein220V);
		dstate_setinfo("experimental.nhs.hw.tensionout120V", "%u", lastpkthwinfo.tensionout120V);
		dstate_setinfo("experimental.nhs.hw.tensionout220V", "%u", lastpkthwinfo.tensionout220V);
		dstate_setinfo("experimental.nhs.hw.statusval", "%u", lastpkthwinfo.statusval);
		for (i = 0; i < 6; i++) {
			/* Reusing variable */
			snprintf(hw_scratch_buf, sizeof(hw_scratch_buf), "experimental.nhs.hw.status_p%u", i);
			dstate_setinfo(hw_scratch_buf, "%u", lastpkthwinfo.status[i]);
		}  /* end for */
		dstate_setinfo("experimental.nhs.hw.s_220V_in", "%s", lastpkthwinfo.s_220V_in ? "true" : "false");
		dstate_setinfo("experimental.nhs.hw.s_220V_out", "%s", lastpkthwinfo.s_220V_out ? "true" : "false");
		dstate_setinfo("experimental.nhs.hw.s_sealed_battery", "%s", lastpkthwinfo.s_sealed_battery ? "true" : "false");
		dstate_setinfo("experimental.nhs.hw.s_show_out_tension", "%s", lastpkthwinfo.s_show_out_tension ? "true" : "false");
		dstate_setinfo("experimental.nhs.hw.s_show_temperature", "%s", lastpkthwinfo.s_show_temperature ? "true" : "false");
		dstate_setinfo("experimental.nhs.hw.s_show_charger_current", "%s", lastpkthwinfo.s_show_charger_current ? "true" : "false");
		dstate_setinfo("experimental.nhs.hw.chargercurrent", "%u", lastpkthwinfo.chargercurrent);
		dstate_setinfo("experimental.nhs.hw.checksum", "%u", lastpkthwinfo.checksum);
		dstate_setinfo("experimental.nhs.hw.checksum_calc", "%u", lastpkthwinfo.checksum_calc);
		dstate_setinfo("experimental.nhs.hw.checksum_ok", "%s", lastpkthwinfo.checksum_ok ? "true" : "false");
		dstate_setinfo("experimental.nhs.hw.serial", "%s", lastpkthwinfo.serial);
		dstate_setinfo("experimental.nhs.hw.year", "%u", lastpkthwinfo.year);
		dstate_setinfo("experimental.nhs.hw.month", "%u", lastpkthwinfo.month);
		dstate_setinfo("experimental.nhs.hw.wday", "%u", lastpkthwinfo.wday);
		dstate_setinfo("experimental.nhs.hw.hour", "%u", lastpkthwinfo.hour);
		dstate_setinfo("experimental.nhs.hw.minute", "%u", lastpkthwinfo.minute);
		dstate_setinfo("experimental.nhs.hw.second", "%u", lastpkthwinfo.second);
		dstate_setinfo("experimental.nhs.hw.alarmyear", "%u", lastpkthwinfo.alarmyear);
		dstate_setinfo("experimental.nhs.hw.alarmmonth", "%u", lastpkthwinfo.alarmmonth);
		dstate_setinfo("experimental.nhs.hw.alarmwday", "%u", lastpkthwinfo.alarmwday);
		dstate_setinfo("experimental.nhs.hw.alarmday", "%u", lastpkthwinfo.alarmday);
		dstate_setinfo("experimental.nhs.hw.alarmhour", "%u", lastpkthwinfo.alarmhour);
		dstate_setinfo("experimental.nhs.hw.alarmminute", "%u", lastpkthwinfo.alarmminute);
		dstate_setinfo("experimental.nhs.hw.alarmsecond", "%u", lastpkthwinfo.alarmsecond);
		dstate_setinfo("experimental.nhs.hw.end_marker", "%u", lastpkthwinfo.end_marker);
	}  /* end if */
}

static void interpret_pkt_data(void) {
	/* TOTHINK: Consider passing in the packet struct as parameter?
	 * Note that certain points from lastpkthwinfo do play a role
	 * in decisions here (so maybe two parameters?)
	 */
	static unsigned int  va = 0;
	static unsigned int  ah = 0;
	static unsigned int  numbat = 0;
	static float         vbat = 0;
	static int           min_input_power = 0;
	static float         pf = 0;
	static int           pf_settled = 0;  /* pf already resolved with the model known */

	int           got_hwinfo = (lastpkthwinfo.checksum_ok && lastpkthwinfo.size > 0);
	char          data_scratch_buf[1024];  /* General-purpose string buffer */
	unsigned int  vin_underv = 0;
	unsigned int  vin_overv = 0;
	unsigned int  perc = 0;
	unsigned int  vin = 0;
	unsigned int  vout = 0;
	unsigned int  autonomy_secs = 0;
	float         vin_low_warn = 0;
	float         vin_low_crit = 0;
	float         vin_high_warn = 0;
	float         vin_high_crit = 0;
	float         vapower = 0;
	float         vpower = 0;
	long          bcharge = 0;
	unsigned int  battery_units = 0;    /* batteries covered by the voltage reading */
	unsigned int  bank_units = 0;       /* batteries in series in the bank (packet diagnostics) */
	float         battery_nominal = 0;  /* nominal voltage of that reading, V */
	double        battery_full = 0;     /* reading that counts as 100% of charge, V */
	double        battery_empty = 0;    /* reading that counts as 0% of charge, V */
	float         abat = 0;             /* A through each battery, published */
	float         actual_current = 0;   /* A through each battery, for the runtime */
	float         inverter_eff = 0;     /* % used by the battery current and the runtime */
	double        runtime_hours = 0;
	float         efficiency = 0;
	unsigned int  i = 0;
	upsinfo       ups;

	if (!lastpktdata.checksum_ok) {
		upslogx(LOG_WARNING, "%s: bad lastpktdata.checksum", __func__);
		return;
	}  /* end if */

	/* checksum is OK, then use it to set values */
	upsdebugx(4, "%s: Data Packet seems be OK", __func__);

	/*
	 * Another UPS answered on the port: the ratings kept in the static
	 * variables of this function belong to the previous one and must not
	 * be used for this one. Clearing them makes this pass resolve all of
	 * them again, as on the first packet after startup. What else is
	 * dropped, and why, is written at forget_previous_ups().
	 */
	if (ups_replaced) {
		va = 0;
		ah = 0;
		numbat = 0;
		vbat = 0;
		pf = 0;
		pf_settled = 0;
		min_input_power = 0;
		ups_replaced = false;
	}  /* end if */

	/* Not return, but we would miss some data points */
	if (!got_hwinfo)
		upsdebugx(2, "%s: Pkt HWINFO is not OK. See if will be requested next time. Some data points will not be set on this pass!", __func__);

	/* Setting UPS Status:
	 * OL      -- On line (mains is present): Code below
	 * OB      -- On battery (mains is not present) : Code below
	 * LB      -- Low battery: Code below
	 * HB      -- High battery: NHS doesn't have any variable with that information. Feel free to discover a way to set it
	 * RB      -- The battery needs to be replaced: Well, as mentioned, we can write some infos on nobreak fw, on structures like pkt_hwinfo.year, pkt_hwinfo.month, etc. I never found any equipment with these values.
	 * CHRG    -- The battery is charging: Code below
	 * DISCHRG -- The battery is discharging (inverter is providing load power): Code Below
	 * BYPASS  -- UPS bypass circuit is active -- no battery protection is available: It's another PROBLEM, because NHS can work in bypass mode in some models, even if you have sealed batteries on it (without any external battery device). On the moment, i'll won't work with that. Feel free to discover how it work correctly.
	 * CAL     -- UPS is currently performing runtime calibration (on battery)
	 * OFF     -- UPS is offline and is not supplying power to the load
	 * OVER    -- UPS is overloaded
	 * TRIM    -- UPS is trimming incoming voltage (called "buck" in some hardware)
	 * BOOST   -- UPS is boosting incoming voltage
	 * FSD     -- Forced Shutdown (restricted use, see the note below)
	 */

	/* Decision Chain commented below */

	/* First we check if system is on battery or not */
	upsdebugx(4, "Set UPS status as OFF and start checking. s_battery_mode is %d", lastpktdata.s_battery_mode);

	if (got_hwinfo) {
		if (lastpkthwinfo.s_220V_in) {
			upsdebugx(4, "I'm on 220v IN!. My undervoltage is %u", lastpkthwinfo.undervoltagein220V);
			min_input_power = lastpkthwinfo.undervoltagein220V;
		}  /* end if */
		else {
			upsdebugx(4, "I'm on 120v IN!. My undervoltage is %u", lastpkthwinfo.undervoltagein120V);
			min_input_power = lastpkthwinfo.undervoltagein120V;
		}  /* end else */
	}  /* end if */
	else {
		if (!min_input_power) {
			min_input_power = 96;
			upsdebugx(4, "I'm on unknown input!. My undervoltage is default %d", min_input_power);
		}  /* end if */
	}  /* end else */

	/* No ups.status changes above this line */
	status_init();

	if (lastpktdata.s_battery_mode) {
		/* ON BATTERY */
		upsdebugx(4, "UPS is on Battery Mode");
		status_set("OB");
		if (lastpktdata.s_battery_low) {
			/* If battery is LOW, warn user! */
			upsdebugx(4, "UPS is on Battery Mode and in Low Battery State");
			status_set("LB");
		}  /* end if */
	}  /* end if */
	else {
		/* Check if MAINS (power) is not preset.
		 * Well, we can check pkt_data.s_network_failure too... */
		if ((lastpktdata.vacinrms <= min_input_power) || (lastpktdata.s_network_failure)) {
			upsdebugx(4, "UPS has power-in value %0.2f and min_input_power is %d, or network is in failure. Network failure is %d", lastpktdata.vacinrms, min_input_power, lastpktdata.s_network_failure);
			status_set("DISCHRG");
		}  /* end if */
		else {
			/* MAINS is present. We need to check some situations.
			 * NHS only charge if have more than min_input_power.
			 * If MAINS is less than or equal to min_input_power,
			 * then the UPS goes to BATTERY
			 */
			if (lastpktdata.vacinrms > min_input_power) {
				upsdebugx(4, "UPS is on MAINS");
				if (lastpktdata.s_charger_on) {
					upsdebugx(4, "UPS Charging...");
					status_set("CHRG");
				}  /* end if */
				else {
					if ((lastpktdata.s_network_failure) || (lastpktdata.s_fast_network_failure)) {
						upsdebugx(4, "UPS is on battery mode because network failure or fast network failure");
						status_set("OB");
					}  /* end if */
					else {
						upsdebugx(4, "All is OK. UPS is on ONLINE!");
						status_set("OL");
					}  /* end else */
				}  /* end else */
			}  /* end if */
			else {
				/* Energy is below limit.
				* Nobreak is probably in battery mode... */
				if (lastpktdata.s_battery_low)
					status_set("LB");
				else
					/* ...or network failure */
					status_set("OB");
			}  /* end else */
		}  /* end else */
	}  /* end else */

	/* TODO: Report in NUT datapoints (battery.packs etc.),
	 *  Perhaps set in upsdrv_initinfo() and refresh in
	 *  interpret_pkt_hwinfo() if needed, but not here?
	 * NOTE: values are cached in static C vars so as to
	 *  not re-getval on every loop.
	 */
	/*
	 * The ratings come from getupsinfo(), which settles ups.conf, the case of
	 * the model and the UNKNOWN device. The model is known only after HWINFO,
	 * so a value that may still change with it is resolved again on every
	 * pass until HWINFO has been taken into account. A rating that stays at 0
	 * is not an error and gets no special handling here: what depends on it
	 * is simply not published further down.
	 *
	 * Once resolved, the ratings are kept for as long as the same UPS is on
	 * the port, across reconnections too. They are cleared only when another
	 * UPS answers (see the top of this function), and resolved again here.
	 */
	if (vbat <= 0)
		vbat = get_vbat(got_hwinfo ? (int)lastpkthwinfo.model : 0);
	if (!ah)
		ah = get_ah(got_hwinfo ? (int)lastpkthwinfo.model : 0);
	if (va == 0 && got_hwinfo)
		va = get_va((int)lastpkthwinfo.model);
	if (!pf_settled) {
		pf = get_pf(got_hwinfo ? (int)lastpkthwinfo.model : 0);
		pf_settled = got_hwinfo;
	}  /* end if */
	if (!numbat) {
		numbat = get_numbat();
		if (numbat == 0 && got_hwinfo)
			numbat = lastpkthwinfo.numbatteries;
		else
			upsdebugx(4, "Number of batteries is set to %u", numbat);
	}  /* end if */

	/* No ups.alarm changes above this line */
	alarm_init();
	ups = getupsinfo(lastpkthwinfo.model);

	if (lastpktdata.s_battery_low)
		alarm_set("[LOW BATTERY]");
	/*
	 * On most units seen so far the bypass bit stays set during normal mains
	 * operation (it mirrors "not on inverter"), so the alarm is raised only
	 * when bypassasalarm asks for it. It is off by default; the reasoning and
	 * the sources are at DEFAULT_BYPASS_AS_ALARM. The decoded bit stays
	 * available in the packet diagnostics either way.
	 */
	if (lastpktdata.s_bypass_on && ups.bypassasalarm == 1)
		alarm_set("[ON BYPASS]");

	if (lastpktdata.s_network_failure)
		alarm_set("[NETWORK FAILURE]");
	if (lastpktdata.s_fast_network_failure)
		alarm_set("[FAST NETWORK FAILURE]");

	/*
	 * The data packet also tells whether the input and the output are at
	 * 220V (s_220_in and s_220_out). Until now each of them raised an
	 * alarm here, "[220V IN]" and "[220V OUT]". They are not alarms and
	 * are not raised any more.
	 *
	 * THE PROBLEM
	 *
	 * Any alarm puts ALARM in ups.status. A unit fed from 220V mains
	 * therefore showed "ALARM OL" for as long as it ran, with nothing
	 * wrong with it, and whoever watches that status (upsmon, Home
	 * Assistant) could not tell it from a real fault.
	 *
	 * HOW IT GOT HERE
	 *
	 *  - My first version only wrote texts into ups.alarm, as a
	 *    description of what the UPS was doing: "|220v IN|", "|220v OUT|",
	 *    "|CHARGER ON|", "|BYPASS ON|" and so on. ups.status was set
	 *    apart from them and never carried ALARM on their account. I also
	 *    made a copy and paste mistake there: the two 220V texts were
	 *    tied to the fast network failure bit, not to the 220V bits, so
	 *    they hardly ever showed up.
	 *
	 *  - The commit "modernize status and alarm handling" (2025-05-01)
	 *    moved the driver to alarm_set() and alarm_commit() and corrected
	 *    the condition to the 220V bits. The charger and battery mode
	 *    texts were dropped in that change; the two 220V ones stayed,
	 *    and from that day they were real alarms on every unit working
	 *    at 220V.
	 *
	 * WHY THEY ARE NOT ALARMS
	 *
	 *  - In the manufacturer's "Protocolo NHS 3.0" document they are
	 *    "BIT4 - ENTRADA DE REDE EM 220V" and "BIT5 - SAIDA DO NOBREAK EM
	 *    220V" of the STATUS byte (byte [17]) of the "PACOTE DE DADOS"
	 *    table. They tell how the unit is working, like "BIT7 - CARREGADOR
	 *    ATIVO" beside them, and repeat what the "PACOTE DE RESPOSTA"
	 *    already says in its own STATUS byte (byte [14], "BIT0 - ENTRADA
	 *    EM 220V" and "BIT1 - SAIDA EM 220V"). The faults have bits of
	 *    their own: "BATERIA BAIXA", "FALHA DE REDE", "FALHA RAPIDA DE
	 *    REDE".
	 *
	 *  - The manufacturer's monitoring software keeps them as data, not
	 *    as events. Its SQLite file nhsupsserver.dat stores them in the
	 *    columns "tensao_entrada_nominal" and "tensao_saida_nominal" of
	 *    the tables "equipamento" and "evento"; the flag columns of
	 *    "evento" are "potencia_excessiva", "modo_inversor",
	 *    "bateria_baixa", "rede_em_falha", "queda_abrupta_rede",
	 *    "bypass_ativo" and "carregador_ativo", with nothing about 220V.
	 *
	 *  - NUT has the proper place for this information and the driver
	 *    already fills it: input.voltage.nominal and
	 *    output.voltage.nominal, further down in this function.
	 *
	 * Unlike the bypass bit, there is no setting to bring these alarms
	 * back: on no model is working at 220V a fault. The two bits are
	 * still shown by the packet diagnostics (the debug_pkt_data flag).
	 */

	/*
	 * Overload. The UPS has no bit for it; it only reports the load as a
	 * percentage of its nominal power. The manufacturer's monitoring
	 * software flags "Potencia excessiva" when that goes above 105%, and
	 * the driver had nothing for it. OVER is the NUT status for the
	 * condition. The limit is overload_perc in upsinfo: 105 unless
	 * ups.conf says otherwise, and 0 turns the check off.
	 */
	if (ups.overload_perc > 0 && lastpktdata.potrms > ups.overload_perc)
		status_set("OVER");

	/* Commit alarm and status information */
	alarm_commit(); /* alarm first */
	status_commit();

	dstate_setinfo("ups.temperature", "%0.2f", lastpktdata.tempmed_real);
	dstate_setinfo("ups.load", "%u", lastpktdata.potrms);

	/*
	 * ups.efficiency is the manufacturer's full-load rating for the current
	 * operating mode, taken from the model data or from ups.conf. The protocol
	 * reports no input power or current, so efficiency cannot be measured, and
	 * a model without a known rating does not publish the variable. This
	 * replaces a formula that multiplied the input voltage by the output
	 * voltage and reported values such as 155%.
	 */
	if (lastpktdata.s_battery_mode)
		efficiency = ups.eff_inverter;
	else {
		if (lastpktdata.s_220_in != lastpktdata.s_220_out && ups.eff_line_conv > 0)
			efficiency = ups.eff_line_conv;
		else
			efficiency = ups.eff_line;
	}  /* end else */
	if (efficiency > 0)
		dstate_setinfo("ups.efficiency", "%0.1f", (double)efficiency);
	else
		dstate_delinfo("ups.efficiency");

	/* We've got the power? */
	if (va > 0 && pf > 0) {
		/*
		 * Apparent and real power. The UPS reports only its load, as a
		 * percentage of the nominal power, so:
		 *
		 *   apparent power (VA) = nominal VA * load / 100
		 *   real power (W)      = apparent power * power factor
		 *
		 * Reading my mapping spreadsheet (Protocolo_NHS_v3.4.ods, sheet
		 * "MAPEAMENTO NUT") and docs/nut-names.txt again, against this code,
		 * turned up four errors in this block:
		 *
		 *  - ups.power is the apparent power, in VA. I published the real power
		 *    in it, so it always showed the same number as ups.realpower. It
		 *    had been like that since the first version of the driver.
		 *
		 *  - output.power and output.realpower were exchanged: the first
		 *    carried W and the second carried VA. They were right while the
		 *    variables were called just "power" and "realpower"; the two values
		 *    got crossed when the variables were renamed to output.*.
		 *
		 *  - output.power.maximum and output.power.minimum followed the real
		 *    power. The spreadsheet defines both as apparent power.
		 *
		 *  - The minimum never left 0: it started at 0 and was replaced only by
		 *    a smaller reading, which cannot happen. The same went for
		 *    output.power.minimum.percent. Both now start from the first
		 *    reading (see minpower_seen).
		 *
		 * As an example, a 2600VA unit at 20% load with a power factor of 0.9
		 * used to publish ups.power 468, output.power 468 and output.realpower
		 * 520. It now publishes 520, 520 and 468.
		 */

		/* vapower is the apparent power in VA, vpower the real power in Watts */
		vapower = (float)(va * (lastpktdata.potrms / 100.0));
		vpower = vapower * pf;
		if (vapower > maxpower)
			maxpower = vapower;
		if (!minpower_seen || vapower < minpower)
			minpower = vapower;
		dstate_setinfo("ups.power", "%0.2f", (double)vapower);
		dstate_setinfo("ups.realpower", "%ld", lrint(round(vpower)));

		/* FIXME: Move nominal settings to upsdrv_initinfo()
		 *  or at worst to interpret_pkt_hwinfo() */
		dstate_setinfo("ups.power.nominal", "%u", va);
		dstate_setinfo("ups.realpower.nominal", "%ld", lrint(round((double)va * (double)pf)));

		dstate_setinfo("output.realpower", "%ld", lrint(round(vpower)));
		dstate_setinfo("output.power", "%0.2f", (double)vapower);
		dstate_setinfo("output.power.maximum", "%0.2f", maxpower);
		dstate_setinfo("output.power.minimum", "%0.2f", minpower);
		dstate_setinfo("output.power.percent", "%u", lastpktdata.potrms);

		if (lastpktdata.potrms > maxpowerperc)
			maxpowerperc = lastpktdata.potrms;
		if (!minpower_seen || lastpktdata.potrms < minpowerperc)
			minpowerperc = lastpktdata.potrms;
		dstate_setinfo("output.power.maximum.percent", "%u", maxpowerperc);
		dstate_setinfo("output.power.minimum.percent", "%u", minpowerperc);

		/* From here on the two minimums hold a real reading. */
		minpower_seen = true;
	}  /* end if */

	dstate_setinfo("output.voltage", "%0.2f", lastpktdata.vacoutrms);
	dstate_setinfo("input.voltage", "%0.2f", lastpktdata.vacinrms);
	/* Map the packet's observed minimum and maximum to their matching NUT names. */
	dstate_setinfo("input.voltage.maximum", "%0.2f", lastpktdata.vacinrmsmax);
	dstate_setinfo("input.voltage.minimum", "%0.2f", lastpktdata.vacinrmsmin);

	if (got_hwinfo) {
		dstate_setinfo("ups.beeper.status", "%d", !lastpkthwinfo.c_buzzer_disable);

		vin_underv = lastpkthwinfo.s_220V_in ? lastpkthwinfo.undervoltagein220V : lastpkthwinfo.undervoltagein120V;
		vin_overv = lastpkthwinfo.s_220V_in ? lastpkthwinfo.overvoltagein220V : lastpkthwinfo.overvoltagein120V;

		/*
		 * Warning and critical thresholds for the input voltage, derived from
		 * the two transfer points that the UPS reports. A margin in percent is
		 * taken from each transfer point toward the normal range, so that a
		 * voltage leaving the normal range crosses the warning first, then the
		 * critical, and only then the point where the UPS goes to battery. When
		 * the warning and the critical margins are equal, which is the default,
		 * the warning one is doubled to keep the two thresholds apart.
		 *
		 * On the low side "toward the normal range" means adding the margin to
		 * the low transfer point. On the high side it means subtracting it from
		 * the high transfer point.
		 *
		 * The high side used to add the margin too. That was a reading error of
		 * mine when I turned my protocol spreadsheet (Protocolo_NHS_v3.4.ods,
		 * sheet "MAPEAMENTO NUT") into code: its rows for
		 * input.voltage.high.warning and input.voltage.high.critical say the
		 * high transfer point "minus" the percentage, and I wrote both with "+",
		 * the same as the low side. The driver has done so since its first
		 * version. The effect was two thresholds above the transfer point, which
		 * the input never reaches while the UPS is still on mains, and with the
		 * warning above the critical: for a transfer point of 156V it published
		 * a warning at 162.24V and a critical at 159.12V. With the sign
		 * corrected they are 149.76V and 152.88V.
		 *
		 * The doubling of the warning margin was also decided only once, from
		 * the low pair of settings, and reused for the high pair. Each side now
		 * looks at its own pair.
		 */
		perc = f_equal(get_vin_perc("vin_low_warn_perc", (int)lastpkthwinfo.model), get_vin_perc("vin_low_crit_perc", (int)lastpkthwinfo.model)) ?  2 : 1;
		vin_low_warn = vin_underv + (vin_underv * ((get_vin_perc("vin_low_warn_perc", (int)lastpkthwinfo.model) * perc) / 100.0));
		dstate_setinfo("input.voltage.low.warning", "%0.2f", vin_low_warn);
		vin_low_crit = vin_underv + (vin_underv * (get_vin_perc("vin_low_crit_perc", (int)lastpkthwinfo.model) / 100.0));
		dstate_setinfo("input.voltage.low.critical", "%0.2f", vin_low_crit);

		perc = f_equal(get_vin_perc("vin_high_warn_perc", (int)lastpkthwinfo.model), get_vin_perc("vin_high_crit_perc", (int)lastpkthwinfo.model)) ?  2 : 1;
		vin_high_warn = vin_overv - (vin_overv * ((get_vin_perc("vin_high_warn_perc", (int)lastpkthwinfo.model) * perc) / 100.0));
		dstate_setinfo("input.voltage.high.warning", "%0.2f", vin_high_warn);
		vin_high_crit = vin_overv - (vin_overv * (get_vin_perc("vin_high_crit_perc", (int)lastpkthwinfo.model) / 100.0));
		dstate_setinfo("input.voltage.high.critical", "%0.2f", vin_high_crit);

		dstate_setinfo("input.transfer.low", "%u", lastpkthwinfo.s_220V_in ? lastpkthwinfo.undervoltagein220V : lastpkthwinfo.undervoltagein120V);
		dstate_setinfo("input.transfer.high", "%u", lastpkthwinfo.s_220V_in ? lastpkthwinfo.overvoltagein220V : lastpkthwinfo.overvoltagein120V);

		/* FIXME: Move nominal settings to upsdrv_initinfo()
		 *  or at worst to interpret_pkt_hwinfo() */
		vin = lastpkthwinfo.s_220V_in ? lastpkthwinfo.tensionout220V : lastpkthwinfo.tensionout120V;
		dstate_setinfo("input.voltage.nominal", "%u", vin);
		vout = lastpkthwinfo.s_220V_out ? lastpkthwinfo.tensionout220V : lastpkthwinfo.tensionout120V;
		dstate_setinfo("output.voltage.nominal", "%u", vout);
	}  /* end if */

	/*
	 * Battery electric info.
	 *
	 * The charge is worked out from the voltage reading and from its nominal
	 * voltage, so the nominal voltage has to refer to the same thing that the
	 * reading does. That used to depend on "vbat" alone, and "vbat" had two
	 * meanings in this driver: the charge treated it as the voltage of one
	 * battery, while battery.voltage.nominal and the runtime treated it as the
	 * voltage of the bank. No value suited both. A unit with two batteries
	 * that reports 13.6V showed 57% of charge with vbat = 24 and, with
	 * vbat = 12, a nominal voltage of 12V for a 24V bank.
	 *
	 * Going through the NHS product sheets settled what can be relied on:
	 * every battery is a 12V unit, and what changes from one model to another
	 * is how many there are and how they are combined, with banks from 12V to
	 * 480V. The combination is not always one series string either (the
	 * sheets have, for example, 4 x 58Ah forming a 24V bank), so the number of
	 * batteries cannot be used to work out the voltage of the bank.
	 *
	 * So "vbat" now means only the nominal voltage of ONE battery, and the
	 * reading itself tells how many batteries it covers: a single 12V battery
	 * never goes much above 15V and two in series never go much below 19V.
	 * The reading is divided by the resting voltage of one full battery and
	 * rounded, and the nominal voltage is that many batteries. A unit that
	 * reports one battery and a unit that reports a 24V pair both get the
	 * right charge without any setting.
	 *
	 * Limit of this rule: with three batteries or more in one reading the
	 * ranges of neighbouring counts overlap when the bank is very discharged
	 * (three at 10V each read the same as two at 15V). No unit seen so far
	 * reports that much in one reading.
	 */
	battery_units = (unsigned int)lrint((double)lastpktdata.vdcmed_real / ((double)vbat * BATTERY_FULL_RATIO));
	if (battery_units < 1)
		battery_units = 1;
	battery_nominal = vbat * (float)battery_units;

	/*
	 * Battery charge, in percent.
	 *
	 * WHAT WAS HERE
	 *
	 * The charge was the reading as a percentage of the nominal voltage,
	 * limited to 100:
	 *
	 *   charge = reading * 100 / nominal
	 *
	 * That is how I wrote it in the first version, and it does not work as
	 * a charge. A 12V lead-acid battery is full at about 12.6V and is taken
	 * as empty at 10.5V, so over a whole discharge that formula only goes
	 * from 100% (anything from 12V up) down to 87%. The UPS raised its own
	 * low battery flag with the driver showing 91%, and a rule such as
	 * "shut down below 20%" could never fire.
	 *
	 * HOW IT WAS MEASURED
	 *
	 * The event log of the manufacturer's monitoring software (the SQLite
	 * file nhsupsserver.dat, table "evento", columns "tensao_bateria",
	 * "modo_inversor" and "bateria_baixa") has one complete discharge of my
	 * own unit: 2024-11-22, from 05:15:18 to 05:48:51, 33.5 minutes and
	 * 1343 readings at about 21% of load. The unit has 8 batteries and
	 * reports the voltage of one. The reading was 13.6V on mains, fell to
	 * 12.5V within the first minute of the discharge and was 10.4V when
	 * the UPS shut down; the low battery bit came on at 10.9V, 1.8 minutes
	 * before the end. Against the share of the time that was really left:
	 *
	 *   time left   reading   old formula   this scale
	 *      93%       12.5V       100%           95%
	 *      44%       12.0V       100%           71%
	 *      19%       11.6V        97%           52%
	 *      12%       11.3V        94%           38%
	 *       1%       10.5V        88%            0%
	 *
	 * The old formula stayed at 100% until only 38% of the time was left.
	 * Its mean distance from the time left, over the 1343 readings, was 47
	 * points of percentage; with this scale it is 21.
	 *
	 * THE SCALE
	 *
	 * A straight line between two voltages, both given as ratios of the
	 * nominal voltage so that they follow vbat and the number of batteries
	 * that the reading covers:
	 *
	 *   100% at nominal * BATTERY_FULL_RATIO   (12.6V for a 12V battery)
	 *     0% at nominal * BATTERY_EMPTY_RATIO  (10.5V for a 12V battery)
	 *
	 *   charge = (reading - empty) * 100 / (full - empty)
	 *
	 * limited to the range from 0 to 100. On mains the charger keeps the
	 * battery above 12.6V (13.6V on my unit), so a healthy unit on mains
	 * still shows 100%. The low battery bit of my unit, at 10.9V, falls at
	 * 19%.
	 *
	 * I also tried 11.0V as the empty point. It brings the mean distance
	 * down to 13 points, but the UPS kept running for almost two minutes
	 * below it, and the scale would read 0% before the UPS itself said
	 * that the battery was low. 10.5V stays.
	 *
	 * LIMITS
	 *
	 *  - It is a voltage, not a measure of the energy left; the UPS reports
	 *    nothing better. The discharge of a lead-acid battery is not a
	 *    straight line: the voltage holds for most of the time and drops
	 *    fast at the end, so in the middle of a discharge the scale still
	 *    shows more than what is left (71% with 44% of the time left, in
	 *    the table above).
	 *
	 *  - The voltage sags with the load. Under a heavy load the same
	 *    battery reads lower, and so does the charge.
	 *
	 *  - Right after mains returns the charger pulls the voltage up, and
	 *    the charge shows more than the battery has taken back.
	 *
	 *  - It was checked against one discharge, of one unit, at a light
	 *    load.
	 */
	battery_full = (double)battery_nominal * BATTERY_FULL_RATIO;
	battery_empty = (double)battery_nominal * BATTERY_EMPTY_RATIO;
	bcharge = lrint(((double)lastpktdata.vdcmed_real - battery_empty) * 100.0 / (battery_full - battery_empty));
	if (bcharge > 100)
		bcharge = 100;
	if (bcharge < 0)
		bcharge = 0;
	dstate_setinfo("battery.charge", "%ld", bcharge);
	dstate_setinfo("battery.voltage", "%0.2f", lastpktdata.vdcmed_real);
	dstate_setinfo("battery.temperature", "%ld", lrint(round(lastpktdata.tempmed_real)));

	/*
	 * Battery current, in A: battery.current is the current through each
	 * battery and battery.current.total the current of the whole bank.
	 *
	 * WHAT WAS HERE
	 *
	 *   battery.current       = (load in W / voltage reading) / batteries
	 *   battery.current.total = battery.current * batteries
	 *
	 * That is how I wrote it in the first version, and my mapping
	 * spreadsheet (Protocolo_NHS_v3.4.ods, sheet "MAPEAMENTO NUT") is of
	 * no help here: it points both variables at voltages. Three things
	 * were wrong:
	 *
	 *  1. It was published as if the UPS were always on battery. On mains
	 *     the load is fed from mains and the batteries supply nothing, but
	 *     the variable showed the current the load would draw from them.
	 *
	 *  2. It ignored the inverter. The batteries supply the load plus
	 *     what the inverter loses, the same mistake the runtime had.
	 *
	 *  3. The total was always the current of one battery times the
	 *     number of batteries. That only holds when the batteries are in
	 *     parallel. In series, which is how nearly every NHS model is
	 *     built, the same current runs through all of them: a unit with 8
	 *     batteries showed 8 times the current of its bank.
	 *
	 * THE CALCULATION
	 *
	 * While the UPS runs on battery:
	 *
	 *  a) Power taken from the batteries: the load in W divided by the
	 *     inverter efficiency, exactly as in the runtime estimate further
	 *     down (RUNTIME_ASSUMED_INVERTER_EFF when the model has no known
	 *     rating).
	 *
	 *  b) Current through each battery: that power divided by the number
	 *     of batteries and by the voltage of one battery, taken from the
	 *     reading (the reading over the number of batteries it covers).
	 *     Every battery supplies an equal share of the power however they
	 *     are wired, so this step does not depend on the wiring.
	 *
	 *  c) Current of the bank, which does depend on it (bat_series in
	 *     upsinfo): in series it is the current of one battery, in
	 *     parallel the current of one battery times the number of
	 *     batteries. The factory wiring comes from the product sheets
	 *     through the case of the model, in series when nothing is known;
	 *     battery_series in ups.conf replaces it for a unit that was
	 *     wired differently, as numbatteries does for the count.
	 *
	 * As an example, a Laser Senoidal 2600VA GII (1560W, two batteries in
	 * series, inverter efficiency 75%) at half load, on battery, reading
	 * 12.0V:
	 *
	 *  a) 780W / 0.75        = 1040W taken from the batteries
	 *  b) 1040W / (2 x 12V)  = 43.33A through each battery
	 *  c) in series          = 43.33A for the bank
	 *
	 * The old code published 32.50 and 65.00, and kept publishing the
	 * same kind of figure (28.68 and 57.35, at 13.6V) with the unit on
	 * mains.
	 *
	 * ON MAINS
	 *
	 * Both variables are published as 0: the batteries are not supplying
	 * the load. What flows then is the charger current, into the
	 * batteries. The protocol has a field for it (ICARREGRMS, byte [16] of
	 * the data packet), but none of the 13 units registered in the event
	 * log of the manufacturer's monitoring software fills it, mine
	 * included in 742 events with the charger on. It stays in the packet
	 * diagnostics (debug_pkt_data) for a model that does.
	 *
	 * WITHOUT DATA
	 *
	 * On battery without a nominal power, a number of batteries or a
	 * voltage reading there is nothing to calculate from. The two
	 * variables are not removed in that case: they are always published,
	 * and for them no data is 0.
	 */
	abat = 0;
	if (lastpktdata.s_battery_mode && va > 0 && pf > 0 && numbat > 0 && lastpktdata.vdcmed_real > 0) {
		inverter_eff = ups.eff_inverter > 0 ? ups.eff_inverter : (float)RUNTIME_ASSUMED_INVERTER_EFF;
		/* a) and b): W taken from the batteries, then A through each battery */
		abat = (vpower / (inverter_eff / 100.0f)) / ((float)numbat * (lastpktdata.vdcmed_real / (float)battery_units));
	}  /* end if */
	dstate_setinfo("battery.current", "%0.2f", (double)abat);

	/* c): one current in series, the sum of them in parallel */
	if (ups.bat_series)
		dstate_setinfo("battery.current.total", "%0.2f", (double)abat);
	else
		dstate_setinfo("battery.current.total", "%0.2f", (double)abat * (double)numbat);

	/* FIXME: Move nominal and other static settings to upsdrv_initinfo()
	 *  or at worst to interpret_pkt_hwinfo() */
	dstate_setinfo("battery.packs", "%u", numbat);
	/* Nominal voltage of what battery.voltage reports, worked out above. */
	dstate_setinfo("battery.voltage.nominal", "%0.2f", (double)battery_nominal);
	/*
	 * A capacity of 0 is "not known" (it is what the UNKNOWN device holds),
	 * so battery.capacity and, below, battery.runtime are left out rather
	 * than published as 0. This is not special handling of a missing value,
	 * it only avoids publishing a number that is false: a runtime of 0 would
	 * be read as a real measurement, and with the "ignorelb" flag the NUT
	 * core compares battery.runtime with battery.runtime.low to raise LB,
	 * which would shut the protected systems down for no reason.
	 */
	if (ah > 0) {
		dstate_setinfo("battery.capacity", "%u", ah);
		dstate_setinfo("battery.capacity.nominal", "%0.2f", (float)ah * pf);
	}  /* end if */
	dstate_setinfo("battery.runtime.low", "%d", 30);

	if (vpower > 0 && ah > 0 && numbat > 0) {
		/*
		 * Battery runtime, in seconds.
		 *
		 * WHAT WAS HERE
		 *
		 * The runtime was the capacity divided by the current, with the
		 * current taken as the load in W over the voltage of one battery:
		 *
		 *   runtime (h) = Ah / (W / 12V)
		 *
		 * It was my third attempt. Before it the code had, in turn,
		 * "Ah / (W / V read from the battery)" and, until 2024-11-27,
		 * "Ah / V read from the battery", which does not even look at the
		 * load. On 2024-11-28 I changed it to the formula above (commit
		 * "Another miscalculation in battery time duration") and left this
		 * note beside it: "Is that correct? I don't know. I'll use it for
		 * now." It was not, for three reasons:
		 *
		 *  1. It sees a single battery. With N batteries the energy stored
		 *     is N times larger, so a unit with 16 batteries got a sixteenth
		 *     of its real runtime.
		 *
		 *  2. It ignores the inverter. The batteries have to supply the load
		 *     plus what the inverter loses, 20% to 25% in the line-interactive
		 *     models, so they empty sooner than the load alone suggests.
		 *
		 *  3. It takes the capacity printed on the battery as available at
		 *     any current. That figure is valid for a 20 hour discharge. A UPS
		 *     empties its batteries in minutes, and at such currents a
		 *     lead-acid battery delivers a fraction of it. This is why units
		 *     with one large battery got up to four times their real runtime.
		 *
		 * Errors 1 and 3 pull in opposite directions, so the result was too
		 * low on some models and too high on others, with no rule a user could
		 * correct for.
		 *
		 * HOW IT WAS MEASURED
		 *
		 * The NHS product sheets ("Ficha tecnica de produto", one PDF per
		 * product, taken from nhs.com.br in November 2026) declare, for each
		 * product, the battery set, the nominal W, the inverter efficiency
		 * and the runtime at full and at half load (the fields "Aut. plena
		 * carga" and "Aut. meia carga", in minutes). 110 sheets have all of
		 * these, which gives 224 points to compare a formula against.
		 * Calling a point "close" when the formula is within one third of
		 * the declared runtime:
		 *
		 *   old formula   close in 11% of the points,
		 *                 from 9 times too low to 4.3 times too high
		 *   this formula  close in 88% of the points,
		 *                 from 1.5 times too low to 1.5 times too high
		 *
		 * Three of those sheets, declared / old / new, in minutes at full load:
		 *
		 *   Premium 1500VA, 1 x 58Ah, 750W          13 / 55.7 / 19.6
		 *   Laser Senoidal 2600VA, 2 x 58Ah, 1560W  17 / 26.8 / 17.2
		 *   Laser On Line 6000VA, 16 x 9Ah, 6000W    4 /  1.1 /  5.5
		 *
		 * THE CALCULATION
		 *
		 *  a) Power taken from the batteries: the load in W divided by the
		 *     inverter efficiency. The efficiency is the one of the model (or
		 *     of ups.conf); when it is not known,
		 *     RUNTIME_ASSUMED_INVERTER_EFF is assumed. Assuming it for every
		 *     one of the 110 sheets still leaves 83% of the points close.
		 *
		 *  b) Current through each battery: that power divided by the number
		 *     of batteries and by the nominal voltage of one battery. Sharing
		 *     the power per battery is what makes the calculation independent
		 *     of how the batteries are wired: in series every battery carries
		 *     the same current, in parallel strings the current splits, and
		 *     in both cases each battery supplies an equal share of the power.
		 *
		 *  c) Time that one battery lasts at that current, by Peukert's law:
		 *
		 *       t = H * (Ah / (I * H)) ^ k
		 *
		 *     H is RUNTIME_RATING_HOURS, the discharge time for which the Ah
		 *     figure is valid, and k is RUNTIME_PEUKERT_EXPONENT. With k = 1
		 *     this is just Ah / I; k above 1 shortens the result more and more
		 *     as the current grows, which is how a lead-acid battery behaves.
		 *     Among the values tried against the sheets, from 1.10 to 1.35,
		 *     1.25 left the most points close and almost none too high (3%
		 *     above one third), and an estimate that errs on the short side is
		 *     the safe one for something used to decide a shutdown.
		 *
		 *  d) Below the rated current, Ah / H, Peukert's law would credit the
		 *     battery with more than its printed capacity. There the plain
		 *     Ah / I is used, so the capacity never exceeds the rated one.
		 *
		 * A WORKED EXAMPLE
		 *
		 * Laser Senoidal 2600VA GII at full load: 1560W, two 12V batteries
		 * of 58Ah, inverter efficiency 75%.
		 *
		 *  a) 1560W / 0.75               = 2080W taken from the batteries
		 *  b) 2080W / (2 x 12V)          = 86.67A through each battery
		 *     rated current, 58Ah / 20h  = 2.9A, far below it: c) applies
		 *  c) 58Ah / (86.67A x 20h)      = 0.03346
		 *     0.03346 ^ 1.25             = 0.01431
		 *     20h x 0.01431              = 0.286h, or 17.2 minutes
		 *
		 * battery.runtime is published as 1030, in seconds. The sheet
		 * declares 17 minutes; the old formula, 58Ah / (1560W / 12V), gave
		 * 26.8 minutes.
		 *
		 * The same unit feeding only 26W: a) 26W / 0.75 = 34.7W,
		 * b) 34.7W / (2 x 12V) = 1.44A. That is below the rated 2.9A, so d)
		 * applies: 58Ah / 1.44A = 40.2h.
		 *
		 * WHAT IT DEPENDS ON
		 *
		 * Every input can be set in ups.conf, which matters for units whose
		 * battery set was changed: ah, vbat, numbatteries, pf (through the
		 * load in W) and efficiency_inverter.
		 *
		 * LIMITS
		 *
		 *  - It is an estimate. Against the sheets, most results are within
		 *    one third; the sheets themselves describe new batteries.
		 *
		 *  - It does not count down while the batteries discharge: it answers
		 *    "how long would full batteries last at this load" and changes
		 *    only when the load changes. The old formula had the same limit.
		 *    The UPS still raises its own low battery flag, which is what sets
		 *    LB.
		 *
		 *  - It is only as good as the capacity and the number of batteries
		 *    it is given.
		 *
		 *  - Without a load reading (0W), a capacity or a battery count
		 *    there is nothing to calculate from, and battery.runtime is
		 *    removed: see the else branch below.
		 */
		inverter_eff = ups.eff_inverter > 0 ? ups.eff_inverter : (float)RUNTIME_ASSUMED_INVERTER_EFF;

		/* a) and b): W taken from the batteries, then A through each battery */
		actual_current = (vpower / (inverter_eff / 100.0f)) / ((float)numbat * vbat);

		/* d) is the starting point, c) replaces it above the rated current */
		runtime_hours = (double)ah / (double)actual_current;
		if ((double)actual_current > (double)ah / RUNTIME_RATING_HOURS)
			runtime_hours = RUNTIME_RATING_HOURS * pow((double)ah / ((double)actual_current * RUNTIME_RATING_HOURS), RUNTIME_PEUKERT_EXPONENT);

		autonomy_secs = (unsigned int)lrint(runtime_hours * 3600.0);

		dstate_setinfo("battery.runtime", "%u", autonomy_secs);
	} else {
		/*
		 * Nothing to calculate from: the UPS reports no load (0W), or
		 * the capacity or the number of batteries is not known.
		 *
		 * This case used to skip the calculation and nothing else, so
		 * battery.runtime kept the value of the last packet that had a
		 * load, for as long as the load stayed at 0W. A client would go
		 * on reading, say, 17 minutes from a UPS that was feeding
		 * nothing, with no way to tell that the figure was old.
		 *
		 * The variable is removed instead. In NUT an absent variable
		 * means "not known", which is the truth here: with no load the
		 * batteries would last for a time that this calculation cannot
		 * put a number on. It is published again by the first packet
		 * that brings a load. Removing a variable that is not there is
		 * harmless, so this needs no "was it published" flag.
		 */
		dstate_delinfo("battery.runtime");
	}  /* end if */

	/* Battery charger status
	 * @freechurros reported in issue #3592 that Home Assistant rejects the
	 * former upper-case strings. Use the lower-case vocabulary documented in
	 * docs/nut-names.txt for interoperability with NUT clients.
	 */
	if (lastpktdata.s_charger_on)
		dstate_setinfo("battery.charger.status", "%s", "charging");
	else {
		if (lastpktdata.s_battery_mode)
			dstate_setinfo("battery.charger.status", "%s", "discharging");
		else
			dstate_setinfo("battery.charger.status", "%s", "resting");
	}  /* end else */

	if (debug_pkt_data) {
		/* Now, creating a structure called NHS.DATA, for latest
		 * data packet contents and raw data points, including those
		 * that were sorted above into NUT standard variables -
		 * for debug.
		 */
		dstate_setinfo("experimental.nhs.data.header", "%u", lastpktdata.header);
		dstate_setinfo("experimental.nhs.data.length", "%u", lastpktdata.length);
		dstate_setinfo("experimental.nhs.data.packet_type", "%c", lastpktdata.packet_type);
		dstate_setinfo("experimental.nhs.data.vacinrms_high", "%u", lastpktdata.vacinrms_high);
		dstate_setinfo("experimental.nhs.data.vacinrms_low", "%u", lastpktdata.vacinrms_low);
		dstate_setinfo("experimental.nhs.data.vacinrms", "%0.2f", lastpktdata.vacinrms);
		dstate_setinfo("experimental.nhs.data.vdcmed_high", "%u", lastpktdata.vdcmed_high);
		dstate_setinfo("experimental.nhs.data.vdcmed_low", "%u", lastpktdata.vdcmed_low);
		dstate_setinfo("experimental.nhs.data.vdcmed", "%0.2f", lastpktdata.vdcmed);
		dstate_setinfo("experimental.nhs.data.vdcmed_real", "%0.2f", lastpktdata.vdcmed_real);
		dstate_setinfo("experimental.nhs.data.potrms", "%u", lastpktdata.potrms);
		dstate_setinfo("experimental.nhs.data.vacinrmsmin_high", "%u", lastpktdata.vacinrmsmin_high);
		dstate_setinfo("experimental.nhs.data.vacinrmsmin_low", "%u", lastpktdata.vacinrmsmin_low);
		dstate_setinfo("experimental.nhs.data.vacinrmsmin", "%0.2f", lastpktdata.vacinrmsmin);
		dstate_setinfo("experimental.nhs.data.vacinrmsmax_high", "%u", lastpktdata.vacinrmsmax_high);
		dstate_setinfo("experimental.nhs.data.vacinrmsmax_low", "%u", lastpktdata.vacinrmsmax_low);
		dstate_setinfo("experimental.nhs.data.vacinrmsmax", "%0.2f", lastpktdata.vacinrmsmax);
		dstate_setinfo("experimental.nhs.data.vacoutrms_high", "%u", lastpktdata.vacoutrms_high);
		dstate_setinfo("experimental.nhs.data.vacoutrms_low", "%u", lastpktdata.vacoutrms_low);
		dstate_setinfo("experimental.nhs.data.vacoutrms", "%0.2f", lastpktdata.vacoutrms);
		dstate_setinfo("experimental.nhs.data.tempmed_high", "%u", lastpktdata.tempmed_high);
		dstate_setinfo("experimental.nhs.data.tempmed_low", "%u", lastpktdata.tempmed_low);
		dstate_setinfo("experimental.nhs.data.tempmed", "%0.2f", lastpktdata.tempmed);
		dstate_setinfo("experimental.nhs.data.tempmed_real", "%0.2f", lastpktdata.tempmed_real);
		dstate_setinfo("experimental.nhs.data.icarregrms", "%u", lastpktdata.icarregrms);
		dstate_setinfo("experimental.nhs.data.icarregrms_real", "%u", lastpktdata.icarregrms_real);

		/*
		 * Voltage of the whole bank: battery_tension is the measured one
		 * and nominaltension the nominal one.
		 *
		 * Both used to be worked out while the packet was decoded, as the
		 * reading (or vbat) times the number of batteries reported by the
		 * UPS. That is how I wrote them in the first version, and they
		 * fell behind as the rest of the driver was corrected: they
		 * ignored "numbatteries" from ups.conf, took every bank as wired
		 * in series, and took the reading as the voltage of one battery
		 * even on a unit that reports a pair. They now come from the same
		 * values the published variables use:
		 *
		 *   in series    volts of one battery * number of batteries
		 *   in parallel  volts of one battery
		 *
		 * where the volts of one battery are the reading over the number
		 * of batteries it covers (measured) or vbat (nominal). A number of
		 * batteries that is not known yet gives 0, as elsewhere.
		 *
		 * Two other values lived beside them and are gone:
		 *
		 *  - perc_output was the first divided by the second, that is, the
		 *    battery charge by the formula that battery.charge no longer
		 *    uses. With the diagnostics on it showed 91% beside a
		 *    battery.charge of 19% for the same reading.
		 *
		 *  - timeremain was declared, printed and published but never
		 *    calculated: it was always 0. The estimate is battery.runtime.
		 */
		bank_units = ups.bat_series ? numbat : 1;
		dstate_setinfo("experimental.nhs.data.battery_tension", "%0.2f", (double)(lastpktdata.vdcmed_real / (float)battery_units) * (double)bank_units);
		dstate_setinfo("experimental.nhs.data.nominaltension", "%0.2f", (double)vbat * (double)bank_units);
		dstate_setinfo("experimental.nhs.data.statusval", "%u", lastpktdata.statusval);
		for (i = 0; i < 8; i++) {
			/* Reusing variable */
			snprintf(data_scratch_buf, sizeof(data_scratch_buf), "experimental.nhs.data.status_p%u", i);
			dstate_setinfo(data_scratch_buf, "%u", lastpktdata.status[i]);
		}  /* end for */
		dstate_setinfo("experimental.nhs.data.s_battery_mode", "%s", lastpktdata.s_battery_mode ? "true" : "false");
		dstate_setinfo("experimental.nhs.data.s_battery_low", "%s", lastpktdata.s_battery_low ? "true" : "false");
		dstate_setinfo("experimental.nhs.data.s_network_failure", "%s", lastpktdata.s_network_failure ? "true" : "false");
		dstate_setinfo("experimental.nhs.data.s_fast_network_failure", "%s", lastpktdata.s_fast_network_failure ? "true" : "false");
		dstate_setinfo("experimental.nhs.data.s_220_in", "%s", lastpktdata.s_220_in ? "true" : "false");
		dstate_setinfo("experimental.nhs.data.s_220_out", "%s", lastpktdata.s_220_out ? "true" : "false");
		dstate_setinfo("experimental.nhs.data.s_bypass_on", "%s", lastpktdata.s_bypass_on ? "true" : "false");
		dstate_setinfo("experimental.nhs.data.s_charger_on", "%s", lastpktdata.s_charger_on ? "true" : "false");
		dstate_setinfo("experimental.nhs.data.checksum", "%u", lastpktdata.checksum);
		dstate_setinfo("experimental.nhs.data.checksum_ok", "%s", lastpktdata.checksum_ok ? "true" : "false");
		dstate_setinfo("experimental.nhs.data.checksum_calc", "%u", lastpktdata.checksum_calc);
		dstate_setinfo("experimental.nhs.data.end_marker", "%u", lastpktdata.end_marker);
		dstate_setinfo("experimental.nhs.param.va", "%u", va);
		dstate_setinfo("experimental.nhs.param.pf", "%0.2f", pf);
		dstate_setinfo("experimental.nhs.param.ah", "%u", ah);
		dstate_setinfo("experimental.nhs.param.vin_low_warn_perc", "%0.2f", get_vin_perc("vin_low_warn_perc", (int)lastpkthwinfo.model));
		dstate_setinfo("experimental.nhs.param.vin_low_crit_perc", "%0.2f", get_vin_perc("vin_low_crit_perc", (int)lastpkthwinfo.model));
		dstate_setinfo("experimental.nhs.param.vin_high_warn_perc", "%0.2f", get_vin_perc("vin_high_warn_perc", (int)lastpkthwinfo.model));
		dstate_setinfo("experimental.nhs.param.vin_high_crit_perc", "%0.2f", get_vin_perc("vin_high_crit_perc", (int)lastpkthwinfo.model));
	}  /* end if */
}

void upsdrv_updateinfo(void) {
	double      tempodecorrido = 0.0;
	time_t      now, timeout_sec;
	useconds_t  timeout_usec;
	ssize_t     read_result;
	int         randval = 0;
	pkt_hwinfo  received_hwinfo;

	upsdebugx(3, "%s: starting...", __func__);

	/* If comms failed earlier, try to resuscitate */
	if (INVALID_FD_SER(reconnect_ups_if_needed()))
		return;

	chr = '\0';

	/* ser_get_char accepts separate seconds and microseconds components. */
	timeout_sec = (time_t)(serial_read_timeout_ms / 1000);
	timeout_usec = (useconds_t)((serial_read_timeout_ms % 1000) * 1000);

	/*
	 * A positive result supplies one byte, zero is the configured normal
	 * timeout, and a negative result is handled as a communication error.
	 */
	read_result = ser_get_char(serial_fd, &chr, timeout_sec, timeout_usec);
	while (read_result > 0) {
		/* The length-aware framing and the need to preserve marker values inside
		 * packet payloads were demonstrated by raw captures contributed by
		 * @freechurros in issue #3592.
		 *
		 * A 0xFF byte starts a packet only while the reader is idle. After
		 * the packet starts, byte [1] declares its total size, including the
		 * initial 0xFF, checksum and final 0xFE. Any 0xFF or 0xFE received
		 * before the declared final position belongs to the packet contents.
		 * The packet is interpreted only when byte [size - 1] is 0xFE.
		 */
		if ((chr == 0xFF) && (!datapacketstart)) {
			datapacketstart = true;
			memset(datapacket, 0, sizeof(datapacket));
			datapacket_index = 0;
			datapacketsize = 0;
		}  /* end if */
		if (datapacketstart) {
			/* Assume that second position is PACKET SIZE. */
			if (datapacket_index == 1)
				datapacketsize = chr;
			datapacket[datapacket_index] = chr;
			if ((datapacket_index == 1) && ((datapacketsize < 18) || (datapacketsize > sizeof(datapacket)))) {
				upslogx(LOG_WARNING, "Incoming packet declares an invalid size, discarding!");
				datapacketstart = false;
			}  /* end if */
			else {
				if ((datapacketsize > 0) && (datapacket_index == (datapacketsize - 1))) {
					if (datapacket[datapacket_index] != 0xFE)
						upslogx(LOG_WARNING, "Incoming packet does not end with 0xFE, discarding!");
					else {
						now = time(NULL);
						upsdebugx(4, "DATAPACKET SIZE IS %u", datapacketsize);

						if (lastdp != 0)
							tempodecorrido = difftime(now, lastdp);

						lastdp = now;

						switch (datapacketsize) {
							case 18:
							case 50:
								received_hwinfo = mount_hwinfo(datapacket, datapacketsize);

								/* Always accept a new valid HWINFO packet. The UPS may be
								 * replaced while the driver remains active, so retaining only
								 * the first packet would leave model-specific data stale until
								 * NUT is restarted. Preserve the last valid data if the newly
								 * received packet has a bad checksum.
								 */
								if (received_hwinfo.checksum_ok) {
									/* Data from one UPS is never valid for
									 * another. When this answer does not come
									 * from the unit the driver was talking to,
									 * what was learned from that one is dropped
									 * before the new answer is taken in; see
									 * forget_previous_ups().
									 */
									if (lastpkthwinfo.size > 0 && !same_ups(&lastpkthwinfo, &received_hwinfo)) {
										upslogx(LOG_NOTICE, "A different UPS answered on %s (model code %u, it was %u): the data and the statistics of the previous one were discarded", porta, received_hwinfo.model, lastpkthwinfo.model);
										forget_previous_ups();
									}  /* end if */
									lastpkthwinfo = received_hwinfo;
									interpret_pkt_hwinfo();
									dstate_dataok();
								}  /* end if */
								break;

							case 21:
								lastpktdata = mount_datapacket(datapacket, datapacketsize, tempodecorrido);

								if (lastpktdata.checksum_ok) {
									interpret_pkt_data();
									dstate_dataok();
								}  /* end if */
								break;

							default:
								upslogx(LOG_WARNING, "Incoming packet size not recognized, discarding!");
								/* Suggested by @freechurros in issue #3592: expose the
								 * rejected bytes when raw packet debugging is enabled so
								 * future framing variants can be diagnosed from the log.
								 */
								pdatapacket(datapacket, datapacketsize);
								break;
						}  /* end switch */
					}  /* end else */

					datapacketstart = false;
				}  /* end if */
			}  /* end else */
		}  /* end if */
		/* Advance only after consuming the current byte. Keeping both the
		 * index and declared size outside this function lets a packet resume
		 * at the correct position after a normal serial-read timeout.
		 */
		datapacket_index++;
		read_result = ser_get_char(serial_fd, &chr, timeout_sec, timeout_usec);
	}  /* end while */

	if (read_result < 0) {
		upsdebug_with_errno(1, "%s: Serial read failed on %s", __func__, porta);
		close_serial_port();
		dstate_datastale();
		return;
	}  /* end if */
	if (datapacketstart) {
		upsdebugx(2, "%s: packet reading did not finish, not interpreting yet", __func__);
		return;
	}  /* end if */

	/* Now the nobreak read buffer is empty.
	 * We need a hw info packet to discover several variables,
	 * like number of batteries, to calculate some data
	 * FIXME: move (semi)static info discovery to upsdrv_initinfo() or so
	 */
	if (!lastpkthwinfo.checksum_ok) {
		upsdebugx(4, "pkt_hwinfo loss -- Requesting");
		/* If size == 0, packet maybe not initizated,
		 * then send an initialization packet to obtain data.
		 * Send six times the extended initialization string,
		 * but, on fail, try randomly send extended, normal or compatibility.
		 */
		if (send_extended < 6) {
			upsdebugx(4, "Sending extended initialization packet. Try %u", send_extended+1);
			bwritten = write_serial_int(serial_fd, string_initialization_long, 9);
			send_extended++;
		}  /* end if */
		else {
			/* randomly send */
			randval = rand() % 3;
			switch (randval) {
				case 0:
					upsdebugx(4, "Sending long initialization packet (random)");
					bwritten = write_serial_int(serial_fd, string_initialization_long, 9);
					break;
				case 1:
					upsdebugx(4, "Sending short initialization packet (random)");
					bwritten = write_serial_int(serial_fd, string_initialization_short, 9);
					break;
				case 2:
					upsdebugx(4, "Sending compatibility initialization packet (random)");
					bwritten = write_serial_int(serial_fd, string_initialization_comptmode, 5);
					break;
				default:
					/* rand() % 3 currently limits this switch to cases 0..2.
					 * Keep a deterministic fallback so future changes to the
					 * selection range cannot leave bwritten with stale data.
					 */
					upsdebugx(4, "Initialization selection out of range; sending long packet");
					bwritten = write_serial_int(serial_fd, string_initialization_long, 9);
					break;
			}  /* end switch */
		}  /* end else */
		if (bwritten < 0) {
			upsdebugx(1, "%s: Problem to write data to %s", __func__, porta);
			if (bwritten == -1)
				upsdebugx(1, "%s: Data problem", __func__);
			close_serial_port();
		}  /* end if */
		else {
			if (checktime > max_checktime)
				checktime = max_checktime;
			else {
				upsdebugx(3, "Increase checktime to %u", checktime + 100000);
				checktime = checktime + 100000;
			}  /* end else */
			usleep(checktime);
		}  /* end else */
	}  /* end if lastpkthwinfo good/bad checksum */  /* end if */

	upsdebugx(3, "%s: finished", __func__);
}

void upsdrv_shutdown(void) {
	upsdebugx(3, "%s: starting...", __func__);

	/* replace with a proper shutdown function */
	upslogx(LOG_ERR, "shutdown not supported");
	set_exit_flag(EF_EXIT_FAILURE);

	upsdebugx(1, "Driver shutdown");
}

void upsdrv_cleanup(void) {
	upsdebugx(3, "%s: starting...", __func__);

	close_serial_port();

	upsdebugx(3, "%s: finished", __func__);
}

void upsdrv_initups(void) {
	/* From docs/new-drivers.txt:
	 * Open the port (`device_path`) and do any low-level
	 * things that it may need to start using that port.
	 * If you have to set DTR or RTS on a serial port,
	 * do it here.
	 * Don't do any sort of hardware detection here, since
	 * you may be quickly going into upsdrv_shutdown next.
	 */

	upsdebugx(3, "%s: starting...", __func__);

	/* Process optional configuration flags that may
	 * impact HW init methods (debug them or not)
	 */
	if (getval("debug_pkt_raw"))
		debug_pkt_raw = 1;
	if (getval("debug_pkt_data"))
		debug_pkt_data = 1;
	if (getval("debug_pkt_hwinfo"))
		debug_pkt_hwinfo = 1;

	/* Validate the complete serial configuration before accessing the device. */
	parse_serial_options();

	upsdebugx(1, "%s: Port is %s and baud_rate is %d", __func__, device_path, baudrate);

	if (device_path) {
		if (strcasecmp(device_path, "auto") == 0)
			strncpy(porta, DEFAULTPORT, sizeof(porta) - 1);
		else
			strncpy(porta, device_path, sizeof(porta) - 1);
		serial_fd = openfd(porta, baudrate);
		if (INVALID_FD_SER(serial_fd))
			fatalx(EXIT_FAILURE, "Unable to open port %s with baud %d", porta, baudrate);
		else
			upsdebugx(1, "%s: Communication started on port %s, baud rate %d", __func__, porta, baudrate);
	}  /* end if */
	else
		fatalx(EXIT_FAILURE, "Unable to define port and baud");

	/* If we got here, the port is opened with desired baud rate.
	 * If not shutting down ASAP, soon we will call upsdrv_initinfo()
	 * and "infinitely" loop calling upsdrv_updateinfo() afterwards.
	 */
	upsdebugx(3, "%s: finished", __func__);
}

void upsdrv_makevartable(void) {
	char  help[4096];

	/* Standard variable in main.c */
	/* //addvar(VAR_VALUE, "port", "Port to communication"); */

	/* Expose only the seven conventional serial settings supported above. */
	addvar(VAR_VALUE, "baud", "Serial baud rate from the rates compiled into this driver (default: 2400)");

	addvar(VAR_VALUE, "serial_data_bits", "Serial data bits: 5, 6, 7 or 8 (default: 8)");

	addvar(VAR_VALUE, "serial_parity", "Serial parity: none, even or odd (default: none)");

	addvar(VAR_VALUE, "serial_stop_bits", "Serial stop bits: 1 or 2 (default: 1)");

	addvar(VAR_VALUE, "serial_flow_control", "Serial flow control: none, hardware, software or both (default: none, as the NHS protocol document specifies)");

	addvar(VAR_VALUE, "serial_read_timeout_ms", "Timeout per serial byte: 0 to 60000 ms (default: 100)");

	addvar(VAR_VALUE, "serial_send_pace_us", "Delay between transmitted bytes: 0 to 999999 us (default: 0)");

	/*
	 * Model ratings. Each of these replaces the value that getupsinfo() has
	 * for the UPS being monitored; see the upsinfo structure.
	 */
	addvar(VAR_VALUE, "ah", "Capacity of each battery in Ampere-hour (replaces the built-in model value)");

	addvar(VAR_VALUE, "va", "Nominal power in VA (replaces the built-in model value)");

	snprintf(help, sizeof(help), "Power factor, nominal W divided by nominal VA (replaces the built-in model value; default without one is %0.2f)", DEFAULTPF);
	addvar(VAR_VALUE, "pf", help);

	addvar(VAR_VALUE, "efficiency_line", "Full-load efficiency on mains with the same input and output voltage, in percent (replaces the built-in model value)");

	addvar(VAR_VALUE, "efficiency_line_conv", "Full-load efficiency on mains when converting between 120V and 220V, in percent (replaces the built-in model value)");

	addvar(VAR_VALUE, "efficiency_inverter", "Full-load efficiency on battery, in percent (replaces the built-in model value)");

	snprintf(help, sizeof(help), "Voltage In Percentage to calculate warning low level. Default is %0.2f", DEFAULTPERC);
	addvar(VAR_VALUE, "vin_low_warn_perc", help);

	snprintf(help, sizeof(help), "Voltage In Percentage to calculate critical low level. Default is %0.2f", DEFAULTPERC);
	addvar(VAR_VALUE, "vin_low_crit_perc", help);

	snprintf(help, sizeof(help), "Voltage In Percentage to calculate warning high level. Default is %0.2f", DEFAULTPERC);
	addvar(VAR_VALUE, "vin_high_warn_perc", help);

	snprintf(help, sizeof(help), "Voltage In Percentage to calculate critical high level. Default is %0.2f", DEFAULTPERC);
	addvar(VAR_VALUE, "vin_high_crit_perc", help);

	snprintf(help, sizeof(help), "Num Batteries (override value from nobreak)");
	addvar(VAR_VALUE, "numbatteries", help);

	addvar(VAR_VALUE, "battery_series", "How the batteries are wired: 1 in series, 0 in parallel (replaces the built-in model value)");

	addvar(VAR_VALUE, "read16_vin", "Input voltage bytes: 0 read as \"first.second\" (default), 1 read as a 16-bit number");
	addvar(VAR_VALUE, "read16_vin_min", "Minimum input voltage bytes: 0 read as \"first.second\" (default), 1 read as a 16-bit number");
	addvar(VAR_VALUE, "read16_vin_max", "Maximum input voltage bytes: 0 read as \"first.second\" (default), 1 read as a 16-bit number");
	addvar(VAR_VALUE, "read16_vout", "Output voltage bytes: 0 read as \"first.second\" (default), 1 read as a 16-bit number");
	addvar(VAR_VALUE, "read16_temp", "Temperature bytes: 0 read as \"first.second\" (default), 1 read as a 16-bit number");
	addvar(VAR_VALUE, "read16_vbat", "Battery voltage bytes: 0 read as \"first.second\" (default), 1 read as a 16-bit number in tenths of a volt");

	snprintf(help, sizeof(help), "Load, in percent, above which the UPS is reported as overloaded (OVER); 0 turns the check off. Default is %d", DEFAULTOVERLOAD);
	addvar(VAR_VALUE, "overload_perc", help);

	snprintf(help, sizeof(help), "Nominal voltage of ONE battery, not of the bank; decimals are accepted. Default is %0.2f, which fits every NHS model", DEFAULTBATV);
	addvar(VAR_VALUE, "vbat", help);

	addvar(VAR_FLAG, "debug_pkt_raw", "Enable debug logging of packet bytes");

	addvar(VAR_FLAG, "debug_pkt_data", "Enable debug logging of data packet decoding");

	addvar(VAR_FLAG, "debug_pkt_hwinfo", "Enable debug logging of hwinfo packet decoding");

	addvar(VAR_VALUE, "overridemodel", "Override the UPS model name reported by the driver");

	addvar(VAR_VALUE, "protocolversion", "Override the NHS protocol version stored for the model (default: 3)");

	addvar(VAR_VALUE, "bypassasalarm", "Report the UPS bypass bit as an alarm: 0 or 1 (default: 0, most models set the bit in normal operation)");
}

void upsdrv_help(void) {
}

/* optionally tweak prognames[] entries */
void upsdrv_tweak_prognames(void)
{
}
