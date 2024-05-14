/*	adelsystem_cbi.c - driver for ADELSYSTEM CB/CBI DC-UPS
 *
 *	Copyright (C)
 *	  2022 Dimitris Economou <dimitris.s.economou@gmail.com>
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

/*
 *	code indentation with tabstop=4
 */

#include "main.h"
#include "adelsystem_cbi.h"
#include <modbus.h>
#include <timehead.h>

#define DRIVER_NAME "NUT ADELSYSTEM DC-UPS CB/CBI driver"
#define DRIVER_VERSION "0.03"

/* variables */
static modbus_t *mbctx = NULL;							/* modbus memory context */
static devstate_t *dstate = NULL;						/* device state context */
static int errcnt = 0;									/* modbus access error counter */
static char *device_mfr = DEVICE_MFR;					/* device manufacturer */
static char *device_model = DEVICE_MODEL;				/* device model */
static char *device_type = DEVICE_TYPE_STRING;				/* device type (e.g. UPS, PDU...) */
static int ser_baud_rate = BAUD_RATE;					/* serial port baud rate */
static char ser_parity = PARITY;						/* serial port parity */
static int ser_data_bit = DATA_BIT;						/* serial port data bit */
static int ser_stop_bit = STOP_BIT;						/* serial port stop bit */
static int dev_slave_id = MODBUS_SLAVE_ID;				/* set device ID to default value */
static uint32_t mod_resp_to_s = MODRESP_TIMEOUT_s;		/* set the modbus response time out (s) */
static uint32_t mod_resp_to_us = MODRESP_TIMEOUT_us;	/* set the modbus response time out (us) */
static uint32_t mod_byte_to_s = MODBYTE_TIMEOUT_s;		/* set the modbus byte time out (us) */
static uint32_t mod_byte_to_us = MODBYTE_TIMEOUT_us;	/* set the modbus byte time out (us) */


/* initialize alarm structs */
void alrminit(void);

/* initialize register start address and hex address from register number */
void reginit(void);

/* read registers' memory region */
int read_all_regs(modbus_t *mb, uint16_t *data);

/* get config vars set by -x or defined in ups.conf driver section */
void get_config_vars(void);

/* get device state */
int get_dev_state(devreg_t regindx, devstate_t **dvstat);

/* create a new modbus context based on connection type (serial or TCP) */
modbus_t *modbus_new(const char *port);

/* reconnect upon communication error */
void modbus_reconnect(void);

/* modbus register read function */
int register_read(modbus_t *mb, int addr, regtype_t type, void *data);

/* modbus register write function */
int register_write(modbus_t *mb, int addr, regtype_t type, void *data);

/* instant command triggered by upsd */
int upscmd(const char *cmd, const char *arg);

/* count the time elapsed since start */
long time_elapsed(struct timeval *start);


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

/* read configuration variables from ups.conf and connect to ups device */
void upsdrv_initups(void)
{
	int rval;
	upsdebugx(2, "upsdrv_initups");

	dstate = (devstate_t *)xmalloc(sizeof(devstate_t));
	alrminit();
	reginit();
	get_config_vars();

	/* open communication port */
	mbctx = modbus_new(device_path);
	if (mbctx == NULL) {
		fatalx(EXIT_FAILURE, "modbus_new_rtu: Unable to open communication port context");
	}

	/* set slave ID */
	rval = modbus_set_slave(mbctx, dev_slave_id);
	if (rval < 0) {
		modbus_free(mbctx);
		fatalx(EXIT_FAILURE, "modbus_set_slave: Invalid modbus slave ID %d", dev_slave_id);
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
	{	/* see comments above */
		struct timeval to;
		memset(&to, 0, sizeof(struct timeval));
		to.tv_sec = mod_byte_to_s;
		to.tv_usec = mod_byte_to_us;
		/* void */ modbus_set_byte_timeout(mbctx, &to);
	}
/* #elif (defined NUT_MODBUS_TIMEOUT_ARG_timeval) // some un-castable type in fields */
#endif /* NUT_MODBUS_TIMEOUT_ARG_* */

}

/* initialize ups driver information */
void upsdrv_initinfo(void)
{
	devstate_t *ds = dstate; /* device state context */
	upsdebugx(2, "upsdrv_initinfo");

	/* set device information */
	dstate_setinfo("device.mfr", "%s", device_mfr);
	dstate_setinfo("device.model", "%s", device_model);
	dstate_setinfo("device.type", "%s", device_type);

	/* read ups model */
	get_dev_state(PRDN, &ds);
	dstate_setinfo("ups.model", "%s", ds->product.name);
	upslogx(LOG_INFO, "ups.model = %s", ds->product.name);

	/* register instant commands */
	dstate_addcmd("load.off");

	/* set callback for instant commands */
	upsh.instcmd = upscmd;
}


/* update UPS signal state */
void upsdrv_updateinfo(void)
{
	int rval;					/* return value */
	int i;						/* local index */
	devstate_t *ds = dstate;	/* device state */

	upsdebugx(2, "upsdrv_updateinfo");

	errcnt = 0;			/* initialize error counter to zero */
	status_init();		/* initialize ups.status update */
	alarm_init();		/* initialize ups.alarm update */
#if READALL_REGS == 1
	rval = read_all_regs(mbctx, regs_data);
	if (rval == -1) {
		errcnt++;
	} else {
#endif
	/*
	 * update UPS status regarding MAINS and SHUTDOWN request
	 *	- OL:  On line (mains is present)
	 *	- OB:  On battery (mains is not present)
	 */
	rval = get_dev_state(MAIN, &ds);
	if (rval == -1) {
	   errcnt++;
	} else {
		if (ds->alrm->alrm[MAINS_AVAIL_I].actv) {
			status_set("OB");
			alarm_set(mains->alrm[MAINS_AVAIL_I].descr);
			upslogx(LOG_INFO, "ups.status = OB");
		} else {
			status_set("OL");
			upslogx(LOG_INFO, "ups.status = OL");
		}
		if (ds->alrm->alrm[SHUTD_REQST_I].actv) {
			status_set("FSD");
			alarm_set(mains->alrm[SHUTD_REQST_I].descr);
			upslogx(LOG_INFO, "ups.status = FSD");
		}
	}

	/*
	 * update UPS status regarding battery voltage
	 */
	rval = get_dev_state(BVAL, &ds);
	if (rval == -1) {
		errcnt++;
	} else {
		if (ds->alrm->alrm[BVAL_LOALRM_I].actv) {
			status_set("LB");
			alarm_set(bval->alrm[BVAL_LOALRM_I].descr);
			upslogx(LOG_INFO, "ups.status = LB");
		}
		if (ds->alrm->alrm[BVAL_HIALRM_I].actv) {
			status_set("HB");
			alarm_set(bval->alrm[BVAL_HIALRM_I].descr);
			upslogx(LOG_INFO, "ups.status = HB");
		}
		if (ds->alrm->alrm[BVAL_BSTSFL_I].actv) {
			alarm_set(bval->alrm[BVAL_BSTSFL_I].descr);
			upslogx(LOG_INFO, "battery start with battery flat");
		}
	}

	/* get "battery.voltage" */
	rval = get_dev_state(BATV, &ds);
	if (rval == -1) {
		errcnt++;
	} else {
		dstate_setinfo("battery.voltage", "%s", ds->reg.strval);
		upslogx(LOG_DEBUG, "battery.voltage = %s", ds->reg.strval);
	}
	/*
	 * update UPS status regarding battery charger status
	 */

	/* get "battery.charger.status" */
	rval = get_dev_state(CHRG, &ds);
	if (rval == -1) {
		errcnt++;
	} else {
		if (ds->charge.state == CHRG_BULK || ds->charge.state == CHRG_ABSR) {
			status_set("CHRG");
			upslogx(LOG_INFO, "ups.status = CHRG");
		}
		dstate_setinfo("battery.charger.status", "%s", ds->charge.info);
		upslogx(LOG_DEBUG, "battery.charger.status = %s", ds->charge.info);
	}
	rval = get_dev_state(PMNG, &ds);
	if (rval == -1) {
		errcnt++;
	} else {
		if (ds->power.state == PMNG_BCKUP) {
			status_set("DISCHRG");
			dstate_setinfo("battery.charger.status", "discharging");
			upslogx(LOG_INFO, "ups.status = DISCHRG");
		}
		if (ds->power.state == PMNG_BOOST) {
			status_set("BOOST");
			upslogx(LOG_INFO, "ups.status = BOOST");
		}
	}

	/*
	 * update UPS battery state of charge
	 */
	rval = get_dev_state(BSOC, &ds);
	if (rval == -1) {
		errcnt++;
	} else {
		dstate_setinfo("battery.charge", "%s", ds->reg.strval);
		upslogx(LOG_DEBUG, "battery.charge = %s", ds->reg.strval);
	}

	/*
	 * update UPS AC input state
	 */
	rval = get_dev_state(VACA, &ds);
	if (rval == -1) {
		errcnt++;
	} else {
		for (i = 0; i < ds->alrm->alrm_c; i++) {
			if (ds->alrm->alrm[i].actv) {
				alarm_set(ds->alrm->alrm[i].descr);
				upsdebugx(3, "%s is active", ds->alrm->alrm[i].descr);
			}
		}
	}
	rval = get_dev_state(VAC, &ds);
	if (rval == -1) {
		errcnt++;
	} else {
		dstate_setinfo("input.voltage", "%s", ds->reg.strval);
		upslogx(LOG_DEBUG, "input.voltage = %s", ds->reg.strval);
	}

	/*
	 * update UPS onboard temperature state
	 */
	rval = get_dev_state(OBTA, &ds);
	if (rval == -1) {
		errcnt++;
	} else {
		for (i = 0; i < ds->alrm->alrm_c; i++) {
			if (ds->alrm->alrm[i].actv) {
				alarm_set(ds->alrm->alrm[i].descr);
				upsdebugx(3, "%s is active", ds->alrm->alrm[i].descr);
			}
		}
	}
	rval = get_dev_state(OTMP, &ds);
	if (rval == -1) {
		errcnt++;
	} else {
		dstate_setinfo("ups.temperature", "%s", ds->reg.strval);
		upslogx(LOG_DEBUG, "ups.temperature = %s", ds->reg.strval);
	}
	/*
	 * update UPS battery temperature state
	 */
	rval = get_dev_state(BSTA, &ds);
	if (rval == -1) {
		errcnt++;
	} else {
		for (i = 0; i < ds->alrm->alrm_c; i++) {
			if (ds->alrm->alrm[i].actv) {
				alarm_set(ds->alrm->alrm[i].descr);
				upsdebugx(3, "%s alarm is active", ds->alrm->alrm[i].descr);
			}
		}
	}
	rval = get_dev_state(BTMP, &ds);
	if (rval == -1) {
		errcnt++;
	} else {
		dstate_setinfo("battery.temperature", "%s", ds->reg.strval);
		upslogx(LOG_DEBUG, "battery.temperature = %s", ds->reg.strval);
	}
	rval = get_dev_state(TBUF, &ds);
	if (rval == -1) {
		errcnt++;
	} else {
		dstate_setinfo("battery.runtime", "%s", ds->reg.strval);
		upslogx(LOG_DEBUG, "battery.runtime = %s", ds->reg.strval);
	}

	/*
	 * update UPS device failure state
	 */
	rval = get_dev_state(DEVF, &ds);
	if (rval == -1) {
		errcnt++;
	} else {
		for (i = 0; i < ds->alrm->alrm_c; i++) {
			if (ds->alrm->alrm[i].actv) {
				alarm_set(ds->alrm->alrm[i].descr);
				upsdebugx(3, "%s alarm is active", ds->alrm->alrm[i].descr);
			}
		}
	}

	/*
	 * update UPS SoH and SoC states
	 */
	rval = get_dev_state(SCSH, &ds);
	if (rval == -1) {
		errcnt++;
	} else {
		for (i = 0; i < ds->alrm->alrm_c; i++) {
			if (ds->alrm->alrm[i].actv) {
				alarm_set(ds->alrm->alrm[i].descr);
				upsdebugx(3, "%s alarm is active", ds->alrm->alrm[i].descr);
			}
		}
	}

	/*
	 * update UPS battery state
	 */
	rval = get_dev_state(BSTA, &ds);
	if (rval == -1) {
		errcnt++;
	} else {
		for (i = 0; i < ds->alrm->alrm_c; i++) {
			if (ds->alrm->alrm[i].actv) {
				alarm_set(ds->alrm->alrm[i].descr);
				upsdebugx(3, "%s alarm is active", ds->alrm->alrm[i].descr);
			}
		}
	}

	/*
	 * update UPS load status
	 */
	rval = get_dev_state(LVDC, &ds);
	if (rval == -1) {
		errcnt++;
	} else {
		dstate_setinfo("output.voltage", "%s", ds->reg.strval);
		upslogx(LOG_DEBUG, "output.voltage = %s", ds->reg.strval);
	}
	rval = get_dev_state(LCUR, &ds);
	if (rval == -1) {
		errcnt++;
	} else {
		dstate_setinfo("output.current", "%s", ds->reg.strval);
		upslogx(LOG_DEBUG, "output.current = %s", ds->reg.strval);
	}
#if READALL_REGS == 1
	}
#endif
	/* check for communication errors */
	if (errcnt == 0) {
		alarm_commit();
		status_commit();
		dstate_dataok();
	} else {
		upsdebugx(2, "Communication errors: %d", errcnt);
		dstate_datastale();
	}
}

/* shutdown UPS */
void upsdrv_shutdown(void)
{
	int rval;
	int cnt = FSD_REPEAT_CNT;	 /* shutdown repeat counter */
	struct timeval start;
	long etime;

	/* retry sending shutdown command on error */
	while ((rval = upscmd("load.off", NULL)) != STAT_INSTCMD_HANDLED && cnt > 0) {
		rval = gettimeofday(&start, NULL);
		if (rval < 0) {
			upslogx(LOG_ERR, "upscmd: gettimeofday: %s", strerror(errno));
		}

		/* wait for an increasing time interval before sending shutdown command */
		while ((etime = time_elapsed(&start)) < ( FSD_REPEAT_INTRV / cnt));
		upsdebugx(2, "ERROR: load.off failed, wait for %lims, retries left: %d\n", etime, cnt - 1);
		cnt--;
	}
	switch (rval) {
		case STAT_INSTCMD_FAILED:
		case STAT_INSTCMD_INVALID:
			fatalx(EXIT_FAILURE, "shutdown failed");
		case STAT_INSTCMD_UNKNOWN:
			fatalx(EXIT_FAILURE, "shutdown not supported");
		default:
			break;
	}
	upslogx(LOG_INFO, "shutdown command executed");
}

/* print driver usage info */
void upsdrv_help(void)
{
}

/* list flags and values that you want to receive via -x */
void upsdrv_makevartable(void)
{
	addvar(VAR_VALUE, "ser_baud_rate", "serial port baud rate");
	addvar(VAR_VALUE, "ser_parity", "serial port parity");
	addvar(VAR_VALUE, "ser_data_bit", "serial port data bit");
	addvar(VAR_VALUE, "ser_stop_bit", "serial port stop bit");
	addvar(VAR_VALUE, "dev_slave_id", "device modbus slave ID");
	addvar(VAR_VALUE, "mod_resp_to_s", "modbus response timeout (s)");
	addvar(VAR_VALUE, "mod_resp_to_us", "modbus response timeout (us)");
	addvar(VAR_VALUE, "mod_byte_to_s", "modbus byte timeout (s)");
	addvar(VAR_VALUE, "mod_byte_to_us", "modbus byte timeout (us)");
}

/* close modbus connection and free modbus context allocated memory */
void upsdrv_cleanup(void)
{
	if (mbctx != NULL) {
		modbus_close(mbctx);
		modbus_free(mbctx);
	}
	if (dstate != NULL) {
		free(dstate);
	}
}

/*
 * driver support functions
 */

/* initialize alarm structs */
void alrminit(void)
{
	mains = alloc_alrm_ar(mains_c, sizeof(mains_ar));
	alrm_ar_init(mains, mains_ar, mains_c);
	vaca = alloc_alrm_ar(vaca_c, sizeof(vaca_ar));
	alrm_ar_init(vaca, vaca_ar, vaca_c);
	devf = alloc_alrm_ar(devf_c, sizeof(devf_ar));
	alrm_ar_init(devf, devf_ar, devf_c);
	btsf = alloc_alrm_ar(btsf_c, sizeof(btsf_ar));
	alrm_ar_init(btsf, btsf_ar, btsf_c);
	bval = alloc_alrm_ar(bval_c, sizeof(bval_ar));
	alrm_ar_init(bval, bval_ar, bval_c);
	shsc = alloc_alrm_ar(shsc_c, sizeof(shsc_ar));
	alrm_ar_init(shsc, shsc_ar, shsc_c);
	bsta = alloc_alrm_ar(bsta_c, sizeof(bsta_ar));
	alrm_ar_init(bsta, bsta_ar, bsta_c);
	obta = alloc_alrm_ar(obta_c, sizeof(obta_ar));
	alrm_ar_init(obta, obta_ar, obta_c);
}

/* initialize register start address and hex address from register number */
void reginit(void)
{
	int i; /* local index */

	for (i = 0; i < MODBUS_NUMOF_REGS; i++) {
		int rnum = regs[i].num;
		switch (regs[i].type) {
			case COIL:
				regs[i].saddr = rnum - 1;
				regs[i].xaddr = 0x0 + regs[i].num - 1;
				break;
			case INPUT_B:
				rnum -= 10000;
				regs[i].saddr = rnum - 1;
				regs[i].xaddr = 0x10000 + rnum - 1;
				break;
			case INPUT_R:
				rnum -= 30000;
				regs[i].saddr = rnum - 1;
				regs[i].xaddr = 0x30000 + rnum - 1;
				break;
			case HOLDING:
				rnum -= 40000;
				regs[i].saddr = rnum - 1;
				regs[i].xaddr = 0x40000 + rnum - 1;
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
				upslogx(LOG_ERR,
						"Invalid register type %d for register %d",
						regs[i].type,
						regs[i].num
				);
				upsdebugx(3,
						  "Invalid register type %d for register %d",
						  regs[i].type,
						  regs[i].num
				);			
#ifdef __clang__
# pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic pop
#endif
		}
		upsdebugx(3,
				  "reginit: num:%d, type: %d saddr: %d, xaddr: 0x%x",
				  regs[i].num,
				  regs[i].type,
				  regs[i].saddr,
				  regs[i].xaddr
		);
	}
}

/* read registers' memory region */
int read_all_regs(modbus_t *mb, uint16_t *data)
{
	int rval;

	/* read all HOLDING registers */
	rval = modbus_read_registers(mb, regs[H_REG_STARTIDX].xaddr, MAX_H_REGS, data);
	if (rval == -1) {
		upslogx(LOG_ERR,
				"ERROR:(%s) modbus_read: addr:0x%x, length:%8d, path:%s\n",
				modbus_strerror(errno),
				regs[H_REG_STARTIDX].xaddr,
				MAX_H_REGS,
				device_path
		);

		/* on BROKEN PIPE, INVALID CRC and INVALID DATA error try to reconnect */
		if (errno == EPIPE || errno == EMBBADDATA || errno == EMBBADCRC) {
			upsdebugx(1, "register_read: error(%s)", modbus_strerror(errno));
			modbus_reconnect();
		}
	}

	/* no COIL, INPUT_B or INPUT_R register regions to read */

	return rval;
}

/* Read a modbus register */
int register_read(modbus_t *mb, int addr, regtype_t type, void *data)
{
	int rval = -1;

	/* register bit masks */
	uint16_t mask8 = 0x00FF;
	uint16_t mask16 = 0xFFFF;

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
			upsdebugx(2,"ERROR: register_read: invalid register type %d\n", type);
			break;
#ifdef __clang__
# pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic pop
#endif
	}
	if (rval == -1) {
		upslogx(LOG_ERR,
				"ERROR:(%s) modbus_read: addr:0x%x, type:%8s, path:%s\n",
				modbus_strerror(errno),
				addr,
				(type == COIL) ? "COIL" :
				(type == INPUT_B) ? "INPUT_B" :
				(type == INPUT_R) ? "INPUT_R" : "HOLDING",
				device_path
		);

		/* on BROKEN PIPE, INVALID CRC and INVALID DATA error try to reconnect */
		if (errno == EPIPE || errno == EMBBADDATA || errno == EMBBADCRC) {
			upsdebugx(1, "register_read: error(%s)", modbus_strerror(errno));
			modbus_reconnect();
		}
	}
	upsdebugx(3, "register addr: 0x%x, register type: %d read: %u",addr, type, *(unsigned int *)data);
	return rval;
}

/* write a modbus register */
int register_write(modbus_t *mb, int addr, regtype_t type, void *data)
{
	int rval = -1;

	/* register bit masks */
	uint16_t mask8 = 0x00FF;
	uint16_t mask16 = 0xFFFF;

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
			upsdebugx(2,"ERROR: register_write: invalid register type %d\n", type);
			break;
	}
	if (rval == -1) {
		upslogx(LOG_ERR,
				"ERROR:(%s) modbus_write: addr:0x%x, type:%8s, path:%s\n",
				modbus_strerror(errno),
				addr,
				(type == COIL) ? "COIL" :
				(type == INPUT_B) ? "INPUT_B" :
				(type == INPUT_R) ? "INPUT_R" : "HOLDING",
				device_path
		);

		/* on BROKEN PIPE error try to reconnect */
		if (errno == EPIPE) {
			upsdebugx(1, "register_write: error(%s)", modbus_strerror(errno));
			modbus_reconnect();
		}
	}
	upsdebugx(3, "register addr: 0x%x, register type: %d read: %u",addr, type, *(unsigned int *)data);
	return rval;
}

/* returns the time elapsed since start in milliseconds */
long time_elapsed(struct timeval *start)
{
	long rval;
	struct timeval end;

	rval = gettimeofday(&end, NULL);
	if (rval < 0) {
		upslogx(LOG_ERR, "time_elapsed: %s", strerror(errno));
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

	if (!strcasecmp(cmd, "load.off")) {
		data = 1;
		rval = register_write(mbctx, regs[FSD].xaddr, regs[FSD].type, &data);
		if (rval == -1) {
			upslogx(2,
					"ERROR:(%s) modbus_write_register: addr:0x%08x, regtype: %d, path:%s\n",
					modbus_strerror(errno),
					regs[FSD].xaddr,
					regs[FSD].type,
					device_path
			);
			upslogx(LOG_NOTICE, "load.off: failed (communication error) [%s] [%s]", cmd, arg);
			rval = STAT_INSTCMD_FAILED;
		} else {
			upsdebugx(2, "load.off: addr: 0x%x, data: %d", regs[FSD].xaddr, data);
			rval = STAT_INSTCMD_HANDLED;
		}
	} else {
		upslogx(LOG_NOTICE, "instcmd: unknown command [%s] [%s]", cmd, arg);
		rval = STAT_INSTCMD_UNKNOWN;
	}
	return rval;
}

/* read device state, returns 0 on success or -1 on communication error
   it formats state depending on register semantics */
int get_dev_state(devreg_t regindx, devstate_t **dvstat)
{
	int i;							/* local index */
	int n;
	int rval;						/* return value */
	static char *ptr = NULL;		/* temporary pointer */
	unsigned int reg_val;					/* register value */
#if READALL_REGS == 0
	unsigned int num;						/* register number */
	regtype_t rtype;				/* register type */
	int addr;						/* register address */
#endif
	devstate_t *state;				/* device state */

	state = *dvstat;
#if READALL_REGS == 1
	reg_val = regs_data[regindx];
	rval = 0;
#elif READALL_REGS == 0
	num = regs[regindx].num;
	addr = regs[regindx].xaddr;
	rtype = regs[regindx].type;
	rval = register_read(mbctx, addr, rtype, &reg_val);
	if (rval == -1) {
		return rval;
	}
	upsdebugx(3,
			  "get_dev_state: num: %d, addr: 0x%x, regtype: %d, data: %d",
			  num,
			  addr,
			  rtype,
			  reg_val
	);
#endif
	/* process register data */
	switch (regindx) {
		case CHRG:					/* "ups.charge" */
			if (reg_val == CHRG_NONE) {
				state->charge.state = CHRG_NONE;
				state->charge.info = chrgs_i[CHRG_NONE];
			} else if (reg_val == CHRG_RECV) {
				state->charge.state = CHRG_RECV;
				state->charge.info = chrgs_i[CHRG_RECV];
			} else if (reg_val == CHRG_BULK) {
				state->charge.state = CHRG_BULK;
				state->charge.info = chrgs_i[CHRG_BULK];
			} else if (reg_val == CHRG_ABSR) {
				state->charge.state = CHRG_ABSR;
				state->charge.info = chrgs_i[CHRG_ABSR];
			} else if (reg_val == CHRG_FLOAT) {
				state->charge.state = CHRG_FLOAT;
				state->charge.info = chrgs_i[CHRG_FLOAT];
			}
			upsdebugx(3, "get_dev_state: charge.state: %s", state->charge.info);
			break;
		case BATV:					/* "battery.voltage" */
		case LVDC:					/* "output.voltage" */
		case LCUR:					/* "output.current" */
			if (reg_val != 0) {
				char	*fval_s;
				double	fval;

				state->reg.val.ui16 = reg_val;
				fval = reg_val / 1000.00; /* convert mV to V, mA to A */
				n = snprintf(NULL, 0, "%.2f", fval);
				if (ptr != NULL) {
					free(ptr);
				}
				fval_s = (char *)xmalloc(sizeof(char) * (n + 1));
				ptr = fval_s;
				sprintf(fval_s, "%.2f", fval);
				state->reg.strval = fval_s;
			} else {
				state->reg.val.ui16 = 0;
				state->reg.strval = "0.00";
			}
			upsdebugx(3, "get_dev_state: variable: %s", state->reg.strval);
			break;
		case TBUF:
		case BSOH:
		case BCEF:
		case VAC:					/* "input.voltage" */
			if (reg_val != 0) {
				char	*reg_val_s;

				state->reg.val.ui16 = reg_val;
				n = snprintf(NULL, 0, "%d", reg_val);
				if (ptr != NULL) {
					free(ptr);
				}
				reg_val_s = (char *)xmalloc(sizeof(char) * (n + 1));
				ptr = reg_val_s;
				sprintf(reg_val_s, "%d", reg_val);
				state->reg.strval = reg_val_s;
			} else {
				state->reg.val.ui16 = 0;
				state->reg.strval = "0";
			}
			upsdebugx(3, "get_dev_state: variable: %s", state->reg.strval);
			break;
		case BSOC:					/* "battery.charge" */
			if (reg_val != 0) {
				double	fval;
				char	*fval_s;

				state->reg.val.ui16 = reg_val;
				fval = (double )reg_val * regs[BSOC].scale;
				n = snprintf(NULL, 0, "%.2f", fval);
				if (ptr != NULL) {
					free(ptr);
				}
				fval_s = (char *)xmalloc(sizeof(char) * (n + 1));
				ptr = fval_s;
				sprintf(fval_s, "%.2f", fval);
				state->reg.strval = fval_s;
			} else {
				state->reg.val.ui16 = 0;
				state->reg.strval = "0.00";
			}
			upsdebugx(3, "get_dev_state: variable: %s", state->reg.strval);
			break;
		case BTMP:					/* "battery.temperature" */
		case OTMP:					/* "ups.temperature" */
			{ /* scoping */
				double	fval;
				char	*fval_s;

				state->reg.val.ui16 = reg_val;
				fval = reg_val - 273.15;
				n = snprintf(NULL, 0, "%.2f", fval);
				fval_s = (char *)xmalloc(sizeof(char) * (n + 1));
				if (ptr != NULL) {
					free(ptr);
				}
				ptr = fval_s;
				sprintf(fval_s, "%.2f", fval);
				state->reg.strval = fval_s;
			}
			upsdebugx(3, "get_dev_state: variable: %s", state->reg.strval);
			break;
		case PMNG:					/* "ups.status" & "battery.charge" */
			if (reg_val == PMNG_BCKUP) {
				state->power.state = PMNG_BCKUP;
				state->power.info = pwrmng_i[PMNG_BCKUP];
			} else if (reg_val == PMNG_CHRGN) {
				state->power.state = PMNG_CHRGN;
				state->power.info = pwrmng_i[PMNG_CHRGN];
			} else if (reg_val == PMNG_BOOST) {
				state->power.state = PMNG_BOOST;
				state->power.info = pwrmng_i[PMNG_BOOST];
			} else if (reg_val == PMNG_NCHRG) {
				state->power.state = PMNG_NCHRG;
				state->power.info = pwrmng_i[PMNG_NCHRG];
			}
			upsdebugx(3, "get_dev_state: power.state: %s", state->reg.strval);
			break;
		case PRDN:					/* "ups.model" */
			for (i = 0; i < DEV_NUMOF_MODELS; i++) {
				if (prdnm_i[i].val == reg_val) {
					break;
				}
			}
			state->product.val = reg_val;
			state->product.name = prdnm_i[i].name;
			upsdebugx(3, "get_dev_state: product.name: %s", state->product.name);
			break;
		case BSTA:
			if (reg_val & BSTA_REVPOL_M) {
				bsta->alrm[BSTA_REVPOL_I].actv = 1;
			} else {
				bsta->alrm[BSTA_REVPOL_I].actv = 0;
			}
			if (reg_val & BSTA_NOCNND_M) {
				bsta->alrm[BSTA_NOCNND_I].actv = 1;
			} else {
				bsta->alrm[BSTA_NOCNND_I].actv = 0;
			}
			if (reg_val & BSTA_CLSHCR_M) {
				bsta->alrm[BSTA_CLSHCR_I].actv = 1;
			} else {
				bsta->alrm[BSTA_CLSHCR_I].actv = 0;
			}
			if (reg_val & BSTA_SULPHD_M) {
				bsta->alrm[BSTA_SULPHD_I].actv = 1;
			} else {
				bsta->alrm[BSTA_SULPHD_I].actv = 0;
			}
			if (reg_val & BSTA_CHEMNS_M) {
				bsta->alrm[BSTA_CHEMNS_I].actv = 1;
			} else {
				bsta->alrm[BSTA_CHEMNS_I].actv = 0;
			}
			if (reg_val & BSTA_CNNFLT_M) {
				bsta->alrm[BSTA_CNNFLT_I].actv = 1;
			} else {
				bsta->alrm[BSTA_CNNFLT_I].actv = 0;
			}
			state->alrm = bsta;
			break;
		case SCSH:
			if (reg_val & SHSC_HIRESI_M) {
				shsc->alrm[SHSC_HIRESI_I].actv = 1;
			} else {
				shsc->alrm[SHSC_HIRESI_I].actv = 0;
			}
			if (reg_val & SHSC_LOCHEF_M) {
				shsc->alrm[SHSC_LOCHEF_I].actv = 1;
			} else {
				shsc->alrm[SHSC_LOCHEF_I].actv = 0;
			}
			if (reg_val & SHSC_LOEFCP_M) {
				shsc->alrm[SHSC_LOEFCP_I].actv = 1;
			} else {
				shsc->alrm[SHSC_LOEFCP_I].actv = 0;
			}
			if (reg_val & SHSC_LOWSOC_M) {
				shsc->alrm[SHSC_LOWSOC_I].actv = 1;
			} else {
				shsc->alrm[SHSC_LOWSOC_I].actv = 0;
			}
			state->alrm = shsc;
			break;
		case BVAL:
			if (reg_val & BVAL_HIALRM_M) {
				bval->alrm[BVAL_HIALRM_I].actv = 1;
			} else {
				bval->alrm[BVAL_HIALRM_I].actv = 0;
			}
			if (reg_val & BVAL_LOALRM_M) {
				bval->alrm[BVAL_LOALRM_I].actv = 1;
			} else {
				bval->alrm[BVAL_LOALRM_I].actv = 0;
			}
			if (reg_val & BVAL_BSTSFL_M) {
				bval->alrm[BVAL_BSTSFL_I].actv = 1;
			} else {
				bval->alrm[BVAL_BSTSFL_I].actv = 0;
			}
			state->alrm = bval;
			break;
		case BTSF:
			if (reg_val & BTSF_FCND_M) {
				btsf->alrm[BTSF_FCND_I].actv = 1;
			} else {
				btsf->alrm[BTSF_FCND_I].actv = 0;
			}
			if (reg_val & BTSF_NCND_M) {
				btsf->alrm[BTSF_NCND_I].actv = 1;
			} else {
				btsf->alrm[BTSF_NCND_I].actv = 0;
			}
			state->alrm = btsf;
			break;
		case DEVF:
			if (reg_val & DEVF_RCALRM_M) {
				devf->alrm[DEVF_RCALRM_I].actv = 1;
			} else {
				devf->alrm[DEVF_RCALRM_I].actv = 0;
			}
			if (reg_val & DEVF_INALRM_M) {
				devf->alrm[DEVF_INALRM_I].actv = 1;
			} else {
				devf->alrm[DEVF_INALRM_I].actv = 0;
			}
			if (reg_val & DEVF_LFNAVL_M) {
				devf->alrm[DEVF_LFNAVL_I].actv = 1;
			} else {
				devf->alrm[DEVF_LFNAVL_I].actv = 0;
			}
			state->alrm = devf;
			break;
		case VACA:
			if (reg_val & VACA_HIALRM_M) {
				vaca->alrm[VACA_HIALRM_I].actv = 1;
			} else {
				vaca->alrm[VACA_HIALRM_I].actv = 0;
			}
			if (reg_val == VACA_LOALRM_M) {
				vaca->alrm[VACA_LOALRM_I].actv = 1;
			} else {
				vaca->alrm[VACA_LOALRM_I].actv = 0;
			}
			state->alrm = vaca;
			break;
		case MAIN:
			if (reg_val & MAINS_AVAIL_M) {
				mains->alrm[MAINS_AVAIL_I].actv = 1;
			} else {
				mains->alrm[MAINS_AVAIL_I].actv = 0;
			}
			if (reg_val == SHUTD_REQST_M) {
				mains->alrm[SHUTD_REQST_I].actv = 1;
			} else {
				mains->alrm[SHUTD_REQST_I].actv = 0;
			}
			state->alrm = mains;
			break;
		case OBTA:
			if (reg_val == OBTA_HIALRM_V) {
				obta->alrm[OBTA_HIALRM_I].actv = 1;
			}
			state->alrm = obta;
			break;
		case BINH:
		case FSD:
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
			{ /* scoping */
				char	*reg_val_s;
				state->reg.val.ui16 = reg_val;
				n = snprintf(NULL, 0, "%d", reg_val);
				if (ptr != NULL) {
					free(ptr);
				}
				reg_val_s = (char *)xmalloc(sizeof(char) * (n + 1));
				ptr = reg_val_s;
				sprintf(reg_val_s, "%d", reg_val);
				state->reg.strval = reg_val_s;
			}
			break;
#ifdef __clang__
# pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic pop
#endif
	}
	return rval;
}

/* get driver configuration parameters */
void get_config_vars(void)
{

	/* check if serial baud rate is set and get the value */
	if (testvar("ser_baud_rate")) {
		ser_baud_rate = (int)strtol(getval("ser_baud_rate"), NULL, 10);
	}
	upsdebugx(2, "ser_baud_rate %d", ser_baud_rate);

	/* check if serial parity is set and get the value */
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

	/* check if serial data bit is set and get the value */
	if (testvar("ser_data_bit")) {
		ser_data_bit = (int)strtol(getval("ser_data_bit"), NULL, 10);
	}
	upsdebugx(2, "ser_data_bit %d", ser_data_bit);

	/* check if serial stop bit is set and get the value */
	if (testvar("ser_stop_bit")) {
		ser_stop_bit = (int)strtol(getval("ser_stop_bit"), NULL, 10);
	}
	upsdebugx(2, "ser_stop_bit %d", ser_stop_bit);

	/* check if device ID is set and get the value */
	if (testvar("dev_slave_id")) {
		dev_slave_id = (int)strtol(getval("dev_slave_id"), NULL, 10);
	}
	upsdebugx(2, "dev_slave_id %d", dev_slave_id);

	/* check if response time out (s) is set and get the value */
	if (testvar("mod_resp_to_s")) {
		mod_resp_to_s = (uint32_t)strtol(getval("mod_resp_to_s"), NULL, 10);
	}
	upsdebugx(2, "mod_resp_to_s %d", mod_resp_to_s);

	/* check if response time out (us) is set and get the value */
	if (testvar("mod_resp_to_us")) {
		mod_resp_to_us = (uint32_t) strtol(getval("mod_resp_to_us"), NULL, 10);
		if (mod_resp_to_us > 999999) {
			fatalx(EXIT_FAILURE, "get_config_vars: Invalid mod_resp_to_us %d", mod_resp_to_us);
		}
	}
	upsdebugx(2, "mod_resp_to_us %d", mod_resp_to_us);

	/* check if byte time out (s) is set and get the value */
	if (testvar("mod_byte_to_s")) {
		mod_byte_to_s = (uint32_t)strtol(getval("mod_byte_to_s"), NULL, 10);
	}
	upsdebugx(2, "mod_byte_to_s %d", mod_byte_to_s);

	/* check if byte time out (us) is set and get the value */
	if (testvar("mod_byte_to_us")) {
		mod_byte_to_us = (uint32_t) strtol(getval("mod_byte_to_us"), NULL, 10);
		if (mod_byte_to_us > 999999) {
			fatalx(EXIT_FAILURE, "get_config_vars: Invalid mod_byte_to_us %d", mod_byte_to_us);
		}
	}
	upsdebugx(2, "mod_byte_to_us %d", mod_byte_to_us);
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

	upsdebugx(1, "modbus_reconnect, trying to reconnect to modbus server");
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
	rval = modbus_set_slave(mbctx, dev_slave_id);
	if (rval < 0) {
		modbus_free(mbctx);
		fatalx(EXIT_FAILURE, "modbus_set_slave: Invalid modbus slave ID %d", dev_slave_id);
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
	{	/* see comments above */
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
	{	/* see comments above */
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

