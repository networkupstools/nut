/* belkinunv.c - driver for newer Belkin models, such as "Belkin
   Universal UPS" (ca. 2003)

   Copyright (C) 2003 Peter Selinger <selinger@users.sourceforge.net>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

/* SOFT SHUTDOWN WORKAROUND

   One problem with the Belkin Universal UPS is that it cannot enter a
   soft shutdown (shut down until AC power returns) unless the
   batteries are completely depleted. Thus, one cannot just shut off
   the UPS after operating system shutdown; it will not come back on
   when the power comes back on. The belkinunv driver should never be
   used with the -k option. Instead, we provide a "standalone" mode
   for this driver via some -x options, which is intended to be used
   in startup and shutdown scripts. Please see the belkinunv(8) man
   page for details.

   VARIABLES:

   battery.charge
   battery.runtime
   battery.voltage
   battery.voltage.nominal
   input.frequency
   input.frequency.nominal      e.g. 60 for 60Hz
   input.sensitivity            (RW) normal/medium/low
   input.transfer.high          (RW)
   input.transfer.low           (RW)
   input.voltage
   input.voltage.maximum
   input.voltage.minimum
   input.voltage.nominal
   output.frequency
   output.voltage
   ups.beeper.status            (RW) enabled/disabled/muted
   ups.firmware
   ups.load
   ups.model
   ups.power.nominal            e.g. 800 for an 800VA system
   ups.status
   ups.temperature
   ups.test.result
   ups.delay.restart            read-only: time to restart
   ups.delay.shutdown           read-only: time to shutdown
   ups.type                     ONLINE/OFFLINE/LINEINT

   COMMANDS:

   beeper.disable
   beeper.enable
   beeper.mute
   reset.input.minmax
   shutdown.reboot              shut down load immediately for 1-2 minutes
   shutdown.reboot.graceful     shut down load after 40 seconds for 1-2 minutes
   shutdown.stayoff             shut down load immediately and stay off
   test.battery.start           start 10-second battery test
   test.battery.stop
   test.failure.start           start "deep" battery test
   test.failure.stop

   STATUS FLAGS:

   OB                           load is on battery, including during tests
   OFF                          load is off
   OL                           load is online
   ACFAIL                       AC failure
   OVER                         overload
   OVERHEAT                     overheat
   COMMFAULT                    UPS Fault
   LB                           low battery
   CHRG                         charging
   DEPLETED                     battery depleted
   RB                           replace battery

*/


#include "main.h"
#include "serial.h"

#define DRIVER_NAME	"Belkin 'Universal UPS' driver"
#define DRIVER_VERSION	"0.07"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Peter Selinger <selinger@users.sourceforge.net>",
	DRV_STABLE,
	{ NULL }
};

/* somewhat arbitrary buffer size - the longest actually occuring
   message is 18 bytes for the F6C800-UNV. But since message length is
   arbitrary in principle, we allow for some extra bytes. */
#define MAXMSGSIZE 25

/* definitions of register numbers for Belkin UPS */
#define REG_VOLTRATING    0x01
#define REG_FREQRATING    0x02
#define REG_POWERRATING   0x03
#define REG_BATVOLTRATING 0x04
#define REG_XFER_LO       0x06
#define REG_XFER_LO_MAX   0x07
#define REG_XFER_LO_MIN   0x08
#define REG_XFER_HI       0x09
#define REG_XFER_HI_MAX   0x0a
#define REG_XFER_HI_MIN   0x0b
#define REG_VOLTSENS      0x0c
#define REG_UPSMODEL      0x0d
#define REG_UPSMODEL2     0x0e
#define REG_FIRMWARE      0x0f
#define REG_TESTSTATUS    0x10
#define REG_ALARMSTATUS   0x11
#define REG_SHUTDOWNTIMER 0x15
#define REG_RESTARTTIMER  0x16
#define REG_INPUTVOLT     0x18
#define REG_INPUTFREQ     0x19
#define REG_TEMPERATURE   0x1a
#define REG_OUTPUTVOLT    0x1b
#define REG_OUTPUTFREQ    0x1c
#define REG_LOAD          0x1e
#define REG_BATSTAT2      0x1f
#define REG_BATVOLT       0x20
#define REG_BATLEVEL      0x21
#define REG_UPSSTATUS     0x22
#define REG_BATSTATUS     0x23
#define REG_TIMELEFT      0x3f

/* flags for REG_UPSSTATUS */
#define US_ACFAILURE 0x0001
#define US_OVERLOAD  0x0010
#define US_OFF       0x0020
#define US_OVERHEAT  0x0040
#define US_UPSFAULT  0x0080
#define US_WAITING   0x2000
#define US_BUZZER    0x8000

/* flags for REG_BATSTATUS */
#define BS_LOW       0x04
#define BS_CHARGING  0x10
#define BS_ONBATTERY 0x20
#define BS_DEPLETED  0x40
#define BS_REPLACE   0x80

/* size of an array */
#define asize(x) ((int)(sizeof(x)/sizeof(x[0])))

static const char *upstype[3] = {
	"ONLINE",
	"OFFLINE",
	"LINEINT"
};

static const char *voltsens[3] = {
	"normal",
	"medium",
	"low"
};

static const char *teststatus[6] = {
	"no test performed",
	"test passed",
	"test failed",
	"test failed",
	"test aborted",
	"test in progress"
};

#define ST_OFF 0
#define ST_ONLINE 1
#define ST_BATTERY 2

static const char *status[] = {
	"UPS is off",             /* ST_OFF */
	"UPS is on AC power",     /* ST_ONLINE */
	"UPS is on battery"       /* ST_BATTERY */
};

/* some useful strings */
#define ESC     "\033"
#define COL0    ESC "[G" ESC "[K"  /* terminal control: clear line */

static int minutil = -1;
static int maxutil = -1;

static int xfer_lo_min = -1;
static int xfer_lo_max = -1;
static int xfer_hi_min = -1;
static int xfer_hi_max = -1;

int instcmd(const char *cmdname, const char *extra);
static int setvar(const char *varname, const char *val);

/* ---------------------------------------------------------------------- */
/* a general purpose Belkin-specific function: */

/* calculate a Belkin checksum, i.e., add buf[0]...buf[n-1] */
static unsigned char belkin_checksum(unsigned char *buf, int n) {
	int i, res;

	res = 0;
	for (i=0; i<n; i++) {
		res += buf[i];
	}
	return res & 0xff;
}

/* ---------------------------------------------------------------------- */
/* some private functions for talking to the UPS - "driver mode"
   versions.  The functions in this section have _nut_ in their name,
   and they use standard NUT components (including NUT error handling)
   for file i/o. Note that stand-alone versions of these functions are
   provided in the next section. */

/* open serial port and switch to "smart" mode */
static void belkin_nut_open_tty(void)
{
	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, B2400);

	/* must clear DTR and set RTS for 1 second for UPS to go to
	   "smart" mode */
	ser_set_dtr(upsfd, 0);
	ser_set_rts(upsfd, 1);
	sleep(1);

	ser_flush_io(upsfd);
}

/* receive Belkin message from UPS, check for well-formedness (leading
   byte, checksum). Return length of message, or -1 if not
   well-formed */
static int belkin_nut_receive(unsigned char *buf, int bufsize) {
	int r;
	int n=0;
	int len;

	/* read 0x7e */
	if (n+1 > bufsize) {
		return -1;
	}
	r = ser_get_buf_len(upsfd, &buf[0], 1, 3, 0);
	if (r<0) {
		upslog_with_errno(LOG_ERR, "Error reading from UPS");
		return -1;
	} else if (r==0) {
		upslogx(LOG_ERR, "No response from UPS");
		return -1;
	} else if (buf[0]!=0x7e) {
		upslogx(LOG_ERR, "Garbage read from UPS");
		return -1;
	}
	n+=r;

	/* read instruction, size, and register */
	if (n+3 > bufsize) {
		return -1;
	}
	r = ser_get_buf_len(upsfd, &buf[1], 3, 3, 0);
	if (r!=3) {
		upslogx(LOG_ERR, "Short read from UPS");
		return -1;
	}
	n+=r;

	len = buf[2];

	/* read data and checksum */
	if (n+len > bufsize) {
		return -1;
	}
	r = ser_get_buf_len(upsfd, &buf[4], len, 3, 0);
	if (r!=len) {
		upslogx(LOG_ERR, "Short read from UPS");
		return -1;
	}
	n+=r;

	/* check checksum */
	if (belkin_checksum(buf, len+3) != buf[len+3]) {
		upslogx(LOG_ERR, "Bad checksum from UPS");
		return -1;
	}
	return n;
}

/* read the value of a string register from UPS. Return NULL on
   failure, else an allocated string. */
static char *belkin_nut_read_str(int reg) {
	unsigned char buf[MAXMSGSIZE];
	int len, r;
	char *str;

	/* send the request */
	buf[0] = 0x7e;
	buf[1] = 0x03;
	buf[2] = 0x02;
	buf[3] = reg;
	buf[4] = 0;
	buf[5] = belkin_checksum(buf, 5);

	r = ser_send_buf(upsfd, buf, 6);
	if (r<0) {
		upslogx(LOG_ERR, "Failed write to UPS");
		return NULL;
	}

	/* receive the answer */
	r = belkin_nut_receive(buf, MAXMSGSIZE);
	if (r<0) {
		return NULL;
	}
	if ((buf[1]!=0x05 && buf[1]!=0x01) || buf[3] != reg) {
		upslogx(LOG_ERR, "Invalid response from UPS");
		return NULL;
	}
	if (buf[1]==0x01) {
		return NULL;
	}

	/* convert the answer to a string */
	len = buf[2]-1;
	str = (char *)xmalloc(len+1);
	memcpy(str, &buf[4], len);
	str[len]=0;
	return str;
}

/* read the value of an integer register from UPS. Return -1 on
   failure. */
static int belkin_nut_read_int(int reg) {
	unsigned char buf[MAXMSGSIZE];
	int len, r;

	/* send the request */
	buf[0] = 0x7e;
	buf[1] = 0x03;
	buf[2] = 0x02;
	buf[3] = reg;
	buf[4] = 0;
	buf[5] = belkin_checksum(buf, 5);

	r = ser_send_buf(upsfd, buf, 6);
	if (r<0) {
		upslogx(LOG_ERR, "Failed write to UPS");
		return -1;
	}

	/* receive the answer */
	r = belkin_nut_receive(buf, MAXMSGSIZE);
	if (r<0) {
		return -1;
	}
	if ((buf[1]!=0x05 && buf[1]!=0x01) || buf[3] != reg) {
		upslogx(LOG_ERR, "Invalid response from UPS");
		return -1;
	}
	if (buf[1]==0x01) {
		return -1;
	}

	/* convert the answer to an integer */
	len = buf[2]-1;
	if (len==1) {
		return buf[4];
	} else if (len==2) {
		return buf[4] + 256*buf[5];
	} else {
		upslogx(LOG_ERR, "Invalid response from UPS");
		return -1;
	}
}

/* write the value of an integer register to UPS. Return -1 on
   failure, else 0 */
static int belkin_nut_write_int(int reg, int val) {
	unsigned char buf[MAXMSGSIZE];
	int r;

	/* send the request */
	buf[0] = 0x7e;
	buf[1] = 0x04;
	buf[2] = 0x03;
	buf[3] = reg;
	buf[4] = val & 0xff;
	buf[5] = (val>>8) & 0xff;
	buf[6] = belkin_checksum(buf, 6);

	r = ser_send_buf(upsfd, buf, 7);
	if (r<0) {
		upslogx(LOG_ERR, "Failed write to UPS");
		return -1;
	}

	/* receive the acknowledgement */
	r = belkin_nut_receive(buf, MAXMSGSIZE);
	if (r<0) {
		return -1;
	}
	if ((buf[1]!=0x02 && buf[1]!=0x01) || buf[3] != reg) {
		upslogx(LOG_ERR, "Invalid response from UPS");
		return -1;
	}
	if (buf[1]==0x01) {
		return -1;
	}
	return 0;
}

/* ---------------------------------------------------------------------- */
/* some private functions for talking to the UPS - "standalone"
   versions.  The functions in this section have _std_ in their name,
   and they do not use default NUT error handling (this would not be
   desirable during standalone operation, i.e., when the -x wait
   option is given). These functions also take an additional file
   descriptor argument. */

/* Open and prepare a serial port for communication with a Belkin
   Universal UPS.  DEVICE is the name of the serial port. It will be
   opened in non-blocking read/write mode, and the appropriate
   communications parameters will be set.  The device will also be
   sent a special signal (clear DTR, set RTS) to cause the UPS to
   switch from "dumb" to "smart" mode, and any pending data (=garbage)
   will be discarded. After this call, the device is ready for reading
   and writing via read(2) and write(2). Return a valid file
   descriptor on success, or else -1 with errno set. */
static int belkin_std_open_tty(const char *device) {
	int fd;
	struct termios tios;
	struct flock flock;
	char buf[128];
	int r;

	/* open the device */
	fd = open(device, O_RDWR | O_NONBLOCK);
	if (fd == -1) {
		return -1;
	}

	/* set communications parameters: 2400 baud, 8 bits, 1 stop bit, no
	   parity, enable reading, hang up when done, ignore modem control
	   lines. */
	memset(&tios, 0, sizeof(tios));
	tios.c_cflag = B2400 | CS8 | CREAD | HUPCL | CLOCAL;
	tios.c_cc[VMIN] = 1;
	tios.c_cc[VTIME] = 0;
	r = tcsetattr(fd, TCSANOW, &tios);
	if (r == -1) {
		close(fd);
		return -1;
	}

	/* signal the UPS to enter "smart" mode. This is done by setting RTS
	   and dropping DTR for at least 0.25 seconds. RTS and DTR refer to
	   two specific pins in the 9-pin serial connector. Note: this must
	   be done for at least 0.25 seconds for the UPS to react. Ignore
	   any errors, as this probably means we are not on a "real" serial
	   port. */
	ser_set_dtr(upsfd, 0);
	ser_set_rts(upsfd, 1);

	/* flush both directions of serial port: throw away all data in
	   transit */
	r = ser_flush_io(fd);
	if (r == -1) {
		close(fd);
		return -1;
	}

	/* lock the port */
	memset(&flock, 0, sizeof(flock));
	flock.l_type = F_RDLCK;
	r = fcntl(fd, F_SETLK, &flock);
	if (r == -1) {
		close(fd);
		return -1;
	}

	/* sleep at least 0.25 seconds for the UPS to wake up. Belkin's own
	   software sleeps 1 second, so that's what we do, too. */
	usleep(1000000);

	/* flush incoming data again, and read any remaining garbage
	   bytes. There should not be any. */
	r = tcflush(fd, TCIFLUSH);
	if (r == -1) {
		close(fd);
		return -1;
	}

	r = read(fd, buf, 127);
	if (r == -1 && errno != EAGAIN) {
		close(fd);
		return -1;
	}

	/* leave port in non-blocking state */

	return fd;
}

/* blocking read with 1-second timeout (use non-blocking i/o) */
static int belkin_std_upsread(int fd, unsigned char *buf, int n) {
	int count = 0;
	int r;
	int tries = 0;

	while (count < n) {
		r = read(fd, &buf[count], n-count);
		if (r==-1 && errno==EAGAIN) {
			/* non-blocking i/o, no data available */
			usleep(100000);
			tries++;
		} else if (r == -1) {
			return -1;
		} else {
			count += r;
		}
		if (tries > 10) {
			return -1;
		}
	}
	return count;
}

/* blocking write with 1-second timeout (use non-blocking i/o) */
static int belkin_std_upswrite(int fd, unsigned char *buf, int n) {
	int count = 0;
	int r;
	int tries = 0;

	while (count < n) {
		r = write(fd, &buf[count], n-count);
		if (r==-1 && errno==EAGAIN) {
			/* non-blocking i/o, no data available */
			usleep(100000);
			tries++;
		} else if (r == -1) {
			return -1;
		} else {
			count += r;
		}
		if (tries > 10) {
			return -1;
		}
	}
	return count;
}

/* receive Belkin message from UPS, check for well-formedness (leading
   byte, checksum). Return length of message, or -1 if not
   well-formed */
static int belkin_std_receive(int fd, unsigned char *buf, int bufsize) {
	int r;
	int n=0;
	int len;

	/* read 0x7e */
	if (n+1 > bufsize) {
		return -1;
	}
	r = belkin_std_upsread(fd, &buf[0], 1);
	if (r==-1 || buf[0]!=0x7e) {
		return -1;
	}
	n+=r;

	/* read instruction, size, and register */
	if (n+3 > bufsize) {
		return -1;
	}
	r = belkin_std_upsread(fd, &buf[1], 3);
	if (r!=3) {
		return -1;
	}
	n+=r;

	len = buf[2];

	/* read data and checksum */
	if (n+len > bufsize) {
		return -1;
	}
	r = belkin_std_upsread(fd, &buf[4], len);
	if (r!=len) {
		return -1;
	}
	n+=r;

	/* check checksum */
	if (belkin_checksum(buf, len+3) != buf[len+3]) {
		return -1;
	}
	return n;
}

/* read the value of an integer register from UPS. Return -1 on
   failure. */
static int belkin_std_read_int(int fd, int reg) {
	unsigned char buf[MAXMSGSIZE];
	int len, r;

	/* send the request */
	buf[0] = 0x7e;
	buf[1] = 0x03;
	buf[2] = 0x02;
	buf[3] = reg;
	buf[4] = 0;
	buf[5] = belkin_checksum(buf, 5);

	r = belkin_std_upswrite(fd, buf, 6);
	if (r<0) {
		return -1;
	}

	/* receive the answer */
	r = belkin_std_receive(fd, buf, MAXMSGSIZE);
	if (r<0) {
		return -1;
	}
	if ((buf[1]!=0x05 && buf[1]!=0x01) || buf[3] != reg) {
		return -1;
	}
	if (buf[1]==0x01) {
		return -1;
	}

	/* convert the answer to an integer */
	len = buf[2]-1;
	if (len==1) {
		return buf[4];
	} else if (len==2) {
		return buf[4] + 256*buf[5];
	} else {
		return -1;
	}
}

/* write the value of an integer register to UPS. Return -1 on
   failure, else 0 */
static int belkin_std_write_int(int fd, int reg, int val) {
	unsigned char buf[MAXMSGSIZE];
	int r;

	/* send the request */
	buf[0] = 0x7e;
	buf[1] = 0x04;
	buf[2] = 0x03;
	buf[3] = reg;
	buf[4] = val & 0xff;
	buf[5] = (val>>8) & 0xff;
	buf[6] = belkin_checksum(buf, 6);

	r = belkin_std_upswrite(fd, buf, 7);
	if (r<0) {
		return -1;
	}

	/* receive the acknowledgement */
	r = belkin_std_receive(fd, buf, MAXMSGSIZE);
	if (r<0) {
		return -1;
	}
	if ((buf[1]!=0x02 && buf[1]!=0x01) || buf[3] != reg) {
		return -1;
	}
	if (buf[1]==0x01) {
		return -1;
	}
	return 0;
}

/* ---------------------------------------------------------------------- */
/* "standalone" program executed when driver is called with the '-x
   wait' or '-x wait=<level>' flag or option */

/* this function updates the status line, as specified by the smode
   parameter (0=silent, 1=normal, 2=dumbterminal). This is only done
   if the status has not changed from the previous call */
static void updatestatus(int smode, const char *fmt, ...) {
	char buf[1024];  /* static string limit is OK */
	static char oldbuf[1024] = { 0 };
	static int init = 1;
	va_list ap;

	if (smode==0) {
		return;
	}

	if (init) {
		init = 0;
		oldbuf[0] = 0;
	}

	/* read formatted argument string */
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	buf[sizeof(buf)-1] = 0;
	va_end(ap);

	if (strcmp(oldbuf, buf)==0) {
		return;
	}
	strcpy(oldbuf, buf);

	if (smode==2) {
		/* "dumbterm" version just prints a new line each time */
		printf("%s\n", buf);
	} else {
		/* "normal" version overwrites same line each time */
		printf(COL0 "%s", buf);
	}
	fflush(stdout);
}

/* switch from status line display to normal output mode */
static void endstatus(int smode) {
	if (smode==1) {
		fprintf(stdout, "\n");
		fflush(stdout);
	}
}

static int belkin_wait(void)
{
	int level = 0;   /* battery level to wait for */
	int smode = 1;   /* statusline mode: 0=silent, 1=normal, 2=dumbterm */
	int nohang = 0;  /* nohang flag */
	int flash = 0;   /* flash flag */

	char *val;
	int failcount = 0;  /* count consecutive failed connection attempts */
	int failerrno = 0;
	int fd;
	int r;
	int bs, ov, bl, st;

	/* read command line '-x' options */
	val = getval("wait");
	if (val) {
		level = atoi(val);
	}

	if (dstate_getinfo("driver.flag.nohang")) {
		nohang = 1;
	}

	if (dstate_getinfo("driver.flag.flash")) {
		flash = 1;
	}

	if (dstate_getinfo("driver.flag.silent")) {
		smode = 0;
	} else if (dstate_getinfo("driver.flag.dumbterm")) {
		smode = 2;
	}

	updatestatus(smode, "Connecting to UPS...");
	failcount = 0;
	fd = -1;

	while (1) {
		if (failcount >= 3 && nohang) {
			endstatus(smode);
			printf("UPS is not responding: %s\n", strerror(failerrno));
			return 1;
		} else if (failcount >= 3) {
			updatestatus(smode, "UPS is not responding, will keep trying: %s", strerror(failerrno));
		}
		if (fd == -1) {
			fd = belkin_std_open_tty(device_path);
		}
		if (fd == -1) {
			failcount++;
			failerrno = errno;
			sleep(1);
			continue;
		}

		/* wait until the UPS is online and the battery level
		   is >= level */
		bs = belkin_std_read_int(fd, REG_BATSTATUS);  /* battery status */
		if (bs==-1) {
			failcount++;
			failerrno = errno;
			close(fd);
			fd = -1;
			sleep(1);
			continue;
		}
		ov = belkin_std_read_int(fd, REG_OUTPUTVOLT); /* output voltage */
		if (ov==-1) {
			failcount++;
			failerrno = errno;
			close(fd);
			fd = -1;
			sleep(1);
			continue;
		}
		bl = belkin_std_read_int(fd, REG_BATLEVEL);   /* battery level */
		if (bl==-1) {
			failcount++;
			failerrno = errno;
			close(fd);
			fd = -1;
			sleep(1);
			continue;
		}
		/* successfully got data from UPS */
		failcount = 0;

		if (bs & BS_ONBATTERY) {
			st = ST_BATTERY;
		} else if (ov>0) {
			st = ST_ONLINE;
		} else {
			st = ST_OFF;
		}
		updatestatus(smode, "%s, battery level: %d%%", status[st], bl);
		if (st == ST_ONLINE && bl >= level) {
			break;
		}
		sleep(1);
	}

	/* termination condition reached */
	endstatus(smode);
	if (flash) {
		printf("Interrupting UPS load for ca. 2 minutes.\n");
		r = belkin_std_write_int(fd, REG_RESTARTTIMER, 2);
		if (r==0) {
			r = belkin_std_write_int(fd, REG_SHUTDOWNTIMER, 1);
		}
		if (r) {
			printf("Timed shutdown operation failed.\n");
			close(fd);
			return 2;
		}
	}
	close(fd);
	return 0;
}

/* ---------------------------------------------------------------------- */
/* functions which interface with main.c */

/* read all hardcoded info about this UPS */
void upsdrv_initinfo(void)
{
	char *str;
	int val;
	int i;

	/* read hard-wired values */
	val = belkin_nut_read_int(REG_VOLTRATING);
	if (val!=-1) {
		dstate_setinfo("input.voltage.nominal", "%d", val);
	}

	val = belkin_nut_read_int(REG_FREQRATING);
	if (val!=-1) {
		dstate_setinfo("input.frequency.nominal", "%d", val);
	}

	val = belkin_nut_read_int(REG_POWERRATING);
	if (val!=-1) {
		dstate_setinfo("ups.power.nominal", "%d", val);
	}

	val = belkin_nut_read_int(REG_BATVOLTRATING);
	if (val!=-1) {
		dstate_setinfo("battery.voltage.nominal", "%d", val);
	}

	xfer_lo_max = belkin_nut_read_int(REG_XFER_LO_MAX);
	xfer_lo_min = belkin_nut_read_int(REG_XFER_LO_MIN);
	xfer_hi_max = belkin_nut_read_int(REG_XFER_HI_MAX);
	xfer_hi_min = belkin_nut_read_int(REG_XFER_HI_MIN);

	str = belkin_nut_read_str(REG_UPSMODEL);
	if (str) {
		dstate_setinfo("ups.model", "%s", str);
		free(str);
	}

	val = belkin_nut_read_int(REG_FIRMWARE);
	if (val!=-1) {
		dstate_setinfo("ups.firmware", "%d", (val>>4) & 0xf);
		dstate_setinfo("ups.type", "%s", upstype[(val & 0x0f) % 3]);
	}

	/* read writable values and declare them writable */
	val = belkin_nut_read_int(REG_VOLTSENS);
	if (val!=-1) {
		dstate_setinfo("input.sensitivity", "%s", (val>=0 && val<asize(voltsens)) ? voltsens[val] : "?");
		/* declare variable writable */
		/* note: enumerated variables apparently don't need the ST_FLAG_STRING flag */
		dstate_setflags("input.sensitivity", ST_FLAG_RW);
		for (i=0; i<asize(voltsens); i++) {
			dstate_addenum("input.sensitivity", "%s", voltsens[i]);
		}
	}

	val = belkin_nut_read_int(REG_ALARMSTATUS);
	if (val!=-1) {
		dstate_setinfo("ups.beeper.status", "%s", (val==1) ? "disabled" : (val&1) ? "muted" : "enabled");

		/* declare variable writable */
		dstate_setflags("ups.beeper.status", ST_FLAG_RW);
		dstate_addenum("ups.beeper.status", "enabled");
		dstate_addenum("ups.beeper.status", "disabled");
		dstate_addenum("ups.beeper.status", "muted");
	}

	val = belkin_nut_read_int(REG_XFER_LO);
	if (val!=-1) {
		dstate_setinfo("input.transfer.low", "%d", val);

		/* declare variable writable */
		dstate_setflags("input.transfer.low", ST_FLAG_RW);

		if (xfer_lo_min != -1 && xfer_lo_max != -1) {
			/* make it enumerated */
			for (i=xfer_lo_min; i<=xfer_lo_max; i++) {
				dstate_addenum("input.transfer.low", "%d", i);
			}
		}
	}

	val = belkin_nut_read_int(REG_XFER_HI);
	if (val!=-1) {
		dstate_setinfo("input.transfer.high", "%d", val);

		/* declare variable writable */
		dstate_setflags("input.transfer.high", ST_FLAG_RW);

		if (xfer_hi_min != -1 && xfer_hi_max != -1) {
			/* make it enumerated */
			for (i=xfer_hi_min; i<=xfer_hi_max; i++) {
				dstate_addenum("input.transfer.high", "%d", i);
			}
		}
	}

	/* declare handlers for instand commands and writable variables */
	upsh.instcmd = instcmd;
	upsh.setvar = setvar;

	/* declare instant commands */
	dstate_addcmd("test.failure.start");
	dstate_addcmd("test.failure.stop");
	dstate_addcmd("test.battery.start");
	dstate_addcmd("test.battery.stop");
	dstate_addcmd("beeper.disable");
	dstate_addcmd("beeper.enable");
	dstate_addcmd("beeper.mute");
	dstate_addcmd("beeper.on");
	dstate_addcmd("beeper.off");
	dstate_addcmd("shutdown.stayoff");
	dstate_addcmd("shutdown.reboot");
	dstate_addcmd("shutdown.reboot.graceful");
	dstate_addcmd("reset.input.minmax");
}

/* update whatever info we can */
void upsdrv_updateinfo(void)
{
	int val, bs, us, ov;

	/* first read "vital" flags */
	us = belkin_nut_read_int(REG_UPSSTATUS);  /* UPS status */
	bs = belkin_nut_read_int(REG_BATSTATUS);  /* battery status */
	ov = belkin_nut_read_int(REG_OUTPUTVOLT); /* output voltage */

	if (us==-1 || bs==-1 || ov==-1) {
		upslogx(LOG_ERR, "Cannot read from UPS");
		dstate_datastale();
		return;
	}

	dstate_setinfo("output.voltage", "%.1f", 0.1*ov);

	status_init();

	if (bs & BS_ONBATTERY) {
		status_set("OB");	 /* on battery, including tests */
	} else if (ov > 0) {
		status_set("OL");	 /* online */
	} else {
		status_set("OFF");	 /* off */
	}
	if (us & US_ACFAILURE) {
		status_set("ACFAIL");	 /* AC failure, self-invented */
		/* Note: this is not the same as "on battery", because this
		   flag makes sense even during a test, or when the load is
		   off. It simply reflects the status of utility power.	 A
		   "critical" situation should be OB && BL && ACFAIL. */
	}
	if (us & US_OVERLOAD) {
		status_set("OVER");	 /* overload */
	}
	if (us & US_OVERHEAT) {
		status_set("OVERHEAT");	 /* overheat, self-invented */
	}
	if (us & US_UPSFAULT) {
		status_set("COMMFAULT"); /* UPS Fault */
	}
	if (bs & BS_LOW) {
		status_set("LB");	 /* low battery */
	}
	if (bs & BS_CHARGING) {
		status_set("CHRG");	 /* charging */
	}
	if (bs & BS_DEPLETED) {
		status_set("DEPLETED");	 /* battery depleted, self-invented */
	}
	if (bs & BS_REPLACE) {
		status_set("RB");	 /* replace battery */
	}

	status_commit();

	/* new read everything else */

	val = belkin_nut_read_int(REG_XFER_LO);
	if (val!=-1) {
		dstate_setinfo("input.transfer.low", "%d", val);
	}

	val = belkin_nut_read_int(REG_XFER_HI);
	if (val!=-1) {
		dstate_setinfo("input.transfer.high", "%d", val);
	}

	val = belkin_nut_read_int(REG_VOLTSENS);
	if (val!=-1) {
		dstate_setinfo("input.sensitivity", "%s", (val>=0 && val<asize(voltsens)) ? voltsens[val] : "?");
	}

	val = belkin_nut_read_int(REG_TESTSTATUS);
	if (val!=-1) {
		dstate_setinfo("ups.test.result", "%s", (val>=0 && val<asize(teststatus)) ? teststatus[val] : "?");
	}

	val = belkin_nut_read_int(REG_ALARMSTATUS);
	if (val!=-1) {
		dstate_setinfo("ups.beeper.status", "%s", (val==1) ? "disabled" : (val&1) ? "muted" : "enabled");
	}

	val = belkin_nut_read_int(REG_SHUTDOWNTIMER);
	if (val!=-1) {
		dstate_setinfo("ups.delay.shutdown", "%d", val);
	}

	val = belkin_nut_read_int(REG_RESTARTTIMER);
	if (val!=-1) {
		dstate_setinfo("ups.delay.restart", "%d", 60*val);
	}

	val = belkin_nut_read_int(REG_INPUTVOLT);
	if (val!=-1) {
		dstate_setinfo("input.voltage", "%.1f", 0.1*val);

		/* UPS does not keep track of min/maxutil, but we can */
		if (val>0 && (maxutil==-1 || val>maxutil)) {
			maxutil = val;
		}
		if (val>0 && (minutil==-1 || val<minutil)) {
			minutil = val;
		}
		dstate_setinfo("input.voltage.maximum", "%.1f", 0.1*maxutil);
		dstate_setinfo("input.voltage.minimum", "%.1f", 0.1*minutil);
	}

	val = belkin_nut_read_int(REG_INPUTFREQ);
	if (val!=-1) {
		dstate_setinfo("input.frequency", "%.1f", 0.1*val);
	}

	val = belkin_nut_read_int(REG_TEMPERATURE);
	if (val!=-1) {
		dstate_setinfo("ups.temperature", "%d", val);
	}

	val = belkin_nut_read_int(REG_OUTPUTFREQ);
	if (val!=-1) {
		dstate_setinfo("output.frequency", "%.1f", 0.1*val);
	}

	val = belkin_nut_read_int(REG_LOAD);
	if (val!=-1) {
		dstate_setinfo("ups.load", "%d", val);
	}

	val = belkin_nut_read_int(REG_BATVOLT);
	if (val!=-1) {
		dstate_setinfo("battery.voltage", "%.1f", 0.1*val);
	}

	val = belkin_nut_read_int(REG_BATLEVEL);
	if (val!=-1) {
		dstate_setinfo("battery.charge", "%d", val);
	}

	val = belkin_nut_read_int(REG_TIMELEFT);
	if (val!=-1) {
		dstate_setinfo("battery.runtime", "%d", 60*val);
	}

	dstate_dataok();
}

/* tell the UPS to shut down, then return - DO NOT SLEEP HERE */
void upsdrv_shutdown(void)
{
	/* Note: this UPS cannot (apparently) be put into "soft
	   shutdown" mode; thus the -k option should not normally be
	   used; instead, a workaround using the "-x wait" option
	   should be used; see belkinunv(8) for details.

	   In case somebody uses the -k option, the best we can do
	   here is a timed shutdown; this will wake up the attached
	   load after 10 minutes, come rain come shine. If AC power
	   does not return, this will probably lead to a few
	   shutdown/reboot cycles, until the batteries finally die and
	   possibly cause a system crash.

	   Don't use this! Use the solution involving the "-x wait"
	   option instead, as suggested on the belkinunv(8) man
	   page. */

	upslogx(LOG_WARNING, "You are using the -k option, which is broken for this driver.\nShutting down for 10 minutes and hoping for the best");

	belkin_nut_write_int(REG_RESTARTTIMER, 10);  /* 10 minutes */
	belkin_nut_write_int(REG_SHUTDOWNTIMER, 1);  /* 1 second */
}

int instcmd(const char *cmdname, const char *extra)
{
	int r;

	/* We use test.failure.start to initiate a "deep battery test".
	   This does not really simulate a 'power failure', because we
	   won't start shutdown procedures during a test.

	   We use test.battery.start to initiate a "10-second battery test".  */

	if (!strcasecmp(cmdname, "beeper.off")) {
		/* compatibility mode for old command */
		upslogx(LOG_WARNING,
			"The 'beeper.off' command has been renamed to 'beeper.disable'");
		return instcmd("beeper.disable", NULL);
	}

	if (!strcasecmp(cmdname, "beeper.on")) {
		/* compatibility mode for old command */
		upslogx(LOG_WARNING,
			"The 'beeper.on' command has been renamed to 'beeper.enable'");
		return instcmd("beeper.enable", NULL);
	}

	if (!strcasecmp(cmdname, "test.failure.start")) {
		r = belkin_nut_write_int(REG_TESTSTATUS, 2);
		return STAT_INSTCMD_HANDLED;  /* Future: failure if r==-1 */
	}
	if (!strcasecmp(cmdname, "test.failure.stop")) {
		r = belkin_nut_write_int(REG_TESTSTATUS, 3);
		return STAT_INSTCMD_HANDLED;  /* Future: failure if r==-1 */
	}
	if (!strcasecmp(cmdname, "test.battery.start")) {
		r = belkin_nut_write_int(REG_TESTSTATUS, 1);
		return STAT_INSTCMD_HANDLED;  /* Future: failure if r==-1 */
	}
	if (!strcasecmp(cmdname, "test.battery.stop")) {
		r = belkin_nut_write_int(REG_TESTSTATUS, 3);
		return STAT_INSTCMD_HANDLED;  /* Future: failure if r==-1 */
	}
	if (!strcasecmp(cmdname, "beeper.disable")) {
		r = belkin_nut_write_int(REG_ALARMSTATUS, 1);
		return STAT_INSTCMD_HANDLED;  /* Future: failure if r==-1 */
	}
	if (!strcasecmp(cmdname, "beeper.enable")) {
		r = belkin_nut_write_int(REG_ALARMSTATUS, 2);
		return STAT_INSTCMD_HANDLED;  /* Future: failure if r==-1 */
	}
	if (!strcasecmp(cmdname, "beeper.mute")) {
		r = belkin_nut_write_int(REG_ALARMSTATUS, 3);
		return STAT_INSTCMD_HANDLED;  /* Future: failure if r==-1 */
	}
	if (!strcasecmp(cmdname, "shutdown.stayoff")) {
		r = belkin_nut_write_int(REG_RESTARTTIMER, 0);
		r |= belkin_nut_write_int(REG_SHUTDOWNTIMER, 1); /* 1 second */
		return STAT_INSTCMD_HANDLED;  /* Future: failure if r==-1 */
	}
	if (!strcasecmp(cmdname, "shutdown.reboot")) {
		/* restarttimer is in minutes, shutdowntimer is in
		   seconds.  Still, restarttimer=1 is not safe,
		   because it might be decremented before
		   shutdowntimer is set, which would cause the UPS to
		   stay off. So we need restarttimer=2, which means,
		   the UPS will stay off between 60 and 120 seconds */
		r = belkin_nut_write_int(REG_RESTARTTIMER, 2); /* 2 minutes */
		r |= belkin_nut_write_int(REG_SHUTDOWNTIMER, 1); /* 1 second */
		return STAT_INSTCMD_HANDLED;  /* Future: failure if r==-1 */
	}
	if (!strcasecmp(cmdname, "shutdown.reboot.graceful")) {
		r = belkin_nut_write_int(REG_RESTARTTIMER, 2); /* 2 minutes */
		r |= belkin_nut_write_int(REG_SHUTDOWNTIMER, 40); /* 40 seconds */
		return STAT_INSTCMD_HANDLED;  /* Future: failure if r==-1 */
	}
	if (!strcasecmp(cmdname, "reset.input.minmax")) {
		minutil = maxutil = -1;
		dstate_setinfo("input.voltage.maximum", "none");
		dstate_setinfo("input.voltage.minimum", "none");
		return STAT_INSTCMD_HANDLED;
	}

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s] [%s]", cmdname, extra);
	return STAT_INSTCMD_UNKNOWN;
}

/* set a variable */
static int setvar(const char *varname, const char *val)
{
	int i;

	if (!strcasecmp(varname, "input.sensitivity")) {
		for (i=0; i<asize(voltsens); i++) {
			if (!strcasecmp(val, voltsens[i])) {
				belkin_nut_write_int(REG_VOLTSENS, i);
				return STAT_SET_HANDLED;  /* Future: failure if result==-1 */
			}
		}
		return STAT_SET_HANDLED;  /* Future: failure */
	} else if (!strcasecmp(varname, "ups.beeper.status")) {
		if (!strcasecmp(val, "disabled")) {
			i=1;
		} else if (!strcasecmp(val, "on") ||
			   !strcasecmp(val, "enabled")) {
			i=2;
		} else if (!strcasecmp(val, "off") ||
			   !strcasecmp(val, "muted")) {
			i=3;
		} else {
			i=atoi(val);
		}
		belkin_nut_write_int(REG_ALARMSTATUS, i);
		return STAT_SET_HANDLED;  /* Future: failure if result==-1 */
	} else if (!strcasecmp(varname, "input.transfer.low")) {
		belkin_nut_write_int(REG_XFER_LO, atoi(val));
		return STAT_SET_HANDLED;  /* Future: failure if result==-1 */
	} else if (!strcasecmp(varname, "input.transfer.high")) {
		belkin_nut_write_int(REG_XFER_HI, atoi(val));
		return STAT_SET_HANDLED;  /* Future: failure if result==-1 */
	}

	upslogx(LOG_NOTICE, "setvar: unknown var [%s]", varname);
	return STAT_SET_UNKNOWN;
}

/* extra help text for "-h" option */
void upsdrv_help(void)
{
	printf("\n");
	printf("Writable variables:\n");
	printf(" input.sensitivity: normal, medium, low\n");
	printf(" ups.beeper.status: enabled, disabled, muted\n");
	printf(" input.transfer.low: (in V)\n");
	printf(" input.transfer.high: (in V)\n");
}

/* list flags and values that you want to receive via -x */
void upsdrv_makevartable(void)
{
	/* allow '-x wait' and '-x wait=<level>' */
	addvar(VAR_FLAG, "wait",     "Wait for AC power                  ");
	addvar(VAR_VALUE, "wait",    "Wait for AC power and battery level");

	/* allow '-x nohang' */
	addvar(VAR_FLAG, "nohang",   "In wait mode: quit if UPS dead     ");

	/* allow '-x flash' */
	addvar(VAR_FLAG, "flash",    "In wait mode: do brief shutdown    ");

	/* allow '-x silent' */
	addvar(VAR_FLAG, "silent",   "In wait mode: suppress status line ");

	/* allow '-x dumbterm' */
	addvar(VAR_FLAG, "dumbterm", "In wait mode: simpler status line  ");

}

/* prep the serial port */
void upsdrv_initups(void)
{
	/* If '-x wait' or '-x wait=<level>' option given, branch into
	   standalone behavior. */
	if (getval("wait") || dstate_getinfo("driver.flag.wait")) {
	  exit(belkin_wait());
	}

	belkin_nut_open_tty();
}

void upsdrv_cleanup(void)
{
	ser_close(upsfd, device_path);
}
