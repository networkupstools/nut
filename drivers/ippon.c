/*
   ippon.c - driver for Ippon UPS devices v 0.02

   Copyright (C) 2004  Alexander Fedorov <alexf@land.ru>
             
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

#define ENDCHAR		13	/* end == CR */
#define DRV_VERSION 	"0.02" 
#define SECS            3       /* 3sec delay when get           */
#define USEC            0 	/* 0ms delay when get            */
#define SENDDELAY 	100000  /* 100ms between chars when send */
#define UPSDELAY 	150000  /* 150ms between send and get    */

static float lowvlt = 0,voltrange = 0,highvolt = 0;


static int instcmd(const char *cmdname, const char *extra)
{
	/*Start battery test*/
	if (!strcasecmp(cmdname,"test.battery.start"))
	{
	    ser_send_pace(upsfd,SENDDELAY,"T\r");
	    return STAT_INSTCMD_HANDLED;
	}
	
	/*Beeper on*/
	if (!strcasecmp(cmdname,"beeper.on"))
	{
	    ser_send_pace(upsfd,SENDDELAY,"Q\r");
	    return STAT_INSTCMD_HANDLED;
	}
	
	/*Beeper off*/
	if (!strcasecmp(cmdname,"beeper.off"))
	{
	    ser_send_pace(upsfd,SENDDELAY,"Q\r");
	    return STAT_INSTCMD_HANDLED;
	}
	
	upslogx(LOG_NOTICE, "instcmd: unknown command [%s]", cmdname);
	return STAT_INSTCMD_UNKNOWN;
}


void upsdrv_initinfo(void)
{
	dstate_setinfo("driver.version.internal", "%s", DRV_VERSION);
	dstate_setinfo("ups.mfr" , "Ippon");
	dstate_setinfo("ups.model" , "universal driver");
	
	dstate_addcmd("test.battery.start");
	dstate_addcmd("beeper.on");
	dstate_addcmd("beeper.off");
	
	upsh.instcmd = instcmd;
}

void upsdrv_updateinfo(void)
{
	char	buff[256],inpvlt[8],inpflt[8],outvlt[8],outcur[8],inpfrq[8],
		batvlt[8],temp[8],type[8];

	int 	ret;
	float 	bcharge;

	ret = ser_send_pace(upsfd,SENDDELAY,"Q1\r");
	if (ret != 3) 
	{
	    ser_comm_fail("Can't send status command");
	    dstate_datastale();
	    return;
	}

	usleep(UPSDELAY);
	
	ret = ser_get_line(upsfd,buff,sizeof(buff),ENDCHAR,"",SECS,USEC);
	
	if ( ret < 1 ) 
	{
	    ser_comm_fail("Can't read status");
	    dstate_datastale();
	    return;
	}
	
	if (strlen(buff) < 46)
	{
	    ser_comm_fail("Short read from UPS");
	    dstate_datastale();
	    return;
	}
	
	if (strlen(buff) > 46)
	{
	    ser_comm_fail("Long  read from UPS");
	    dstate_datastale();
	    return;
	}
	
	if (buff[0] !=  '(')
	{
	    ser_comm_fail("Wrong start char in sequence");
	    dstate_datastale();
	    return;
	}
	
	sscanf(buff,"%*c%s %s %s %s %s %s %s %s",inpvlt,inpflt,outvlt,outcur,
		inpfrq,batvlt,temp,type);
	
	bcharge = ((atof (batvlt) - 10.2) / 3.4) *100.0;
	
	if (bcharge > 100.0 ) bcharge = 100.0;

	dstate_setinfo("input.voltage" , "%s", inpvlt);
/*	dstate_setinfo("input.voltage.minimum", "%s", inpflt); */
	dstate_setinfo("output.voltage", "%s", outvlt);
	dstate_setinfo("ups.load", "%s" , outcur);
	dstate_setinfo("input.frequency", "%s" , inpfrq);
	dstate_setinfo("battery.charge", "%02.1f" , bcharge);
	dstate_setinfo("battery.voltage", "%s" , batvlt);
	dstate_setinfo("ups.temperature", "%s" , temp);
	
	status_init();
	
	if(type[0] == '0') 
	{
	    status_set("OL");
	}
	else status_set("OB");
	
	if (type[1] == '1') status_set("LB");    
	
	status_commit();
	dstate_dataok();
}

void upsdrv_shutdown(void)
{
	int sdelay = 1, rdelay = 1;
	char str[10];
	if (testvar("sdelay"))
		sdelay = atoi(getval("sdelay"));
	if (testvar("rdelay"))
		rdelay = atoi(getval("rdelay"));
	sprintf(str, "S%.2dR%.4d\r", sdelay, rdelay);
	ser_send_pace(upsfd, SENDDELAY, str); 
}




void upsdrv_help(void)
{
}

void upsdrv_makevartable(void)
{
	addvar(VAR_VALUE, "sdelay", "Specify delay before switching off the load");
	addvar(VAR_VALUE, "rdelay", "Specify delay before restoring power (0 - never)");
}

void upsdrv_banner(void)
{
	printf("Network UPS Tools - Ippon UPS driver %s (%s)\n\n", 
		DRV_VERSION, UPS_VERSION);
}

void upsdrv_initups(void)
{
	char 	buff[256];
	int  	ret;
	int 	ratecurrent;
	float 	ratevolt,ratefreq;
	
	
	upsfd =	ser_open(device_path);
	ser_set_speed(upsfd,device_path,B2400);
/* we need send "F\r" twice.. and get answer only once,
   after second "F\r" sent (???)     				 */
	ret = ser_send_pace(upsfd,SENDDELAY,"F\r");
	usleep(UPSDELAY);
	ret = ser_get_line(upsfd,buff,sizeof(buff),ENDCHAR,"",SECS,USEC);
	
	ret = ser_send_pace(upsfd,SENDDELAY,"F\r");
	if (ret != 2) 
	{
	    ser_comm_fail("Can't send init command");
	    dstate_datastale();
	    return;
	}

	usleep(UPSDELAY);

	ret = ser_get_line(upsfd,buff,sizeof(buff),ENDCHAR,"",SECS,USEC);
	
	if ( ret < 1 ) 
	{
	    ser_comm_fail("Can't read init info");
	    dstate_datastale();
	    return;
	}

		
	if ((buff[0]=='#') && (strlen(buff) == 21)) 
	    {
		 printf("UPS identified\n") ;
	    }
	else 
	    {
		ser_comm_fail("Can't init UPS");
		dstate_datastale();
		return;
	    }
	
	sscanf(buff,"%*c %f %d %f %f",&ratevolt,&ratecurrent,&lowvlt,&ratefreq);
	upsdebugx(2, "UPS rated at %.2fV %dA %.2fHz \n ",ratevolt,ratecurrent,ratefreq);
	
	highvolt = 13.6;
	voltrange = highvolt - lowvlt;
	
}

void upsdrv_cleanup(void)
{
	ser_close(upsfd, device_path);
}
