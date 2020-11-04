/* gamatronic.c
 *
 * SEC UPS Driver ported to the new NUT API for Gamatronic UPS Usage.
 *
 * Copyright (C) 
 *   2001 John Marley <John.Marley@alcatel.com.au>
 *   2002 Jules Taplin <jules@netsitepro.co.uk>
 *   2002 Eric Lawson <elawson@inficad.com>
 *   2005 Arnaud Quette <arnaud.quette@gmail.com>
 *   2005 Nadav Moskovitch <blutz@walla.com / http://www.gamatronic.com>
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
 *
 */
 
#include "main.h"
#include "serial.h" 
#include "gamatronic.h"

#define DRIVER_NAME	"Gamatronic UPS driver"
#define DRIVER_VERSION	"0.02"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"John Marley <John.Marley@alcatel.com.au>\n" \
	"Jules Taplin <jules@netsitepro.co.uk>\n" \
	"Eric Lawson <elawson@inficad.com>\n" \
	"Arnaud Quette <arnaud.quette@gmail.com>\n" \
	"Nadav Moskovitch <blutz@walla.com / http://www.gamatronic.com>",
	DRV_STABLE,
	{ NULL }
};

#define ENDCHAR	'\r'	
#define IGNCHARS ""
#define SER_WAIT_SEC    1	/* allow 3.0 sec for ser_get calls */
#define SER_WAIT_USEC	0

int sec_upsrecv (char *buf)
{

 char lenbuf[4];
 int  ret;
	

        ser_get_line(upsfd, buf, 140, ENDCHAR, IGNCHARS,SER_WAIT_SEC, SER_WAIT_USEC);
	if (buf[0] ==  SEC_MSG_STARTCHAR){
		switch (buf[1]){
		case  SEC_NAK:
		return(-1);
		case  SEC_ACK:
		return(0);
		case SEC_DATAMSG:
		strncpy(lenbuf,buf+2,3);
		lenbuf[3] = '\0';
		ret = atoi(lenbuf);
		if (ret > 0){
		strcpy(buf,buf+5);
		return(ret);}
		else return (-2);
		default:
		return(-2);
		}
	}	
	else 
         { return (-2); }
}

int sec_cmd(const char mode, const char *command, char *msgbuf, int *buflen)
{
    char msg[140];
    int ret;

    memset(msg, 0, sizeof(msg));

    /* create the message string */
    if (*buflen > 0) {
	snprintf(msg, sizeof(msg), "%c%c%03d%s%s", SEC_MSG_STARTCHAR,
		mode, (*buflen)+3, command, msgbuf);
    }
    else {
	snprintf(msg, sizeof(msg), "%c%c003%s", SEC_MSG_STARTCHAR,
		mode, command);
    }	
    upsdebugx(1, "PC-->UPS: \"%s\"",msg);
    ret = ser_send(upsfd, "%s", msg);
    
    upsdebugx(1, " send returned: %d",ret);

    if (ret == -1) return -1;

    ret = sec_upsrecv(msg);


    if (ret < 0) return -1;

    strncpy(msgbuf, msg, ret);
    upsdebugx(1, "UPS<--PC: \"%s\"",msg);

/*    *(msgbuf+ret) = '\0';*/

    *buflen = ret;
    return ret;
}

void addquery(const char *cmd, int field, int varnum, int pollflag)
{
    int q;

    for (q=0; q<SEC_QUERYLIST_LEN; q++) {
	if (sec_querylist[q].command == NULL) {
	    /* command has not been recorded yet */
	    sec_querylist[q].command = cmd;
	    sec_querylist[q].pollflag = pollflag;
	    upsdebugx(1, " Query %d is %s",q,cmd);
	}
	if (sec_querylist[q].command == cmd) {
	    sec_querylist[q].varnum[field-1] = varnum;
	    upsdebugx(1, " Querying varnum %d",varnum);
	    break;
	}
    }
}

void sec_setinfo(int varnum, char *value)
{	

	if (*sec_varlist[varnum].setcmd){/*Not empty*/
		
		if (sec_varlist[varnum].flags == FLAG_STRING) {
			dstate_setinfo(sec_varlist[varnum].setcmd,"%s", value);
	 	}
		else if (sec_varlist[varnum].unit == 1) {
			dstate_setinfo(sec_varlist[varnum].setcmd,"%s", value);
		}
		
		else if (sec_varlist[varnum].flags == FLAG_MULTI) {
			if (atoi(value) < 0) { 
			dstate_setinfo(sec_varlist[varnum].setcmd,"0");
			}
			else
			{dstate_setinfo(sec_varlist[varnum].setcmd,"%d", atoi(value) * sec_varlist[varnum].unit);
			}
			}
		else { 
			dstate_setinfo(sec_varlist[varnum].setcmd,"%.1f", atof(value) / sec_varlist[varnum].unit);}
			
		}
		
}
	
	
	
void update_pseudovars( void )
{
	
	status_init();
	
	if(strcmp(sec_varlist[9].value,"1")== 0) {
	status_set("OFF");
	}
	if(strcmp(sec_varlist[76].value,"0")== 0) {
	status_set("OL");
	}
	if(strcmp(sec_varlist[76].value,"1")== 0) {
	status_set("OB");
	}
	if(strcmp(sec_varlist[76].value,"2")== 0) {
	status_set("BYPASS");
	}
	if(strcmp(sec_varlist[76].value,"3")== 0) {
	status_set("TRIM");
	}
	if(strcmp(sec_varlist[76].value,"4")== 0) {
	status_set("BOOST");
	}
	if(strcmp(sec_varlist[10].value,"1")== 0) {
	status_set("OVER");
	}
	if(strcmp(sec_varlist[22].value,"1")== 0) {
	status_set("LB");
	}
	
	if(strcmp(sec_varlist[19].value,"2")== 0) {
	status_set("RB");
	}
	
	status_commit();


}

void sec_poll ( int pollflag ) {
	
	int msglen,f,q;
	char retbuf[140],*n,*r;
  
	 
  for (q=0; q<SEC_QUERYLIST_LEN; q++) {
	if (sec_querylist[q].command == NULL) break;
        if (sec_querylist[q].pollflag != pollflag) continue;
	msglen = 0;
 	sec_cmd(SEC_POLLCMD, sec_querylist[q].command, retbuf, &msglen);
	r = retbuf;
        *(r+msglen) = '\0';
	for (f=0; f<SEC_MAXFIELDS; f++) {
	    n = strchr(r, ',');
	   if (n != NULL) *n = '\0';
           if (sqv(q,f) > 0) {
	     
	   if (strcmp(sec_varlist[sqv(q,f)].value, r) != 0  ) {

		    snprintf(sec_varlist[sqv(q,f)].value, 
			sizeof(sec_varlist[sqv(q,f)].value), "%s", r);
                  
		    sec_setinfo(sqv(q,f), r);
		}
		
	/* If SEC VAR is alarm and its on, add it to the alarm property */
	
	if (sec_varlist[sqv(q,f)].flags & FLAG_ALARM && strcmp(r,"1")== 0) {
           alarm_set(sec_varlist[sqv(q,f)].name);  }
           
	  }
	   
	    
	   if (n == NULL) break;
	   r = n+1;
	}
	}

 }

void upsdrv_initinfo(void)
{
    int msglen, v;
    char *a,*p,avail_list[300];
 
    /* find out which variables/commands this UPS supports */
    msglen = 0;
    sec_cmd(SEC_POLLCMD, SEC_AVAILP1, avail_list, &msglen);
    p = avail_list + msglen;
    if (p != avail_list) *p++ = ',';
    msglen = 0;
    sec_cmd(SEC_POLLCMD, SEC_AVAILP2, p, &msglen);
    *(p+msglen) = '\0';
 
    
    if (strlen(avail_list) == 0){
     fatalx(EXIT_FAILURE, "No available variables found!");}
    a = avail_list;
   while ((p = strtok(a, ",")) != NULL) {  
    a = NULL;
    v = atoi(p);
    /* don't bother adding a write-only variable */
   if (sec_varlist[v].flags == FLAG_WONLY) continue;
    addquery(sec_varlist[v].cmd, sec_varlist[v].field, v, sec_varlist[v].poll);
    }  
    
    /* poll one time values */
    
   sec_poll(FLAG_POLLONCE);
   
   printf("UPS: %s %s\n", dstate_getinfo("ups.mfr"), dstate_getinfo("ups.model"));
    
    
}

void upsdrv_updateinfo(void)
{
   
	
	alarm_init();
	/* poll status values values */
	sec_poll(FLAG_POLL);
	alarm_commit();
	update_pseudovars();
	dstate_dataok();
	
}

void upsdrv_shutdown(void)
{
	int msg_len;
	char msgbuf[SMALLBUF];

	msg_len = snprintf(msgbuf, sizeof(msgbuf), "-1");
	sec_cmd(SEC_SETCMD, SEC_SHUTDOWN, msgbuf, &msg_len);

	msg_len = snprintf(msgbuf, sizeof(msgbuf), "1");
	sec_cmd(SEC_SETCMD, SEC_AUTORESTART, msgbuf, &msg_len);

	msg_len = snprintf(msgbuf, sizeof(msgbuf), "2");
	sec_cmd(SEC_SETCMD, SEC_SHUTTYPE,msgbuf, &msg_len);

	msg_len = snprintf(msgbuf, sizeof(msgbuf), "5");
	sec_cmd(SEC_SETCMD, SEC_SHUTDOWN, msgbuf, &msg_len);
}

/*
static int instcmd(const char *cmdname, const char *extra)
{
	if (!strcasecmp(cmdname, "test.battery.stop")) {
		ser_send_buf(upsfd, ...);
		return STAT_INSTCMD_HANDLED;
	}

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s]", cmdname);
	return STAT_INSTCMD_UNKNOWN;
}
*/

void upsdrv_help(void)
{
}

/* list flags and values that you want to receive via -x */
void upsdrv_makevartable(void)
{
	/* allow '-x xyzzy' */
	/* addvar(VAR_FLAG, "xyzzy", "Enable xyzzy mode"); */

	/* allow '-x foo=<some value>' */
	/* addvar(VAR_VALUE, "foo", "Override foo setting"); */
}

void setup_serial(const char *port)
{
    char temp[140];
    int i,ret;
 

   /* Detect the ups baudrate  */
    
    
   for (i=0; i<5; i++) {
	
        ser_set_speed(upsfd, device_path,baud_rates[i].rate);
        ret = ser_send(upsfd, "^P003MAN");
	ret = sec_upsrecv(temp);
	if (ret >= -1) break;

   }
    if (i == 5) {
	printf("Can't talk to UPS on port %s!\n",port);
	printf("Check the cabling and portname and try again\n");
	printf("Please note that this driver only support UPS Models with SEC Protocol\n");
	ser_close(upsfd, device_path);
	exit (1);
    }
    else
      printf("Connected to UPS on %s baudrate: %d\n",port, baud_rates[i].name);
}

void upsdrv_initups(void)
{
	  upsfd = ser_open(device_path);
          setup_serial(device_path);
	/* upsfd = ser_open(device_path); */
	/* ser_set_speed(upsfd, device_path, B1200); */
   
	/* probe ups type */

	/* to get variables and flags from the command line, use this:
	 *
	 * first populate with upsdrv_buildvartable above, then...
	 *
	 *                   set flag foo : /bin/driver -x foo
	 * set variable 'cable' to '1234' : /bin/driver -x cable=1234
	 *
	 * to test flag foo in your code:
	 *
	 * 	if (testvar("foo"))
	 * 		do_something();
	 *
	 * to show the value of cable:
	 *
	 *      if ((cable == getval("cable")))
	 *		printf("cable is set to %s\n", cable);
	 *	else
	 *		printf("cable is not set!\n");
	 *
	 * don't use NULL pointers - test the return result first!
	 */

	/* the upsh handlers can't be done here, as they get initialized
	 * shortly after upsdrv_initups returns to main.
	 */


}

void upsdrv_cleanup(void)
{
	/* free(dynamic_mem); */
	 ser_close(upsfd, device_path); 
}
