/* energizerups.c - model specific routines for Energizer USB units

   Copyright (C) 2003  Viktor T. Toth <vttoth@vttoth.com>

   Based in part on the fentonups.c driver by Russell Kroll:

   Copyright (C) 1999  Russell Kroll <rkroll@exploits.org>

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

#include <asm/types.h>
#include <sys/ioctl.h>

#ifndef HID_MAX_USAGES
#define HID_MAX_USAGES 1024 /* horrible workaround hack */
#endif

#include <linux/hiddev.h>
#include <fcntl.h>
#include <stdio.h>
#include <signal.h>
#include <sys/select.h>
#include <ctype.h>

#include "main.h"

#define DRV_VERSION "0.02"

#define NUM_EVTS 64

#define RETRIES 3

/* Response to I identification queries is in the following format:
 *
 * 012345678901234567890123456789012345678
 * #Energizer       ER-HMOF600 A0        ^M
 * #Energizer       ER-OF800   A0        ^M
 */
#define MANUFR (buf+1)
#define MDLNUM (buf+17)
#define HWVERS (buf+28)

/* Response to Q1 queries is in the following format:
 *
 * 01234567890123456789012345678901234567890123456
 * (118.6 118.6 118.6 020 60.0 13.8 32.0 00001000^M
 */
#define INVOLT (buf+1)
#define OUTVOLT (buf+13)
#define LOADPCT (buf+19)
#define ACFREQ (buf+23)
#define BATVOLT (buf+28)
#define UPSTEMP (buf+33)
#define STATUS (buf+38)

#define OBFLAG (STATUS[0]=='1'||STATUS[5]=='1')
#define LBFLAG (STATUS[1]=='1')
#define BYPASS (STATUS[2]=='1')
#define BEEPER (STATUS[7]=='1')

/* A calibration test of the ER-HMOF600 shows its maximum voltage at
 * 14.0V during charging, falling to 13.6 when the UPS is turned off
 * for three minutes (while still connected to line power) and to 12.8V
 * immediately after starting a test with a 60W load. The 'low battery'
 * warning came on at 11.0V. The discharge time to LB with a 60W light
 * bulb was 35 minutes. The UPS ran another 5 minutes on LB before
 * shutting down, at which time the battery voltage was 10.1V. When
 * charging resumed at the moment the LB warning came on, the battery
 * started at 12.0V.
 *
 * With my ER-OF800, the maximum voltage is 13.8V. The difference is
 * probably within circuit tolerance and not due to a substantial design
 * difference between the units.
 *
 * For now, these are the only two units I have. Both are 115VAC units
 * with a 12V internal battery and an identical software interface. The
 * characteristic AC voltage values here are reasonable guesses based on
 * the values of other similar UPSs.
 */

#define LOWVOLT (OBFLAG?11.0:12.0)
#define VOLTRNG 1.8
#define LOXFER 84
#define LONORM 98
#define HINORM 126
#define HIXFER 142

#define hidsend(x) hidcmd(x,NULL,0)


/*
 * Encode and send a string to the UPS. A trailing <cr> (0x0D) is appended.
 *
 * Commands are encoded in 8-byte reports, each consisting of 64 settable
 * bits. The last report is padded with null bits, just to be safe and not
 * send the UPS garbage by accident.
 */

void sendstring(int fd, char *psz)
{
    struct hiddev_report_info rinfo;
    struct hiddev_usage_ref uref;
    int i, j;
    unsigned char c;

    uref.usage_index = 0;
    i = 0;

    do
    {
        c = (*psz) ? *psz :0x0D;
        for (j = 0; j < 8; j++)
        {
            uref.report_type = HID_REPORT_TYPE_OUTPUT;
            uref.report_id = 0;
            uref.field_index = 0;
            uref.usage_code = 0x90001 + uref.usage_index;
            uref.value = (c & 1);
            if (ioctl(fd, HIDIOCSUSAGE, &uref) < 0)
                fatalx(EXIT_FAILURE, "Error (SUSAGE) while talking to the UPS");
            uref.usage_index++;
            c >>= 1;
        }
        if (*psz == 0 || ++i == 8)
        {
            while (uref.usage_index++ < 63)
            {
                uref.report_type = HID_REPORT_TYPE_OUTPUT;
                uref.report_id = 0;
                uref.field_index = 0;
                uref.usage_code = 0x90001 + uref.usage_index;
                uref.value = 0;
                if (ioctl(fd, HIDIOCSUSAGE, &uref) < 0)
                    fatalx(EXIT_FAILURE, "Error (SUSAGE) while talking to the UPS");
            }
            uref.usage_index = 0;

            rinfo.report_type = HID_REPORT_TYPE_OUTPUT;
            rinfo.report_id = 0;
            rinfo.num_fields = 1;
            if (ioctl(fd, HIDIOCSREPORT, &rinfo) < 0)
                fatalx(EXIT_FAILURE, "Error (SREPORT) while talking to the UPS");
        }
    } while (*psz++);
}

/*
 * Helper function to "fake" the first eight bytes of the UPS response. For
 * reasons that I don't understand, on 2.6.x kernels the first eight characters
 * never appear (i.e., the event interface starts with a set of blank
 * characters, then we receive events that show changed bits... for the
 * SECOND set of 8 characters. This is admittedly a disgusting hack, but then
 * again, this serial-to-USB interface is such a hack anyway, it's a miracle it
 * works! In any case, the point is that this way, we can make the UPS work,
 * which is the whole point of this exercise.
 */
int fake_hid_ev_bits(int r, char *buf, int size)
{
    if (r == 31)
    {
        memmove(buf + 8, buf, size - 8);
        strncpy(buf, "#Energiz", 8);
        r += 8;
    }
    else if ((r >= 38 && r <= 40) && isdigit(buf[0]))
    {
        memmove(buf + 8, buf, size - 8);
        strncpy(buf, "(117.0 1", 8);
        r += 8;
    }
    return r;
}

/*
 * Read HID USAGE bits
 */

void get_hid_usage(int fd, unsigned char *data)
{
    struct hiddev_usage_ref uref;
    int i;

    memset(data, 0, 8);
    for (i = 0; i < 64; i++)
    {
        uref.report_type = HID_REPORT_TYPE_INPUT;
        uref.report_id = HID_REPORT_ID_FIRST;
        uref.field_index = 0;
        uref.usage_index = i;
        if (ioctl(fd, HIDIOCGUCODE, &uref) < 0) perror("qups: GUCODE");
        if (ioctl(fd, HIDIOCGUSAGE, &uref) < 0) perror("qups: GUSAGE");
        if (uref.value) data[i >> 3] |= 1 << (i & 7);
    }
}

/*
 * Send a command to the UPS and (optionally) receive a reply. The parameter
 * pRsp can be zero, in which case no reply is expected.
 *
 * A new file descriptor is opened as experience has shown that a previously
 * opened descriptor cannot be reused reliably for commanding.
 *
 * Data is received in the form of an 8-byte buffer. After the initial report
 * has been read, HID events are received indicating bit changes in the 8-byte
 * buffer. When the bit counter rolls over, we know we have received a full
 * buffer. This doesn't seem to be a 100% reliable method, since it'd not tell
 * us, for instance, if the same 8 bytes are sent twice in a row, but in
 * practice, it does appear to reliably transmit all UPS response messages.
 */

int hidcmd(unsigned char *pCmd, unsigned char *pRsp, int l)
{
    unsigned int i;
    int fd, /*rd, j,*/ k;//, hid;
    fd_set rdfs;
    struct timeval tv;
    //struct hiddev_event ev[NUM_EVTS];
    struct hiddev_usage_ref uref;
    unsigned char data[8];

    if ((fd = open(device_path, O_RDWR)) < 0)
        fatalx(EXIT_FAILURE, "Cannot communicate with UPS at %s", device_path);

    FD_ZERO(&rdfs);
    FD_SET(fd, &rdfs);

    memset(data, 0, sizeof(data));
    for (i = 0; i < 64; i++)
    {
        uref.report_type = HID_REPORT_TYPE_INPUT;
        uref.report_id = 0;
        uref.field_index = 0;
        uref.usage_index = i;
        ioctl(fd, HIDIOCGUCODE, &uref);
        ioctl(fd, HIDIOCGUSAGE, &uref);

        if (uref.value) data[i >> 3] |= 1 << (i & 7);
    }

    sendstring(fd, pCmd);

    k = 0;

    if (pRsp != NULL)
    {
		char data2[8];
		int nChanged = -1;

		get_hid_usage(fd, data);
		memcpy(data2, data, 8);

		for (i = 0; i < 100; i++)
		{
			tv.tv_sec = 0;
			tv.tv_usec = 5000;
			select(0, 0, 0, 0, &tv);
			get_hid_usage(fd, data);
			if (memcmp(data, data2, 8))
			{
				memcpy(pRsp + k, data, 8);
				k += 8;
				nChanged = 5;
				memcpy(data2, data, 8);
			}
			else if (--nChanged == 0) break;
		}
		pRsp[k] = 0;
	}
	close(fd);
	return k;
}


int instcmd(const char *cmdname, const char *extra)
{
    if (!strcasecmp(cmdname, "beeper.off")) {
	/* compatibility mode for old command */
	upslogx(LOG_WARNING,
		"The 'beeper.off' command has been renamed to 'beeper.mute' for this driver!");
	return instcmd("beeper.mute", NULL);
    }

    if (!strcasecmp(cmdname, "beeper.on")) {
	/* compatibility mode for old command */
	upslogx(LOG_WARNING,
		"The 'beeper.on' command has been renamed to 'beeper.enable'");
	return instcmd("beeper.enable", NULL);
    }

    if (!strcasecmp(cmdname, "test.battery.start"))
    {
        hidsend("TL");
        upslogx(LOG_NOTICE, "UPS test start");
        return STAT_INSTCMD_HANDLED;
    }

    if (!strcasecmp(cmdname, "test.battery.stop"))
    {
        hidsend("CT");
        upslogx(LOG_NOTICE, "UPS test stop");
        return STAT_INSTCMD_HANDLED;
    }

    if (!strcasecmp(cmdname, "beeper.enable") ||
        !strcasecmp(cmdname, "beeper.mute"))
    {
        char buf[128];
        int r;
        int c = 0;

        /* Find out beeper status and send appropriate command. */

        r = hidcmd("Q1", buf, sizeof(buf));
        while (c++ < RETRIES && (r < 46 || r > 47 || buf[0] != '('))
            r = hidcmd("Q1", buf, sizeof(buf));
        if (r >= 46 && r <= 47 && buf[0] == '(')
        {
            if (OBFLAG && !LBFLAG)  /* Otherwise there's not much we can do */
            {
                if ((BEEPER && !strcasecmp(cmdname, "beeper.mute")) ||
                    (!BEEPER && !strcasecmp(cmdname, "beeper.enable")))
                        hidsend("Q");
                return STAT_INSTCMD_HANDLED;
            }
        }
        return STAT_INSTCMD_HANDLED;    /* FUTURE: failure */
    }

    if (!strcasecmp(cmdname, "beeper.toggle"))
    {
        hidsend("Q");
        return STAT_INSTCMD_HANDLED;
    }

    upslogx(LOG_NOTICE, "instcmd: unknown command [%s]", cmdname);
    return STAT_INSTCMD_UNKNOWN;
}

/*
 * Some trivial initialization. Let's detect the UPS.
 */

void upsdrv_initinfo(void)
{
    char buf[128];
    int i, r;

    /* Yes, sometimes it took as many as 6-7 tries after the UPS has just
     * been turned on or after a communications problem.
     */
    for (i = 0; i < 10; i++)
    {
        r = hidcmd("I", buf, sizeof(buf));
        if ((r == 39 || r == 40) && buf[0] == '#') break;
        sleep(3);
    }
    if (r < 39 || r > 40 || buf[0] != '#') fatalx(EXIT_FAILURE, "No Energizer UPS detected");
    buf[16]=buf[27]=buf[38] = '\0';
    rtrim(MANUFR, ' ');
    rtrim(MDLNUM, ' ');
    rtrim(HWVERS, ' ');

    dstate_setinfo("driver.version.internal", "%s", DRV_VERSION);
    dstate_setinfo("ups.mfr", "%s", MANUFR);
    dstate_setinfo("ups.model", "%s", MDLNUM);

    dstate_setinfo("input.transfer.low", "%d", LOXFER);
    dstate_setinfo("input.transfer.high", "%d", HIXFER);

    hidsend("C");

    /* now add instant command support info */
    dstate_addcmd("test.battery.start");
    dstate_addcmd("test.battery.stop");
    dstate_addcmd("beeper.enabled");
    dstate_addcmd("beeper.mute");
    dstate_addcmd("beeper.toggle");
    dstate_addcmd("beeper.on");
    dstate_addcmd("beeper.off");

    upsh.instcmd = instcmd;
}

/*
 * Query the UPS and update values.
 */

void upsdrv_updateinfo(void)
{
    char buf[128];
    int r;
    static int f;  /* To prevent excessive logging */
    double v;


    r = hidcmd("Q1", buf, sizeof(buf));
    if (r < 46 || r > 48)
    {
        if (f++ < 3)
            upslogx(LOG_ERR, "Invalid response length from UPS [%s]", buf);
        else
            dstate_datastale();
        return;
    }
    if (buf[0] != '(')
    {
        if (f++ < 3)
            upslogx(LOG_ERR, "Invalid response data from UPS [%s]", buf);
        else
            dstate_datastale();
        return;
    }
    f = 0;

    buf[6]=buf[12]=buf[18]=buf[22]=buf[27]=buf[32]=buf[37]=buf[46] = '\0';

    dstate_setinfo("input.voltage", "%s", INVOLT);
    dstate_setinfo("output.voltage", "%s", OUTVOLT);
    dstate_setinfo("battery.voltage", "%s", BATVOLT);

    v = ((atof(BATVOLT) - LOWVOLT) / VOLTRNG) * 100.0;
    if (v > 100.0) v = 100.0;
    if (v < 0.0) v = 0.0;
    dstate_setinfo("battery.charge", "%02.1f", v);

    status_init();
    if (OBFLAG) status_set("OB");               /* on battery */
    else
    {
        status_set("OL");                       /* on line */
        /* only allow these when OL since they're bogus when OB */
        if (BYPASS)                             /* boost or trim in effect */
        {
            if (atoi(INVOLT) < LONORM) status_set("BOOST");
            else if (atoi(INVOLT) > HINORM) status_set("TRIM");
        }
    }
    if (LBFLAG) status_set("LB");               /* low battery */

    status_commit();

    dstate_setinfo("ups.temperature", "%s", UPSTEMP);
    dstate_setinfo("input.frequency", "%s", ACFREQ);
    dstate_setinfo("ups.load", "%s", LOADPCT);
    dstate_dataok();
}

void upsdrv_shutdown(void)
{
    char buf[128];
    int r;
    int b = 1;

    /* Basic idea: find out line status and send appropriate command.
     * If the status query fails, we do not retry more than once (we
     * may be short on time with a dying UPS) but assume that we're
     * on battery.
     */

    r = hidcmd("Q1", buf, sizeof(buf));
    if (r < 46 || r > 47 || buf[0] != '(') r = hidcmd("Q1", buf, sizeof(buf));

    if (r >= 46 && r <= 47 && buf[0] == '(')
    {
        if (OBFLAG)
            upslogx(LOG_WARNING, "On battery, sending shutdown command...");
        else
        {
            b = 0;
            upslogx(LOG_WARNING, "On line, sending shutdown+return command...");
        }
    }
    else upslogx(LOG_WARNING, "Status undetermined, assuming battery power, "
                "sending shutdown command...");

    hidsend(b ? "S01" : "S01R0003");
}


void upsdrv_help(void)
{
}

/* list flags and values that you want to receive via -x */
void upsdrv_makevartable(void)
{
}

void upsdrv_banner(void)
{
    printf("Network UPS Tools - Energizer USB UPS driver %s (%s)\n\n", 
        DRV_VERSION, UPS_VERSION);
    experimental_driver = 1;    /* Causes a warning to be printed */
}

void upsdrv_initups(void)
{
    /* No initialization needed */
}

void upsdrv_cleanup(void)
{
    /* No cleanup needed */
}
