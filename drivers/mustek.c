/* mustek.c - model specific routines for mustek UPS models

Based on blazer.c driver from nut 2.0.0.

Modified to work with Mustek PowerMust by Martin Hajduch (martin@hajduch.de).
If it does not work as expected, please send me a note.
Well, you can send me a note even if it works as expected ;-)

Copyright (C) 1999  Russell Kroll <rkroll@exploits.org>
2002  Phil Hutton <mustek-driver@hutton.sh>
2003  Arnaud Quette <arnaud.quette@free.fr>
2004  Martin Hajduch <martin@hajduch.de>

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

#include "main.h"
#include "serial.h"

#define DRV_VERSION	"0.2"

#define ENDCHAR		13	/* replies end with CR */
#define UPSDELAY 	3
#define MAXTRIES	10
#define SENDDELAY	100000	/* 100 ms delay between chars on transmit */
#define SER_WAIT_SEC	2	/* allow 3.0 sec for responses */
#define SER_WAIT_USEC	0

static	float	lowvolt = 0, highvolt = 0, voltrange = 0;
static	int	poll_failures = 0;
static	int	inverted_bypass_bit = 0;


static int instcmd(const char *cmdname, const char *extra)
{
  /* Stop battery test */
  if (!strcasecmp(cmdname, "test.battery.stop")) {
    ser_send_pace(upsfd, SENDDELAY, "CT\r");
    return STAT_INSTCMD_HANDLED;
  }
  
  /* Start battery test */
  if (!strcasecmp(cmdname, "test.battery.start")) {
    ser_send_pace(upsfd, SENDDELAY, "T\r");
    return STAT_INSTCMD_HANDLED;
  }
  
  upslogx(LOG_NOTICE, "instcmd: unknown command [%s]", cmdname);
  return STAT_INSTCMD_UNKNOWN;
}

void upsdrv_initinfo(void)
{
  dstate_setinfo("driver.version.internal", "%s", DRV_VERSION);
  dstate_setinfo("ups.mfr", "Mustek");
  dstate_setinfo("ups.model", "PowerMust");
  
  dstate_addcmd("test.battery.start");
  dstate_addcmd("test.battery.stop");
  
  printf("Detected UPS on %s\n", device_path);
  
  upsh.instcmd = instcmd;
  
  /* paranoia - cancel any shutdown that might already be running */
  ser_send_pace(upsfd, SENDDELAY, "C\r");
}

static void ups_sync(void)
{
  char	buf[256];
  int	tries = 0, ret;
  
  printf("Syncing with UPS: ");
  fflush(stdout);
  
  for (;;) {
    tries++;
    if (tries > MAXTRIES) {
      fatalx("\nFailed - giving up...");
    }
    
    printf(".");
    fflush(stdout);
    
    ret = ser_send_pace(upsfd, SENDDELAY, "F\r");
    
    if (ret > 0) {
      
      ret = ser_get_line(upsfd, buf, sizeof(buf), ENDCHAR, "",
			 SER_WAIT_SEC, SER_WAIT_USEC);
      if (buf[0]=='#' && strlen(buf)>1) {
	/* F command successful ! */
	ret = ser_send_pace(upsfd, SENDDELAY, "Q1\r");
	
	if (ret > 0) {
	  
	  ret = ser_get_line(upsfd, buf, sizeof(buf), ENDCHAR, "",
			     SER_WAIT_SEC, SER_WAIT_USEC);
	  if (buf[0]=='(' && strlen(buf)>2) {
	    break;
	  }
	}
      }
    }
    /* not successful ! */
    sleep(UPSDELAY);
    printf(".");
    fflush(stdout);
    
  }
  
  printf(" done\n");
}

void upsdrv_shutdown(void)
{
	ser_send_pace(upsfd, SENDDELAY, "S01R0001\r");
}

static void ups_ident(void)
{
  char	buf[256];
  int	tries = 0, ret;
  int	ratecurrent;
  float	ratevolt, ratefreq;
  
  printf("Identifying UPS: ");
  fflush (stdout);
  
  for (;;) {
    tries++;
    if (tries > MAXTRIES) {
      upsdebugx(2, "Failed - giving up...");
      exit (1);
    }
    
    printf(".");
    fflush(stdout);
    ret = ser_send_pace(upsfd, SENDDELAY, "F\r");
    
    if (ret>0) {
      
      ret = ser_get_line(upsfd, buf, sizeof(buf), ENDCHAR, "",
			 SER_WAIT_SEC, SER_WAIT_USEC);
      
      /* don't be so strict about the return,
	 it should look reasonable, though */
      if ((ret > 0) && (buf[0] == '#') && (strlen(buf) > 15)) {
	break;
      }
    }
    sleep(UPSDELAY);
    printf(".");
    fflush(stdout);
  }
  
  printf(" done\n");
  
  sscanf(buf, "%*c %f %d %*f %f", &ratevolt, &ratecurrent, &ratefreq);
  upsdebugx(2, "UPS is rated at %.2fV, %dA, %.2fHz.\n",
	    ratevolt, ratecurrent, ratefreq);
  
  /* Just some guess ... */
  lowvolt = 9.7;
  highvolt = 13.7;
  
  voltrange = highvolt - lowvolt;
}

static void pollfail(const char *why)
{
  poll_failures++;
  
  /* ignore the first few since these UPSes tend to drop characters */
  if (poll_failures == 3) {
    ser_comm_fail(why);
  }
  
  return;
}

void upsdrv_updateinfo(void)
{
  char	utility[16], outvolt[16], loadpct[16], acfreq[16], 
    battvolt[16], upstemp[16], pstat[16], buf[256];
  float	bvoltp;
  int	ret;
  
  ret = ser_send_pace(upsfd, SENDDELAY, "Q1\r");
  
  if (ret < 1) {
    pollfail("Poll failed: send status request failed");
    dstate_datastale();
    return;
  }
  
  sleep(UPSDELAY); 
  
  ret = ser_get_line(upsfd, buf, sizeof(buf), ENDCHAR, "",
		     SER_WAIT_SEC, SER_WAIT_USEC);
  
  if (ret < 1) {
    pollfail("Poll failed: read failed");
    dstate_datastale();
    return;
  }
  
  if (strlen(buf) < 40) {
    pollfail("Poll failed: short read from UPS");
    dstate_datastale();
    return;
  }
  
  if (strlen(buf) > 50) {
    pollfail("Poll failed: oversized read from UPS");
    dstate_datastale();
    return;
  }
  
  if (buf[0] != '(') {
    pollfail("Poll failed: invalid start character");
    return;
  }
  
  /* only say this if it got high enough to log a failure note */
  if (poll_failures >= 3) {
    ser_comm_good();
  }
  
  poll_failures = 0;
  
  sscanf(buf, "%*c%s %*s %s %s %s %s %s %s", utility, outvolt, 
	 loadpct, acfreq, battvolt, upstemp, pstat);
  
  bvoltp = ((atof (battvolt) - lowvolt) / voltrange) * 100.0;
  
  if (bvoltp > 100.0) {
    bvoltp = 100.0;
  }
  
  dstate_setinfo("input.voltage", "%s", utility);
  dstate_setinfo("input.frequency", "%s", acfreq);
  dstate_setinfo("output.voltage", "%s", outvolt);
  dstate_setinfo("battery.charge", "%02.1f", bvoltp);
  dstate_setinfo("battery.voltage", "%s", battvolt);
  dstate_setinfo("ups.load", "%s", loadpct);
  
  status_init();
  
  if (pstat[0] == '0') {
    status_set("OL");		/* on line */
    
    /* only allow these when OL since they're bogus when OB */
    
    if (pstat[2] == (inverted_bypass_bit ? '0' : '1')) {
      /* boost or trim in effect */
      if (atof(utility) < atof(outvolt))
	status_set("BOOST");
      
      if (atof(utility) > atof(outvolt))
	status_set("TRIM");
    }
    
  } else {
    status_set("OB");		/* on battery */
  }
  
  if (pstat[1] == '1') {
    status_set("LB");		/* low battery */
  }
  
  status_commit();
  dstate_dataok();
}

void upsdrv_help(void)
{
}

void upsdrv_makevartable(void)
{
}

void upsdrv_banner(void)
{
  printf("Network UPS Tools - mustek UPS driver %s (%s)\n", 
	 DRV_VERSION, UPS_VERSION);
}

void upsdrv_initups(void)
{
  upsfd = ser_open(device_path);
  ser_set_speed(upsfd, device_path, B2400);
  
  ups_sync();
  ups_ident();
}

void upsdrv_cleanup(void)
{
  ser_close(upsfd, device_path);
}
