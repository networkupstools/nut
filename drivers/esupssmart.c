/* esupssmart.c - model specific routines for Energy Sistem and Infosec models

   Copyright (C) 2003 Antonio Trujillo Coronado <lunatc@terra.es>

   Based on the file masterguard.c included in nut 1.2.2.
   (Copyright (C) 2001 Michael Spanier <mail@michael-spanier.de>)

   epsupssmart.c created on 20.5.2003
   epsupssmart.c updated on 04.6.2003: supporting 24 Volt "ES Advanced" series
   epsupssmart.c updated on 07.6.2003: adapted to the nut 1.3.X series style 

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
#include "esupssmart.h"

#define UPSDELAY	3
#define MAXTRIES	10
#define UPSMFR		"Energy Sistem"
#define SENDDELAY	100		/* 100 usec between chars on send */
#define SER_WAIT_SEC	3		/* max 3.0 sec window for reads */
#define SER_WAIT_USEC	0

#define UPSTYPE "UPS Smart/Advanced"

static	char UPSFIRM[20] = " ";
static	int UPSFIRMSET = 0;

#define Q1  1
#define F  2

static	int     DEBUG;
static	int     type;
static	char    strwakedelay[5];
static	char    strdelayshut[3];


#define MINVOLTGESSFACT12 0.91667
#define MINVOLTGESSFACT24 0.91667
#define MAXVOLTGESSFACT12 1.16667
#define MAXVOLTGESSFACT24 1.14167

static	float   minbattvolt = -1;
static	float   maxbattvolt = -1;


/**********************************************************************
* Checks that a string contains a number between 'min' and 'max' 
* and converts it into another string with 'prec' width (0 letf-padded)
* If the number is out of the range 'min-max' the output string goes to defval
***********************************************************************/

static void parse_str_num(const char *org, char *dest, int prec, 
	int numdec, float min, float max, float defval) {
      float num;
      char myfmtstr[7] = "%04.0f";

      *dest='\0';
      if (prec < 0 || prec > 99) prec = 5;
      if (numdec < 0 || prec > 10) prec = 1;
      sprintf(myfmtstr,"%%0%d.%df",prec,numdec);
      num = atof(org);
      if (num < min || num > max)
             num = defval;
      sprintf(dest,myfmtstr,num);
}

static void get_battpct(char * org, char * dest, float minv, float maxv) {
   float curvolt;
   curvolt = atof(org);
   curvolt = (((curvolt < minv ? minv : curvolt) - minv) / (maxv - minv))*100;
   sprintf(dest, "%03.0f", curvolt);
}


/********************************************************************
 * (From the original masterguard.c)
 * Helper function to split a sting into words by splitting at the 
 * SPACE character.
 *
 * Adds up to maxlen characters to the char word. 
 * Returns NULL on reaching the end of the string.
 * 
 ********************************************************************/
static char *StringSplit( char *source, char *word, int maxlen )
{
    int     i;
    int     len;
    int     wc=0;

    word[0] = '\0';
    len = strlen( source );
    for( i = 0; i < len; i++ )
    {
        if( source[i] == ' ' )
        {
            word[wc] = '\0';
            return source + i + 1;
        }
        word[wc] = source[i];
        wc++;
    }
    word[wc] = '\0';
    return NULL;
}

/********************************************************************
 * (From the original masterguard.c) 
 * Helper function to drop all whitespaces within a string.
 * 
 * "word" must be large enought to hold "source", for the worst case
 * "word" has to be exacly the size of "source".
 * 
 ********************************************************************/
static void StringStrip( char *source, char *word )
{
    int     wc=0;
    int     i;
    int     len;

    word[0] = '\0';
    len = strlen( source );
    for( i = 0; i < len; i++ )
    {
        if( source[i] == ' ' )
            continue;
        if( source[i] == '\n' )
            continue;
        if( source[i] == '\t' )
            continue;
        word[wc] = source[i];
        wc++;
    }
    word[wc] = '\0';
}

/********************************************************************
 * 
 * Function parses the status flags which occure in the Q1 and F
 * command. Sets the INFO_STATUS value ( OL, OB, ... )
 * 
 ********************************************************************/
static void parseFlags( char *flags )
{

    status_init();
    
    if( flags[0] == '1' )
        status_set("OB");
    else
        status_set("OL");

    if( flags[1] == '1' )
        status_set("LB");
        
    if( flags[2] == '1' )
        status_set("BOOST");
        
    if( flags[5] == '1' )
        status_set("CAL");

    if( flags[6] == '1' )
        status_set("OFF");


    status_commit(); 

    if( DEBUG )
          upslogx(LOG_INFO,"Status value='%s'", dstate_getinfo("ups.status"));
     
}

/********************************************************************
 *
 * Function parses the response of the query1 ( "Q1" ) command.
 * Also sets various values (IPFreq ... )
 * 
 ********************************************************************/
static void query1( char *buf )
{
    #define WORDMAXLEN 255
    char    value[WORDMAXLEN];
    char    word[WORDMAXLEN];
    char    *newPOS;
    char    *oldPOS;
    int     count = 0;
    char    battpct[10];
    

    if( DEBUG ) {
        upslogx(LOG_INFO,"Q1 Query response='%s'", buf+1); 
    }    
    oldPOS = buf + 1;
    newPOS = oldPOS;
  
    do
    {
        newPOS = StringSplit( oldPOS, word, WORDMAXLEN );
        StringStrip( word, value);
        oldPOS = newPOS;

        switch( count ) 
        {
            case  0:
                    /* Input Voltage */
                    dstate_setinfo( "input.voltage", "%s", value ); /* INFO_UTILITY */
                    break;
            case  1:
                    /* Input Voltage too. Not always same value as previous 
                       Don't know what meaning it has. */  
                    dstate_setinfo( "input.voltage.nominal", "%s", value ); /* INFO_NOM_IN_VOLT */
                    break;
            case  2:
                    /* Output Voltage */
                    dstate_setinfo( "output.voltage", "%s", value); /* INFO_OUTVOLT */
                    break;
            case  3:
                    /* Seems to be UPS Load */
                    dstate_setinfo( "ups.load", "%s", value ); /* INFO_LOADPCT */
                    break;
            case  4:
                    /* AC input Frequency */
                    dstate_setinfo( "input.frequency", "%s", value); /* INFO_ACFREQ */
                    break;
            case  5:
                    /* On Line Current Battery Voltage */
                    dstate_setinfo( "battery.voltage", "%s", value); /* INFO_BATTVOLT */
                    get_battpct(value, battpct, minbattvolt, maxbattvolt);
                    dstate_setinfo( "battery.charge", "%s", battpct);  /* INFO_BATTPCT */
                    break;
            case  6:
                    /* UPS Temperature */
                    dstate_setinfo( "ups.temperature", "%s", value ); /* INFO_UPSTEMP */
                    break;
            case  7:
                    /* Flags */
                    parseFlags( value );
                    break;
            default:
                    /* Should never be reached */
                    break;
        }
        count ++;
        oldPOS = newPOS;
    }
    while( newPOS != NULL );
    status_commit();
    dstate_dataok();	
}

/********************************************************************
 *
 * Function parses the response of the query F command.
 * On this ups seems to be static data from firmware.
 * 
 ********************************************************************/
static void queryF( char *buf )
{
    #define WORDMAXLEN 255
    char    value[WORDMAXLEN];
    char    word[WORDMAXLEN];
    char    *newPOS;
    char    *oldPOS;
    int     count = 0;
    float   ModGuess = 0;
    

    if( DEBUG ) 
    {
        upslogx(LOG_INFO,"F Query Response='%s'", buf+1); 
    }  
    oldPOS = buf + 1;
    newPOS = oldPOS;
  
    do
    {
        newPOS = StringSplit( oldPOS, word, WORDMAXLEN );
        StringStrip( word, value);
        oldPOS = newPOS;
        switch( count ) 
        {
            case  0:
                    /* Seems to be UPS On Battery Output preset. */
                    dstate_setinfo( "output.voltage.nominal", "%s", value ); /* INFO_OUTVLTSEL */
                    if (UPSFIRMSET == 0) 
                        ModGuess = atof(value);
                    break;
            case  1:
                    /* Seems to be Max Output UPS Current  */
                    dstate_setinfo( "output.current", "%s", value ); /* INFO_CURRENT */
                    if (UPSFIRMSET == 0) {
                          if (UPSFIRM[0] == ' ') { 
                              ModGuess = ModGuess * atof(value);
                              sprintf(UPSFIRM,"%04.0f", ModGuess);
                              dstate_setinfo( "ups.firmware", "%s", UPSFIRM); /* INFO_FIRMREV */
                          }    
                          else
                          {
                              sprintf(UPSFIRM,"%s", "UNKNOWN");
                              dstate_setinfo( "ups.firmware" , "%s", UPSFIRM); /* INFO_FIRMREV */
                          }
                          UPSFIRMSET = 1;
                    }
                    break;
            case  2:
                    /* Seems to be Nominal battery voltage */
                    dstate_setinfo( "battery.voltage.nominal", "%s", value); /* INFO_NOMBATVLT */
                    if (minbattvolt == -1) {
                       /* upslogx(LOG_INFO,"Guessing max/min batt.charge values");  */
                       minbattvolt = atof(value);
                       if (minbattvolt == 24) {
                          maxbattvolt = minbattvolt * MAXVOLTGESSFACT24;
                          minbattvolt = minbattvolt * MINVOLTGESSFACT24;                          
                       } else {
                          maxbattvolt = minbattvolt * MAXVOLTGESSFACT12;
                          minbattvolt = minbattvolt * MINVOLTGESSFACT12;                          
                       }
                    }
                    break;
            case  3:
                    /* Seems to be UPS On Battery Output freq.  */
                  /*  dstate_setinfo( INFO_NOM_OUT_FREQ, "%s", value); NO NEW VAR */
                    break;
                    
            default:
                    /* This should never be reached */
                    /* printf( "DEFAULT\n" ); */
                    break;
        }
        count ++;
        oldPOS = newPOS;
    }
    
    while( newPOS != NULL );
    dstate_dataok();
     
}


/********************************************************************
 *
 * Check ups.
 * 
 ********************************************************************/

static int ups_ident( int displaymsg )
{
    char    buf[255];
    int     ret;

    if (displaymsg == 1) 
       printf("Checking UPS...");
       fflush(stdout);
          
    /* Check presence of Q1 */
    ret = ser_send_pace(upsfd, SENDDELAY, "Q1\r");

    ret = ser_get_line(upsfd, buf, sizeof(buf), '\r', "",
	SER_WAIT_SEC, SER_WAIT_USEC);

    ret = strlen( buf );
    if( ret != 46 ) 
    {
        /* No Q1 response found */
        /* upslogx(LOG_INFO,"NO Q1 Query Response from UPS");  */
        printf(".");
        fflush(stdout);        
        type   = 0;
        return -1;
    }
    else
    {
        if (displaymsg > 1) 
           printf( "Found Q1..." );
        type = Q1;
    }

    /* COMANDO F */
    ret = ser_send_pace(upsfd, SENDDELAY, "F\r");

    ret = ser_get_line(upsfd, buf, sizeof(buf), '\r', "",
	SER_WAIT_SEC, SER_WAIT_USEC);

    ret = strlen( buf );
    if( ret == 21 ) 
    {
        if ( displaymsg > 1)
           printf( "Found F..." );
        type = Q1 | F;
    }
   if ( displaymsg > 1) 
    printf( "OK\n");
    return 1;
} 

/*************** instcmd *************************/

static int instcmd(const char *cmdname, const char *extra)
{
	int	ret;
	char	aux[50];

        if (!strcasecmp(cmdname, "shutdown.stop")) {
		ret = ser_send_pace(upsfd, SENDDELAY, "C\r");
		upslogx(LOG_INFO,"Cancel shutdown sent to UPS.");
		return STAT_INSTCMD_HANDLED;
        }     

        if (!strcasecmp(cmdname, "test.battery.start")) {
		ret = ser_send_pace(upsfd, SENDDELAY, "T\r");
		upslogx(LOG_INFO,"Start 10 Seconds battery test.");
		return STAT_INSTCMD_HANDLED;
        }     

        if (!strcasecmp(cmdname, "shutdown.return")) {
		snprintf(aux, sizeof(aux), "S%sR%s\r", 
			strdelayshut, strwakedelay);
		ret = ser_send_pace(upsfd, SENDDELAY, "%s", aux);
             
		upslogx(LOG_INFO,"Sending shutdown command '%s' to UPS", aux);
		return STAT_INSTCMD_HANDLED;
        }     

        if (!strcasecmp(cmdname, "shutdown.stayoff")) {
		snprintf(aux, sizeof(aux), "S%sR0000\r", strdelayshut);
		ret = ser_send_pace(upsfd, SENDDELAY, "%s", aux);

		upslogx(LOG_INFO,"Sending shutdown command '%s' to UPS", aux);
		return STAT_INSTCMD_HANDLED;
        }     

        if (!strcasecmp(cmdname, "load.off")) {
		ret = ser_send_pace(upsfd, SENDDELAY, "C\r");
		ret = ser_send_pace(upsfd, SENDDELAY, "S00R0000\r");
		upslogx(LOG_INFO,"Turning off load on UPS.");
		return STAT_INSTCMD_HANDLED;
        }     

        if (!strcasecmp(cmdname, "load.on")) {
		ret = ser_send_pace(upsfd, SENDDELAY, "C\r");
		upslogx(LOG_INFO,"Turning on load on UPS.");
		return STAT_INSTCMD_HANDLED;
        }     

	upslogx(LOG_INFO,"Unknown command '%s'", cmdname);   
	return STAT_INSTCMD_UNKNOWN;         
}



/********************************************************************
 *
 * 
 * 
 * 
 ********************************************************************/
void upsdrv_help( void )
{

}

static int ups_set_var(const char* varname, const char *val) {

    
    /* Note Command for instant shutdown with no restart: S00R0000 */
    if (!strcasecmp(varname, "ups.delay.shutdown")) {
		parse_str_num(val, strdelayshut, 2, 0, 0, 99, 2);
		dstate_setinfo( "ups.delay.shutdown", "%s", strdelayshut); /* INFO_DELAYSHUT */
    		dstate_dataok();
    		return STAT_SET_HANDLED;
     }

     if (!strcasecmp(varname, "ups.delay.start")) {    
                parse_str_num(val, strwakedelay, 4, 0, 0, 9999, 3);
		dstate_setinfo( "ups.delay.start", "%s", strwakedelay); /* INFO_WAKEDELAY */
     		dstate_dataok();
        	return STAT_SET_HANDLED;
     }		
    return STAT_SET_UNKNOWN;
}

/********************************************************************
 *
 * Function to initialize the fields of the ups driver.
 * 
 ********************************************************************/
void upsdrv_initinfo(void)
{
    dstate_setinfo("driver.version.internal", "%s", DRV_VERSION);

  /* Q1 Query */ 
    dstate_setinfo( "ups.mfr", "%s", UPSMFR); /* INFO_MFR*/
    dstate_setinfo( "ups.model", "%s", UPSTYPE); /*INFO_MODEL*/
    dstate_setinfo( "ups.firmware", "%s", UPSFIRM); /* INFO_FIRMREV */
/*    dstate_setinfo( INFO_STATUS, "");    DUDA  */
  
  /* F query */
    dstate_setinfo( "output.voltage.nominal", "220" ); /* INFO_OUTVLTSEL */ /*  F VOLTAJE NOMINAL DE SALIDA 0 */
    dstate_setinfo( "battery.voltage.nominal", "12"); /* INFO_NOMBATVLT */   /*  F VOLTAJE NOMINAL DE BATERIA 2 */

    dstate_setinfo( "ups.delay.shutdown", "%s", strdelayshut); /* INFO_DELAYSHUT */
    dstate_setinfo( "ups.delay.start", "%s", strwakedelay);  /* INFO_WAKEDELAY */
    dstate_setflags("ups.delay.shutdown", ST_FLAG_RW | ST_FLAG_STRING );
    dstate_setaux  ("ups.delay.shutdown", 2);
    dstate_setflags("ups.delay.start", ST_FLAG_RW | ST_FLAG_STRING );
    dstate_setaux  ("ups.delay.start", 4);
    

    dstate_addcmd("shutdown.stop"); /* (INFO_INSTCMD, "", 0, CMD_STOPSHUTD);*/
    dstate_addcmd("test.battery.start"); /* (INFO_INSTCMD, "", 0, CMD_SHUTDOWN);*/
    dstate_addcmd("shutdown.return"); /* (INFO_INSTCMD, "", 0, CMD_BTEST1);*/
    dstate_addcmd("shutdown.stayoff"); /* (INFO_INSTCMD, "", 0, CMD_OFF);    */
    dstate_addcmd("load.off"); /* (INFO_INSTCMD, "", 0, CMD_ON);*/
    dstate_addcmd("load.on"); /* (INFO_INSTCMD, "", 0, CMD_ON);*/

    upsh.instcmd = instcmd; 
    upsh.setvar = ups_set_var;


}


/********************************************************************
 *
 * This is the main function. It gets called if the driver wants 
 * to update the ups status and the informations.
 * 
 ********************************************************************/
void upsdrv_updateinfo(void)
{
    char    buf[255];
    int     ret;
    
    if( DEBUG ) 
        upslogx(LOG_INFO,"Updating UPS info.");

    if (type == 0)
    {
        /* Should never be reached */
        fatalx("Error, no Query mode defined. Please file bug against driver.");
    }
    
     /* F found ? */
    if( type & F ) 
    {
        ser_send_pace(upsfd, SENDDELAY, "%s", "F\x0D" );
        sleep( UPSDELAY );
        buf[0] = '\0';
	ser_get_line(upsfd, buf, sizeof(buf), '\r', "\n", 3, 0);
        ret = strlen( buf );
        if( ret != 21 ) 
        {
            upslogx( LOG_ERR, "Error in UPS F command response " );
            return;
        }
        else 
        {
            queryF( buf );
        }
    }    
    

    /* Q1 found ? */
    if( type & Q1 )
    {
        ser_send_pace(upsfd, SENDDELAY, "%s", "Q1\x0D" );
        sleep( UPSDELAY );
        buf[0] = '\0';
	ser_get_line(upsfd, buf, sizeof(buf), '\r', "\n", 3, 0);
        ret = strlen( buf );
        if( ret != 46 ) 
        {
            upslogx( LOG_ERR, "Error in UPS Q1 response " );
            return;
        }
        else 
        {        
            /* Parse the response from the UPS */
            query1( buf );
        }    
    }    

}

/********************************************************************
 *
 * Called if the driver wants to shutdown the UPS.
 * ( also used by the "-k" command line switch )
 * 
 * This cuts the utility from the UPS after about 10 seconds and restores
 * the utility one minute _after_ the utility to the UPS has restored
 * On this UPS each unit in the dot-number (".x") part means 5*x seconds
 * approx.
 *
 ********************************************************************/
void upsdrv_shutdown(void)
{
    upslogx( LOG_INFO, "Sending inmediate shutdown to UPS.");
    ser_send_pace(upsfd, SENDDELAY, "%s", "C\x0D");
    ser_send_pace(upsfd, SENDDELAY, "%s", "S00R0001\x0D" );
}

/********************************************************************
 *
 * Populate the command line switches.
 * 
 *   CS:  Cancel the shutdown process
 * Next vars exists only on the driver, NOT in the ups:
 *   WAKEDELAY: MODIFY THE INFO_WAKEDELAY var. Used by the SHUTDOWN command
 *   DELAYSHUT: MODIFY THE INFO_DELAYSHUT var. Used by the SHUTDOWN command
 *   minbattvolt
 *   maxbattvolt
 * 
 ********************************************************************/

void upsdrv_makevartable(void)
{
    addvar( VAR_FLAG, "CS", "Cancel Shutdown" );
    addvar( VAR_FLAG, "DEBUG", "Show driver poll info on syslog." );    
    addvar( VAR_VALUE, "WAKEDELAY", "Minutes to restart UPS after OFF command. 0 means no restart UPS." ); 
    addvar( VAR_VALUE, "DELAYSHUT", "Minutes to init shutdown after OFF command. 0 means inmediately." );    
    addvar( VAR_VALUE, "minbattvolt", "Battery voltage to consider battery as discharged" ); 
    addvar( VAR_VALUE, "maxbattvolt", "Battery voltage to consider battery as full charged" );    
}

/********************************************************************
 *
 * The UPS banner shown during the startup.
 *
 ********************************************************************/
void upsdrv_banner(void)
{
	printf("Network UPS Tools - Energy Sistems UPS Smart/Advanced UPS driver %s (%s)\n\n",
            DRV_VERSION, UPS_VERSION);
}

/********************************************************************
 *
 * This is the first function called by the UPS driver.
 * Detects the UPS and handles the command line args.
 * 
 ********************************************************************/
void upsdrv_initups(void)
{
    int     count = 0;
    int     fail  = 0;
    int     good  = 0;
    char    aux[10];
    aux[0] = '\0';
    
    
    DEBUG = 0;
	/* setup serial port */
    upsfd = ser_open(device_path);
    ser_set_speed(upsfd, device_path, B2400);
   
    
    strcpy(strwakedelay,"0003");
    strcpy(strdelayshut,"02");

	/* probe ups type */
    do
    {
        count++;

        if( ups_ident( count ) != 1 )
            fail++;
        /* at least two good identifications */
        if( (count - fail) == 2 )
        {
            good = 1;
            break;
        }
    } while( (count<MAXTRIES) | (good) );

    if( ! good )
    {        
        printf( "No UPS Smart UPS found!\n" );
        exit(EXIT_FAILURE);
    }
       
    printf( "Energy Sistem UPS Smart found\n" );
    
    /* Cancel Shutdown */
    if( getval("CS")) 
    {
       ser_send_pace(upsfd, SENDDELAY, "%s", "C\x0D" );
       exit(EXIT_FAILURE);
    }
    if ( getval("DEBUG")) 
    {
       DEBUG = 1;
    }    
    if( getval("DELAYSHUT"))
    {
       parse_str_num(getval("DELAYSHUT"), strdelayshut, 2, 0, 0, 99, 2);
    }  
    if( getval("WAKEDELAY")) 
    {
       parse_str_num(getval("WAKEDELAY"), strwakedelay, 4, 0, 0, 9999, 3);
    }      
    if( getval("minbattvolt"))  
    {
       parse_str_num(getval("minbattvolt"), aux , 3, 1, 0, 99, 2);
       minbattvolt = atof(aux);
    }  
    if( getval("maxbattvolt")) 
    {
       parse_str_num(getval("maxbattvolt"), aux, 3, 1, 0, 9999, 3);
       maxbattvolt = atof(aux);
    }          
}

/********************************************************************
 * From the masterguard.c Author:
 *   VIM Preferences.
 *   As you probably know vim is the best editor ever ;-)
 *   http://www.vim.org
 *
 * vim:ts=4:sw=4:tw=78:et
 * 
 * ME: Anyway I use joe ;)
 ********************************************************************/

void upsdrv_cleanup(void)
{
	ser_close(upsfd, device_path);
}
