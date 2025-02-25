/*  generic_modbus.c - Driver for generic UPS connected via modbus RIO
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

#include "main.h"
#include "generic_modbus.h"
#include <modbus.h>
#include "timehead.h"
#include "nut_stdint.h"

#define DRIVER_NAME "NUT Generic Modbus driver"
#define DRIVER_VERSION  "0.06"

/* variables */
static modbus_t *mbctx = NULL;                             /* modbus memory context */
static sigattr_t sigar[NUMOF_SIG_STATES];                  /* array of ups signal attributes */
static int errcnt = 0;                                     /* modbus access error counter */

static char *device_mfr = DEVICE_MFR;                      /* device manufacturer */
static char *device_model = DEVICE_MODEL;                  /* device model */
static int ser_baud_rate = BAUD_RATE;                      /* serial port baud rate */
static char ser_parity = PARITY;                           /* serial port parity */
static int ser_data_bit = DATA_BIT;                        /* serial port data bit */
static int ser_stop_bit = STOP_BIT;                        /* serial port stop bit */
static int rio_slave_id = MODBUS_SLAVE_ID;                 /* set device ID to default value */
static int FSD_pulse_duration = SHTDOWN_PULSE_DURATION;    /* set the FSD pulse duration */
static uint32_t mod_resp_to_s = MODRESP_TIMEOUT_s;         /* set the modbus response time out (s) */
static uint32_t mod_resp_to_us = MODRESP_TIMEOUT_us;       /* set the modbus response time out (us) */
static uint32_t mod_byte_to_s = MODBYTE_TIMEOUT_s;         /* set the modbus byte time out (us) */
static uint32_t mod_byte_to_us = MODBYTE_TIMEOUT_us;       /* set the modbus byte time out (us) */

/* get config vars set by -x or defined in ups.conf driver section */
void get_config_vars(void);

/* create a new modbus context based on connection type (serial or TCP) */
modbus_t *modbus_new(const char *port);

/* reconnect upon communication error */
void modbus_reconnect(void);

/* modbus register read function */
int register_read(modbus_t *mb, int addr, regtype_t type, void *data);

/* instant command triggered by upsd */
int upscmd(const char *cmd, const char *arg);

/* read signal status */
int get_signal_state(devstate_t state);

/* count the time elapsed since start */
long time_elapsed(struct timeval *start);

int register_write(modbus_t *mb, int addr, regtype_t type, void *data);

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Dimitris Economou <dimitris.s.economou@gmail.com>\n",
	DRV_BETA,
	{NULL}
};

/*
 * driver functions
 */

/* initialize ups driver information */
void upsdrv_initinfo(void) {
	upsdebugx(2, "upsdrv_initinfo");

	/* set device information */
	dstate_setinfo("device.mfr", "%s", device_mfr);
	dstate_setinfo("device.model", "%s", device_model);

	/* register instant commands */
	if (sigar[FSD_T].addr != NOTUSED) {
		dstate_addcmd("load.off");

		/* FIXME: Check with the device what this instcmd
		 * (nee upsdrv_shutdown() contents) actually does!
		 */
		dstate_addcmd("shutdown.stayoff");
	}

	/* set callback for instant commands */
	upsh.instcmd = upscmd;
}

/* open serial connection and connect to modbus RIO */
void upsdrv_initups(void)
{
	int rval;
	upsdebugx(2, "upsdrv_initups");

	get_config_vars();

	/* open communication port */
	mbctx = modbus_new(device_path);
	if (mbctx == NULL) {
		fatalx(EXIT_FAILURE, "modbus_new_rtu: Unable to open communication port context");
	}

	/* set slave ID */
	rval = modbus_set_slave(mbctx, rio_slave_id);
	if (rval < 0) {
		modbus_free(mbctx);
		fatalx(EXIT_FAILURE, "modbus_set_slave: Invalid modbus slave ID %d", rio_slave_id);
	}

	/* connect to modbus device  */
	if (modbus_connect(mbctx) == -1) {
		modbus_free(mbctx);
		fatalx(EXIT_FAILURE, "modbus_connect: unable to connect: error(%s)", modbus_strerror(errno));
	}

	/* set modbus response timeout */
#if (defined NUT_MODBUS_TIMEOUT_ARG_sec_usec_uint32) || (defined NUT_MODBUS_TIMEOUT_ARG_sec_usec_uint32_cast_timeval_fields)
	rval = modbus_set_response_timeout(mbctx, mod_resp_to_s, mod_resp_to_us);
	if (rval < 0) {
		modbus_free(mbctx);
		fatalx(EXIT_FAILURE, "modbus_set_response_timeout: error(%s)", modbus_strerror(errno));
	}
#elif (defined NUT_MODBUS_TIMEOUT_ARG_timeval_numeric_fields)
	{
		/* Older libmodbus API (with timeval), and we have
		 * checked at configure time that we can put uint32_t
		 * into its fields. They are probably "long" on many
		 * systems as respectively time_t and suseconds_t -
		 * but that is not guaranteed; for more details see
		 * https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/sys_time.h.html
		 */
		struct timeval to;
		memset(&to, 0, sizeof(struct timeval));
		to.tv_sec = mod_resp_to_s;
		to.tv_usec = mod_resp_to_us;
		/* void */ modbus_set_response_timeout(mbctx, &to);
	}
/* #elif (defined NUT_MODBUS_TIMEOUT_ARG_timeval) // some un-castable type in fields */
#else
# error "Can not use libmodbus API for timeouts"
#endif /* NUT_MODBUS_TIMEOUT_ARG_* */

	/* set modbus byte time out */
#if (defined NUT_MODBUS_TIMEOUT_ARG_sec_usec_uint32) || (defined NUT_MODBUS_TIMEOUT_ARG_sec_usec_uint32_cast_timeval_fields)
	rval = modbus_set_byte_timeout(mbctx, mod_byte_to_s, mod_byte_to_us);
	if (rval < 0) {
		modbus_free(mbctx);
		fatalx(EXIT_FAILURE, "modbus_set_byte_timeout: error(%s)", modbus_strerror(errno));
	}
#elif (defined NUT_MODBUS_TIMEOUT_ARG_timeval_numeric_fields)
	{   /* see comments above */
		struct timeval to;
		memset(&to, 0, sizeof(struct timeval));
		to.tv_sec = mod_byte_to_s;
		to.tv_usec = mod_byte_to_us;
		/* void */ modbus_set_byte_timeout(mbctx, &to);
	}
/* #elif (defined NUT_MODBUS_TIMEOUT_ARG_timeval) // some un-castable type in fields */
#endif /* NUT_MODBUS_TIMEOUT_ARG_* */
}

/* update UPS signal state */
void upsdrv_updateinfo(void)
{
	int rval;
	int online = -1;    /* keep online state */
	errcnt = 0;

	upsdebugx(2, "upsdrv_updateinfo");
	status_init();      /* initialize ups.status update */
	alarm_init();       /* initialize ups.alarm update */

	/*
	 * update UPS status regarding MAINS state either via OL | OB.
	 * if both statuses are mapped to contacts then only OL is evaluated.
	 */
	if (sigar[OL_T].addr != NOTUSED) {
		rval = get_signal_state(OL_T);
		upsdebugx(2, "OL value: %d", rval);
		if (rval == -1) {
			errcnt++;
		} else if (rval == (1 ^ sigar[OL_T].noro)) {
			status_set("OL");
			online = 1;
		} else {
			status_set("OB");
			online = 0;

			/* if DISCHRG state is not mapped to a contact and UPS is on
			 * batteries set status to DISCHRG state */
			if (sigar[DISCHRG_T].addr == NOTUSED) {
				status_set("DISCHRG");
				dstate_setinfo("battery.charger.status", "discharging");
			}

		}
	} else if (sigar[OB_T].addr != NOTUSED) {
		rval = get_signal_state(OB_T);
		upsdebugx(2, "OB value: %d", rval);
		if (rval == -1) {
			errcnt++;
		} else if (rval == (1 ^ sigar[OB_T].noro)) {
			status_set("OB");
			online = 0;
			if (sigar[DISCHRG_T].addr == NOTUSED) {
				status_set("DISCHRG");
				dstate_setinfo("battery.charger.status", "discharging");
			}
		} else {
			status_set("OL");
			online = 1;
		}
	}

	/*
	 * update UPS status regarding CHARGING state via HB. HB is usually
	 * mapped to "ready" contact when closed indicates a charging state > 85%
	 */
	if (sigar[HB_T].addr != NOTUSED) {
		rval = get_signal_state(HB_T);
		upsdebugx(2, "HB value: %d", rval);
		if (rval == -1) {
			errcnt++;
		} else if (rval == (1 ^ sigar[HB_T].noro)) {
			status_set("HB");
			dstate_setinfo("battery.charger.status", "resting");
		} else if (online == 1 && sigar[CHRG_T].addr == NOTUSED && errcnt == 0) {
			status_set("CHRG");
			dstate_setinfo("battery.charger.status", "charging");
		} else if (online == 0 && sigar[DISCHRG_T].addr == NOTUSED && errcnt == 0) {
			status_set("DISCHRG");
			dstate_setinfo("battery.charger.status", "discharging");
		}
	}

	/*
	 * update UPS status regarding DISCHARGING state via LB. LB is mapped
	 * to "battery low" contact.
	 */
	if (sigar[LB_T].addr != NOTUSED) {
		rval = get_signal_state(LB_T);
		upsdebugx(2, "LB value: %d", rval);
		if (rval == -1) {
			errcnt++;
		} else if (rval == (1 ^ sigar[LB_T].noro)) {
			status_set("LB");
			alarm_set("Low Battery (Charge)");
		}
	}

	/*
	 * update UPS status regarding battery HEALTH state via RB. RB is mapped
	 * to "replace battery" contact
	 */
	if (sigar[RB_T].addr != NOTUSED) {
		rval = get_signal_state(RB_T);
		upsdebugx(2, "RB value: %d", rval);
		if (rval == -1) {
			errcnt++;
		} else if (rval == (1 ^ sigar[RB_T].noro)) {
			status_set("RB");
			alarm_set("Replace Battery");
		}
	}

	/*
	 * update UPS status regarding battery HEALTH state via RB. RB is mapped
	 * to "replace battery" contact
	 */
	if (sigar[CHRG_T].addr != NOTUSED) {
		rval = get_signal_state(CHRG_T);
		upsdebugx(2, "CHRG value: %d", rval);
		if (rval == -1) {
			errcnt++;
		} else if (rval == (1 ^ sigar[CHRG_T].noro)) {
			status_set("CHRG");
			dstate_setinfo("battery.charger.status", "charging");
		}
	} else if (sigar[DISCHRG_T].addr != NOTUSED) {
		rval = get_signal_state(DISCHRG_T);
		upsdebugx(2, "DISCHRG value: %d", rval);
		if (rval == -1) {
			errcnt++;
		} else if (rval == (1 ^ sigar[DISCHRG_T].noro)) {
			status_set("DISCHRG");
			dstate_setinfo("battery.charger.status", "discharging");
		}
	}

	/* check for communication errors */
	if (errcnt == 0) {
		alarm_commit();
		status_commit();
		dstate_dataok();
	} else {
		upsdebugx(2,"Communication errors: %d", errcnt);
		dstate_datastale();
	}
}

/* shutdown UPS */
void upsdrv_shutdown(void)
{
	/* Only implement "shutdown.default"; do not invoke
	 * general handling of other `sdcommands` here */

	/*
	 * WARNING: When using RTU TCP, this driver will probably
	 * never support shutdowns properly, except on some systems:
	 * In order to be of any use, the driver should be called
	 * near the end of the system halt script (or a service
	 * management framework's equivalent, if any). By that
	 * time we, in all likelyhood, won't have basic network
	 * capabilities anymore, so we could never send this
	 * command to the UPS. This is not an error, but rather
	 * a limitation (on some platforms) of the interface/media
	 * used for these devices.
	 */

	int	ret = do_loop_shutdown_commands("shutdown.stayoff", NULL);
	if (handling_upsdrv_shutdown > 0)
		set_exit_flag(ret == STAT_INSTCMD_HANDLED ? EF_EXIT_SUCCESS : EF_EXIT_FAILURE);
}

/* print driver usage info */
void upsdrv_help(void)
{
}

/* list flags and values that you want to receive via -x */
void upsdrv_makevartable(void)
{
	addvar(VAR_VALUE, "device_mfr", "device manufacturer");
	addvar(VAR_VALUE, "device_model", "device model");
	addvar(VAR_VALUE, "ser_baud_rate", "serial port baud rate");
	addvar(VAR_VALUE, "ser_parity", "serial port parity");
	addvar(VAR_VALUE, "ser_data_bit", "serial port data bit");
	addvar(VAR_VALUE, "ser_stop_bit", "serial port stop bit");
	addvar(VAR_VALUE, "rio_slave_id", "RIO modbus slave ID");
	addvar(VAR_VALUE, "mod_resp_to_s", "modbus response timeout (s)");
	addvar(VAR_VALUE, "mod_resp_to_us", "modbus response timeout (us)");
	addvar(VAR_VALUE, "mod_byte_to_s", "modbus byte timeout (s)");
	addvar(VAR_VALUE, "mod_byte_to_us", "modbus byte timeout (us)");
	addvar(VAR_VALUE, "OL_addr", "modbus address for OL state");
	addvar(VAR_VALUE, "OB_addr", "modbus address for OB state");
	addvar(VAR_VALUE, "LB_addr", "modbus address for LB state");
	addvar(VAR_VALUE, "HB_addr", "modbus address for HB state");
	addvar(VAR_VALUE, "RB_addr", "modbus address for RB state");
	addvar(VAR_VALUE, "CHRG_addr", "modbus address for CHRG state");
	addvar(VAR_VALUE, "DISCHRG_addr", "modbus address for DISCHRG state");
	addvar(VAR_VALUE, "FSD_addr", "modbus address for FSD command");
	addvar(VAR_VALUE, "OL_regtype", "modbus register type for OL state");
	addvar(VAR_VALUE, "OB_regtype", "modbus register type for OB state");
	addvar(VAR_VALUE, "LB_regtype", "modbus register type for LB state");
	addvar(VAR_VALUE, "HB_regtype", "modbus register type for HB state");
	addvar(VAR_VALUE, "RB_regtype", "modbus register type for RB state");
	addvar(VAR_VALUE, "CHRG_regtype", "modbus register type for CHRG state");
	addvar(VAR_VALUE, "DISCHRG_regtype", "modbus register type for DISCHRG state");
	addvar(VAR_VALUE, "FSD_regtype", "modbus register type for FSD command");
	addvar(VAR_VALUE, "OL_noro", "NO/NC configuration for OL state");
	addvar(VAR_VALUE, "OB_noro", "NO/NC configuration for OB state");
	addvar(VAR_VALUE, "LB_noro", "NO/NC configuration for LB state");
	addvar(VAR_VALUE, "HB_noro", "NO/NC configuration for HB state");
	addvar(VAR_VALUE, "RB_noro", "NO/NC configuration for RB state");
	addvar(VAR_VALUE, "CHRG_noro", "NO/NC configuration for CHRG state");
	addvar(VAR_VALUE, "DISCHRG_noro", "NO/NC configuration for DISCHRG state");
	addvar(VAR_VALUE, "FSD_noro", "NO/NC configuration for FSD state");
	addvar(VAR_VALUE, "FSD_pulse_duration", "FSD pulse duration");
}

/* close modbus connection and free modbus context allocated memory */
void upsdrv_cleanup(void)
{
	if (mbctx != NULL) {
		modbus_close(mbctx);
		modbus_free(mbctx);
	}
}

/*
 * driver support functions
 */

/* Read a modbus register */
int register_read(modbus_t *mb, int addr, regtype_t type, void *data)
{
	int rval = -1;

	/* register bit masks */
	uint16_t mask8 = 0x000F;
	uint16_t mask16 = 0x00FF;

	switch (type) {
		case COIL:
			rval = modbus_read_bits(mb, addr, 1, (uint8_t *)data);
			*(uint16_t *)data = *(uint16_t *)data & mask8;
			break;
		case INPUT_B:
			rval = modbus_read_input_bits(mb, addr, 1, (uint8_t *)data);
			*(uint16_t *)data = *(uint16_t *)data & mask8;
			break;
		case INPUT_R:
			rval = modbus_read_input_registers(mb, addr, 1, (uint16_t *)data);
			*(uint16_t *)data = *(uint16_t *)data & mask16;
			break;
		case HOLDING:
			rval = modbus_read_registers(mb, addr, 1, (uint16_t *)data);
			*(uint16_t *)data = *(uint16_t *)data & mask16;
			break;

#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT
# pragma GCC diagnostic ignored "-Wcovered-switch-default"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
# pragma GCC diagnostic ignored "-Wunreachable-code"
#endif
/* Older CLANG (e.g. clang-3.4) seems to not support the GCC pragmas above */
#ifdef __clang__
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wunreachable-code"
# pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif
		/* All enum cases defined as of the time of coding
		 * have been covered above. Handle later definitions,
		 * memory corruptions and buggy inputs below...
		 */
		default:
			upsdebugx(2, "ERROR: register_read: invalid register type %u", type);
			break;
#ifdef __clang__
# pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic pop
#endif
	}
	if (rval == -1) {
		upslogx(LOG_ERR, "ERROR:(%s) modbus_read: addr:0x%x, type:%8s, path:%s",
			modbus_strerror(errno),
			(unsigned int)addr,
			(type == COIL) ? "COIL" :
			(type == INPUT_B) ? "INPUT_B" :
			(type == INPUT_R) ? "INPUT_R" : "HOLDING",
			device_path
		);

		/* on BROKEN PIPE error try to reconnect */
		if (errno == EPIPE) {
			upsdebugx(2, "register_read: error(%s)", modbus_strerror(errno));
			modbus_reconnect();
		}
	}
	upsdebugx(3, "register addr: 0x%x, register type: %u read: %u",
		(unsigned int)addr, type, *(unsigned int *)data);
	return rval;
}

/* write a modbus register */
int register_write(modbus_t *mb, int addr, regtype_t type, void *data)
{
	int rval = -1;

	/* register bit masks */
	uint16_t mask8 = 0x000F;
	uint16_t mask16 = 0x00FF;

	switch (type) {
		case COIL:
			*(uint16_t *)data = *(uint16_t *)data & mask8;
			rval = modbus_write_bit(mb, addr, *(uint8_t *)data);
			break;
		case HOLDING:
			*(uint16_t *)data = *(uint16_t *)data & mask16;
			rval = modbus_write_register(mb, addr, *(uint16_t *)data);
			break;

		case INPUT_B:
		case INPUT_R:
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wcovered-switch-default"
#endif
		/* All enum cases defined as of the time of coding
		 * have been covered above. Handle later definitions,
		 * memory corruptions and buggy inputs below...
		 */
		default:
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT)
# pragma GCC diagnostic pop
#endif
			upsdebugx(2, "ERROR: register_write: invalid register type %u", type);
			break;
	}
	if (rval == -1) {
		upslogx(LOG_ERR, "ERROR:(%s) modbus_read: addr:0x%x, type:%8s, path:%s",
			modbus_strerror(errno),
			(unsigned int)addr,
			(type == COIL) ? "COIL" :
			(type == INPUT_B) ? "INPUT_B" :
			(type == INPUT_R) ? "INPUT_R" : "HOLDING",
			device_path
		);

		/* on BROKEN PIPE error try to reconnect */
		if (errno == EPIPE) {
			upsdebugx(2, "register_write: error(%s)", modbus_strerror(errno));
			modbus_reconnect();
		}
	}
	upsdebugx(3, "register addr: 0x%x, register type: %u read: %u",
		(unsigned int)addr, type, *(unsigned int *)data);
	return rval;
}

/* returns the time elapsed since start in milliseconds */
long time_elapsed(struct timeval *start)
{
	long rval;
	struct timeval end;

	rval = gettimeofday(&end, NULL);
	if (rval < 0) {
		upslog_with_errno(LOG_ERR, "time_elapsed");
	}
	if (start->tv_usec < end.tv_usec) {
		suseconds_t nsec = (end.tv_usec - start->tv_usec) / 1000000 + 1;
		end.tv_usec -= 1000000 * nsec;
		end.tv_sec += nsec;
	}
	if (start->tv_usec - end.tv_usec > 1000000) {
		suseconds_t nsec = (start->tv_usec - end.tv_usec) / 1000000;
		end.tv_usec += 1000000 * nsec;
		end.tv_sec -= nsec;
	}
	rval = (end.tv_sec - start->tv_sec) * 1000 + (end.tv_usec - start->tv_usec) / 1000;

	return rval;
}

/* instant command triggered by upsd */
int upscmd(const char *cmd, const char *arg)
{
	int rval;
	int data;
	struct timeval start;
	long etime;

	if (!strcasecmp(cmd, "load.off")) {
		if (sigar[FSD_T].addr != NOTUSED &&
		    (sigar[FSD_T].type == COIL || sigar[FSD_T].type == HOLDING)
		) {
			data = 1 ^ sigar[FSD_T].noro;
			rval = register_write(mbctx, sigar[FSD_T].addr, sigar[FSD_T].type, &data);
			if (rval == -1) {
				upslogx(2, "ERROR:(%s) modbus_write_register: addr:0x%08x, regtype: %u, path:%s",
					modbus_strerror(errno),
					(unsigned int)(sigar[FSD_T].addr),
					sigar[FSD_T].type,
					device_path
				);
				upslogx(LOG_NOTICE, "load.off: failed (communication error) [%s] [%s]", cmd, arg);
				rval = STAT_INSTCMD_FAILED;
			} else {
				upsdebugx(2, "load.off: addr: 0x%x, data: %d",
					(unsigned int)(sigar[FSD_T].addr), data);
				rval = STAT_INSTCMD_HANDLED;
			}

			/* if pulse has been defined and rising edge was successful */
			if (FSD_pulse_duration != NOTUSED && rval == STAT_INSTCMD_HANDLED) {
				rval = gettimeofday(&start, NULL);
				if (rval < 0) {
					upslog_with_errno(LOG_ERR, "upscmd: gettimeofday");
				}

				/* wait for FSD_pulse_duration ms */
				while ((etime = time_elapsed(&start)) < FSD_pulse_duration);

				data = 0 ^ sigar[FSD_T].noro;
				rval = register_write(mbctx, sigar[FSD_T].addr, sigar[FSD_T].type, &data);
				if (rval == -1) {
					upslogx(LOG_ERR, "ERROR:(%s) modbus_write_register: addr:0x%08x, regtype: %u, path:%s\n",
						modbus_strerror(errno),
						(unsigned int)(sigar[FSD_T].addr),
						sigar[FSD_T].type,
						device_path
					);
					upslogx(LOG_NOTICE, "load.off: failed (communication error) [%s] [%s]", cmd, arg);
					rval = STAT_INSTCMD_FAILED;
				} else {
					upsdebugx(2, "load.off: addr: 0x%x, data: %d, elapsed time: %lims",
						(unsigned int)(sigar[FSD_T].addr),
						data,
						etime
					);
					rval = STAT_INSTCMD_HANDLED;
				}
			}
		} else {
			upslogx(LOG_NOTICE,"load.off: failed (FSD address undefined or invalid register type)  [%s] [%s]",
				cmd,
				arg
			);
			rval = STAT_INSTCMD_FAILED;
		}
	} else if (!strcasecmp(cmd, "shutdown.stayoff")) {
		/* FIXME: Which one is this actually -
		 * "shutdown.stayoff" or "shutdown.return"? */
		int cnt = FSD_REPEAT_CNT;    /* shutdown repeat counter */

		/* retry sending shutdown command on error */
		while ((rval = upscmd("load.off", NULL)) != STAT_INSTCMD_HANDLED && cnt > 0) {
			rval = gettimeofday(&start, NULL);
			if (rval < 0) {
				upslog_with_errno(LOG_ERR, "upscmd: gettimeofday");
			}

			/* wait for an increasing time interval before sending shutdown command */
			while ((etime = time_elapsed(&start)) < ( FSD_REPEAT_INTRV / cnt));
			upsdebugx(2,"ERROR: load.off failed, wait for %lims, retries left: %d\n", etime, cnt - 1);
			cnt--;
		}
		switch (rval) {
			case STAT_INSTCMD_FAILED:
			case STAT_INSTCMD_INVALID:
				upslogx(LOG_ERR, "shutdown failed");
				if (handling_upsdrv_shutdown > 0)
					set_exit_flag(EF_EXIT_FAILURE);
				return rval;
			case STAT_INSTCMD_UNKNOWN:
				upslogx(LOG_ERR, "shutdown not supported");
				if (handling_upsdrv_shutdown > 0)
					set_exit_flag(EF_EXIT_FAILURE);
				return rval;
			default:
				upslogx(LOG_INFO, "shutdown command executed");
				if (handling_upsdrv_shutdown > 0)
					set_exit_flag(EF_EXIT_SUCCESS);
				break;
		}
	} else {
		upslogx(LOG_NOTICE, "instcmd: unknown command [%s] [%s]", cmd, arg);
		rval = STAT_INSTCMD_UNKNOWN;
	}
	return rval;
}

/* read signal state from modbus RIO, returns 0|1 state or -1 on communication error */
int get_signal_state(devstate_t state)
{
	int rval = -1;
	int reg_val;
	regtype_t rtype = 0;    /* register type */
	int addr = -1;          /* register address */

	/* assign register address and type  */
	switch (state) {
		case OL_T:
			addr = sigar[OL_T].addr;
			rtype = sigar[OL_T].type;
			break;
		case OB_T:
			addr = sigar[OB_T].addr;
			rtype = sigar[OB_T].type;
			break;
		case LB_T:
			addr = sigar[LB_T].addr;
			rtype = sigar[LB_T].type;
			break;
		case HB_T:
			addr = sigar[HB_T].addr;
			rtype = sigar[HB_T].type;
			break;
		case RB_T:
			addr = sigar[RB_T].addr;
			rtype = sigar[RB_T].type;
			break;
		case CHRG_T:
			addr = sigar[CHRG_T].addr;
			rtype = sigar[CHRG_T].type;
			break;
		case DISCHRG_T:
			addr = sigar[DISCHRG_T].addr;
			rtype = sigar[DISCHRG_T].type;
			break;

		case BYPASS_T:
		case CAL_T:
		case FSD_T:
		case OFF_T:
		case OVER_T:
		case TRIM_T:
		case BOOST_T:
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wcovered-switch-default"
#endif
		/* All enum cases defined as of the time of coding
		 * have been covered above. Handle later definitions,
		 * memory corruptions and buggy inputs below...
		 */
		default:
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT)
# pragma GCC diagnostic pop
#endif
			break;
	}

	rval = register_read(mbctx, addr, rtype, &reg_val);
	if (rval > -1) {
		rval = reg_val;
	}
	upsdebugx(3, "get_signal_state: state: %d", reg_val);
	return rval;
}

/* get driver configuration parameters */
void get_config_vars(void)
{
	int i; /* local index */

	/* initialize sigar table */
	for (i = 0; i < NUMOF_SIG_STATES; i++) {
		sigar[i].addr = NOTUSED;
		sigar[i].noro = 0;	/* ON corresponds to 1 (closed contact) */
	}

	/* check if device manufacturer is set ang get the value */
	if (testvar("device_mfr")) {
		device_mfr = getval("device_mfr");
	}
	upsdebugx(2, "device_mfr %s", device_mfr);

	/* check if device model is set ang get the value */
	if (testvar("device_model")) {
		device_model = getval("device_model");
	}
	upsdebugx(2, "device_model %s", device_model);

	/* check if serial baud rate is set ang get the value */
	if (testvar("ser_baud_rate")) {
		ser_baud_rate = (int)strtol(getval("ser_baud_rate"), NULL, 10);
	}
	upsdebugx(2, "ser_baud_rate %d", ser_baud_rate);

	/* check if serial parity is set ang get the value */
	if (testvar("ser_parity")) {
		/* Dereference the char* we get */
		char *sp = getval("ser_parity");
		if (sp) {
			/* TODO? Sanity-check the char we get? */
			ser_parity = *sp;
		} else {
			upsdebugx(2, "Could not determine ser_parity, will keep default");
		}
	}
	upsdebugx(2, "ser_parity %c", ser_parity);

	/* check if serial data bit is set ang get the value */
	if (testvar("ser_data_bit")) {
		ser_data_bit = (int)strtol(getval("ser_data_bit"), NULL, 10);
	}
	upsdebugx(2, "ser_data_bit %d", ser_data_bit);

	/* check if serial stop bit is set ang get the value */
	if (testvar("ser_stop_bit")) {
		ser_stop_bit = (int)strtol(getval("ser_stop_bit"), NULL, 10);
	}
	upsdebugx(2, "ser_stop_bit %d", ser_stop_bit);

	/* check if device ID is set ang get the value */
	if (testvar("rio_slave_id")) {
		rio_slave_id = (int)strtol(getval("rio_slave_id"), NULL, 10);
	}
	upsdebugx(2, "rio_slave_id %d", rio_slave_id);

	/* check if response time out (s) is set ang get the value */
	if (testvar("mod_resp_to_s")) {
		mod_resp_to_s = (uint32_t)strtol(getval("mod_resp_to_s"), NULL, 10);
	}
	upsdebugx(2, "mod_resp_to_s %u", mod_resp_to_s);

	/* check if response time out (us) is set ang get the value */
	if (testvar("mod_resp_to_us")) {
		mod_resp_to_us = (uint32_t) strtol(getval("mod_resp_to_us"), NULL, 10);
		if (mod_resp_to_us > 999999) {
			fatalx(EXIT_FAILURE, "get_config_vars: Invalid mod_resp_to_us %u", mod_resp_to_us);
		}
	}
	upsdebugx(2, "mod_resp_to_us %u", mod_resp_to_us);

	/* check if byte time out (s) is set ang get the value */
	if (testvar("mod_byte_to_s")) {
		mod_byte_to_s = (uint32_t)strtol(getval("mod_byte_to_s"), NULL, 10);
	}
	upsdebugx(2, "mod_byte_to_s %u", mod_byte_to_s);

	/* check if byte time out (us) is set ang get the value */
	if (testvar("mod_byte_to_us")) {
		mod_byte_to_us = (uint32_t) strtol(getval("mod_byte_to_us"), NULL, 10);
		if (mod_byte_to_us > 999999) {
			fatalx(EXIT_FAILURE, "get_config_vars: Invalid mod_byte_to_us %u", mod_byte_to_us);
		}
	}
	upsdebugx(2, "mod_byte_to_us %u", mod_byte_to_us);

	/* check if OL address is set and get the value */
	if (testvar("OL_addr")) {
		sigar[OL_T].addr = (int)strtol(getval("OL_addr"), NULL, 0);
		if (testvar("OL_noro")) {
			sigar[OL_T].noro = (int)strtol(getval("OL_noro"), NULL, 10);
			if (sigar[OL_T].noro != 1) {
				sigar[OL_T].noro = 0;
			}
		}
	}

	/* check if OL register type is set and get the value otherwise set to INPUT_B */
	if (testvar("OL_regtype")) {
		sigar[OL_T].type = (unsigned int)strtol(getval("OL_regtype"), NULL, 10);
		if (sigar[OL_T].type < COIL || sigar[OL_T].type > HOLDING) {
			sigar[OL_T].type = INPUT_B;
		}
	} else {
		sigar[OL_T].type = INPUT_B;
	}

	/* check if OB address is set and get the value */
	if (testvar("OB_addr")) {
		sigar[OB_T].addr = (int)strtol(getval("OB_addr"), NULL, 0);
	}
	if (testvar("OB_noro")) {
		sigar[OB_T].noro = (int)strtol(getval("OB_noro"), NULL, 10);
		if (sigar[OB_T].noro != 1) {
			sigar[OB_T].noro = 0;
		}
	}

	/* check if OB register type is set and get the value otherwise set to INPUT_B */
	if (testvar("OB_regtype")) {
		sigar[OB_T].type = (unsigned int)strtol(getval("OB_regtype"), NULL, 10);
		if (sigar[OB_T].type < COIL || sigar[OB_T].type > HOLDING) {
			sigar[OB_T].type = INPUT_B;
		}
	} else {
		sigar[OB_T].type = INPUT_B;
	}

	/* check if LB address is set and get the value */
	if (testvar("LB_addr")) {
		sigar[LB_T].addr = (int)strtol(getval("LB_addr"), NULL, 0);
		if (testvar("LB_noro")) {
			sigar[LB_T].noro = (int)strtol(getval("LB_noro"), NULL, 10);
			if (sigar[LB_T].noro != 1) {
				sigar[LB_T].noro = 0;
			}
		}
	}

	/* check if LB register type is set and get the value otherwise set to INPUT_B */
	if (testvar("LB_regtype")) {
		sigar[LB_T].type = (unsigned int)strtol(getval("OB_regtype"), NULL, 10);
		if (sigar[LB_T].type < COIL || sigar[LB_T].type > HOLDING) {
			sigar[LB_T].type = INPUT_B;
		}
	} else {
		sigar[LB_T].type = INPUT_B;
	}

	/* check if HB address is set and get the value */
	if (testvar("HB_addr")) {
		sigar[HB_T].addr = (int)strtol(getval("HB_addr"), NULL, 0);
		if (testvar("HB_noro")) {
			sigar[HB_T].noro = (int)strtol(getval("HB_noro"), NULL, 10);
			if (sigar[HB_T].noro != 1) {
				sigar[HB_T].noro = 0;
			}
		}
	}

	/* check if HB register type is set and get the value otherwise set to INPUT_B */
	if (testvar("HB_regtype")) {
		sigar[HB_T].type = (unsigned int)strtol(getval("HB_regtype"), NULL, 10);
		if (sigar[HB_T].type < COIL || sigar[HB_T].type > HOLDING) {
			sigar[HB_T].type = INPUT_B;
		}
	} else {
		sigar[HB_T].type = INPUT_B;
	}

	/* check if RB address is set and get the value */
	if (testvar("RB_addr")) {
		sigar[RB_T].addr = (int)strtol(getval("RB_addr"), NULL, 0);
		if (testvar("RB_noro")) {
			sigar[RB_T].noro = (int)strtol(getval("RB_noro"), NULL, 10);
			if (sigar[RB_T].noro != 1) {
				sigar[RB_T].noro = 0;
			}
		}
	}

	/* check if RB register type is set and get the value otherwise set to INPUT_B */
	if (testvar("RB_regtype")) {
		sigar[RB_T].type = (unsigned int)strtol(getval("RB_regtype"), NULL, 10);
		if (sigar[RB_T].type < COIL || sigar[RB_T].type > HOLDING) {
			sigar[RB_T].type = INPUT_B;
		}
	} else {
		sigar[RB_T].type = INPUT_B;
	}

	/* check if CHRG address is set and get the value */
	if (testvar("CHRG_addr")) {
		sigar[CHRG_T].addr = (int)strtol(getval("CHRG_addr"), NULL, 0);
		if (testvar("CHRG_noro")) {
			sigar[CHRG_T].noro = (int)strtol(getval("CHRG_noro"), NULL, 10);
			if (sigar[CHRG_T].noro != 1) {
				sigar[CHRG_T].noro = 0;
			}
		}
	}

	/* check if CHRG register type is set and get the value otherwise set to INPUT_B */
	if (testvar("CHRG_regtype")) {
		sigar[CHRG_T].type = (unsigned int)strtol(getval("CHRG_regtype"), NULL, 10);
		if (sigar[CHRG_T].type < COIL || sigar[CHRG_T].type > HOLDING) {
			sigar[CHRG_T].type = INPUT_B;
		}
	} else {
		sigar[CHRG_T].type = INPUT_B;
	}

	/* check if DISCHRG address is set and get the value */
	if (testvar("DISCHRG_addr")) {
		sigar[DISCHRG_T].addr = (int)strtol(getval("DISCHRG_addr"), NULL, 0);
		if (testvar("DISCHRG_noro")) {
			sigar[DISCHRG_T].noro = (int)strtol(getval("DISCHRG_noro"), NULL, 10);
			if (sigar[DISCHRG_T].noro != 1) {
				sigar[DISCHRG_T].noro = 0;
			}
		}
	}

	/* check if DISCHRG register type is set and get the value otherwise set to INPUT_B */
	if (testvar("DISCHRG_regtype")) {
		sigar[DISCHRG_T].type = (unsigned int)strtol(getval("DISCHRG_regtype"), NULL, 10);
		if (sigar[DISCHRG_T].type < COIL || sigar[DISCHRG_T].type > HOLDING) {
			sigar[DISCHRG_T].type = INPUT_B;
		}
	} else {
		sigar[DISCHRG_T].type = INPUT_B;
	}

	/* check if FSD address is set and get the value */
	if (testvar("FSD_addr")) {
		sigar[FSD_T].addr = (int)strtol(getval("FSD_addr"), NULL, 0);
		if (testvar("FSD_noro")) {
			sigar[FSD_T].noro = (int)strtol(getval("FSD_noro"), NULL, 10);
			if (sigar[FSD_T].noro != 1) {
				sigar[FSD_T].noro = 0;
			}
		}
	}

	/* check if FSD register type is set and get the value otherwise set to COIL */
	if (testvar("FSD_regtype")) {
		sigar[FSD_T].type = (unsigned int)strtol(getval("FSD_regtype"), NULL, 10);
		if (sigar[FSD_T].type < COIL || sigar[FSD_T].type > HOLDING) {
			sigar[FSD_T].type = COIL;
		}
	} else {
		sigar[FSD_T].type = COIL;
	}

	/* check if FSD pulse duration is set and get the value */
	if (testvar("FSD_pulse_duration")) {
		FSD_pulse_duration = (int) strtol(getval("FSD_pulse_duration"), NULL, 10);
	}
	upsdebugx(2, "FSD_pulse_duration %d", FSD_pulse_duration);

	/* debug loop over signal array */
	for (i = 0; i < NUMOF_SIG_STATES; i++) {
		if (sigar[i].addr != NOTUSED) {
			char *signame;
			switch (i) {
				case OL_T:
					signame = "OL";
					break;
				case OB_T:
					signame = "OB";
					break;
				case LB_T:
					signame = "LB";
					break;
				case HB_T:
					signame = "HB";
					break;
				case RB_T:
					signame = "RB";
					break;
				case FSD_T:
					signame = "FSD";
					break;
				case CHRG_T:
					signame = "CHRG";
					break;
				case DISCHRG_T:
					signame = "DISCHRG";
					break;
				default:
					signame = "NOTUSED";
					break;
			}
			upsdebugx(2, "%s, addr:0x%x, type:%u",
				signame,
				(unsigned int)(sigar[i].addr),
				sigar[i].type);
		}
	}
}

/* create a new modbus context based on connection type (serial or TCP) */
modbus_t *modbus_new(const char *port)
{
	modbus_t *mb;
	char *sp;
	if (strstr(port, "/dev/tty") != NULL) {
		mb = modbus_new_rtu(port, ser_baud_rate, ser_parity, ser_data_bit, ser_stop_bit);
		if (mb == NULL) {
			upslogx(LOG_ERR, "modbus_new_rtu: Unable to open serial port context\n");
		}
	} else if ((sp = strchr(port, ':')) != NULL) {
		char *tcp_port = xmalloc(sizeof(sp));
		strcpy(tcp_port, sp + 1);
		*sp = '\0';
		mb = modbus_new_tcp(port, (int)strtoul(tcp_port, NULL, 10));
		if (mb == NULL) {
			upslogx(LOG_ERR, "modbus_new_tcp: Unable to connect to %s\n", port);
		}
		free(tcp_port);
	} else {
		mb = modbus_new_tcp(port, 502);
		if (mb == NULL) {
			upslogx(LOG_ERR, "modbus_new_tcp: Unable to connect to %s\n", port);
		}
	}
	return mb;
}

/* reconnect to modbus server upon connection error */
void modbus_reconnect(void)
{
	int rval;

	upsdebugx(2, "modbus_reconnect, trying to reconnect to modbus server");
	dstate_setinfo("driver.state", "reconnect.trying");

	/* clear current modbus context */
	modbus_close(mbctx);
	modbus_free(mbctx);

	/* open communication port */
	mbctx = modbus_new(device_path);
	if (mbctx == NULL) {
		fatalx(EXIT_FAILURE, "modbus_new_rtu: Unable to open communication port context");
	}

	/* set slave ID */
	rval = modbus_set_slave(mbctx, rio_slave_id);
	if (rval < 0) {
		modbus_free(mbctx);
		fatalx(EXIT_FAILURE, "modbus_set_slave: Invalid modbus slave ID %d", rio_slave_id);
	}

	/* connect to modbus device  */
	if (modbus_connect(mbctx) == -1) {
		modbus_free(mbctx);
		fatalx(EXIT_FAILURE, "modbus_connect: unable to connect: %s", modbus_strerror(errno));
	}

	/* set modbus response timeout */
#if (defined NUT_MODBUS_TIMEOUT_ARG_sec_usec_uint32) || (defined NUT_MODBUS_TIMEOUT_ARG_sec_usec_uint32_cast_timeval_fields)
	rval = modbus_set_response_timeout(mbctx, mod_resp_to_s, mod_resp_to_us);
	if (rval < 0) {
		modbus_free(mbctx);
		fatalx(EXIT_FAILURE, "modbus_set_response_timeout: error(%s)", modbus_strerror(errno));
	}
#elif (defined NUT_MODBUS_TIMEOUT_ARG_timeval_numeric_fields)
	{   /* see comments above */
		struct timeval to;
		memset(&to, 0, sizeof(struct timeval));
		to.tv_sec = mod_resp_to_s;
		to.tv_usec = mod_resp_to_us;
		/* void */ modbus_set_response_timeout(mbctx, &to);
	}
/* #elif (defined NUT_MODBUS_TIMEOUT_ARG_timeval) // some un-castable type in fields */
#endif /* NUT_MODBUS_TIMEOUT_ARG_* */

	/* set modbus byte timeout */
#if (defined NUT_MODBUS_TIMEOUT_ARG_sec_usec_uint32) || (defined NUT_MODBUS_TIMEOUT_ARG_sec_usec_uint32_cast_timeval_fields)
	rval = modbus_set_byte_timeout(mbctx, mod_byte_to_s, mod_byte_to_us);
	if (rval < 0) {
		modbus_free(mbctx);
		fatalx(EXIT_FAILURE, "modbus_set_byte_timeout: error(%s)", modbus_strerror(errno));
	}
#elif (defined NUT_MODBUS_TIMEOUT_ARG_timeval_numeric_fields)
	{   /* see comments above */
		struct timeval to;
		memset(&to, 0, sizeof(struct timeval));
		to.tv_sec = mod_byte_to_s;
		to.tv_usec = mod_byte_to_us;
		/* void */ modbus_set_byte_timeout(mbctx, &to);
	}
/* #elif (defined NUT_MODBUS_TIMEOUT_ARG_timeval) // some un-castable type in fields */
#endif /* NUT_MODBUS_TIMEOUT_ARG_* */

	dstate_setinfo("driver.state", "quiet");
}
