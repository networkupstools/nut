/* ragtech.c - driver for Ragtech UPSes (Easy Pro family -- NEP/TORO/INNERGIE/OneUP,
 *              and the Easy Way family 14 "NEW/QUADRI")
 *
 * Target devices: Ragtech UPSes exposing a USB CDC-ACM serial interface
 * (PIC firmware, USB VID 0x04D8, PID 0x000A). Connected as /dev/ttyACM*.
 *
 * Protocol notes (derived from Ragtech's OEM devices.xml -- family 10 -- and
 * cross-checked against UPS_ESP32_tinySrv, https://github.com/antunesls/UPS_ESP32_tinySrv).
 *
 *   Line config: 8N1 (on family 10 the CDC-ACM endpoint ignores baud, but the
 *   OEM client sets it to 2560; family 14 below is different). DTR=0 and RTS=0 MUST be asserted right after opening --
 *   non-zero levels are interpreted by some Ragtech families as a remote
 *   shutdown signal.
 *
 *   The UPS exposes a flat register address space; the client reads contiguous
 *   ranges of bytes by address. Family 10 advertises five ranges (devices.xml):
 *
 *     0x0080..0x009D   30 bytes -- main status range, polled every cycle
 *     0x00F3..0x00F3    1 byte  -- V_IOUTCALIB (per-unit current calibration)
 *     0x0136..0x0136    1 byte  -- V_CAPBATNEW (replacement battery capacity)
 *     0x0171..0x0174    4 bytes -- RGB LED + random
 *     0x0202..0x0203    2 bytes -- V_OSC53 / V_OSC57 (frequency calibration)
 *
 *   Read command (6 bytes):
 *     0xAA 0x04 ADDR_HI ADDR_LO COUNT CHECKSUM
 *     where CHECKSUM = (ADDR_HI + ADDR_LO + COUNT) & 0xFF.
 *
 *   Reply: 1 + COUNT bytes -- SOF=0xAA followed by COUNT raw register bytes.
 *   No checksum in the reply; we resynchronise on SOF.
 *
 *   Write command (6 bytes):
 *     0xAA OPCODE ADDR_HI ADDR_LO VALUE CHECKSUM
 *     CHECKSUM = (ADDR_HI + ADDR_LO + VALUE) & 0xFF.
 *
 *   Four opcodes mapped:
 *     0x01   write byte (reg = VALUE)
 *     0x02   AND mask   (reg = reg & VALUE)   -- atomic bit-clear
 *     0x03   OR  mask   (reg = reg | VALUE)   -- atomic bit-set
 *     0x04   read range (with VALUE = byte count)
 *
 *   The 0x02/0x03 atomic forms are required for any operation that has
 *   to trip a firmware state machine (e.g. arming the shutdown counter
 *   only works if F_AUTOSTART was modified via 0x02 / 0x03; a plain 0x01
 *   write to the same byte does not). Opcodes 0x05..0x07 were probed
 *   and have no observable effect.
 *
 *   Replies to writes (if any) are not consumed by the OEM client; we
 *   drain whatever comes back.
 *
 *   Family 10 register map (buf[i] = reg(0x80 + i - 1) for the main range):
 *
 *     buf[ 1]  0x80  F_AUTOSTART(0) F_ICBATTERY(2) F_LINESENS(7)
 *     buf[ 8]  0x87  V_CBATTERY      battery.charge      = raw / 2.55  (0..100 %)
 *     buf[11]  0x8A  V_VBATTERY      battery.voltage     = raw * 0.0670 (or 0.1340 for 24V models)
 *     buf[12]  0x8B  V_VINPUT        input.voltage       = raw * 1.0600
 *     buf[13]  0x8C  V_IOUTPUT       output.current      = raw * model_imult / iout_calib
 *     buf[14]  0x8D  V_POUTPUT       firmware load %, integer; floors to 0
 *                                   below ~1%. Not used -- ups.load is
 *                                   computed as (V_out * I_out) / VA * 100.
 *     buf[15]  0x8E  V_TEMPER        ups.temperature     = raw (°C)
 *     buf[17]  0x90  F_NOBAT(0) F_OLDBAT(1) F_OPCHECKUP(2) F_NOVINPUT(3)
 *                   F_LOVINPUT(4) F_HIVINPUT(5) F_OPBATTERY(6) F_HIPOUTPUT(7)
 *     buf[18]  0x91  F_LOBATTERY(0) F_FOVERTEMP(1) F_FENDBATTERY(2) F_FOVERLOAD(3)
 *                   F_FABNORMALVOUT(4) F_FABNORMALVBAT(5) F_FINVERTER(6) F_FSHORTCIRCUIT(7)
 *     buf[19]  0x92  F_SYNCIN(0) F_SUPERVON(1) F_MOREBAT(2) F_LESSBAT(3)
 *                   F_RCTRLON(5) F_POWERLOON(6) F_NIGHTOFFON(7)
 *     buf[22]  0x95  F_TRANSFINV(7)
 *     buf[24]  0x97  V_FOUTPUT       output.frequency    = interp(V_FOUTPUT, V_OSC53, V_OSC57)
 *     buf[25]  0x98  V_SHUTDOWNTIMER
 *     buf[27]  0x9A  V_MODEL         model id (see ragtech_models[])
 *     buf[28]  0x9B  V_VERSION       ups.firmware        = raw * 0.1
 *     buf[30]  0x9D  V_VOUTPUT       output.voltage      = raw * model_vmult
 *
 *   Frequency formula (OSC57 != OSC53):
 *     fOutput = (53.0 + 4.0 * (V_FOUTPUT - V_OSC53) / (V_OSC57 - V_OSC53)) * 0.9806 + 1.46
 *
 *   The pre-poll handshake "0xFF 0xFE 0x00 0x8E 0x01 0x8F" sent by the OEM
 *   firmware has unknown semantics; treating as opaque wake-up. The firmware
 *   answers it with a byte or two (e.g. 0xCA); upsdrv_initinfo drains that
 *   reply before the first read, since half-duplex units otherwise drop a
 *   read command that arrives while the handshake reply is still in flight.
 *
 *   Write/action commands (shutdown, fullDischarge, setLineSens, ...) are
 *   declared in the OEM XML as register-level writes (e.g. shutdown writes
 *   V_SHUTDOWNTIMER at 0x98 plus a bit of F_AUTOSTART at 0x80) but the on-wire
 *   encoding of the write command is not yet captured. Strace of the OEM
 *   supsvc against /dev/ttyACM0 should reveal it.
 *
 *
 * Family 14 (Easy Way, "NEW/QUADRI" in the OEM table) -- read only
 * -----------------------------------------------------------------
 *
 *   Same USB ids (04D8:000A) as family 10 but a different, push-only
 *   protocol: the UPS never waits for a command. Nothing is ever written to
 *   it by this driver in this family (no bulk OUT traffic was seen from the
 *   OEM client either, and the OEM table lists no actions for family 14).
 *
 *   Line config: 2560 8N1 -- NOT 2400. Unlike family 10, the PIC does apply
 *   the line coding to its UART here. The OEM devices.xml lists 2560 as the
 *   first serial speed of this family, and the OEM client used it (which is
 *   why USBPcap captures made with it are clean). Read at 2400 the receiver
 *   samples bit 6 right on the bit 6 / bit 7 boundary of the transmitter and
 *   bit 7 inside the stop bit: bit 7 arrives set where it should be clear,
 *   bit 6 is unreliable, and most frames fail the checksum. Read at 4800 the
 *   stream turns into byte pairs such as 66 e6, and at 9600 or above into
 *   pulses of 4 or more bit times.
 *   2560 has no Bxxx constant: ragtech_speed.c sets it (termios2 + BOTHER on
 *   Linux, a numeric speed_t on the BSDs and macOS). If that fails the driver
 *   logs a warning and uses 2400, where it can only reject corrupted frames.
 *   DTR=0 and RTS=0 as for family 10.
 *
 *   Every ~1.25 to 1.3 s the UPS sends one frame of 32 bytes:
 *
 *     0x55 0x55 0xAE | reg[0x00..0x1A] (27 bytes) | tail | CS
 *
 *   CS is the 8-bit sum of the 31 preceding bytes (sync bytes included).
 *   The byte "tail" was seen as 0x09 on mains and 0x08 on battery (and
 *   sporadically after mains returned), so it is NOT checked. The meaning of
 *   0xAE is unknown.
 *
 *   The 8-bit sum is a weak check: two bytes that both gain bit 7 add up to
 *   0x100 and cancel, so a corrupted frame can pass it (it did, at 2400 baud:
 *   ups.load 385, a 180 C temperature, "Inverter fault"). A frame is
 *   therefore only accepted if, besides sync and CS:
 *
 *   - registers 0x00..0x09 (version, model, calibration constants) equal the
 *     reference. These are fixed in the unit. The reference is taken from
 *     two consecutive checksum-valid frames with identical 0x00..0x09; until
 *     then nothing is published. A frame whose 0x00..0x09 differ is dropped.
 *     If that goes on for more than 30 s and two consecutive valid frames
 *     agree on a new set, that set becomes the reference (so a wrong
 *     reference, or a unit that was changed, cannot lock the driver out);
 *   - ACTUALTAP <= 3, load (IOUTPUT / IOUTCALIB) <= 150 %, TEMPER <= 100,
 *     and battery voltage (VBATTERY / VBATCALIB * 13.8) within 6..18 V;
 *   - and the rare indications -- reg 0x0E bits 4..7 (ups.alarm), OVER and
 *     LB -- are published only when seen in two consecutive accepted frames
 *     (and cleared only when absent from two). OL/OB and CHRG are not
 *     debounced: they were confirmed on real transitions, a mains cut must
 *     not be delayed, and the corruption seen only ever touched bits 6 and 7.
 *
 *   Register map (the register index is the offset into the 27 bytes):
 *
 *     0x00 version (x0.1)   0x01 model id (see f14_models[])
 *     0x02 OSC53, 0x03 OSC57 (frequency calibration)
 *     0x04 FOUTCALIB, 0x05 VBATCALIB, 0x06 VINCALIB, 0x07 VOUTCALIB, 0x09 IOUTCALIB
 *     0x0A bit2 F_VINSEL220
 *     0x0C bit0 SYNCIN, bit4 SHUTDOWNTIMERON, bit7 MAXBATTERY
 *     0x0D bit0 NOVINPUT
 *     0x0E bit1 FENDBATTERY, bit2 FOVERLOAD, bit4 FSHORTCIRCUIT,
 *          bit5 FABNORMALVOUT, bit6 FABNORMALVBAT, bit7 FINVERTER
 *     0x0F bit1 LOFINPUT, bit2 HIFINPUT, bit3 NOSYNCIN, bit4 LOVINPUT,
 *          bit5 HIVINPUT, bit6 LOBATTERY, bit7 NOISEIN
 *     0x10 bit3 OPBATTERY, bit4 OPSTANDBY, bit5 OPWARNING, bit6 OPSTARTUP
 *     0x11 ACTUALTAP   0x12 VINPUT   0x13 VOUTPUT   0x14 IOUTPUT (a load ratio)
 *     0x15 VBATTERY    0x16 SHUTDOWNTIMER           0x17 FOUTPUT
 *     0x1A TEMPER (degrees C)
 *
 *   Conversions (OEM table; "115" and the tap factors are model dependent,
 *   see f14_models[]):
 *
 *     input.voltage    = VINPUT / VINCALIB * 115 (0 when NOVINPUT); TI models
 *                        strapped for 220 V on mains use
 *                        VINPUT / VINCALIB * 230 * (IOUTPUT / IOUTCALIB * 0.05 + 1)
 *     output.voltage   = VOUTPUT / VOUTCALIB * TAP[ACTUALTAP] * 115 (0 on OPSTANDBY)
 *     output.frequency = same OSC53/OSC57 interpolation as family 10
 *     ups.load         = IOUTPUT / IOUTCALIB * 100
 *     battery.voltage  = VBATTERY / VBATCALIB * 13.8
 *     battery.charge   = (VBATTERY / VBATCALIB - 0.725) / 0.275 * 100, kept in 0..100,
 *                        then post-processed (next paragraph)
 *
 *   The charge is only an estimate from the battery voltage, which wobbles
 *   from frame to frame (14.04 V, 13.68 V, 14.04 V on a healthy unit on
 *   mains: 100, 96.8, 100 %). The NUT core (status_commit() in dstate.c)
 *   reads any change of battery.charge below 100 as charging or discharging
 *   and adds CHRG or DISCHRG when the driver set neither, or logs a warning
 *   when the driver set the opposite one. So, per accepted frame:
 *   - on mains with MAXBATTERY set the charge is 100.0 flat;
 *   - otherwise the calculated charge goes through an exponential average
 *     (F14_CHG_ALPHA per frame, about 5 s), restarted from the calculated
 *     value whenever the state (full / charging / on battery) changes;
 *   - the published value never falls while CHRG is reported and never
 *     rises while on battery (DISCHRG is reported there), so it cannot
 *     contradict the status. On entering CHRG or on battery it starts from
 *     the calculated value limited by the one published before.
 *   battery.voltage is the instant value, not averaged.
 *
 *   Status: OB = OPBATTERY (confirmed on a mains cut: NOVINPUT and LOVINPUT
 *   agree) and then also DISCHRG; OL otherwise. CHRG = OL with MAXBATTERY
 *   clear (seen clear for the whole recharge after a cut; inferred, not
 *   verified). OVER = FOVERLOAD.
 *   LB = LOBATTERY or FENDBATTERY, and the bits of reg 0x0E (bits 4..7) go to
 *   ups.alarm; none of these bits has been observed set yet, so they come
 *   from the OEM bit names only. BOOST/TRIM are not reported (ACTUALTAP
 *   stayed at 3 on mains and on battery) and the beeper bit is unknown.
 *
 *   Detection: with the "family" option at its default ("auto") the driver
 *   first listens, writing nothing, for a checksum-valid frame at 2560 baud for
 *   a few seconds. If one arrives the unit is family 14; otherwise the termios
 *   state is put back and the family 10 poll runs as before.
 *
 *
 * Copyright (C) 2026  juslex
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
 */

#include "main.h"
#include "serial.h"
#include "nut_stdint.h"
#include "ragtech_speed.h"

#define DRIVER_NAME	"Ragtech UPS driver"
#define DRIVER_VERSION	"0.11"

upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"juslex",
	DRV_EXPERIMENTAL,
	{ NULL }
};

#define RAGTECH_SOF		0xAA
#define RAGTECH_CMD_READ	0x04
#define RAGTECH_CMD_WRITE	0x01
#define RAGTECH_CMD_AND		0x02
#define RAGTECH_CMD_OR		0x03
#define RAGTECH_MAIN_BASE	0x0080
#define RAGTECH_MAIN_LEN	30
#define RAGTECH_TIMEOUT_SEC	1
#define RAGTECH_TIMEOUT_USEC	0
#define RAGTECH_POST_OPEN_MS	200
#define RAGTECH_INTER_CMD_MS	100
#define RAGTECH_INIT_RETRIES	10	/* initial-poll attempts before giving up */

/* Opaque wake-up that the OEM firmware sends once before the first poll. */
static const uint8_t cmd_handshake[] = { 0xFF, 0xFE, 0x00, 0x8E, 0x01, 0x8F };

/* offsets into the 30-byte main range (buf[1..30] = reg 0x80..0x9D) */
#define OFF_R(reg)		(1 + ((reg) - 0x80))
#define OFF_F_FLAGS		OFF_R(0x80)
#define OFF_V_CBATTERY		OFF_R(0x87)
#define OFF_V_VBATTERY		OFF_R(0x8A)
#define OFF_V_VINPUT		OFF_R(0x8B)
#define OFF_V_IOUTPUT		OFF_R(0x8C)
#define OFF_V_POUTPUT		OFF_R(0x8D)
#define OFF_V_TEMPER		OFF_R(0x8E)
#define OFF_F_STATUS		OFF_R(0x90)
#define OFF_F_FAULT		OFF_R(0x91)
#define OFF_F_MISC		OFF_R(0x92)
#define OFF_V_FOUTPUT		OFF_R(0x97)
#define OFF_V_SHUTTIMER		OFF_R(0x98)
#define OFF_V_MODEL		OFF_R(0x9A)
#define OFF_V_VERSION		OFF_R(0x9B)
#define OFF_V_VOUTPUT		OFF_R(0x9D)

/* reg 0x80 bits */
#define B_AUTOSTART		0x01
#define B_ICBATTERY		0x04
#define B_LINESENS		0x80
/* reg 0x90 bits (status) */
#define S_NO_BATTERY		0x01
#define S_OLD_BATTERY		0x02
#define S_SELF_TEST		0x04
#define S_NO_INPUT		0x08
#define S_INPUT_LOW		0x10
#define S_INPUT_HIGH		0x20
#define S_ON_BATTERY		0x40
#define S_OVERLOAD_WARN		0x80
/* reg 0x91 bits (fault) */
#define F_BATTERY_LOW		0x01
#define F_OVERTEMP		0x02
#define F_BATTERY_DEPLETED	0x04
#define F_OVERLOAD		0x08
#define F_OUTPUT_ABNORMAL	0x10
#define F_BATTERY_ABNORMAL	0x20
#define F_INVERTER_FAIL		0x40
#define F_SHORT_CIRCUIT		0x80
/* reg 0x92 bits (misc) */
#define M_SYNC_IN		0x01
#define M_MORE_BAT		0x04
#define M_LESS_BAT		0x08
#define M_REMOTE_CTL		0x20

struct ragtech_model {
	uint8_t id;
	const char *name;
	unsigned int va;
	double imult;		/* V_IOUTPUT scaling */
	double vmult;		/* V_VOUTPUT scaling (0.555 for 115V TI, 1.030 for 220V M2) */
	double bmult;		/* V_VBATTERY scaling (0.067 for 12V, 0.134 for 24V) */
	unsigned int vin_nominal;
	unsigned int vout_nominal;
	double pf;		/* power factor (0.7 per OneUP Nitro 2000 / Easy 2000 TI datasheet;
				 * assumed shared across family 10 until per-model data arrives) */
};

/* family 10 -- order matches V_MODEL ids */
static const struct ragtech_model ragtech_models[] = {
	{  0, "Easy 600 TI",   600,  3.2000, 0.5550, 0.0670, 115, 115, 0.7 },
	{  1, "Easy 600 M2",   600,  1.7000, 1.0300, 0.0670, 220, 220, 0.7 },
	{  2, "Easy 700 TI",   700,  3.2000, 0.5550, 0.0670, 115, 115, 0.7 },
	{  3, "Easy 700 M2",   700,  2.3000, 1.0300, 0.0670, 220, 220, 0.7 },
	{  4, "Easy 900 TI",   900,  4.9400, 0.5550, 0.0670, 115, 115, 0.7 },
	{  5, "Easy 900 M2",   900,  2.6400, 1.0300, 0.0670, 220, 220, 0.7 },
	{  6, "Easy 1200 TI", 1200,  6.3700, 0.5550, 0.0670, 115, 115, 0.7 },
	{  7, "Easy 1200 M2", 1200,  3.4100, 1.0300, 0.0670, 220, 220, 0.7 },
	{  8, "Easy 1300 TI", 1300,  9.5600, 0.5550, 0.1340, 115, 115, 0.7 },
	{  9, "Easy 1300 M2", 1300,  5.1100, 1.0300, 0.1340, 220, 220, 0.7 },
	{ 10, "Easy 1400 TI", 1400, 11.1500, 0.5550, 0.1340, 115, 115, 0.7 },
	{ 11, "Easy 1400 M2", 1400,  5.9600, 1.0300, 0.1340, 220, 220, 0.7 },
	{ 12, "Easy 1600 TI", 1600, 12.8000, 0.5550, 0.1340, 115, 115, 0.7 },
	{ 13, "Easy 1600 M2", 1600,  6.8000, 1.0300, 0.1340, 220, 220, 0.7 },
	{ 14, "Easy 1800 TI", 1800, 14.4000, 0.5550, 0.1340, 115, 115, 0.7 },
	{ 15, "Easy 1800 M2", 1800,  7.6500, 1.0300, 0.1340, 220, 220, 0.7 },
	{ 16, "Easy 2000 TI", 2000, 15.9300, 0.5550, 0.1340, 115, 115, 0.7 },
	{ 17, "Easy 2000 M2", 2000,  8.5200, 1.0300, 0.1340, 220, 220, 0.7 },
	{ 18, "Easy 2200 TI", 2200, 17.5200, 0.5550, 0.1340, 115, 115, 0.7 },
	{ 19, "Easy 2200 M2", 2200,  9.3700, 1.0300, 0.1340, 220, 220, 0.7 },
};

/* per-session state populated by upsdrv_initinfo */
static const struct ragtech_model *model;
static struct ragtech_model fallback_model = {
	0xFF, "Unknown", 0, 6.3700, 0.5550, 0.0670, 115, 115, 0.7
};
static double iout_calib = 16.0;	/* read from reg 0xF3 at init */
static uint8_t osc53, osc57;		/* read from 0x202..0x203 at init */
static int shutdown_enabled;		/* opt-in via ups.conf "allow_shutdown" */
static unsigned int va_override = 0;	/* ups.conf "va" override (0 = use model table) */
static unsigned int effective_va = 0;	/* va_override if set, else model->va */

static const struct ragtech_model *find_model(uint8_t id)
{
	size_t i;
	for (i = 0; i < sizeof(ragtech_models) / sizeof(ragtech_models[0]); i++)
		if (ragtech_models[i].id == id)
			return &ragtech_models[i];
	return NULL;
}

/* Send a read for `count` registers starting at `addr` and collect the reply.
 * The reply is 1 + count bytes: SOF (0xAA) followed by count register bytes.
 * Returns count on success, -1 on failure. The SOF is consumed; `out` receives
 * only register bytes. */
static ssize_t ragtech_read(uint16_t addr, uint8_t count, uint8_t *out)
{
	uint8_t cmd[6];
	uint8_t scratch[64];
	const size_t reply_len = (size_t)count + 1;
	size_t total = 0;
	int got_sof = 0;
	int attempts = 0;
	ssize_t i, n;

	cmd[0] = RAGTECH_SOF;
	cmd[1] = RAGTECH_CMD_READ;
	cmd[2] = (addr >> 8) & 0xFF;
	cmd[3] = addr & 0xFF;
	cmd[4] = count;
	cmd[5] = (cmd[2] + cmd[3] + cmd[4]) & 0xFF;

	/* tcflush() -- ser_flush_in() does a select+read loop that misses
	 * bytes still queued inside the USB CDC layer under O_NONBLOCK */
	ser_flush_io(upsfd);
	upsdebug_hex(4, "TX", cmd, sizeof(cmd));
	if (ser_send_buf(upsfd, cmd, sizeof(cmd)) != (ssize_t)sizeof(cmd)) {
		upsdebugx(2, "ragtech_read(0x%04X,%u): short TX", addr, count);
		return -1;
	}
	tcdrain(upsfd);
	usleep(50000);	/* let the UPS firmware queue the reply */

	while (total < (size_t)count && attempts++ < 10) {
		/* ser_get_buf returns what it manages to read in one select+read;
		 * ser_get_buf_len would discard partial reads on timeout, which
		 * happens here because the USB CDC layer fragments the reply. */
		n = ser_get_buf(upsfd, scratch, sizeof(scratch),
				RAGTECH_TIMEOUT_SEC, RAGTECH_TIMEOUT_USEC);
		if (n <= 0)
			break;
		upsdebug_hex(4, "RX chunk", scratch, (size_t)n);

		for (i = 0; i < n; i++) {
			if (!got_sof) {
				if (scratch[i] == RAGTECH_SOF)
					got_sof = 1;
				continue;
			}
			if (total < (size_t)count)
				out[total++] = scratch[i];
		}
	}

	if (total < (size_t)count) {
		upsdebugx(2, "ragtech_read(0x%04X,%u): short RX (%zu/%u)",
			  addr, count, total, count);
		return -1;
	}
	(void)reply_len;
	return (ssize_t)count;
}

/* Send a 6-byte write-style command (opcode + addr + value). Returns 0 on
 * success, -1 on TX failure. The UPS does not appear to ack; we drain
 * whatever comes back. */
static int ragtech_send_op(uint8_t opcode, uint16_t addr, uint8_t value)
{
	uint8_t cmd[6];
	uint8_t scratch[16];

	cmd[0] = RAGTECH_SOF;
	cmd[1] = opcode;
	cmd[2] = (addr >> 8) & 0xFF;
	cmd[3] = addr & 0xFF;
	cmd[4] = value;
	cmd[5] = (cmd[2] + cmd[3] + cmd[4]) & 0xFF;

	ser_flush_io(upsfd);
	upsdebug_hex(4, "WR", cmd, sizeof(cmd));
	if (ser_send_buf(upsfd, cmd, sizeof(cmd)) != (ssize_t)sizeof(cmd))
		return -1;
	tcdrain(upsfd);
	/* Best-effort drain of any echo / ack the firmware may send. */
	usleep(50000);
	ser_get_buf(upsfd, scratch, sizeof(scratch), 0, 100000);
	return 0;
}

/* Convenience wrappers. */
static int ragtech_write_reg(uint16_t addr, uint8_t value)
{
	return ragtech_send_op(RAGTECH_CMD_WRITE, addr, value);
}
static int ragtech_and_reg(uint16_t addr, uint8_t mask)
{
	/* reg = reg & mask (atomic on the firmware side). */
	return ragtech_send_op(RAGTECH_CMD_AND, addr, mask);
}
static int ragtech_or_reg(uint16_t addr, uint8_t mask)
{
	/* reg = reg | mask (atomic on the firmware side). */
	return ragtech_send_op(RAGTECH_CMD_OR, addr, mask);
}

/* Clear bits using the atomic AND opcode. */
static int ragtech_clear_bits(uint16_t addr, uint8_t bits)
{
	return ragtech_and_reg(addr, (uint8_t)~bits);
}

/* Set bits using the atomic OR opcode. */
static int ragtech_set_bits(uint16_t addr, uint8_t bits)
{
	return ragtech_or_reg(addr, bits);
}

static int instcmd(const char *cmdname, const char *extra)
{
	NUT_UNUSED_VARIABLE(extra);

	/* All shutdown / test instcmds are physically destructive on a UPS
	 * that does not auto-restart, so they are gated behind an explicit
	 * opt-in in ups.conf. */
	if (!shutdown_enabled
	 && (!strcasecmp(cmdname, "shutdown.return")
	  || !strcasecmp(cmdname, "shutdown.stayoff")
	  || !strcasecmp(cmdname, "shutdown.stop")
	  || !strcasecmp(cmdname, "test.battery.start.deep"))) {
		upslogx(LOG_WARNING,
			"%s refused: set 'allow_shutdown' in ups.conf to enable. "
			"WARNING: this UPS firmware does NOT auto-restart after a "
			"coordinated shutdown; the operator must press the power "
			"button when mains return.", cmdname);
		return STAT_INSTCMD_INVALID;
	}

	if (!strcasecmp(cmdname, "shutdown.return")) {
		/* The Easy 2000 TI firmware's shutdown state machine only fires
		 * when F_AUTOSTART is *cleared* via the atomic AND opcode (0x02);
		 * setting it via OR (0x03) followed by writing V_SHUTDOWNTIMER
		 * has been verified to leave the UPS idle. The OEM XML action is
		 *
		 *     shutdown = "F_AUTOSTART=$notmanual; V_SHUTDOWNTIMER=$counter"
		 *
		 * and the OEM `supsvc` always passes `$notmanual = 0` on user-
		 * initiated shutdown, which means real-world traffic only ever
		 * exercises the stayoff variant. Treat shutdown.return as a
		 * stayoff with a warning so upsmon's behaviour is consistent: the
		 * output is cut on schedule, but the operator must press the
		 * power button when mains come back. */
		upslogx(LOG_WARNING,
			"shutdown.return: firmware does not implement auto-restart; "
			"falling back to shutdown.stayoff (manual power-on required)");
		if (ragtech_clear_bits(0x0080, B_AUTOSTART) < 0)
			return STAT_INSTCMD_FAILED;
		if (ragtech_write_reg(0x0098, 30) < 0)
			return STAT_INSTCMD_FAILED;
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "shutdown.stayoff")) {
		/* OEM-confirmed sequence: AND 0x80 with 0xFE (clear F_AUTOSTART),
		 * then write V_SHUTDOWNTIMER. UPS stays off until power button. */
		if (ragtech_clear_bits(0x0080, B_AUTOSTART) < 0)
			return STAT_INSTCMD_FAILED;
		if (ragtech_write_reg(0x0098, 30) < 0)
			return STAT_INSTCMD_FAILED;
		upslogx(LOG_NOTICE, "shutdown.stayoff: UPS will cut output in ~30s and stay off");
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "shutdown.stop")) {
		/* Setting V_SHUTDOWNTIMER back to 0 aborts the countdown. */
		if (ragtech_write_reg(0x0098, 0) < 0)
			return STAT_INSTCMD_FAILED;
		upslogx(LOG_NOTICE, "shutdown.stop: countdown aborted");
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "test.battery.start.deep")) {
		/* devices.xml: fullDischarge = F_OLDBAT=0; F_OPCHECKUP=1; F_TRANSFINV=1 */
		if (ragtech_clear_bits(0x0090, S_OLD_BATTERY) < 0
		 || ragtech_set_bits(0x0090, S_SELF_TEST) < 0
		 || ragtech_set_bits(0x0095, 0x80) < 0)
			return STAT_INSTCMD_FAILED;
		upslogx(LOG_NOTICE, "test.battery.start.deep: full discharge cycle initiated");
		return STAT_INSTCMD_HANDLED;
	}

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s]", cmdname);
	return STAT_INSTCMD_UNKNOWN;
}

static double compute_frequency(uint8_t v_fout)
{
	if (osc57 == osc53)
		return 0.0;
	return (53.0 + 4.0 * ((double)v_fout - (double)osc53)
			/ ((double)osc57 - (double)osc53)) * 0.9806 + 1.46;
}

/* ------------------------------------------------------------------------
 * Family 14 (Easy Way, "NEW/QUADRI") -- push-only, read-only.
 * See the protocol notes in the header comment. Nothing here ever writes
 * to the UPS.
 * ------------------------------------------------------------------------ */

#define F14_SYNC0		0x55
#define F14_SYNC1		0x55
#define F14_SYNC2		0xAE
#define F14_FRAME_LEN		32	/* 3 sync + 27 registers + tail + checksum */
#define F14_NREGS		27
#define F14_BUF_LEN		256	/* receive window; keeps only the newest bytes */
#define F14_DETECT_MS		3000U	/* passive listen for family auto-detection */
#define F14_BAUD		2560U	/* line speed of the Easy Way UART */
#define F14_FIRST_SEC		15	/* wait for the first accepted frame (needs two valid ones) */
#define F14_NCONST		10	/* regs 0x00..0x09 are fixed in the unit */
#define F14_REF_RETRY_SEC	30	/* mismatching this long: allow a new reference */
#define F14_LOAD_MAX_PCT	150	/* plausibility limits, see header comment */
#define F14_TEMP_MAX		100
#define F14_VBAT_MIN		6.0
#define F14_VBAT_MAX		18.0
#define F14_STALE_SEC		5	/* no valid frame for this long: data stale */
#define F14_DRAIN_MAX		64	/* safety bound for the drain loop */

/* register indexes inside the 27-byte block */
#define R14_VERSION		0x00
#define R14_MODEL		0x01
#define R14_OSC53		0x02
#define R14_OSC57		0x03
#define R14_VBATCALIB		0x05
#define R14_VINCALIB		0x06
#define R14_VOUTCALIB		0x07
#define R14_IOUTCALIB		0x09
#define R14_VINSEL		0x0A	/* bit2: 220 V selected */
#define R14_MISC		0x0C	/* bit7: MAXBATTERY (battery full) */
#define R14_INPUT		0x0D	/* bit0: NOVINPUT */
#define R14_FAULT		0x0E
#define R14_WARN		0x0F	/* bit6: LOBATTERY */
#define R14_OP			0x10	/* bit3: OPBATTERY, bit4: OPSTANDBY */
#define R14_TAP			0x11
#define R14_VINPUT		0x12
#define R14_VOUTPUT		0x13
#define R14_IOUTPUT		0x14
#define R14_VBATTERY		0x15
#define R14_FOUTPUT		0x17
#define R14_TEMPER		0x1A

#define B14_VINSEL220		0x04
#define B14_MAXBATTERY		0x80
#define B14_NOVINPUT		0x01
#define B14_FENDBATTERY		0x02
#define B14_FOVERLOAD		0x04
#define B14_FSHORTCIRCUIT	0x10
#define B14_FABNORMALVOUT	0x20
#define B14_FABNORMALVBAT	0x40
#define B14_FINVERTER		0x80
#define B14_LOBATTERY		0x40
#define B14_OPBATTERY		0x08
#define B14_OPSTANDBY		0x10

#define F14_VBAT_FACTOR		13.8	/* C_VBATNOMINAL * 13.8 / 12 with 12 V nominal */
#define F14_BAT_EMPTY		0.725	/* C_BATFACTOR */
#define F14_VEC_FACTOR		0.05	/* C_VECFACTOR (TI, 220 V strap) */
#define F14_CHG_ALPHA		0.25	/* weight of a new frame in the charge average */

enum f14_variant { F14_TI = 0, F14_M1, F14_M2 };

struct f14_model {
	uint8_t id;
	const char *name;
	unsigned int pnom;	/* C_POUTNOMINAL, same unit convention as family 10 (VA) */
	enum f14_variant variant;
};

/* OEM table, family 14 -- ids are V_MODEL */
static const struct f14_model f14_models[] = {
	{  0, "Easy Way 500 TI",   500, F14_TI },
	{  1, "Easy Way 500 M1",   500, F14_M1 },
	{  2, "Easy Way 500 M2",   500, F14_M2 },
	{  3, "Easy Way 600 TI",   600, F14_TI },
	{  4, "Easy Way 600 M1",   600, F14_M1 },
	{  5, "Easy Way 600 M2",   600, F14_M2 },
	{  6, "Easy Way 700 TI",   700, F14_TI },
	{  7, "Easy Way 700 M1",   700, F14_M1 },
	{  8, "Easy Way 700 M2",   700, F14_M2 },
	{  9, "Easy Way 1200 TI", 1200, F14_TI },
	{ 10, "Easy Way 1200 M1", 1200, F14_M1 },
	{ 11, "Easy Way 1200 M2", 1200, F14_M2 },
	{ 12, "Easy Way 1300 TI", 1300, F14_TI },
	{ 13, "Easy Way 1300 M1", 1300, F14_M1 },
	{ 14, "Easy Way 1300 M2", 1300, F14_M2 },
	{ 15, "Easy Way 1400 TI", 1400, F14_TI },
	{ 16, "Easy Way 1400 M1", 1400, F14_M1 },
	{ 17, "Easy Way 1400 M2", 1400, F14_M2 },
	{ 18, "Easy Way 1600 TI", 1600, F14_TI },
	{ 19, "Easy Way 1600 M1", 1600, F14_M1 },
	{ 20, "Easy Way 1600 M2", 1600, F14_M2 },
};

/* output voltage tap factors (ACTUALTAP 0..3); TI and M1 share the first row */
static const double f14_tap_ti[4] = { 1.2898, 1.1558, 1.0172, 0.8812 };
static const double f14_tap_m2[4] = { 1.2460, 1.1620, 1.0705, 0.9998 };

/* What the battery is doing, for the charge estimate (see the header) */
enum f14_chg_state { F14_CHG_NONE = 0, F14_CHG_FULL, F14_CHG_CHRG, F14_CHG_OB };

/* Decoded values of one frame */
struct f14_data {
	double vin, vout, fout, load, vbat, cbat;
	unsigned int temp;
	unsigned int vin_nominal;	/* 115 or 220 */
	int ob, chrg, lb, over;
	unsigned int faults;		/* reg 0x0E, bits 4..7 only */
};

/* fallback profile for an id missing from the table: TI, nominal power unknown */
static const struct f14_model f14_fallback = { 0xFF, "Unknown", 0, F14_TI };

static int family_opt = 0;		/* ups.conf "family": 0 auto, 10, 14 */
static int family = 10;			/* family in use after initups */
static const struct f14_model *f14_model;
static uint8_t f14_buf[F14_BUF_LEN];	/* newest received bytes */
static size_t f14_len;
static uint8_t f14_regs[F14_NREGS];	/* registers of the newest ACCEPTED frame */
static int f14_have;			/* a frame was accepted at least once */
static time_t f14_last;			/* when the newest one was accepted */
static unsigned int f14_accepted;	/* number of frames accepted so far */
static uint8_t f14_ref[F14_NCONST];	/* reference for regs 0x00..0x09 */
static int f14_ref_ok;
static time_t f14_ref_last;		/* newest frame that matched the reference */
static uint8_t f14_cand[F14_NCONST];	/* regs 0x00..0x09 of the last odd frame */
static int f14_cand_have;
static unsigned int f14_rare_pub;	/* debounced rare indications (F14_RARE_*) */
static unsigned int f14_rare_prev;	/* the same, as seen in the previous frame */
static int f14_rare_prev_ok;
static double f14_chg_avg;		/* running average of the calculated charge */
static double f14_chg_pub;		/* battery.charge as published */
static enum f14_chg_state f14_chg_st;	/* state the two above belong to */
static int f14_line_ok;			/* the line is really at F14_BAUD */
#ifndef WIN32
static struct termios f14_tio_saved;	/* termios before the speed change */
static int f14_tio_saved_ok;
#endif	/* !WIN32 */

/* Rare indications that are debounced: reg 0x0E bits 4..7 (kept in place)
 * and these two. */
#define F14_RARE_OVER		0x100
#define F14_RARE_LB		0x200
#define F14_RARE_ALL		(0xF0U | F14_RARE_OVER | F14_RARE_LB)

static const struct f14_model *f14_find_model(uint8_t id)
{
	size_t i;
	for (i = 0; i < sizeof(f14_models) / sizeof(f14_models[0]); i++)
		if (f14_models[i].id == id)
			return &f14_models[i];
	return NULL;
}

/* 1 if f points at a whole valid frame: sync bytes and 8-bit sum */
static int f14_frame_ok(const uint8_t *f)
{
	unsigned int sum = 0;
	size_t i;

	if (f[0] != F14_SYNC0 || f[1] != F14_SYNC1 || f[2] != F14_SYNC2)
		return 0;
	for (i = 0; i < F14_FRAME_LEN - 1; i++)
		sum += f[i];
	return (sum & 0xFF) == f[F14_FRAME_LEN - 1];
}

/* Append received bytes to the window, dropping the oldest when full. */
static void f14_append(const uint8_t *p, size_t n)
{
	size_t drop;

	if (n >= F14_BUF_LEN) {
		p += n - F14_BUF_LEN;
		n = F14_BUF_LEN;
	}
	if (f14_len + n > F14_BUF_LEN) {
		drop = f14_len + n - F14_BUF_LEN;
		memmove(f14_buf, f14_buf + drop, f14_len - drop);
		f14_len -= drop;
	}
	memcpy(f14_buf + f14_len, p, n);
	f14_len += n;
}

/* Publish the static data of the unit identified by the registers r. */
static void f14_set_static(const uint8_t *r)
{
	f14_model = f14_find_model(r[R14_MODEL]);
	if (!f14_model) {
		upslogx(LOG_WARNING, "unknown family 14 model id 0x%02X, using a generic profile",
			r[R14_MODEL]);
		f14_model = &f14_fallback;
	}
	dstate_setinfo("ups.model", "%s", f14_model->name);
	dstate_setinfo("ups.firmware", "%.1f", r[R14_VERSION] * 0.1);
	if (f14_model->pnom)
		dstate_setinfo("ups.power.nominal", "%u", f14_model->pnom);
	dstate_setinfo("output.voltage.nominal", "%u",
		f14_model->variant == F14_M2 ? 220U : 115U);
	dstate_setinfo("output.frequency.nominal", "%u", 60U);
	dstate_setinfo("battery.voltage.nominal", "%u", 12U);
}

/* NULL if the values of a frame are plausible, else what is wrong with it.
 * The limits are generous: they only exist to throw out corrupted frames that
 * got past the 8-bit sum, not to police the UPS. Status bits are not judged. */
static const char *f14_implausible(const uint8_t *r)
{
	double v;

	if (r[R14_TAP] > 3)
		return "ACTUALTAP above 3";
	if (r[R14_IOUTCALIB]
	    && (unsigned int)r[R14_IOUTPUT] * 100U
	       > (unsigned int)F14_LOAD_MAX_PCT * r[R14_IOUTCALIB])
		return "load above 150 %";
	if (r[R14_TEMPER] > F14_TEMP_MAX)
		return "temperature above 100 C";
	if (r[R14_VBATCALIB]) {
		v = (double)r[R14_VBATTERY] / r[R14_VBATCALIB] * F14_VBAT_FACTOR;
		if (v < F14_VBAT_MIN || v > F14_VBAT_MAX)
			return "battery voltage outside 6..18 V";
	}
	return NULL;
}

/* The rare indications of a frame, as F14_RARE_* bits. */
static unsigned int f14_rare_bits(const uint8_t *r)
{
	unsigned int b = r[R14_FAULT] & 0xF0U;

	if (r[R14_FAULT] & B14_FOVERLOAD)
		b |= F14_RARE_OVER;
	if ((r[R14_WARN] & B14_LOBATTERY) || (r[R14_FAULT] & B14_FENDBATTERY))
		b |= F14_RARE_LB;
	return b;
}

/* Charge calculated from the battery voltage of one frame, 0..100. */
static double f14_raw_charge(const uint8_t *r)
{
	double c;

	if (!r[R14_VBATCALIB])
		return 0.0;
	c = ((double)r[R14_VBATTERY] / r[R14_VBATCALIB] - F14_BAT_EMPTY)
		/ (1.0 - F14_BAT_EMPTY) * 100.0;
	if (c < 0.0)
		c = 0.0;
	if (c > 100.0)
		c = 100.0;
	return c;
}

/* Advance the battery.charge estimate by one accepted frame. */
static void f14_charge_step(const uint8_t *r)
{
	double raw = f14_raw_charge(r);
	enum f14_chg_state st;

	if (r[R14_OP] & B14_OPBATTERY)
		st = F14_CHG_OB;
	else if (r[R14_MISC] & B14_MAXBATTERY)
		st = F14_CHG_FULL;
	else
		st = F14_CHG_CHRG;

	if (st != f14_chg_st) {
		/* restart from this frame; the value shown may only move the
		 * way the status says (nothing to limit it by the first time) */
		f14_chg_avg = raw;
		if (f14_chg_st == F14_CHG_NONE)
			f14_chg_pub = raw;
		else if (st == F14_CHG_OB && f14_chg_pub > raw)
			f14_chg_pub = raw;
		else if (st == F14_CHG_CHRG && f14_chg_pub < raw)
			f14_chg_pub = raw;
		f14_chg_st = st;
	} else {
		f14_chg_avg += F14_CHG_ALPHA * (raw - f14_chg_avg);
		if (st == F14_CHG_OB && f14_chg_avg < f14_chg_pub)
			f14_chg_pub = f14_chg_avg;
		else if (st == F14_CHG_CHRG && f14_chg_avg > f14_chg_pub)
			f14_chg_pub = f14_chg_avg;
	}
	if (st == F14_CHG_FULL)
		f14_chg_pub = 100.0;
}

/* One checksum-valid frame (r = its 27 registers) arrived: accept it as the
 * new current data, or drop it. Returns 1 if accepted. */
static int f14_accept(const uint8_t *r)
{
	time_t now = time(NULL);
	const char *why;
	unsigned int rare, agree;

	if (!f14_ref_ok) {
		/* The first frame is only remembered; a second one with the
		 * same constants makes them the reference. */
		if (!f14_cand_have || memcmp(f14_cand, r, F14_NCONST)) {
			memcpy(f14_cand, r, F14_NCONST);
			f14_cand_have = 1;
			upsdebugx(2, "family 14: waiting for a second frame with the same constants");
			return 0;
		}
		memcpy(f14_ref, r, F14_NCONST);
		f14_ref_ok = 1;
		f14_ref_last = now;
		f14_cand_have = 0;
		upsdebugx(1, "family 14: constant registers confirmed by two frames");
	} else if (memcmp(f14_ref, r, F14_NCONST) != 0) {
		/* Corrupted, unless the reference itself was wrong or the unit
		 * changed: that is told by two consecutive frames agreeing on
		 * something else for a good while. */
		if (f14_cand_have && !memcmp(f14_cand, r, F14_NCONST)
		    && now - f14_ref_last > F14_REF_RETRY_SEC) {
			memcpy(f14_ref, r, F14_NCONST);
			f14_ref_last = now;
			f14_cand_have = 0;
			upslogx(LOG_NOTICE, "family 14: the constant registers changed "
				"and stayed so; taking them as the new reference");
			f14_set_static(r);
		} else {
			memcpy(f14_cand, r, F14_NCONST);
			f14_cand_have = 1;
			upsdebugx(2, "family 14: frame dropped, registers 0x00..0x09 "
				"differ from the reference");
			return 0;
		}
	} else {
		f14_ref_last = now;
		f14_cand_have = 0;
	}

	why = f14_implausible(r);
	if (why) {
		upsdebugx(2, "family 14: frame dropped, %s", why);
		return 0;
	}

	/* A rare indication changes only when two accepted frames in a row
	 * agree on it. */
	rare = f14_rare_bits(r);
	if (f14_rare_prev_ok) {
		agree = ~(rare ^ f14_rare_prev) & F14_RARE_ALL;
		f14_rare_pub = (f14_rare_pub & ~agree) | (rare & agree);
	}
	f14_rare_prev = rare;
	f14_rare_prev_ok = 1;

	memcpy(f14_regs, r, F14_NREGS);
	f14_charge_step(r);
	f14_have = 1;
	f14_last = now;
	f14_accepted++;
	return 1;
}

/* Go through the window in order: every checksum-valid frame is offered to
 * f14_accept() and removed; bytes that cannot start a frame any more are
 * dropped, a partial frame at the end is kept. Returns how many
 * checksum-valid frames there were (accepted or not; f14_accepted tells). */
static int f14_take(void)
{
	size_t start = 0;
	int seen = 0;

	while (start + F14_FRAME_LEN <= f14_len) {
		if (f14_frame_ok(f14_buf + start)) {
			seen++;
			(void)f14_accept(f14_buf + start + 3);
			start += F14_FRAME_LEN;
		} else {
			start++;
		}
	}
	if (start) {
		memmove(f14_buf, f14_buf + start, f14_len - start);
		f14_len -= start;
	}
	return seen;
}

/* Read what is pending (without waiting) into the window. */
static void f14_drain(void)
{
	uint8_t tmp[64];
	ssize_t n;
	int i;

	for (i = 0; i < F14_DRAIN_MAX; i++) {
		n = ser_get_buf(upsfd, tmp, sizeof(tmp), 0, 0);
		if (n <= 0)
			break;
		upsdebug_hex(5, "RX14", tmp, (size_t)n);
		f14_append(tmp, (size_t)n);
	}
}

/* Listen up to `ms` milliseconds, returning as soon as a checksum-valid
 * frame is in (that is all the auto-detection needs). */
static int f14_listen(unsigned int ms)
{
	uint8_t tmp[64];
	ssize_t n;
	struct timeval start, now;
	long elapsed_ms;

	gettimeofday(&start, NULL);
	for (;;) {
		n = ser_get_buf(upsfd, tmp, sizeof(tmp), 0, 100000);
		if (n > 0) {
			upsdebug_hex(5, "RX14", tmp, (size_t)n);
			f14_append(tmp, (size_t)n);
			if (f14_take())
				return 1;
		}
		gettimeofday(&now, NULL);
		elapsed_ms = (long)(now.tv_sec - start.tv_sec) * 1000L
			   + (long)(now.tv_usec - start.tv_usec) / 1000L;
		if (elapsed_ms >= (long)ms)
			return 0;
	}
}

/* Forget everything learned from the stream (the family 10 fallback). */
static void f14_reset(void)
{
	f14_len = 0;
	f14_have = 0;
	f14_ref_ok = 0;
	f14_cand_have = 0;
	f14_rare_pub = 0;
	f14_rare_prev = 0;
	f14_rare_prev_ok = 0;
	f14_chg_st = F14_CHG_NONE;
}

/* Put the port at F14_BAUD. The termios state before the change is kept so
 * that the family 10 path can be given back exactly what it had. If the speed
 * cannot be applied the port goes to 2400 and f14_line_ok stays 0; the warning
 * is logged only once the unit is known to be family 14. */
static void f14_set_line(void)
{
#ifndef WIN32
	struct termios tio;

	if (tcgetattr(upsfd, &tio) != 0)
		return;
	f14_tio_saved = tio;
	f14_tio_saved_ok = 1;
	if (ragtech_set_baud(upsfd, F14_BAUD) == 0) {
		f14_line_ok = 1;
		upsdebugx(1, "family 14: line set to %u baud", F14_BAUD);
		return;
	}
	upsdebugx(1, "family 14: could not set %u baud, using 2400", F14_BAUD);
	cfsetispeed(&tio, B2400);
	cfsetospeed(&tio, B2400);
	tcsetattr(upsfd, TCSANOW, &tio);
#endif	/* !WIN32 */
}

static void f14_restore_line(void)
{
#ifndef WIN32
	if (f14_tio_saved_ok) {
		tcsetattr(upsfd, TCSANOW, &f14_tio_saved);
		ragtech_restore_baud(upsfd);
	}
	f14_line_ok = 0;
#endif	/* !WIN32 */
}

/* Convert the 27 registers of one frame. Formulas as in the OEM table. */
static void f14_decode(const uint8_t *r, const struct f14_model *m,
	struct f14_data *d)
{
	const double *tap = (m->variant == F14_M2) ? f14_tap_m2 : f14_tap_ti;
	int novin = (r[R14_INPUT] & B14_NOVINPUT) != 0;
	int opbat = (r[R14_OP] & B14_OPBATTERY) != 0;
	int standby = (r[R14_OP] & B14_OPSTANDBY) != 0;
	int sel220 = (r[R14_VINSEL] & B14_VINSEL220) != 0;
	double ratio;

	memset(d, 0, sizeof(*d));

	/* input voltage */
	if (!novin && r[R14_VINPUT] && r[R14_VINCALIB]) {
		ratio = (double)r[R14_VINPUT] / r[R14_VINCALIB];
		if (m->variant != F14_TI || !sel220 || opbat)
			d->vin = ratio * 115.0;
		else if (r[R14_IOUTCALIB])
			d->vin = ratio * 230.0 * ((double)r[R14_IOUTPUT]
				/ r[R14_IOUTCALIB] * F14_VEC_FACTOR + 1.0);
	}
	if (m->variant == F14_M2)
		d->vin_nominal = 220;
	else if (m->variant == F14_TI && sel220)
		d->vin_nominal = 220;
	else
		d->vin_nominal = 115;

	/* output voltage: tap index beyond 3 gives 0 */
	if (r[R14_VOUTCALIB] && !standby && r[R14_TAP] < 4)
		d->vout = (double)r[R14_VOUTPUT] / r[R14_VOUTCALIB]
			* tap[r[R14_TAP]] * 115.0;

	/* output frequency; OSC57 may be below OSC53 */
	if (r[R14_OSC57] != r[R14_OSC53])
		d->fout = (53.0 + 4.0 * ((double)r[R14_FOUTPUT] - r[R14_OSC53])
			/ ((double)r[R14_OSC57] - r[R14_OSC53])) * 0.9806 + 1.46;

	/* load, battery */
	if (r[R14_IOUTCALIB])
		d->load = (double)r[R14_IOUTPUT] / r[R14_IOUTCALIB] * 100.0;
	if (r[R14_VBATCALIB]) {
		ratio = (double)r[R14_VBATTERY] / r[R14_VBATCALIB];
		d->vbat = ratio * F14_VBAT_FACTOR;
	}
	d->cbat = f14_raw_charge(r);
	d->temp = r[R14_TEMPER];

	d->ob = opbat;
	d->chrg = !opbat && !(r[R14_MISC] & B14_MAXBATTERY);
	d->lb = (r[R14_WARN] & B14_LOBATTERY) || (r[R14_FAULT] & B14_FENDBATTERY);
	d->over = (r[R14_FAULT] & B14_FOVERLOAD) != 0;
	d->faults = r[R14_FAULT] & (B14_FSHORTCIRCUIT | B14_FABNORMALVOUT
				  | B14_FABNORMALVBAT | B14_FINVERTER);
}

/* Values of the newest accepted frame, with the rare indications as
 * debounced instead of as in the frame itself, and the charge as smoothed. */
static void f14_current(struct f14_data *d)
{
	f14_decode(f14_regs, f14_model, d);
	d->faults = f14_rare_pub & 0xF0U;
	d->over = (f14_rare_pub & F14_RARE_OVER) != 0;
	d->lb = (f14_rare_pub & F14_RARE_LB) != 0;
	d->cbat = f14_chg_pub;
}

static void f14_publish(const struct f14_data *d)
{
	dstate_setinfo("input.voltage",         "%.1f", d->vin);
	dstate_setinfo("input.voltage.nominal", "%u",   d->vin_nominal);
	dstate_setinfo("output.voltage",        "%.1f", d->vout);
	dstate_setinfo("output.frequency",      "%.2f", d->fout);
	dstate_setinfo("ups.load",              "%.1f", d->load);
	dstate_setinfo("battery.voltage",       "%.2f", d->vbat);
	dstate_setinfo("battery.charge",        "%.1f", d->cbat);
	dstate_setinfo("ups.temperature",       "%u",   d->temp);

	alarm_init();
	if (d->faults & B14_FSHORTCIRCUIT)
		alarm_set("Short circuit");
	if (d->faults & B14_FABNORMALVOUT)
		alarm_set("Abnormal output voltage");
	if (d->faults & B14_FABNORMALVBAT)
		alarm_set("Abnormal battery voltage");
	if (d->faults & B14_FINVERTER)
		alarm_set("Inverter fault");
	alarm_commit();

	status_init();
	status_set(d->ob ? "OB" : "OL");
	if (d->ob)
		status_set("DISCHRG");
	if (d->chrg)
		status_set("CHRG");
	if (d->lb)
		status_set("LB");
	if (d->over)
		status_set("OVER");
	status_commit();
}

/* Static data, once, from the first accepted frame. */
static void f14_initinfo(void)
{
	struct f14_data d;
	time_t t0 = time(NULL);

	dstate_setinfo("ups.mfr", "%s", "Ragtech");
	dstate_setinfo("ups.model", "%s", "Unknown");

	if (shutdown_enabled)
		upslogx(LOG_WARNING,
			"allow_shutdown is ignored on family 14: no shutdown "
			"command is known for these units, the driver is read-only");

	if (!f14_line_ok)
		upslogx(LOG_WARNING,
			"could not set %u baud; falling back to 2400, expect "
			"corrupted frames", F14_BAUD);

	/* An accepted frame needs the constants confirmed by two frames. */
	while (!f14_have) {
		f14_drain();
		f14_take();
		if (f14_have)
			break;
		if (time(NULL) - t0 > F14_FIRST_SEC)
			fatalx(EXIT_FAILURE,
				"no acceptable family 14 frame received in %d s -- is the UPS "
				"connected and not held by another program (e.g. Ragtech supsvc)? "
				"Corrupted frames mean a wrong line speed (it must be %u baud)",
				F14_FIRST_SEC, F14_BAUD);
		usleep(100000);
	}

	f14_set_static(f14_regs);

	upslogx(LOG_INFO, "family 14 (Easy Way, read-only): %s, firmware %.1f",
		f14_model->name, f14_regs[R14_VERSION] * 0.1);

	/* first values, from the frame that completed the identification */
	f14_current(&d);
	f14_publish(&d);
}

static void f14_updateinfo(void)
{
	struct f14_data d;
	unsigned int before = f14_accepted;

	f14_drain();
	f14_take();

	if (f14_accepted == before
	    && (!f14_have || time(NULL) - f14_last > F14_STALE_SEC)) {
		upsdebugx(2, "family 14: no acceptable frame for more than %d s", F14_STALE_SEC);
		dstate_datastale();
		return;
	}

	if (f14_accepted != before) {
		f14_current(&d);
		f14_publish(&d);
	}
	dstate_dataok();
}

void upsdrv_initinfo(void)
{
	uint8_t reply[64];
	uint8_t osc[2];
	uint8_t calib;
	int tries;

	if (family == 14) {
		f14_initinfo();
		return;
	}

	dstate_setinfo("ups.mfr", "%s", "Ragtech");
	dstate_setinfo("ups.model", "%s", "Unknown");

	/* Retry the wake-up handshake and first poll a few times before bailing.
	 * Right after a cold boot the CDC-ACM device may have only just been
	 * enumerated and the firmware can need a moment to answer the first read.
	 * Retrying here lets the driver come up cleanly when started by the stock
	 * NUT systemd units (which launch it as soon as the device node appears),
	 * instead of relying on a fatal exit plus a Restart=on-failure cycle to
	 * eventually recover -- which is noisy and only retries on a 5 s cadence. */
	for (tries = 0; tries < RAGTECH_INIT_RETRIES; tries++) {
		if (ser_send_buf(upsfd, cmd_handshake, sizeof(cmd_handshake))
		    != (ssize_t)sizeof(cmd_handshake)) {
			upslogx(LOG_WARNING, "handshake TX failed");
		}
		tcdrain(upsfd);

		/* The firmware answers the handshake with a couple of bytes (e.g.
		 * 0xCA). Some models are half-duplex and silently drop the following
		 * command if it arrives while that reply is still being transmitted,
		 * which surfaces as "no reply to initial status poll". Drain the
		 * handshake reply (short per-chunk timeout) so the first read is only
		 * sent once the UPS is idle again. Units that do not answer the
		 * handshake just time out here. */
		{
			uint8_t hs[16];
			int i;
			for (i = 0; i < 4; i++) {
				if (ser_get_buf(upsfd, hs, sizeof(hs), 0,
						RAGTECH_INTER_CMD_MS * 1000) <= 0)
					break;
			}
		}

		if (ragtech_read(RAGTECH_MAIN_BASE, RAGTECH_MAIN_LEN, reply) >= 0)
			break;

		upsdebugx(1, "initial status poll attempt %d/%d got no reply",
			  tries + 1, RAGTECH_INIT_RETRIES);
		usleep(RAGTECH_POST_OPEN_MS * 1000);
	}
	if (tries == RAGTECH_INIT_RETRIES)
		fatalx(EXIT_FAILURE, "no reply to initial status poll -- is the UPS connected and not held by another program (e.g. Ragtech supsvc)?");

	model = find_model(reply[OFF_V_MODEL - 1]);
	if (!model) {
		upslogx(LOG_WARNING, "unknown model id 0x%02X, using generic 115V profile",
			reply[OFF_V_MODEL - 1]);
		model = &fallback_model;
	}
	dstate_setinfo("ups.model", "%s", model->name);

	/* Some product lines (e.g. the GT series) reuse a family-10 model id, so
	 * the id byte alone cannot tell a 2200 TI from a 3200 GT. When the user
	 * knows the real VA rating they can set "va" in ups.conf; that value then
	 * drives ups.power.nominal, ups.realpower.nominal and the computed load. */
	effective_va = va_override ? va_override : model->va;
	if (effective_va) {
		dstate_setinfo("ups.power.nominal",     "%u", effective_va);
		dstate_setinfo("ups.realpower.nominal", "%u",
			       (unsigned int)(effective_va * model->pf + 0.5));
	}
	{
		double v_in_now = reply[OFF_V_VINPUT - 1] * 1.0600;
		dstate_setinfo("input.voltage.nominal", "%u",
			(unsigned int)(v_in_now > 150.0 ? 220 : 115));
	}
	dstate_setinfo("output.voltage.nominal", "%u", model->vout_nominal);
	dstate_setinfo("battery.voltage.nominal", "%u",
		(unsigned int)(model->bmult > 0.1 ? 24 : 12));
	dstate_setinfo("ups.firmware", "%.1f", reply[OFF_V_VERSION - 1] * 0.1);

	if (ragtech_read(0x00F3, 1, &calib) == 1 && calib > 0) {
		iout_calib = (double)calib;
		upsdebugx(1, "iout_calib from reg 0xF3 = %u", calib);
	}
	if (ragtech_read(0x0202, 2, osc) == 2) {
		osc53 = osc[0];
		osc57 = osc[1];
		upsdebugx(1, "osc53=%u osc57=%u", osc53, osc57);
	}

	if (shutdown_enabled) {
		dstate_addcmd("shutdown.return");
		dstate_addcmd("shutdown.stayoff");
		dstate_addcmd("shutdown.stop");
		dstate_addcmd("test.battery.start.deep");
	} else {
		upslogx(LOG_INFO,
			"shutdown / test.battery.start.deep instcmds are disabled "
			"by default. Set 'allow_shutdown' in ups.conf to enable; "
			"the firmware will not auto-restart after a shutdown.");
	}
	upsh.instcmd = instcmd;
}

void upsdrv_updateinfo(void)
{
	uint8_t r[RAGTECH_MAIN_LEN];
	uint8_t st, fa;
	double vout, iout, bcharge;
	int attempt;

	if (family == 14) {
		f14_updateinfo();
		return;
	}

	/* CDC-ACM occasionally returns a short/corrupt reply, more often when
	 * output current is high (bus noise). Retry the transaction a couple
	 * of times before declaring data stale, so single-poll glitches do
	 * not surface as COMMBAD/COMMOK flapping in upsmon. */
	for (attempt = 0; attempt < 3; attempt++) {
		if (ragtech_read(RAGTECH_MAIN_BASE, RAGTECH_MAIN_LEN, r) >= 0)
			break;
		usleep(100000);
	}
	if (attempt == 3) {
		dstate_datastale();
		return;
	}

	vout = r[OFF_V_VOUTPUT - 1] * model->vmult;
	iout = (r[OFF_V_IOUTPUT - 1] * model->imult) / iout_calib;
	/* Battery charge: raw / 2.55 maps the 0..255 byte onto 0..100 % and
	 * yields exactly 100.0 at raw=255 (matches the OEM/Node-RED scaling). */
	bcharge = r[OFF_V_CBATTERY - 1] / 2.55;

	dstate_setinfo("battery.charge",  "%.1f", bcharge);
	dstate_setinfo("battery.voltage", "%.2f", r[OFF_V_VBATTERY - 1]  * model->bmult);
	dstate_setinfo("input.voltage",   "%.1f", r[OFF_V_VINPUT - 1]    * 1.0600);
	dstate_setinfo("output.voltage",  "%.1f", vout);
	dstate_setinfo("output.current",  "%.2f", iout);
	/* Reg 0x8D reports load as integer percent and floors to 0 at sub-1%
	 * loads. Compute apparent-power load for accurate light-load readings. */
	dstate_setinfo("ups.load",        "%.1f",
		effective_va > 0 ? (vout * iout) / effective_va * 100.0 : 0.0);
	dstate_setinfo("ups.temperature", "%u",   r[OFF_V_TEMPER - 1]);
	dstate_setinfo("output.frequency", "%.2f",
		compute_frequency(r[OFF_V_FOUTPUT - 1]));

	st = r[OFF_F_STATUS - 1];
	fa = r[OFF_F_FAULT - 1];

	status_init();
	if (st & S_ON_BATTERY)
		status_set("OB");
	else
		status_set("OL");
	if (fa & F_BATTERY_LOW)
		status_set("LB");
	if (fa & F_OVERLOAD)
		status_set("OVER");
	if (st & S_SELF_TEST)
		status_set("CAL");
	if (st & (S_NO_BATTERY | S_OLD_BATTERY))
		status_set("RB");
	status_commit();

	if (fa & (F_OVERTEMP | F_INVERTER_FAIL | F_SHORT_CIRCUIT))
		alarm_init();
	if (fa & F_OVERTEMP)
		alarm_set("Overtemperature");
	if (fa & F_INVERTER_FAIL)
		alarm_set("Inverter fault");
	if (fa & F_SHORT_CIRCUIT)
		alarm_set("Short circuit");
	if (fa & (F_OVERTEMP | F_INVERTER_FAIL | F_SHORT_CIRCUIT))
		alarm_commit();

	dstate_dataok();
}

void upsdrv_shutdown(void)
{
	int handled;

	if (family == 14) {
		upslogx(LOG_ERR,
			"upsdrv_shutdown invoked on a family 14 (Easy Way) unit: "
			"no shutdown command is known for it, the driver is read-only");
		set_exit_flag(EF_EXIT_FAILURE);
		return;
	}

	if (!shutdown_enabled) {
		upslogx(LOG_ERR,
			"upsdrv_shutdown invoked but 'allow_shutdown' is not set "
			"in ups.conf -- refusing. This firmware does not auto-restart "
			"after shutdown; a coordinated shutdown would leave the UPS "
			"off until manual power-on.");
		set_exit_flag(EF_EXIT_FAILURE);
		return;
	}

	handled = instcmd("shutdown.return", NULL);
	set_exit_flag(handled == STAT_INSTCMD_HANDLED
		? EF_EXIT_SUCCESS : EF_EXIT_FAILURE);
}

void upsdrv_help(void)
{
}

void upsdrv_tweak_prognames(void)
{
}

void upsdrv_makevartable(void)
{
	addvar(VAR_VALUE, "family",
	       "UPS protocol family: \"auto\" (default; listen passively for the "
	       "Easy Way push stream first, then fall back to family 10), \"10\" "
	       "(Easy Pro, polled) or \"14\" (Easy Way, push-only, read-only)");
	addvar(VAR_VALUE, "iout_calib",
	       "Override per-unit output current calibration (default: read from reg 0xF3)");
	addvar(VAR_VALUE, "va",
	       "Override the apparent-power (VA) rating used for ups.power.nominal, "
	       "ups.realpower.nominal and the computed ups.load. Useful when the "
	       "model id is shared between product lines (e.g. a GT unit reporting "
	       "the same id as an Easy 2200 TI).");
	addvar(VAR_FLAG, "allow_shutdown",
	       "Enable the shutdown.* and test.battery.start.deep instcmds. "
	       "WARNING: this UPS firmware does not auto-restart after a "
	       "coordinated shutdown; manual power-button intervention is "
	       "required when mains return. Defaults to disabled.");
}

void upsdrv_initups(void)
{
	const char *v;

	v = getval("family");
	if (v) {
		if (!strcasecmp(v, "auto"))
			family_opt = 0;
		else if (!strcmp(v, "10"))
			family_opt = 10;
		else if (!strcmp(v, "14"))
			family_opt = 14;
		else
			fatalx(EXIT_FAILURE, "invalid 'family' value [%s]: "
				"use auto, 10 or 14", v);
	}

	upsfd = ser_open(device_path);

	/* A freshly enumerated CDC-ACM tty defaults to canonical mode (ICANON),
	 * where the kernel buffers incoming bytes until a newline arrives. The
	 * firmware's replies are raw binary and contain no newline, so the first
	 * poll never sees them and times out as "no reply to initial status poll".
	 * Put the port into raw 8N1 mode. Baud is left untouched (CDC-ACM ignores
	 * it at the wire); CLOCAL plus clearing HUPCL keep this reconfigure and
	 * the eventual close from toggling DTR. */
#ifndef WIN32
	{
		struct termios	tio;

		if (tcgetattr(upsfd, &tio) == 0) {
			tio.c_iflag &= ~(tcflag_t)(IGNBRK | BRKINT | PARMRK | ISTRIP
						   | INLCR | IGNCR | ICRNL | IXON);
			tio.c_oflag &= ~(tcflag_t)OPOST;
			tio.c_lflag &= ~(tcflag_t)(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
			tio.c_cflag &= ~(tcflag_t)(CSIZE | PARENB | HUPCL);
			tio.c_cflag |= (tcflag_t)(CS8 | CLOCAL | CREAD);
			tcsetattr(upsfd, TCSANOW, &tio);
		}
	}
#endif	/* !WIN32 */

	/* Family 14 needs the real line speed (2560); family 10 does not care,
	 * so with family=10 the port is left exactly as before. */
	if (family_opt != 10)
		f14_set_line();

	/* Force the modem-control lines low after the tcsetattr (in case the tty
	 * layer pulsed them); the OEM client keeps DTR/RTS at 0 and some Ragtech
	 * families read a non-zero level as a remote-shutdown signal. */
	ser_set_dtr(upsfd, 0);
	ser_set_rts(upsfd, 0);

	usleep(RAGTECH_POST_OPEN_MS * 1000);

	v = getval("iout_calib");
	if (v) {
		double parsed = atof(v);
		if (parsed > 0.0)
			iout_calib = parsed;
	}

	v = getval("va");
	if (v) {
		long parsed = atol(v);
		if (parsed > 0)
			va_override = (unsigned int)parsed;
	}

	shutdown_enabled = testvar("allow_shutdown") ? 1 : 0;

	if (family_opt == 14) {
		family = 14;
	} else if (family_opt == 0) {
		/* Passive: nothing is written to the UPS while looking for the
		 * family 14 stream. */
		upsdebugx(1, "family auto: listening %u ms for a family 14 frame",
			F14_DETECT_MS);
		if (f14_listen(F14_DETECT_MS)) {
			family = 14;
		} else {
			/* Give family 10 back the termios it had before the
			 * speed change, and re-assert DTR/RTS low after it. */
			f14_restore_line();
			ser_set_dtr(upsfd, 0);
			ser_set_rts(upsfd, 0);
			usleep(RAGTECH_POST_OPEN_MS * 1000);
			f14_reset();
			family = 10;
		}
	} else {
		family = 10;
	}
	upsdebugx(1, "protocol family %d", family);
}

void upsdrv_cleanup(void)
{
	ser_close(upsfd, device_path);
}
