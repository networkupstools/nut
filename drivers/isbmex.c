/*
   isbmex.c - model specific routines for SOLA/BASIC Mexico (ISBMEX) models

   Copyright (C) 2002 Edscott Wilson Garcia <edscott@imp.mx>
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

#define DRV_VERSION "0.03"

#define xDEBUG

#ifdef DEBUG
#define D(x) x
#else
#define D(x)
#endif

#include "main.h"
#include "serial.h"

#include <math.h>		/* for sqrt */
#include <string.h>

/*#define ENDCHAR	'&'*/
#define MAXTRIES 15
/* #define IGNCHARS	""	*/

void upsdrv_initinfo(void)
{
	dstate_setinfo("ups.mfr", "Sola/Basic Mexico");
	dstate_setinfo("ups.model", "SR-Inet 280/300/400/480/500/800");

	/* high/low voltage */
	dstate_setinfo("input.transfer.low", "95.0");	/* defined */
	dstate_setinfo("input.transfer.high", "145.0");	/* defined */

	dstate_setinfo("output.voltage", "120.0");	/* defined */
	 
 	 /* addinfo(INFO_, "", 0, 0); */
	 /*printf("Using %s %s on %s\n", getdata(INFO_MFR), getdata(INFO_MODEL), device_path);*/

	dstate_setinfo("driver.version.internal", "%s", DRV_VERSION);
}

static const char *getpacket(int *we_know){
  fd_set readfds;
  struct timeval tv;
  int bytes_per_packet=0;
  int ret;
  static const char *packet_id=NULL;
  static char buf[256];
  const char *s;
  ssize_t r;

  
  bytes_per_packet=*we_know;
  D(printf("getpacket with %d\n",bytes_per_packet);)
  
  FD_ZERO(&readfds);
  FD_SET(upsfd,&readfds);
	/* Wait up to 2 seconds. */
  tv.tv_sec = 2;
  tv.tv_usec = 0;
 
  ret=select(upsfd+1,  &readfds, NULL, NULL,&tv);
  if (!ret) {
	s="Nothing received from UPS. Check cable conexion";
	upslogx(LOG_ERR, "%s", s);
	D(printf("%s\n",s);)
	return NULL;
  }

  r=read(upsfd,buf,255);
  D(printf("%d bytes read: ",r);)
  buf[r]=0;
  if (bytes_per_packet && r < bytes_per_packet){
	     ssize_t rr;
	     D(printf("short read...\n");)
	     usleep(500000);
             tv.tv_sec = 2;
             tv.tv_usec = 0;
             ret=select(upsfd+1,  &readfds, NULL, NULL,&tv);
             if (!ret) return NULL;
	     rr=read(upsfd,buf+r,255-r);
	     r += rr;
	     if (r < bytes_per_packet) return NULL;
  }
	     
  if (!bytes_per_packet){ /* packet size determination */ 
       /* if (r%10 && r%9) {
	   printf("disregarding incomplete packet\n");
  	   return NULL;
	}*/
	if (r%10==0) *we_know=10;
	else if (r%9==0)  *we_know=9;
	return NULL;
  }
     
     /* by here we have bytes_per_packet and a complete packet */
     /* lets check if within the complete packet we have a valid packet */
 if (bytes_per_packet == 10) packet_id="&&&";  else  packet_id="***";
 s=strstr(buf,packet_id);
     /* check validity of packet */
 if (!s) {
	s="isbmex: no valid packet signature!";
	upslogx(LOG_ERR, "%s", s);
	D(printf("%s\n",s);)
	*we_know=0;
	return NULL;
 }
 D(if (s != buf) printf("overlapping packet received\n");)
 if ((int) strlen(s) < bytes_per_packet) {
		    D(printf("incomplete packet information\n");)
		    return NULL; 
 }
#ifdef DEBUG
    printf("Got signal:");
    {int i;for (i=0;i<strlen(s);i++) printf(" <%d>",(unsigned char)s[i]);}
    printf("\n");
#endif
    
 return s;
}

void upsdrv_updateinfo(void)
{
  static  float high_volt=-1, low_volt=999;
  const char	*buf=NULL;
  char buf2[17];
  int i;
  static int bytes_per_packet=0;

  for (i=0;i<5;i++) {
	  if ((buf=getpacket(&bytes_per_packet))!=NULL) break;
  }
  if (!bytes_per_packet || !buf) {
	dstate_datastale();
	return;
  }
	  
  /* do the parsing */
  {
     float in_volt,battpct,acfreq;
     double d;
     D(printf("parsing (%d bytes per packet)\n",bytes_per_packet);)
     /* input voltage :*/
     if (bytes_per_packet==9) {
	 in_volt = (unsigned char)buf[3] -'A';    
     } else {
	 d=(unsigned char)buf[3]*256+(unsigned char)buf[4];    
	 in_volt = (float) sqrt(d) + 10;
     }
     snprintf(buf2,16,"%5.1f",in_volt);
     D(printf("utility=%s\n",buf2);)
     dstate_setinfo("input.voltage", "%s", buf2);     
     
     if (in_volt >= high_volt) high_volt=in_volt;
     snprintf(buf2,16,"%5.1f",high_volt);
     D(printf("highvolt=%s\n",buf2);)
     dstate_setinfo("input.voltage.maximum", "%s", buf2);       
     
     if (in_volt <= low_volt)  low_volt=in_volt;
     snprintf(buf2,16,"%5.1f",low_volt);
     D(printf("lowvolt=%s\n",buf2);)
     dstate_setinfo("input.voltage.minimum", "%s", buf2);      
 
     battpct = (double)((unsigned char)buf[(bytes_per_packet==10)?5:4])*100.0/255.0;
     snprintf(buf2,16,"%5.1f",battpct);
     D(printf("battpct=%s\n",buf2);)
     dstate_setinfo("battery.charge", "%s", buf2);    
 
     d=(unsigned char)buf[(bytes_per_packet==10)?6:5]*256
	     + (unsigned char)buf[(bytes_per_packet==10)?7:6];    
     acfreq = 1000000/d;
     snprintf(buf2,16,"%5.2f",acfreq);
     D(printf("acfreq=%s\n",buf2);)
     dstate_setinfo("input.frequency", "%s", buf2);    

     D(printf("status: ");)
     status_init();
     switch (buf[(bytes_per_packet==10)?8:7]){
	 case 48: break; /* normal operation */
	 case 49: D(printf("BOOST ");)
		  status_set("BOOST");
		  break;
	 case 50: D(printf("TRIM ");)
		  status_set("TRIM");
	 default: break;
     }
     switch (buf[(bytes_per_packet==10)?9:8]){
	 case 48: D(printf("OL ");)
		  status_set("OL");
		  break; 
	 case 50: D(printf("LB ");)
		  status_set("LB");
	 case 49: D(printf("OB ");)
		  status_set("OB");
		  break;
	 default: break;
     }
     D(printf("\n");)
     status_commit();	     
				  
	     
   } 
   dstate_dataok();
   return;
}

void upsdrv_shutdown(void)
{
	/* shutdown is supported on models with
	 * contact closure. Some ISB models with serial
	 * support support contact closure, some don't.
	 * If yours does support it, then a 12V signal
	 * on pin 9 does the trick (only when ups is 
	 * on OB condition) */
	/* 
	 * here try to do the pin 9 trick, if it does not
	 * work, else:*/
	fatalx("Shutdown only supported with the Generic Driver, type 6 and special cable");
	/*fatalx("shutdown not supported");*/
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
	printf("Network UPS Tools - ISBMEX UPS driver %s (%s)\n\n",
		DRV_VERSION, UPS_VERSION);
}

void upsdrv_initups(void)
{
	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, B2400);
}

void upsdrv_cleanup(void)
{
	ser_close(upsfd, device_path);
}
