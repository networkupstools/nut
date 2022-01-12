<<<<<<< HEAD
<<<<<<< HEAD
/*  adele_cbi.c - driver for ADELE CB/CBI DC-UPS
 *
 *  Copyright (C)
 *    2022 Dimitris Economou <dimitris.s.economou@gmail.com>
<<<<<<< HEAD
=======
/*  adele_cbi.c - Driver for adele CB/CBI UPS
=======
/*  adele_cbi.c - driver for ADELE CB/CBI DC-UPS
>>>>>>> structure device data, code get_dev_state, in progress
 *
 *  Copyright (C)
 *    2021 Dimitris Economou <dimitris.s.economou@gmail.com>
>>>>>>> under construction
=======
>>>>>>> ghost alarms bug fix, other bug fixes
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
#include "adele_cbi.h"
#include <modbus.h>
#include <timehead.h>

<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
#define DRIVER_NAME "NUT Adele DC-UPS CB/CBI driver"
#define DRIVER_VERSION "0.01"

/* variables */
static modbus_t *mbctx = NULL;                              /* modbus memory context */
static devstate_t *dstate = NULL;                           /* device state context */
static int errcnt = 0;                                      /* modbus access error counter */
static char *device_mfr = DEVICE_MFR;                       /* device manufacturer */
static char *device_model = DEVICE_MODEL;                   /* device model */
static char *device_type = DEVICE_TYPE;                     /* device model */
static int ser_baud_rate = BAUD_RATE;                       /* serial port baud rate */
static char ser_parity = PARITY;                            /* serial port parity */
static int ser_data_bit = DATA_BIT;                         /* serial port data bit */
static int ser_stop_bit = STOP_BIT;                         /* serial port stop bit */
static int rio_slave_id = MODBUS_SLAVE_ID;                  /* set device ID to default value */
static uint32_t mod_resp_to_s = MODRESP_TIMEOUT_s;          /* set the modbus response time out (s) */
static uint32_t mod_resp_to_us = MODRESP_TIMEOUT_us;        /* set the modbus response time out (us) */
static uint32_t mod_byte_to_s = MODBYTE_TIMEOUT_s;          /* set the modbus byte time out (us) */
static uint32_t mod_byte_to_us = MODBYTE_TIMEOUT_us;        /* set the modbus byte time out (us) */


/* initialize register start address and hex address from register number */
void reginit();
=======
#define DRIVER_NAME	"NUT Adele CBI driver"
=======
#define DRIVER_NAME	"NUT Adele DC-UPS CB/CBI driver"
>>>>>>> alrm_t, alrm_ar_t data structures, construction of upsdrv_updateinfo in progress
=======
#define DRIVER_NAME "NUT Adele DC-UPS CB/CBI driver"
>>>>>>> ghost alarms bug fix, other bug fixes
#define DRIVER_VERSION "0.01"

/* variables */
static modbus_t *mbctx = NULL;                              /* modbus memory context */
static devstate_t *dstate = NULL;                           /* device state context */
static int errcnt = 0;                                      /* modbus access error counter */
static char *device_mfr = DEVICE_MFR;                       /* device manufacturer */
static char *device_model = DEVICE_MODEL;                   /* device model */
static char *device_type = DEVICE_TYPE;                     /* device model */
static int ser_baud_rate = BAUD_RATE;                       /* serial port baud rate */
static char ser_parity = PARITY;                            /* serial port parity */
static int ser_data_bit = DATA_BIT;                         /* serial port data bit */
static int ser_stop_bit = STOP_BIT;                         /* serial port stop bit */
static int rio_slave_id = MODBUS_SLAVE_ID;                  /* set device ID to default value */
static uint32_t mod_resp_to_s = MODRESP_TIMEOUT_s;          /* set the modbus response time out (s) */
static uint32_t mod_resp_to_us = MODRESP_TIMEOUT_us;        /* set the modbus response time out (us) */
static uint32_t mod_byte_to_s = MODBYTE_TIMEOUT_s;          /* set the modbus byte time out (us) */
static uint32_t mod_byte_to_us = MODBYTE_TIMEOUT_us;        /* set the modbus byte time out (us) */

>>>>>>> under construction

/* initialize register start address and hex address from register number */
void reginit();

/* read all registers */
int read_all_regs(modbus_t *mb, uint16_t *data);

/* get config vars set by -x or defined in ups.conf driver section */
void get_config_vars(void);

<<<<<<< HEAD
<<<<<<< HEAD
/* get device state */
int get_dev_state(devreg_t regindx, devstate_t **dstate);

=======
>>>>>>> under construction
=======
/* get device state */
int get_dev_state(devreg_t regindx, devstate_t **dstate);

>>>>>>> first testing release
/* create a new modbus context based on connection type (serial or TCP) */
modbus_t *modbus_new(const char *port);

/* reconnect upon communication error */
void modbus_reconnect();

/* modbus register read function */
int register_read(modbus_t *mb, int addr, regtype_t type, void *data);

<<<<<<< HEAD
<<<<<<< HEAD
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
=======
=======
/* modbus register write function */
int register_write(modbus_t *mb, int addr, regtype_t type, void *data);

>>>>>>> first testing release
/* instant command triggered by upsd */
int upscmd(const char *cmd, const char *arg);

/* count the time elapsed since start */
long time_elapsed(struct timeval *start);


/* driver description structure */
upsdrv_info_t upsdrv_info = {
<<<<<<< HEAD
	DRIVER_NAME,
	DRIVER_VERSION,
	"Dimitris Economou <dimitris.s.economou@gmail.com>\n",
	DRV_BETA,
	{NULL}
>>>>>>> under construction
=======
    DRIVER_NAME,
    DRIVER_VERSION,
    "Dimitris Economou <dimitris.s.economou@gmail.com>\n",
    DRV_BETA,
    {NULL}
>>>>>>> ghost alarms bug fix, other bug fixes
};

/*
 * driver functions
 */

<<<<<<< HEAD
<<<<<<< HEAD
=======
>>>>>>> alrm_t, alrm_ar_t data structures, construction of upsdrv_updateinfo in progress
/* read configuration variables from ups.conf and connect to ups device */
void upsdrv_initups(void)
{
    int rval;
    upsdebugx(2, "upsdrv_initups");

<<<<<<< HEAD
<<<<<<< HEAD

    dstate = (devstate_t *)xmalloc(sizeof(devstate_t));
=======
>>>>>>> alrm_t, alrm_ar_t data structures, construction of upsdrv_updateinfo in progress
=======

    dstate = (devstate_t *)xmalloc(sizeof(devstate_t));
>>>>>>> ghost alarms bug fix, other bug fixes
    reginit();
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
    rval = modbus_set_response_timeout(mbctx, mod_resp_to_s, mod_resp_to_us);
    if (rval < 0) {
        modbus_free(mbctx);
        fatalx(EXIT_FAILURE, "modbus_set_response_timeout: error(%s)", modbus_strerror(errno));
    }

    /* set modbus response time out to 200ms */
    rval = modbus_set_byte_timeout(mbctx, mod_byte_to_s, mod_byte_to_us);
    if (rval < 0) {
        modbus_free(mbctx);
        fatalx(EXIT_FAILURE, "modbus_set_byte_timeout: error(%s)", modbus_strerror(errno));
    }
}

<<<<<<< HEAD
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
    int rval;                   /* return value */
    int i;                      /* local index */
    devstate_t *ds = dstate;    /* device state */

    upsdebugx(2, "upsdrv_updateinfo");

    errcnt = 0;         /* initialize error counter to zero */
    status_init();      /* initialize ups.status update */
    alarm_init();       /* initialize ups.alarm update */
#if READALL_REGS == 1
    rval = read_all_regs(mbctx, regs_data);
    if (rval == -1) {
        errcnt++;
    } else {
#endif
    /*
     * update UPS status regarding MAINS and SHUTDOWN request
     *  - OL:  On line (mains is present)
     *  - OB:  On battery (mains is not present)
     */
    rval = get_dev_state(MAIN, &ds);
    if (rval == -1) {
       errcnt++;
    } else {
        if (ds->alrm->alrm[MAINS_AVAIL_I].actv) {
            status_set("OB");
            alarm_set(mains.alrm[MAINS_AVAIL_I].descr);
            upslogx(LOG_INFO, "ups.status = OB");
        } else {
            status_set("OL");
            upslogx(LOG_INFO, "ups.status = OL");
        }
        if (ds->alrm->alrm[SHUTD_REQST_I].actv) {
            status_set("FSD");
            alarm_set(mains.alrm[SHUTD_REQST_I].descr);
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
            alarm_set(bval.alrm[BVAL_LOALRM_I].descr);
            upslogx(LOG_INFO, "ups.status = LB");
        }
        if (ds->alrm->alrm[BVAL_HIALRM_I].actv) {
            status_set("HB");
            alarm_set(bval.alrm[BVAL_HIALRM_I].descr);
            upslogx(LOG_INFO, "ups.status = HB");
        }
        if (ds->alrm->alrm[BVAL_BSTSFL_I].actv) {
            alarm_set(bval.alrm[BVAL_BSTSFL_I].descr);
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
        rval = get_dev_state(VAC, &ds);
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
=======
=======
>>>>>>> alrm_t, alrm_ar_t data structures, construction of upsdrv_updateinfo in progress
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
    int rval;                   /* return value */
    int i;                      /* local index */
    devstate_t *ds = dstate;    /* device state */

    upsdebugx(2, "upsdrv_updateinfo");

    errcnt = 0;         /* initialize error counter to zero */
    status_init();      /* initialize ups.status update */
    alarm_init();       /* initialize ups.alarm update */

    /*
     * update UPS status regarding MAINS and SHUTDOWN request
     *  - OL:  On line (mains is present)
     *  - OB:  On battery (mains is not present)
     */
    rval = get_dev_state(MAIN, &ds);
    if (rval == -1) {
       errcnt++;
    } else {
        if (ds->alrm->alrm[MAINS_AVAIL_I].actv) {
            status_set("OB");
            alarm_set(mains.alrm[MAINS_AVAIL_I].descr);
            upslogx(LOG_INFO, "ups.status = OB");
        } else {
            status_set("OL");
            upslogx(LOG_INFO, "ups.status = OL");
        }
        if (ds->alrm->alrm[SHUTD_REQST_I].actv) {
            status_set("FSD");
            alarm_set(mains.alrm[SHUTD_REQST_I].descr);
            upslogx(LOG_INFO, "ups.status = FSD");
        }
    }

<<<<<<< HEAD
    /*
     * update UPS status regarding battery voltage
     */
    rval = get_dev_state(BVAL, &ds);
    if (rval == -1) {
        errcnt++;
    } else {
        if (ds->alrm->alrm[BVAL_LOALRM_I].actv) {
            status_set("LB");
            alarm_set(bval.alrm[BVAL_LOALRM_I].descr);
            upslogx(LOG_INFO, "ups.status = LB");
        }
        if (ds->alrm->alrm[BVAL_HIALRM_I].actv) {
            status_set("HB");
            alarm_set(bval.alrm[BVAL_HIALRM_I].descr);
            upslogx(LOG_INFO, "ups.status = HB");
        }
        if (ds->alrm->alrm[BVAL_BSTSFL_I].actv) {
            alarm_set(bval.alrm[BVAL_BSTSFL_I].descr);
            upslogx(LOG_INFO, "battery start with battery flat");
        }
    }
=======
/* read all register data image */
int read_all_regs(modbus_t *mb, uint16_t *data) 
{
    int rval; 

    rval = modbus_read_registers(mb, regs[REG_STARTIDX].xaddr, MAX_REGS, regs_data);
    if (rval == -1) {
        upslogx(LOG_ERR,"ERROR:(%s) modbus_read: addr:0x%x, length:%8d, path:%s\n",
            modbus_strerror(errno),
            regs[CHRG].xaddr,
            MAX_REGS,
            device_path
        );

        /* on BROKEN PIPE error try to reconnect */
        if (errno == EPIPE) {
            upsdebugx(1, "register_read: error(%s)", modbus_strerror(errno));
            modbus_reconnect();
        }
    }
    return rval;
}

/* Read a modbus register */
int register_read(modbus_t *mb, int addr, regtype_t type, void *data)
{
    int rval = -1;
>>>>>>> read_all_regs aproach integrated

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
        rval = get_dev_state(VAC, &ds);
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
    /* check for communication errors */
<<<<<<< HEAD
	if (errcnt == 0) {
		alarm_commit();
		status_commit();
		dstate_dataok();
	} else {
		upsdebugx(2,"Communication errors: %d", errcnt);
		dstate_datastale();
	}
>>>>>>> under construction
=======
    if (errcnt == 0) {
        alarm_commit();
        status_commit();
        dstate_dataok();
    } else {
        upsdebugx(2, "Communication errors: %d", errcnt);
        dstate_datastale();
    }
>>>>>>> ghost alarms bug fix, other bug fixes
}

/* shutdown UPS */
void upsdrv_shutdown(void)
{
<<<<<<< HEAD
<<<<<<< HEAD
=======
>>>>>>> ghost alarms bug fix, other bug fixes
    int rval;
    int cnt = FSD_REPEAT_CNT;    /* shutdown repeat counter */
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
<<<<<<< HEAD
=======
	int rval;
	int cnt = FSD_REPEAT_CNT;    /* shutdown repeat counter */
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
>>>>>>> under construction
=======
>>>>>>> ghost alarms bug fix, other bug fixes
}

/* print driver usage info */
void upsdrv_help(void)
{
}

/* list flags and values that you want to receive via -x */
void upsdrv_makevartable(void)
{
<<<<<<< HEAD
<<<<<<< HEAD
=======
>>>>>>> ghost alarms bug fix, other bug fixes
    addvar(VAR_VALUE, "device_mfr", "device manufacturer");
    addvar(VAR_VALUE, "device_model", "device model");
    addvar(VAR_VALUE, "ser_baud_rate", "serial port baud rate");
    addvar(VAR_VALUE, "ser_parity", "serial port parity");
    addvar(VAR_VALUE, "ser_data_bit", "serial port data bit");
    addvar(VAR_VALUE, "ser_stop_bit", "serial port stop bit");
    addvar(VAR_VALUE, "rio_slave_id", "RIO modbus slave ID");
<<<<<<< HEAD
=======
	addvar(VAR_VALUE, "device_mfr", "device manufacturer");
	addvar(VAR_VALUE, "device_model", "device model");
	addvar(VAR_VALUE, "ser_baud_rate", "serial port baud rate");
	addvar(VAR_VALUE, "ser_parity", "serial port parity");
	addvar(VAR_VALUE, "ser_data_bit", "serial port data bit");
	addvar(VAR_VALUE, "ser_stop_bit", "serial port stop bit");
	addvar(VAR_VALUE, "rio_slave_id", "RIO modbus slave ID");
>>>>>>> under construction
=======
>>>>>>> ghost alarms bug fix, other bug fixes
    addvar(VAR_VALUE, "mod_resp_to_s", "modbus response timeout (s)");
    addvar(VAR_VALUE, "mod_resp_to_us", "modbus response timeout (us)");
    addvar(VAR_VALUE, "mod_byte_to_s", "modbus byte timeout (s)");
    addvar(VAR_VALUE, "mod_byte_to_us", "modbus byte timeout (us)");
}

/* close modbus connection and free modbus context allocated memory */
void upsdrv_cleanup(void)
{
<<<<<<< HEAD
<<<<<<< HEAD
=======
>>>>>>> ghost alarms bug fix, other bug fixes
    if (mbctx != NULL) {
        modbus_close(mbctx);
        modbus_free(mbctx);
    }
    if (dstate != NULL) {
        free(dstate);
    }
<<<<<<< HEAD
=======
	if (mbctx != NULL) {
		modbus_close(mbctx);
		modbus_free(mbctx);
	}
>>>>>>> under construction
=======
>>>>>>> ghost alarms bug fix, other bug fixes
}

/*
 * driver support functions
 */

<<<<<<< HEAD
<<<<<<< HEAD
=======
>>>>>>> structure device data, code get_dev_state, in progress
/* initialize register start address and hex address from register number */
void reginit()
{
    int i; /* local index */

<<<<<<< HEAD
<<<<<<< HEAD
    for (i = 0; i < MODBUS_NUMOF_REGS; i++) {
=======
    for (i = 1; i < MODBUS_NUMOF_REGS; i++) {
>>>>>>> structure device data, code get_dev_state, in progress
=======
    for (i = 0; i < MODBUS_NUMOF_REGS; i++) {
>>>>>>> ghost alarms bug fix, other bug fixes
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
            default:
<<<<<<< HEAD
<<<<<<< HEAD
                upslogx(LOG_ERR, "Invalid register type %d for register %d", regs[i].type, regs[i].num);
                upsdebugx(3, "Invalid register type %d for register %d", regs[i].type, regs[i].num);
        }
        upsdebugx(3, "reginit: num:%d, type: %d saddr: %d, xaddr: 0x%x",
                                                         regs[i].num,
                                                        regs[i].type,
                                                        regs[i].saddr,
                                                        regs[i].xaddr
=======
                upslogx(LOG_ERR, "Invalid register type %d for register %d\n", regs[i].type, regs[i].num);
                upsdebugx(3, "Invalid register type %d for register %d\n", regs[i].type, regs[i].num);
        }
        upsdebugx(3, "register num:%d, type: %d saddr %d, xaddr x%x\n", regs[i].num,
                  regs[i].type,
                  regs[i].saddr,
                  regs[i].xaddr
>>>>>>> structure device data, code get_dev_state, in progress
=======
                upslogx(LOG_ERR, "Invalid register type %d for register %d", regs[i].type, regs[i].num);
                upsdebugx(3, "Invalid register type %d for register %d", regs[i].type, regs[i].num);
        }
        upsdebugx(3, "reginit: num:%d, type: %d saddr: %d, xaddr: 0x%x",
                                                         regs[i].num,
                                                        regs[i].type,
                                                        regs[i].saddr,
                                                        regs[i].xaddr
>>>>>>> ghost alarms bug fix, other bug fixes
        );
    }
}

<<<<<<< HEAD
/* Read a modbus register */
int register_read(modbus_t *mb, int addr, regtype_t type, void *data)
{
    int rval = -1;

    /* register bit masks */
    uint mask8 = 0x00FF;
    uint mask16 = 0xFFFF;

    switch (type) {
        case COIL:
            rval = modbus_read_bits(mb, addr, 1, (uint8_t *)data);
            *(uint *)data = *(uint *)data & mask8;
            break;
        case INPUT_B:
            rval = modbus_read_input_bits(mb, addr, 1, (uint8_t *)data);
            *(uint *)data = *(uint *)data & mask8;
            break;
        case INPUT_R:
            rval = modbus_read_input_registers(mb, addr, 1, (uint16_t *)data);
            *(uint *)data = *(uint *)data & mask16;
            break;
        case HOLDING:
            rval = modbus_read_registers(mb, addr, 1, (uint16_t *)data);
            *(uint *)data = *(uint *)data & mask16;
            break;
=======
=======
>>>>>>> structure device data, code get_dev_state, in progress
/* Read a modbus register */
int register_read(modbus_t *mb, int addr, regtype_t type, void *data)
{
<<<<<<< HEAD
	int rval = -1;

	/* register bit masks */
	uint mask8 = 0x000F;
	uint mask16 = 0x00FF;

	switch (type) {
		case COIL:
			rval = modbus_read_bits(mb, addr, 1, (uint8_t *)data);
			*(uint *)data = *(uint *)data & mask8;
			break;
		case INPUT_B:
			rval = modbus_read_input_bits(mb, addr, 1, (uint8_t *)data);
			*(uint *)data = *(uint *)data & mask8;
			break;
		case INPUT_R:
			rval = modbus_read_input_registers(mb, addr, 1, (uint16_t *)data);
			*(uint *)data = *(uint *)data & mask16;
			break;
		case HOLDING:
			rval = modbus_read_registers(mb, addr, 1, (uint16_t *)data);
			*(uint *)data = *(uint *)data & mask16;
			break;
>>>>>>> under construction
=======
    int rval = -1;

    /* register bit masks */
    uint mask8 = 0x00FF;
    uint mask16 = 0xFFFF;

    switch (type) {
        case COIL:
            rval = modbus_read_bits(mb, addr, 1, (uint8_t *)data);
            *(uint *)data = *(uint *)data & mask8;
            break;
        case INPUT_B:
            rval = modbus_read_input_bits(mb, addr, 1, (uint8_t *)data);
            *(uint *)data = *(uint *)data & mask8;
            break;
        case INPUT_R:
            rval = modbus_read_input_registers(mb, addr, 1, (uint16_t *)data);
            *(uint *)data = *(uint *)data & mask16;
            break;
        case HOLDING:
            rval = modbus_read_registers(mb, addr, 1, (uint16_t *)data);
            *(uint *)data = *(uint *)data & mask16;
            break;
>>>>>>> ghost alarms bug fix, other bug fixes

#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wcovered-switch-default"
#endif
<<<<<<< HEAD
<<<<<<< HEAD
=======
>>>>>>> ghost alarms bug fix, other bug fixes
        /* All enum cases defined as of the time of coding
         * have been covered above. Handle later definitions,
         * memory corruptions and buggy inputs below...
         */
        default:
<<<<<<< HEAD
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT)
# pragma GCC diagnostic pop
#endif
            upsdebugx(2, "ERROR: register_read: invalid register type %d\n", type);
            break;
    }
    if (rval == -1) {
        upslogx(LOG_ERR,"ERROR:(%s) modbus_read: addr:0x%x, type:%8s, path:%s\n",
            modbus_strerror(errno),
            addr,
            (type == COIL) ? "COIL" :
            (type == INPUT_B) ? "INPUT_B" :
            (type == INPUT_R) ? "INPUT_R" : "HOLDING",
            device_path
        );

        /* on BROKEN PIPE error try to reconnect */
        if (errno == EPIPE) {
            upsdebugx(1, "register_read: error(%s)", modbus_strerror(errno));
            modbus_reconnect();
        }
    }
    upsdebugx(3, "register addr: 0x%x, register type: %d read: %d",addr, type, *(uint *)data);
    return rval;
=======
		/* All enum cases defined as of the time of coding
		 * have been covered above. Handle later definitions,
		 * memory corruptions and buggy inputs below...
		 */
		default:
=======
>>>>>>> ghost alarms bug fix, other bug fixes
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT)
# pragma GCC diagnostic pop
#endif
            upsdebugx(2, "ERROR: register_read: invalid register type %d\n", type);
            break;
    }
    if (rval == -1) {
        upslogx(LOG_ERR,"ERROR:(%s) modbus_read: addr:0x%x, type:%8s, path:%s\n",
            modbus_strerror(errno),
            addr,
            (type == COIL) ? "COIL" :
            (type == INPUT_B) ? "INPUT_B" :
            (type == INPUT_R) ? "INPUT_R" : "HOLDING",
            device_path
        );

        /* on BROKEN PIPE error try to reconnect */
        if (errno == EPIPE) {
            upsdebugx(1, "register_read: error(%s)", modbus_strerror(errno));
            modbus_reconnect();
        }
<<<<<<< HEAD
	}
	upsdebugx(3, "register addr: 0x%x, register type: %d read: %d",addr, type, *(uint *)data);
	return rval;
>>>>>>> under construction
=======
    }
    upsdebugx(3, "register addr: 0x%x, register type: %d read: %d",addr, type, *(uint *)data);
    return rval;
>>>>>>> ghost alarms bug fix, other bug fixes
}

/* write a modbus register */
int register_write(modbus_t *mb, int addr, regtype_t type, void *data)
{
<<<<<<< HEAD
<<<<<<< HEAD
=======
>>>>>>> ghost alarms bug fix, other bug fixes
    int rval = -1;

    /* register bit masks */
    uint mask8 = 0x00FF;
    uint mask16 = 0xFFFF;

    switch (type) {
        case COIL:
            *(uint *)data = *(uint *)data & mask8;
            rval = modbus_write_bit(mb, addr, *(uint8_t *)data);
            break;
        case HOLDING:
            *(uint *)data = *(uint *)data & mask16;
            rval = modbus_write_register(mb, addr, *(uint16_t *)data);
            break;

        case INPUT_B:
        case INPUT_R:
<<<<<<< HEAD
=======
	int rval = -1;

	/* register bit masks */
	uint mask8 = 0x000F;
	uint mask16 = 0x00FF;

	switch (type) {
		case COIL:
			*(uint *)data = *(uint *)data & mask8;
			rval = modbus_write_bit(mb, addr, *(uint8_t *)data);
			break;
		case HOLDING:
			*(uint *)data = *(uint *)data & mask16;
			rval = modbus_write_register(mb, addr, *(uint16_t *)data);
			break;

		case INPUT_B:
		case INPUT_R:
>>>>>>> under construction
=======
>>>>>>> ghost alarms bug fix, other bug fixes
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wcovered-switch-default"
#endif
<<<<<<< HEAD
<<<<<<< HEAD
=======
>>>>>>> ghost alarms bug fix, other bug fixes
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
        upslogx(LOG_ERR,"ERROR:(%s) modbus_write: addr:0x%x, type:%8s, path:%s\n",
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
    upsdebugx(3, "register addr: 0x%x, register type: %d read: %d",addr, type, *(uint *)data);
    return rval;
<<<<<<< HEAD
=======
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
		upslogx(LOG_ERR,"ERROR:(%s) modbus_read: addr:0x%x, type:%8s, path:%s\n",
			modbus_strerror(errno),
			addr,
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
	upsdebugx(3, "register addr: 0x%x, register type: %d read: %d",addr, type, *(uint *)data);
	return rval;
>>>>>>> under construction
=======
>>>>>>> ghost alarms bug fix, other bug fixes
}

/* returns the time elapsed since start in milliseconds */
long time_elapsed(struct timeval *start)
{
<<<<<<< HEAD
<<<<<<< HEAD
=======
>>>>>>> ghost alarms bug fix, other bug fixes
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
<<<<<<< HEAD
=======
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
>>>>>>> under construction
=======
>>>>>>> ghost alarms bug fix, other bug fixes
}

/* instant command triggered by upsd */
int upscmd(const char *cmd, const char *arg)
{
<<<<<<< HEAD
<<<<<<< HEAD
    int rval;
    int data;

    if (!strcasecmp(cmd, "load.off")) {
        data = 1;
        rval = register_write(mbctx, regs[FSD].xaddr, regs[FSD].type, &data);
        if (rval == -1) {
            upslogx(2, "ERROR:(%s) modbus_write_register: addr:0x%08x, regtype: %d, path:%s\n",
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
int get_dev_state(devreg_t regindx, devstate_t **dstate)
{
    int i;                          /* local index */
    int n;
    int rval;                       /* return value */
    static char *ptr = NULL;        /* temporary pointer */
    uint num;                       /* register number */
    uint reg_val;                   /* register value */
    regtype_t rtype;                /* register type */
    int addr;                       /* register address */
    devstate_t *state;              /* device state */

    state = *dstate;
    num = regs[regindx].num;
    addr = regs[regindx].xaddr;
    rtype = regs[regindx].type;

    rval = register_read(mbctx, addr, rtype, &reg_val);
    if (rval == -1) {
        return rval;
    }
    upsdebugx(3, "get_dev_state: num: %d, addr: 0x%x, regtype: %d, data: %d",
                                                                         num,
                                                                         addr,
                                                                         rtype,
                                                                         reg_val
    );

    /* process register data */
    switch (regindx) {
        case CHRG:                  /* "ups.charge" */
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
        case BATV:                  /* "battery.voltage" */
        case LVDC:                  /* "output.voltage" */
        case LCUR:                  /* "output.current" */
            if (reg_val != 0) {
                state->reg.val16 = reg_val;
                double fval = reg_val / 1000.00; /* convert mV to V, mA to A */
                n = snprintf(NULL, 0, "%.2f", fval);
                if (ptr != NULL) {
                    free(ptr);
                }
                char *fval_s = (char *)xmalloc(sizeof(char) * (n + 1));
                ptr = fval_s;
                sprintf(fval_s, "%.2f", fval);
                state->reg.strval = fval_s;
            } else {
                state->reg.val16 = 0;
                state->reg.strval = "0.00";
            }
            upsdebugx(3, "get_dev_state: variable: %s", state->reg.strval);
            break;
        case TBUF:
        case BSOH:
        case BCEF:
        case VAC:                   /* "input.voltage" */
            if (reg_val != 0) {
                state->reg.val16 = reg_val;
                n = snprintf(NULL, 0, "%d", reg_val);
                if (ptr != NULL) {
                    free(ptr);
                }
                char *reg_val_s = (char *)xmalloc(sizeof(char) * (n + 1));
                ptr = reg_val_s;
                sprintf(reg_val_s, "%d", reg_val);
                state->reg.strval = reg_val_s;
            } else {
                state->reg.val16 = 0;
                state->reg.strval = "0";
            }
            upsdebugx(3, "get_dev_state: variable: %s", state->reg.strval);
            break;
        case BSOC:                  /* "battery.charge" */
            if (reg_val != 0) {
                state->reg.val16 = reg_val;
                double fval = (double )reg_val * regs[BSOC].scale;
                n = snprintf(NULL, 0, "%.2f", fval);
                if (ptr != NULL) {
                    free(ptr);
                }
                char *fval_s = (char *)xmalloc(sizeof(char) * (n + 1));
                ptr = fval_s;
                sprintf(fval_s, "%.2f", fval);
                state->reg.strval = fval_s;
            } else {
                state->reg.val16 = 0;
                state->reg.strval = "0.00";
            }
            upsdebugx(3, "get_dev_state: variable: %s", state->reg.strval);
            break;
        case BTMP:                  /* "battery.temperature" */
        case OTMP:                  /* "ups.temperature" */
            state->reg.val16 = reg_val;
            double fval = reg_val - 273.15;
            n = snprintf(NULL, 0, "%.2f", fval);
            char *fval_s = (char *)xmalloc(sizeof(char) * (n + 1));
            if (ptr != NULL) {
                free(ptr);
            }
            ptr = fval_s;
            sprintf(fval_s, "%.2f", fval);
            state->reg.strval = fval_s;
            upsdebugx(3, "get_dev_state: variable: %s", state->reg.strval);
            break;
        case PMNG:                  /* "ups.status" & "battery.charge" */
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
        case PRDN:                  /* "ups.model" */
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
                bsta.alrm[BSTA_REVPOL_I].actv = 1;
            } else {
                bsta.alrm[BSTA_REVPOL_I].actv = 0;
            }
            if (reg_val & BSTA_NOCNND_M) {
                bsta.alrm[BSTA_NOCNND_I].actv = 1;
            } else {
                bsta.alrm[BSTA_NOCNND_I].actv = 0;
            }
            if (reg_val & BSTA_CLSHCR_M) {
                bsta.alrm[BSTA_CLSHCR_I].actv = 1;
            } else {
                bsta.alrm[BSTA_CLSHCR_I].actv = 0;
            }
            if (reg_val & BSTA_SULPHD_M) {
                bsta.alrm[BSTA_SULPHD_I].actv = 1;
            } else {
                bsta.alrm[BSTA_SULPHD_I].actv = 0;
            }
            if (reg_val & BSTA_CHEMNS_M) {
                bsta.alrm[BSTA_CHEMNS_I].actv = 1;
            } else {
                bsta.alrm[BSTA_CHEMNS_I].actv = 0;
            }
            if (reg_val & BSTA_CNNFLT_M) {
                bsta.alrm[BSTA_CNNFLT_I].actv = 1;
            } else {
                bsta.alrm[BSTA_CNNFLT_I].actv = 0;
            }
            state->alrm = &bsta;
            break;
        case SCSH:
            if (reg_val & SHSC_HIRESI_M) {
                shsc.alrm[SHSC_HIRESI_I].actv = 1;
            } else {
                shsc.alrm[SHSC_HIRESI_I].actv = 0;
            }
            if (reg_val & SHSC_LOCHEF_M) {
                shsc.alrm[SHSC_LOCHEF_I].actv = 1;
            } else {
                shsc.alrm[SHSC_LOCHEF_I].actv = 0;
            }
            if (reg_val & SHSC_LOEFCP_M) {
                shsc.alrm[SHSC_LOEFCP_I].actv = 1;
            } else {
                shsc.alrm[SHSC_LOEFCP_I].actv = 0;
            }
            if (reg_val & SHSC_LOWSOC_M) {
                shsc.alrm[SHSC_LOWSOC_I].actv = 1;
            } else {
                shsc.alrm[SHSC_LOWSOC_I].actv = 0;
            }
            state->alrm = &shsc;
            break;
        case BVAL:
            if (reg_val & BVAL_HIALRM_M) {
                bval.alrm[BVAL_HIALRM_I].actv = 1;
            } else {
                bval.alrm[BVAL_HIALRM_I].actv = 0;
            }
            if (reg_val & BVAL_LOALRM_M) {
                bval.alrm[BVAL_LOALRM_I].actv = 1;
            } else {
                bval.alrm[BVAL_LOALRM_I].actv = 0;
            }
            if (reg_val & BVAL_BSTSFL_M) {
                bval.alrm[BVAL_BSTSFL_I].actv = 1;
            } else {
                bval.alrm[BVAL_BSTSFL_I].actv = 0;
            }
            state->alrm = &bval;
            break;
        case BTSF:
            if (reg_val & BTSF_FCND_M) {
                btsf.alrm[BTSF_FCND_I].actv = 1;
            } else {
                btsf.alrm[BTSF_FCND_I].actv = 0;
            }
            if (reg_val & BTSF_NCND_M) {
                btsf.alrm[BTSF_NCND_I].actv = 1;
            } else {
                btsf.alrm[BTSF_NCND_I].actv = 0;
            }
            state->alrm = &btsf;
            break;
        case DEVF:
            if (reg_val & DEVF_RCALRM_M) {
                devf.alrm[DEVF_RCALRM_I].actv = 1;
            } else {
                devf.alrm[DEVF_RCALRM_I].actv = 0;
            }
            if (reg_val & DEVF_INALRM_M) {
                devf.alrm[DEVF_INALRM_I].actv = 1;
            } else {
                devf.alrm[DEVF_INALRM_I].actv = 0;
            }
            if (reg_val & DEVF_LFNAVL_M) {
                devf.alrm[DEVF_LFNAVL_I].actv = 1;
            } else {
                devf.alrm[DEVF_LFNAVL_I].actv = 0;
            }
            state->alrm = &devf;
            break;
        case VACA:
            if (reg_val & VACA_HIALRM_M) {
                vaca.alrm[VACA_HIALRM_I].actv = 1;
            } else {
                vaca.alrm[VACA_HIALRM_I].actv = 0;
            }
            if (reg_val == VACA_LOALRM_M) {
                vaca.alrm[VACA_LOALRM_I].actv = 1;
            } else {
                vaca.alrm[VACA_LOALRM_I].actv = 0;
            }
            state->alrm = &vaca;
            break;
        case MAIN:
            if (reg_val & MAINS_AVAIL_M) {
                mains.alrm[MAINS_AVAIL_I].actv = 1;
            } else {
                mains.alrm[MAINS_AVAIL_I].actv = 0;
            }
            if (reg_val == SHUTD_REQST_M) {
                mains.alrm[SHUTD_REQST_I].actv = 1;
            } else {
                mains.alrm[SHUTD_REQST_I].actv = 0;
            }
            state->alrm = &mains;
            break;
        case OBTA:
            if (reg_val == OBTA_HIALRM_V) {
                obta.alrm[OBTA_HIALRM_I].actv = 1;
            }
            state->alrm = &obta;
            break;
=======
	int rval;
	int data;
=======
    int rval;
    int data;
>>>>>>> ghost alarms bug fix, other bug fixes

    if (!strcasecmp(cmd, "load.off")) {
        data = 1;
        rval = register_write(mbctx, regs[FSD].xaddr, regs[FSD].type, &data);
        if (rval == -1) {
            upslogx(2, "ERROR:(%s) modbus_write_register: addr:0x%08x, regtype: %d, path:%s\n",
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
int get_dev_state(devreg_t regindx, devstate_t **dstate)
{
    int i;                          /* local index */
    int n;
    int rval;                       /* return value */
    static char *ptr = NULL;        /* temporary pointer */
    uint reg_val;                   /* register value */
#if READALL_REGS == 0
    uint num;                       /* register number */
    regtype_t rtype;                /* register type */
    int addr;                       /* register address */
#endif
    devstate_t *state;              /* device state */

    state = *dstate;
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
    upsdebugx(3, "get_dev_state: num: %d, addr: 0x%x, regtype: %d, data: %d",
                                                                         num,
                                                                         addr,
                                                                         rtype,
                                                                         reg_val
    );
#endif
    /* process register data */
    switch (regindx) {
        case CHRG:                  /* "ups.charge" */
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
        case BATV:                  /* "battery.voltage" */
        case LVDC:                  /* "output.voltage" */
        case LCUR:                  /* "output.current" */
            if (reg_val != 0) {
                state->reg.val16 = reg_val;
                double fval = reg_val / 1000.00; /* convert mV to V, mA to A */
                n = snprintf(NULL, 0, "%.2f", fval);
                if (ptr != NULL) {
                    free(ptr);
                }
                char *fval_s = (char *)xmalloc(sizeof(char) * (n + 1));
                ptr = fval_s;
                sprintf(fval_s, "%.2f", fval);
                state->reg.strval = fval_s;
            } else {
                state->reg.val16 = 0;
                state->reg.strval = "0.00";
            }
            upsdebugx(3, "get_dev_state: variable: %s", state->reg.strval);
            break;
        case TBUF:
        case BSOH:
        case BCEF:
        case VAC:                   /* "input.voltage" */
            if (reg_val != 0) {
                state->reg.val16 = reg_val;
                n = snprintf(NULL, 0, "%d", reg_val);
                if (ptr != NULL) {
                    free(ptr);
                }
                char *reg_val_s = (char *)xmalloc(sizeof(char) * (n + 1));
                ptr = reg_val_s;
                sprintf(reg_val_s, "%d", reg_val);
                state->reg.strval = reg_val_s;
            } else {
                state->reg.val16 = 0;
                state->reg.strval = "0";
            }
            upsdebugx(3, "get_dev_state: variable: %s", state->reg.strval);
            break;
        case BSOC:                  /* "battery.charge" */
            if (reg_val != 0) {
                state->reg.val16 = reg_val;
                double fval = (double )reg_val * regs[BSOC].scale;
                n = snprintf(NULL, 0, "%.2f", fval);
                if (ptr != NULL) {
                    free(ptr);
                }
                char *fval_s = (char *)xmalloc(sizeof(char) * (n + 1));
                ptr = fval_s;
                sprintf(fval_s, "%.2f", fval);
                state->reg.strval = fval_s;
            } else {
                state->reg.val16 = 0;
                state->reg.strval = "0.00";
            }
            upsdebugx(3, "get_dev_state: variable: %s", state->reg.strval);
            break;
        case BTMP:                  /* "battery.temperature" */
        case OTMP:                  /* "ups.temperature" */
            state->reg.val16 = reg_val;
            double fval = reg_val - 273.15;
            n = snprintf(NULL, 0, "%.2f", fval);
            char *fval_s = (char *)xmalloc(sizeof(char) * (n + 1));
            if (ptr != NULL) {
                free(ptr);
            }
            ptr = fval_s;
            sprintf(fval_s, "%.2f", fval);
            state->reg.strval = fval_s;
            upsdebugx(3, "get_dev_state: variable: %s", state->reg.strval);
            break;
        case PMNG:                  /* "ups.status" & "battery.charge" */
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
<<<<<<< HEAD
			break;
<<<<<<< HEAD

		case BYPASS_T:
		case CAL_T:
		case FSD_T:
		case OFF_T:
		case OVER_T:
		case TRIM_T:
		case BOOST_T:
>>>>>>> under construction
=======
=======
            upsdebugx(3, "get_dev_state: power.state: %s", state->reg.strval);
            break;
>>>>>>> ghost alarms bug fix, other bug fixes
        case PRDN:                  /* "ups.model" */
            for (i = 0; i < DEV_NUMOF_MODELS; i++) {
                if (prdnm_i[i].val == reg_val) {
                    break;
                }
            }
            state->product.val = reg_val;
            state->product.name = prdnm_i[i].name;
            upsdebugx(3, "get_dev_state: product.name: %s", state->product.name);
            break;
<<<<<<< HEAD
>>>>>>> structure device data, code get_dev_state, in progress
=======
        case BSTA:
            if (reg_val & BSTA_REVPOL_M) {
                bsta.alrm[BSTA_REVPOL_I].actv = 1;
            } else {
                bsta.alrm[BSTA_REVPOL_I].actv = 0;
            }
            if (reg_val & BSTA_NOCNND_M) {
                bsta.alrm[BSTA_NOCNND_I].actv = 1;
            } else {
                bsta.alrm[BSTA_NOCNND_I].actv = 0;
            }
            if (reg_val & BSTA_CLSHCR_M) {
                bsta.alrm[BSTA_CLSHCR_I].actv = 1;
            } else {
                bsta.alrm[BSTA_CLSHCR_I].actv = 0;
            }
            if (reg_val & BSTA_SULPHD_M) {
                bsta.alrm[BSTA_SULPHD_I].actv = 1;
            } else {
                bsta.alrm[BSTA_SULPHD_I].actv = 0;
            }
            if (reg_val & BSTA_CHEMNS_M) {
                bsta.alrm[BSTA_CHEMNS_I].actv = 1;
            } else {
                bsta.alrm[BSTA_CHEMNS_I].actv = 0;
            }
            if (reg_val & BSTA_CNNFLT_M) {
                bsta.alrm[BSTA_CNNFLT_I].actv = 1;
            } else {
                bsta.alrm[BSTA_CNNFLT_I].actv = 0;
            }
            state->alrm = &bsta;
            break;
        case SCSH:
            if (reg_val & SHSC_HIRESI_M) {
                shsc.alrm[SHSC_HIRESI_I].actv = 1;
            } else {
                shsc.alrm[SHSC_HIRESI_I].actv = 0;
            }
            if (reg_val & SHSC_LOCHEF_M) {
                shsc.alrm[SHSC_LOCHEF_I].actv = 1;
            } else {
                shsc.alrm[SHSC_LOCHEF_I].actv = 0;
            }
            if (reg_val & SHSC_LOEFCP_M) {
                shsc.alrm[SHSC_LOEFCP_I].actv = 1;
            } else {
                shsc.alrm[SHSC_LOEFCP_I].actv = 0;
            }
            if (reg_val & SHSC_LOWSOC_M) {
                shsc.alrm[SHSC_LOWSOC_I].actv = 1;
            } else {
                shsc.alrm[SHSC_LOWSOC_I].actv = 0;
            }
            state->alrm = &shsc;
            break;
        case BVAL:
            if (reg_val & BVAL_HIALRM_M) {
                bval.alrm[BVAL_HIALRM_I].actv = 1;
            } else {
                bval.alrm[BVAL_HIALRM_I].actv = 0;
            }
            if (reg_val & BVAL_LOALRM_M) {
                bval.alrm[BVAL_LOALRM_I].actv = 1;
            } else {
                bval.alrm[BVAL_LOALRM_I].actv = 0;
            }
            if (reg_val & BVAL_BSTSFL_M) {
                bval.alrm[BVAL_BSTSFL_I].actv = 1;
            } else {
                bval.alrm[BVAL_BSTSFL_I].actv = 0;
            }
            state->alrm = &bval;
            break;
        case BTSF:
            if (reg_val & BTSF_FCND_M) {
                btsf.alrm[BTSF_FCND_I].actv = 1;
            } else {
                btsf.alrm[BTSF_FCND_I].actv = 0;
            }
            if (reg_val & BTSF_NCND_M) {
                btsf.alrm[BTSF_NCND_I].actv = 1;
            } else {
                btsf.alrm[BTSF_NCND_I].actv = 0;
            }
            state->alrm = &btsf;
            break;
        case DEVF:
            if (reg_val & DEVF_RCALRM_M) {
                devf.alrm[DEVF_RCALRM_I].actv = 1;
            } else {
                devf.alrm[DEVF_RCALRM_I].actv = 0;
            }
            if (reg_val & DEVF_INALRM_M) {
                devf.alrm[DEVF_INALRM_I].actv = 1;
            } else {
                devf.alrm[DEVF_INALRM_I].actv = 0;
            }
            if (reg_val & DEVF_LFNAVL_M) {
                devf.alrm[DEVF_LFNAVL_I].actv = 1;
            } else {
                devf.alrm[DEVF_LFNAVL_I].actv = 0;
            }
            state->alrm = &devf;
            break;
        case VACA:
            if (reg_val & VACA_HIALRM_M) {
                vaca.alrm[VACA_HIALRM_I].actv = 1;
            } else {
                vaca.alrm[VACA_HIALRM_I].actv = 0;
            }
            if (reg_val == VACA_LOALRM_M) {
                vaca.alrm[VACA_LOALRM_I].actv = 1;
            } else {
                vaca.alrm[VACA_LOALRM_I].actv = 0;
            }
            state->alrm = &vaca;
            break;
        case MAIN:
            if (reg_val & MAINS_AVAIL_M) {
                mains.alrm[MAINS_AVAIL_I].actv = 1;
            } else {
                mains.alrm[MAINS_AVAIL_I].actv = 0;
            }
            if (reg_val == SHUTD_REQST_M) {
                mains.alrm[SHUTD_REQST_I].actv = 1;
            } else {
                mains.alrm[SHUTD_REQST_I].actv = 0;
            }
            state->alrm = &mains;
            break;
<<<<<<< HEAD
>>>>>>> alrm_t, alrm_ar_t data structures, construction of upsdrv_updateinfo in progress
=======
        case OBTA:
            if (reg_val == OBTA_HIALRM_V) {
                obta.alrm[OBTA_HIALRM_I].actv = 1;
            }
            state->alrm = &obta;
            break;
>>>>>>> ghost alarms bug fix, other bug fixes
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wcovered-switch-default"
#endif
<<<<<<< HEAD
<<<<<<< HEAD
=======
>>>>>>> ghost alarms bug fix, other bug fixes
        /* All enum cases defined as of the time of coding
         * have been covered above. Handle later definitions,
         * memory corruptions and buggy inputs below...
         */
        default:
<<<<<<< HEAD
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT)
# pragma GCC diagnostic pop
#endif
            state->reg.val16 = reg_val;
            n = snprintf(NULL, 0, "%d", reg_val);
            if (ptr != NULL) {
                free(ptr);
            }
            char *reg_val_s = (char *)xmalloc(sizeof(char) * (n + 1));
            ptr = reg_val_s;
            sprintf(reg_val_s, "%d", reg_val);
            state->reg.strval = reg_val_s;
            break;
    }

    return rval;
=======
		/* All enum cases defined as of the time of coding
		 * have been covered above. Handle later definitions,
		 * memory corruptions and buggy inputs below...
		 */
		default:
=======
>>>>>>> ghost alarms bug fix, other bug fixes
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT)
# pragma GCC diagnostic pop
#endif
            state->reg.val16 = reg_val;
            n = snprintf(NULL, 0, "%d", reg_val);
            if (ptr != NULL) {
                free(ptr);
            }
            char *reg_val_s = (char *)xmalloc(sizeof(char) * (n + 1));
            ptr = reg_val_s;
            sprintf(reg_val_s, "%d", reg_val);
            state->reg.strval = reg_val_s;
            break;
    }

<<<<<<< HEAD
	upsdebugx(3, "get_dev_state: state: %d", reg_val);
	return rval;
>>>>>>> under construction
=======
    return rval;
>>>>>>> ghost alarms bug fix, other bug fixes
}

/* get driver configuration parameters */
void get_config_vars()
{
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
=======
>>>>>>> ghost alarms bug fix, other bug fixes
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
<<<<<<< HEAD
=======
	int i; /* local index */

	/* initialize sigar table */
	for (i = 0; i < NUMOF_SIG_STATES; i++) {
		sigar[i].addr = NOTUSED;
		sigar[i].noro = 0;          /* ON corresponds to 1 (closed contact) */
	}

=======
>>>>>>> first testing release
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
>>>>>>> under construction
=======
>>>>>>> ghost alarms bug fix, other bug fixes

    /* check if response time out (s) is set ang get the value */
    if (testvar("mod_resp_to_s")) {
        mod_resp_to_s = (uint32_t)strtol(getval("mod_resp_to_s"), NULL, 10);
    }
    upsdebugx(2, "mod_resp_to_s %d", mod_resp_to_s);

    /* check if response time out (us) is set ang get the value */
    if (testvar("mod_resp_to_us")) {
        mod_resp_to_us = (uint32_t) strtol(getval("mod_resp_to_us"), NULL, 10);
        if (mod_resp_to_us > 999999) {
            fatalx(EXIT_FAILURE, "get_config_vars: Invalid mod_resp_to_us %d", mod_resp_to_us);
        }
    }
    upsdebugx(2, "mod_resp_to_us %d", mod_resp_to_us);

    /* check if byte time out (s) is set ang get the value */
    if (testvar("mod_byte_to_s")) {
        mod_byte_to_s = (uint32_t)strtol(getval("mod_byte_to_s"), NULL, 10);
    }
    upsdebugx(2, "mod_byte_to_s %d", mod_byte_to_s);

    /* check if byte time out (us) is set ang get the value */
    if (testvar("mod_byte_to_us")) {
        mod_byte_to_us = (uint32_t) strtol(getval("mod_byte_to_us"), NULL, 10);
        if (mod_byte_to_us > 999999) {
            fatalx(EXIT_FAILURE, "get_config_vars: Invalid mod_byte_to_us %d", mod_byte_to_us);
        }
    }
    upsdebugx(2, "mod_byte_to_us %d", mod_byte_to_us);
<<<<<<< HEAD
<<<<<<< HEAD
=======

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
			upsdebugx(2, "%s, addr:0x%x, type:%d", signame, sigar[i].addr, sigar[i].type);
		}
	}
>>>>>>> under construction
=======
>>>>>>> first testing release
}

/* create a new modbus context based on connection type (serial or TCP) */
modbus_t *modbus_new(const char *port)
{
<<<<<<< HEAD
<<<<<<< HEAD
=======
>>>>>>> ghost alarms bug fix, other bug fixes
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
<<<<<<< HEAD
=======
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
>>>>>>> under construction
=======
>>>>>>> ghost alarms bug fix, other bug fixes
}

/* reconnect to modbus server upon connection error */
void modbus_reconnect()
{
    int rval;

<<<<<<< HEAD
<<<<<<< HEAD
    upsdebugx(1, "modbus_reconnect, trying to reconnect to modbus server");
=======
    upsdebugx(2, "modbus_reconnect, trying to reconnect to modbus server");
>>>>>>> under construction
=======
    upsdebugx(1, "modbus_reconnect, trying to reconnect to modbus server");
>>>>>>> ghost alarms bug fix, other bug fixes

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
    rval = modbus_set_response_timeout(mbctx, mod_resp_to_s, mod_resp_to_us);
    if (rval < 0) {
        modbus_free(mbctx);
        fatalx(EXIT_FAILURE, "modbus_set_response_timeout: error(%s)", modbus_strerror(errno));
    }

    /* set modbus response timeout */
    rval = modbus_set_byte_timeout(mbctx, mod_byte_to_s, mod_byte_to_us);
    if (rval < 0) {
        modbus_free(mbctx);
        fatalx(EXIT_FAILURE, "modbus_set_byte_timeout: error(%s)", modbus_strerror(errno));
    }
<<<<<<< HEAD
<<<<<<< HEAD
}
=======
}
>>>>>>> under construction
=======
}
>>>>>>> ghost alarms bug fix, other bug fixes
