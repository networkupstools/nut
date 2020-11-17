/*
 *
 * microdowell.c: support for Microdowell Enterprise Nxx/Bxx serial protocol based UPSes
 *
 * Copyright (C) Elio Corbolante <eliocor at microdowell.com>
 *
 * microdowell.c created on 27/09/2007
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

/*
	anything commented is optional
	anything else is mandatory
*/

#define	ENTERPRISE_PROTOCOL

#include "microdowell.h"

#include "main.h"
#include "serial.h"
#include <sys/ioctl.h>
#include "timehead.h"


#define MAX_START_DELAY    999999
#define MAX_SHUTDOWN_DELAY 32767
/* Maximum length of a string representing these values */
#define MAX_START_DELAY_LEN    6
#define MAX_SHUTDOWN_DELAY_LEN 5

#define DRIVER_NAME	"MICRODOWELL UPS driver"
#define DRIVER_VERSION	"0.01"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Elio Corbolante <eliocor@microdowell.com>",
	DRV_STABLE,
	{ NULL }
};

ENT_STRUCT ups ;
int instcmd(const char *cmdname, const char *extra);
int setvar(const char *varname, const char *val);

/* he knew... macros should evaluate their arguments only once */
#define CLAMP(x, min, max) (((x) < (min)) ? (min) : (((x) > (max)) ? (max) : (x)))

static int CheckDataChecksum(unsigned char *Buff, int Len)
{
   int i, Idx ;
   unsigned char Xor ;

   ups.FramePointer = Xor = 0 ;
   for (Idx=0 ; Idx < Len ; Idx++)
      if (Buff[Idx] == STX_CHAR)
         break ;

	ups.FramePointer = Idx ; /* Memorise start point. */

   /* Check that the message is not to short... */
   if ( (Idx > (Len-4)) || (Idx+Buff[Idx+1]+2 > Len) )
      return(ERR_MSG_TOO_SHORT) ;   /* To short message! */

   /* Calculate checksum */
   for (i=Idx+1 ; i < Idx+Buff[Idx+1]+2 ; i++)
      Xor ^= Buff[i] ;

   /* if Xor != then checksum error */
   if (Xor != Buff[i])
      return(ERR_MSG_CHECKSUM) ; /* error in checksum */

   /* If checksum OK: return */
   return(0) ;
}


static const char *ErrMessages[] = {
/*  0 */   "errorcode NOT DEFINED",   /* default error message */
/*  1 */   "I2C bus busy (e2prom)",
/*  2 */   "Command received: checksum not valid",
/*  3 */   "Command received: unrecognized command",
/*  4 */   "WRITE: eeprom address not multiple of 8",
/*  5 */   "READ: eeprom address (added with size) out of bound ",
/*  6 */   "error writing e2prom address",
/*  7 */   "error writing e2prom subaddress",
/*  8 */   "error reading e2prom data",
/*  9 */   "error writing e2prom address",
/* 10 */   "error reading e2prom subaddress",
/* 11 */   "error writing e2prom data",
/* 12 */   "error writing e2prom address during data verification",
/* 13 */   "error verification e2prom data",
/* 14 */   "e2prom data are different from those in the write buffer",
/* 15 */   "e2prom checksum error",

/* 16 */   "NO CHARS FROM PORT",
/* 17 */   "TOO FEW DATA RECEIVED: [STX] near end of message",
/* 18 */   "CHECKSUM ERROR IN MESSAGE",
/* 19 */   "OK",
/*    */   ""
   } ;

const char *PrintErr(int ErrCode)
{
	int msgIndex = 0 ;

	/* The default 'msgIndex' is 0 (error code not defined) */
	switch (ErrCode) {
		case ERR_NO_ERROR			: msgIndex = 19 ; break ;

		case ERR_I2C_BUSY       : msgIndex =  1 ; break ;
		case ERR_CMD_CHECKSUM   : msgIndex =  2 ; break ;
		case ERR_CMD_UNRECOG    : msgIndex =  3 ; break ;
		case ERR_EEP_NOBLOCK    : msgIndex =  4 ; break ;
		case ERR_EEP_OOBOUND    : msgIndex =  5 ; break ;
		case ERR_EEP_WADDR1     : msgIndex =  6 ; break ;
		case ERR_EEP_WSADDR1    : msgIndex =  7 ; break ;
		case ERR_EEP_RDATA      : msgIndex =  8 ; break ;
		case ERR_EEP_WADDR2     : msgIndex =  9 ; break ;
		case ERR_EEP_WSADDR2    : msgIndex = 10 ; break ;
		case ERR_EEP_WDATA      : msgIndex = 11 ; break ;
		case ERR_EEP_WADDRVER   : msgIndex = 12 ; break ;
		case ERR_EEP_WDATAVER   : msgIndex = 13 ; break ;
		case ERR_EEP_VERIFY     : msgIndex = 14 ; break ;
		case ERR_EEP_CHECKSUM   : msgIndex = 15 ; break ;

		case ERR_COM_NO_CHARS   : msgIndex = 16 ; break ;
		case ERR_MSG_TOO_SHORT  : msgIndex = 17 ; break ;
		case ERR_MSG_CHECKSUM   : msgIndex = 18 ; break ;
		default: msgIndex = 0 ; break ;
		}
	return(ErrMessages[msgIndex]) ;
}


int CheckErrCode(unsigned char * Buff)
{
   auto int Ret ;

   switch (Buff[2]) {
      /* I have found an error */
      case CMD_NACK   :
                  Ret = Buff[3] ;
                  break ;

		case CMD_ACK           :
		case CMD_GET_STATUS    :
		case CMD_GET_MEASURES  :
		case CMD_GET_CONFIG    :
		case CMD_GET_BATT_STAT :
		case CMD_GET_MASK      :
		case CMD_SET_TIMER     :
		case CMD_BATT_TEST     :
		case CMD_GET_BATT_TEST :
		case CMD_SD_ONESHOT    :
		case CMD_GET_SD_ONESHOT:
		case CMD_SET_SCHEDULE  :
		case CMD_GET_SCHEDULE  :
		case CMD_GET_EEP_BLOCK :
		case CMD_SET_EEP_BLOCK :
		case CMD_GET_EEP_SEED  :
		case CMD_INIT          :
						Ret = 0 ;
	   				break ;

      /* command not recognized */
      default:
						Ret = ERR_CMD_UNRECOG ;
	   				break ;
      }
   return(Ret) ;
}


void SendCmdToSerial(unsigned char *Buff, int Len)
{
	int i;
	unsigned char Tmp[20], Xor ;

	Tmp[0] = STX_CHAR ;
	Xor = Tmp[1] = (unsigned char) (Len & 0x1f) ;
	for (i=0 ; i < Tmp[1] ; i++)
	{
		Tmp[i+2] = Buff[i] ;
		Xor ^= Buff[i] ;
	}
	Tmp[Len+2] = Xor ;

	upsdebug_hex(4, "->UPS", Tmp, Len+3) ;

	/* flush serial port */
	ser_flush_in(upsfd, "", 0) ; /* empty input buffer */
	ser_send_buf(upsfd, Tmp, Len+3) ; /* send data to the UPS */
}




unsigned char * CmdSerial(unsigned char *OutBuffer, int Len, unsigned char *RetBuffer)
{
	#define TMP_BUFF_LEN	1024
   unsigned char InpBuff[TMP_BUFF_LEN+1] ;
	unsigned char TmpBuff[3] ;
   int i, ErrCode ;
   unsigned char *p ;
	int BuffLen ;

	/* The default error code (no received character) */
	ErrCode = ERR_COM_NO_CHARS ;

   SendCmdToSerial(OutBuffer, Len) ;
	usleep(10000) ; /* small delay (1/100 s) */

	/* get chars until timeout */
	BuffLen = 0 ;
	while (ser_get_char(upsfd, TmpBuff, 0, 10000) == 1)
		{
		InpBuff[BuffLen++] = TmpBuff[0] ;
		if (BuffLen > TMP_BUFF_LEN)
			break ;
		}

	upsdebug_hex(4, "UPS->", InpBuff, BuffLen) ;

	if (BuffLen > 0)
		{
		ErrCode = CheckDataChecksum(InpBuff, BuffLen) ;
		/* upsdebugx(4, "ErrCode = %d / Len = %d", ErrCode, BuffLen); */

		if (!ErrCode)
			{
			/* FramePointer to valid data! */
			p = InpBuff + ups.FramePointer ;
			/* p now point to valid data.
			 check if it is a error code. */
			ErrCode = CheckErrCode(p) ;
			if (!ErrCode)
				{
				/* I copy the data read in the buffer */
				for(i=0 ; i<(int) (p[1])+3  ; i++)
					RetBuffer[i] = p[i] ;
				ups.ErrCode = ups.ErrCount = ups.CommStatus = 0 ;
				return(RetBuffer) ;
				}
			}
		}

	/* if they have arrived here, wants to say that I have found an error.... */
	ups.ErrCode = ErrCode ;
	ups.ErrCount++ ;
	if (ups.ErrCount > 3)
		{
		ups.CommStatus &= 0x80 ;
		ups.CommStatus |= (unsigned char) (ups.ErrCount & 0x7F)  ;
			if (ups.ErrCount > 100)
			ups.ErrCount = 100 ;
		}
	return(NULL) ;	/* There have been errors in the reading of the data */
}



static int detect_hardware(void)
{
	unsigned char OutBuff[20] ;
	unsigned char InpBuff[260] ;
	unsigned char *p ;
	int i, retries ;
	struct tm *Time ;
	struct tm tmbuf;
	time_t lTime ;

	ups.ge_2kVA = 0 ;

	for (retries=0 ; retries <= 4 ; retries++)
		{
		/* Identify UPS model */
		OutBuff[0] = CMD_GET_EEP_BLOCK ;		/* get EEPROM data */
		OutBuff[1] = EEP_UPS_MODEL ;			/* UPS model */
		OutBuff[2] = 8 ;							/* number of bytes */
		if ((p = CmdSerial(OutBuff, LEN_GET_EEP_BLOCK, InpBuff)) != NULL)
			{
			/* got UPS model */
			for (i=0 ; i<8 ; i++)
				ups.UpsModel[i] = p[i+5] ;
			ups.UpsModel[8] = '\0' ;
			upsdebugx(2, "get 'UPS model': %s", PrintErr(ups.ErrCode));
			break ;	/* UPS identified: exit from ' for' LOOP */
			}
		else
			{
			upsdebugx(1, "[%d] get 'UPS model': %s", retries, PrintErr(ups.ErrCode));
			upslogx(LOG_ERR, "[%d] Unable to identify UPS model [%s]", retries, PrintErr(ups.ErrCode));
			usleep(100000) ; /* small delay (1/10 s) for next retry */
			}
		}

	/* check if I was unable to find the UPS */
	if (retries == 4)	/* UPS not found! */
		return -1;

	/* UPS serial number */
	OutBuff[0] = CMD_GET_EEP_BLOCK ;		/* get EEPROM data */
	OutBuff[1] = EEP_SERIAL_NUM ;			/* UPS serial # */
	OutBuff[2] = 8 ;							/* number of bytes */
	if ((p = CmdSerial(OutBuff, LEN_GET_EEP_BLOCK, InpBuff)) != NULL)
		{
		/* got UPS serial # */
		for (i=0 ; i<8 ; i++)
			ups.SerialNumber[i] = p[i+5] ;
		ups.SerialNumber[8] = '\0' ;
		upsdebugx(2, "get 'UPS Serial #': %s", PrintErr(ups.ErrCode));
		}
	else
		{
		upsdebugx(1, "get 'UPS Serial #': %s", PrintErr(ups.ErrCode));
		upslogx(LOG_ERR, "Unable to identify UPS serial # [%s]", PrintErr(ups.ErrCode));
		return -1;
		}


	/* Get Production date & FW info */
	OutBuff[0] = CMD_GET_EEP_BLOCK ;		/* get EEPROM data */
	OutBuff[1] = EEP_PROD_DATE ;			/* Production date + HW version */
	OutBuff[2] = 8 ;							/* number of bytes */
	if ((p = CmdSerial(OutBuff, LEN_GET_EEP_BLOCK, InpBuff)) != NULL)
		{
		/* got Production date & FW info */
		p += 5 ;	/* 'p' points to eeprom data */
		ups.YearOfProd = 2000 + p[0] ;  /* Production year of the UPS */
		ups.MonthOfProd = p[1] ;        /* Production month of the UPS */
		ups.DayOfProd = p[2] ;          /* Production day of the UPS */
		ups.HW_MajorVersion = (p[3]>>4) & 0x0F ;  /* Hardware: Major version */
		ups.HW_MinorVersion = (p[3] & 0x0F)    ;  /* Hardware: Minor version */
		ups.BR_MajorVersion = (p[4]>>4) & 0x0F ;  /* BoardHardware: Major version */
		ups.BR_MinorVersion = (p[4] & 0x0F)    ;  /* BoardHardware: Minor version */
		ups.FW_MajorVersion = (p[5]>>4) & 0x0F ;  /* Firmware: Major version */
		ups.FW_MinorVersion = (p[5] & 0x0F)    ;  /* Firmware: Minor version */
		ups.FW_SubVersion = p[6] ;      /* Firmware: SUBVERSION (special releases */
		ups.BatteryNumber = p[7] ;      /* number of batteries in UPS */
		upsdebugx(2, "get 'Production date': %s", PrintErr(ups.ErrCode));
		}
	else
		{
		upsdebugx(1, "get 'Production date': %s", PrintErr(ups.ErrCode));
		upslogx(LOG_ERR, "Unable to read Production date [%s]", PrintErr(ups.ErrCode));
		return -1;
		}


	/* Get Battery substitution date */
	OutBuff[0] = CMD_GET_EEP_BLOCK ;		/* get EEPROM data */
	OutBuff[1] = EEP_BATT_SUBST ;			/* Battery substitution dates */
	OutBuff[2] = 8 ;							/* number of bytes */
	if ((p = CmdSerial(OutBuff, LEN_GET_EEP_BLOCK, InpBuff)) != NULL)
		{
		/* got Battery substitution date */
		p += 5 ;	/* 'p' points to eeprom data */
		upsdebugx(2, "get 'Battery Subst. Dates': %s", PrintErr(ups.ErrCode));
		}
	else
		{
		upsdebugx(1, "get 'Battery Subst. Dates': %s", PrintErr(ups.ErrCode));
		upslogx(LOG_ERR, "Unable to read Battery Subst. Dates [%s]", PrintErr(ups.ErrCode));
		return -1;
		}

	/* Get working time (battery+normal)) */
	OutBuff[0] = CMD_GET_EEP_BLOCK ;		/* get EEPROM data */
	OutBuff[1] = EEP_MIN_VBATT ;			/* working time */
	OutBuff[2] = 8 ;							/* number of bytes */
	if ((p = CmdSerial(OutBuff, LEN_GET_EEP_BLOCK, InpBuff)) != NULL)
		{
		/* got working time (battery+normal)) */
		p += 5 ;	/* 'p' points to eeprom data */
		upsdebugx(2, "get 'UPS life info': %s", PrintErr(ups.ErrCode));
		}
	else
		{
		upsdebugx(1, "get 'UPS life info': %s", PrintErr(ups.ErrCode));
		upslogx(LOG_ERR, "Unable to read UPS life info [%s]", PrintErr(ups.ErrCode));
		return -1;
		}


	/* Get the THRESHOLD table (0) */
	OutBuff[0] = CMD_GET_EEP_BLOCK ;		/* get EEPROM data */
	OutBuff[1] = EEP_THRESHOLD_0 ;		/* Thresholds table 0 */
	OutBuff[2] = 8 ;							/* number of bytes */
	if ((p = CmdSerial(OutBuff, LEN_GET_EEP_BLOCK, InpBuff)) != NULL)
		{
		/* got the THRESHOLD table (0) */
		p += 5 ;	/* 'p' points to eeprom data */
		upsdebugx(2, "get 'Thresholds table 0': %s", PrintErr(ups.ErrCode));
		}
	else
		{
		upsdebugx(1, "get 'Thresholds table 0': %s", PrintErr(ups.ErrCode));
		upslogx(LOG_ERR, "Unable to read Thresholds table 0 [%s]", PrintErr(ups.ErrCode));
		return -1;
		}

	/* Get the THRESHOLD table (1) */
	OutBuff[0] = CMD_GET_EEP_BLOCK ;		/* get EEPROM data */
	OutBuff[1] = EEP_THRESHOLD_1 ;		/* Thresholds table 0 */
	OutBuff[2] = 8 ;							/* number of bytes */
	if ((p = CmdSerial(OutBuff, LEN_GET_EEP_BLOCK, InpBuff)) != NULL)
		{
		/* got the THRESHOLD table (1) */
		p += 5 ;	/* 'p' points to eeprom data */
		upsdebugx(2, "get 'Thresholds table 1': %s", PrintErr(ups.ErrCode));
		}
	else
		{
		upsdebugx(1, "get 'Thresholds table 1': %s", PrintErr(ups.ErrCode));
		upslogx(LOG_ERR, "Unable to read Thresholds table 1 [%s]", PrintErr(ups.ErrCode));
		return -1;
		}

	/* Get the THRESHOLD table (2) */
	OutBuff[0] = CMD_GET_EEP_BLOCK ;		/* get EEPROM data */
	OutBuff[1] = EEP_THRESHOLD_2 ;		/* Thresholds table 0 */
	OutBuff[2] = 8 ;							/* number of bytes */
	if ((p = CmdSerial(OutBuff, LEN_GET_EEP_BLOCK, InpBuff)) != NULL)
		{
		/* got the THRESHOLD table (2) */
		p += 5 ;	/* 'p' points to eeprom data */
		upsdebugx(2, "get 'Thresholds table 2': %s", PrintErr(ups.ErrCode));
		}
	else
		{
		upsdebugx(1, "get 'Thresholds table 2': %s", PrintErr(ups.ErrCode));
		upslogx(LOG_ERR, "Unable to read Thresholds table 2 [%s]", PrintErr(ups.ErrCode));
		return -1;
		}


	/* Get Option Bytes */
	OutBuff[0] = CMD_GET_EEP_BLOCK ;			/* get EEPROM data */
	OutBuff[1] = EEP_OPT_BYTE_BLK ;		/* Option Bytes */
	OutBuff[2] = 8 ;					/* number of bytes */
	if ((p = CmdSerial(OutBuff, LEN_GET_EEP_BLOCK, InpBuff)) != NULL)
		{
		/* got Option Bytes */
		p += 5 ;	/* 'p' points to eeprom data */
		dstate_setinfo("input.voltage.nominal", "%s", (p[EEP_OPT_BYTE_1] & 0x02) ? "110": "230") ;
		dstate_setinfo("input.frequency", "%s", (p[EEP_OPT_BYTE_1] & 0x01) ? "60.0": "50.0") ;
		upsdebugx(2, "get 'Option Bytes': %s", PrintErr(ups.ErrCode));
		}
	else
		{
		upsdebugx(1, "get 'Option Bytes': %s", PrintErr(ups.ErrCode));
		upslogx(LOG_ERR, "Unable to read Option Bytes [%s]", PrintErr(ups.ErrCode));
		return -1;
		}



	/* Get UPS sensitivity (fault points) */
	OutBuff[0] = CMD_GET_EEP_BLOCK ;		/* get EEPROM data */
	OutBuff[1] = EEP_FAULT_POINTS ;		/* Number of fault points (sensitivity)) */
	OutBuff[2] = 8 ;							/* number of bytes */
	if ((p = CmdSerial(OutBuff, LEN_GET_EEP_BLOCK, InpBuff)) != NULL)
		{
		/* got UPS sensitivity (fault points) */
		p += 5 ;	/* 'p' points to eeprom data */
		switch (p[0]) {
			case 1 : dstate_setinfo("input.sensitivity", "H") ; break ;
			case 2 : dstate_setinfo("input.sensitivity", "M") ; break ;
			case 3 : dstate_setinfo("input.sensitivity", "L") ; break ;
			default : dstate_setinfo("input.sensitivity", "L") ; break ;
			}
		upsdebugx(2, "get 'Input Sensitivity': %s", PrintErr(ups.ErrCode));
		}
	else
		{
		upsdebugx(1, "get 'Input Sensitivity': %s", PrintErr(ups.ErrCode));
		upslogx(LOG_ERR, "Unable to read Input Sensitivity [%s]", PrintErr(ups.ErrCode));
		return -1;
		}

	/* Set internal UPS clock */
	time(&lTime) ;
	Time = localtime_r(&lTime, &tmbuf);

	OutBuff[0] = CMD_SET_TIMER ;	/* set UPS internal timer */
	OutBuff[1] = (Time->tm_wday+6) % 7 ;	/* week day (0=monday) */
	OutBuff[2] = Time->tm_hour ;	/* hours */
	OutBuff[3] = Time->tm_min ;	/* minutes */
	OutBuff[4] = Time->tm_sec;		/* seconds */
	if ((p = CmdSerial(OutBuff, LEN_SET_TIMER, InpBuff)) != NULL)
		{
		upsdebugx(2, "set 'UPS internal clock': %s", PrintErr(ups.ErrCode));
		}
	else
		{
		upsdebugx(1, "set 'UPS internal clock': %s", PrintErr(ups.ErrCode));
		upslogx(LOG_ERR, "Unable to set UPS internal clock [%s]", PrintErr(ups.ErrCode));
		return -1;
		}

	return 0;	/* everything was OK */
}


/* ========================= */


void upsdrv_updateinfo(void)
{
	unsigned char OutBuff[20] ;
	unsigned char InpBuff[260] ;
	unsigned char *p ;
	/* int i ; */

	OutBuff[0] = CMD_GET_STATUS ;   /* get UPS status */
	if ((p = CmdSerial(OutBuff, LEN_GET_STATUS, InpBuff)) != NULL)
		{
		p += 3 ;	/* 'p' points to received data */

		status_init();	/* reset status flags */

		/* store last UPS status */
		ups.StatusUPS = (int)p[0] | ((int)p[1]<<8) | ((int)p[2]<<16) | ((int)p[3]<<24) ;
		ups.ShortStatus = (int)p[0] | ((int)p[1]<<8) ;
		upsdebugx(1, "ups.StatusUPS: %08lX", ups.StatusUPS);
		upsdebugx(1, "ups.ShortStatus: %04X", ups.ShortStatus);

		/* on battery? */
		if (p[0] & 0x01)
			status_set("OB");	/* YES */

		/* LOW battery? */
		if (p[0] & 0x02)
			status_set("LB");	/* YES */

		/* online? */
		if (p[0] & 0x08)
			status_set("OL");	/* YES */

		/* Overload? */
		if (p[1] & 0xC0)
			status_set("OVER");	/* YES */

		/* Offline/Init/Stanby/Waiting for mains? */
		if (p[0] & 0xE0)
			status_set("OFF");	/* YES */

		/* AVR on (boost)? */
		if (p[4] & 0x04)
			status_set("BOOST");	/* YES */

		/* AVR on (buck)? */
		if (p[4] & 0x08)
			status_set("TRIM");	/* YES */

		dstate_setinfo("ups.time", "%02d:%02d:%02d", p[6], p[7], p[8]) ;
		upsdebugx(3, "get 'Get Status': %s", PrintErr(ups.ErrCode));
		}
	else
		{
		upsdebugx(1, "get 'Get Status': %s", PrintErr(ups.ErrCode));
		/* upslogx(LOG_ERR, "get 'Get Status': %s", PrintErr(ups.ErrCode)); */
		dstate_datastale();
		return;
		}

	/* ========================= */

	OutBuff[0] = CMD_GET_MEASURES ;   /* get UPS values */
	if ((p = CmdSerial(OutBuff, LEN_GET_MEASURES, InpBuff)) != NULL)
		{
		p += 3 ;	/* 'p' points to received data */

		dstate_setinfo("input.voltage", "%d", (int)((float)(p[2]*256 + p[3]) / 36.4)) ;
		if (ups.ge_2kVA)
			{
			dstate_setinfo("output.voltage", "%d", (int)((float)(p[6]*256 + p[7]) / 63.8)) ;
			dstate_setinfo("output.current", "%1.f", ((float)(p[8]*256 + p[9]) / 635.0)) ;
			dstate_setinfo("battery.voltage", "%.1f", ((float) (p[4]*256 + p[5])) / 329.0) ;
			}
		else
			{
			dstate_setinfo("output.voltage", "%d", (int)((float)(p[6]*256 + p[7]) / 36.4)) ;
			dstate_setinfo("output.current", "%1.f", ((float)(p[8]*256 + p[9]) / 1350.0)) ;
			dstate_setinfo("battery.voltage", "%.1f", ((float) (p[4]*256 + p[5])) / 585.0) ;
			}

		dstate_setinfo("ups.temperature", "%d", (int)(((float)(p[10]*256 + p[11])-202.97) / 1.424051)) ;
		upsdebugx(3, "get 'Get Measures': %s", PrintErr(ups.ErrCode));
		}
	else
		{
		/* upsdebugx(1, "get 'Get Measures': %s", PrintErr(ups.ErrCode)); */
		upslogx(LOG_ERR, "get 'Get Measures': %s", PrintErr(ups.ErrCode));
		dstate_datastale();
		return;
		}

	/* ========================= */

	OutBuff[0] = CMD_GET_BAT_LD ;   /* get UPS Battery and Load values */
	if ((p = CmdSerial(OutBuff, LEN_GET_BAT_LD, InpBuff)) != NULL)
		{
		p += 3 ;	/* 'p' points to received data */

		dstate_setinfo("ups.power", "%d", (p[4]*256 + p[5])) ;
		/* dstate_setinfo("ups.realpower", "%d", (int)((float)(p[4]*256 + p[5]) * 0.6)) ; */
		dstate_setinfo("battery.charge", "%d", (int)p[0]) ;
		dstate_setinfo("ups.load", "%d", (int)p[6]) ;
		upsdebugx(3, "get 'Get Batt+Load Status': %s", PrintErr(ups.ErrCode));
		}
	else
		{
		/* upsdebugx(1, "get 'Get Batt+Load Status': %s", PrintErr(ups.ErrCode)); */
		upslogx(LOG_ERR, "get 'Get Batt+Load Status': %s", PrintErr(ups.ErrCode));
		dstate_datastale();
		return;
		}

	status_commit();
	dstate_dataok();

	poll_interval = 2;
}


/* ========================= */




int instcmd(const char *cmdname, const char *extra)
{
	unsigned char OutBuff[20] ;
	unsigned char InpBuff[260] ;
	unsigned char *p ;
	/* int i ; */

	upsdebugx(1, "instcmd(%s, %s)", cmdname, extra);


	if (strcasecmp(cmdname, "load.on") == 0)
		{
		OutBuff[0] = CMD_SD_ONESHOT ;   /* turn ON the outputs */
		OutBuff[1] = 0xFF ;		/* ALL outputs */
		OutBuff[2] = 0x08 ;		/* Enable outputs (immediately) */
		OutBuff[3] = 0 ;
		OutBuff[4] = 0 ;
		OutBuff[5] = 0 ;
		OutBuff[6] = 0 ;
		OutBuff[7] = 0 ;
		if ((p = CmdSerial(OutBuff, LEN_SD_ONESHOT, InpBuff)) != NULL)
			{
			p += 3 ;	/* 'p' points to received data */
			upslogx(LOG_INFO, "Turning load on.");
			upsdebugx(1, "'SdOneshot(turn_ON)': %s", PrintErr(ups.ErrCode));
			}
		else
			{
			upsdebugx(1, "'SdOneshot(turn_ON)': %s", PrintErr(ups.ErrCode));
			upslogx(LOG_ERR, "'SdOneshot(turn_ON)': %s", PrintErr(ups.ErrCode));
			}
		return STAT_INSTCMD_HANDLED;
		}

	if (strcasecmp(cmdname, "load.off") == 0)
		{
		OutBuff[0] = CMD_SD_ONESHOT ;   /* turn ON the outputs */
		OutBuff[1] = 0xFF ;		/* ALL outputs */
		OutBuff[2] = 0x04 ;		/* Disable outputs (immediately) */
		OutBuff[3] = 0 ;
		OutBuff[4] = 0 ;
		OutBuff[5] = 0 ;
		OutBuff[6] = 0 ;
		OutBuff[7] = 0 ;
		if ((p = CmdSerial(OutBuff, LEN_SD_ONESHOT, InpBuff)) != NULL)
			{
			p += 3 ;	/* 'p' points to received data */
			upslogx(LOG_INFO, "Turning load on.");
			upsdebugx(1, "'SdOneshot(turn_OFF)': %s", PrintErr(ups.ErrCode));
			}
		else
			{
			upsdebugx(1, "'SdOneshot(turn_OFF)': %s", PrintErr(ups.ErrCode));
			upslogx(LOG_ERR, "'SdOneshot(turn_OFF)': %s", PrintErr(ups.ErrCode));
			}
		return STAT_INSTCMD_HANDLED;
		}


	if (strcasecmp(cmdname, "shutdown.return") == 0)
		{
		OutBuff[0] = CMD_SD_ONESHOT ;   /* turn ON the outputs */
		OutBuff[1] = 0xFF ;		/* ALL outputs */
		if (ups.StatusUPS & 0x01)
			OutBuff[2] = 0x02 ;		/* Battery shutdown */
		else
			OutBuff[2] = 0x01 ;		/* Online shutdown */

		if (ups.ShutdownDelay < 6)
			ups.ShutdownDelay = 6 ;

		OutBuff[3] = (ups.ShutdownDelay >> 8) & 0xFF ;	/* SDDELAY (MSB) 	Shutdown value (seconds) */
		OutBuff[4] = (ups.ShutdownDelay & 0xFF) ;			/* SDDELAY (LSB) */
		OutBuff[5] = (ups.WakeUpDelay >> 16) & 0xFF ;	/* WUDELAY (MSB)	Wakeup value (seconds) */
		OutBuff[6] = (ups.WakeUpDelay >> 8) & 0xFF ;		/* WUDELAY (...) */
		OutBuff[7] = (ups.WakeUpDelay & 0xFF ) ;			/* WUDELAY (LSB) */

		if ((p = CmdSerial(OutBuff, LEN_SD_ONESHOT, InpBuff)) != NULL)
			{
			p += 3 ;	/* 'p' points to received data */
			upslogx(LOG_INFO, "Shutdown command(TYPE=%02x, SD=%u, WU=%u)", OutBuff[2], ups.ShutdownDelay, ups.WakeUpDelay) ;
			upsdebugx(3, "Shutdown command(TYPE=%02x, SD=%u, WU=%u): %s", OutBuff[2], ups.ShutdownDelay, ups.WakeUpDelay, PrintErr(ups.ErrCode));
			}
		else
			{
			upsdebugx(1, "Shutdown command(TYPE=%02x, SD=%u, WU=%u): %s", OutBuff[2], ups.ShutdownDelay, ups.WakeUpDelay, PrintErr(ups.ErrCode));
			upslogx(LOG_ERR, "Shutdown command(SD=%u, WU=%u): %s", ups.ShutdownDelay, ups.WakeUpDelay, PrintErr(ups.ErrCode));
			}
		return STAT_INSTCMD_HANDLED;
		}


	if (strcasecmp(cmdname, "shutdown.stayoff") == 0)
		{
		OutBuff[0] = CMD_SD_ONESHOT ;   /* turn ON the outputs */
		OutBuff[1] = 0xFF ;		/* ALL outputs */
		if (ups.StatusUPS & 0x01)
			OutBuff[2] = 0x02 ;		/* Battery shutdown */
		else
			OutBuff[2] = 0x01 ;		/* Online shutdown */

		if (ups.ShutdownDelay < 6)
			ups.ShutdownDelay = 6 ;

		OutBuff[3] = (ups.ShutdownDelay >> 8) & 0xFF ;	/* SDDELAY (MSB)	Shutdown value (seconds) */
		OutBuff[4] = (ups.ShutdownDelay & 0xFF) ;			/* SDDELAY (LSB) */
		OutBuff[5] = 0 ;	/* WUDELAY (MSB)	Wakeup value (seconds) */
		OutBuff[6] = 0 ;	/* WUDELAY (...) */
		OutBuff[7] = 0 ;	/* WUDELAY (LSB) */

		if ((p = CmdSerial(OutBuff, LEN_SD_ONESHOT, InpBuff)) != NULL)
			{
			p += 3 ;	/* 'p' points to received data */
			upslogx(LOG_INFO, "shutdown.stayoff - (TYPE=%02x, SD=%u, WU=%u)", OutBuff[2], ups.ShutdownDelay, 0) ;
			upsdebugx(3, "shutdown.stayoff - (TYPE=%02x, SD=%u, WU=%u): %s", OutBuff[2], ups.ShutdownDelay, 0, PrintErr(ups.ErrCode));
			}
		else
			{
			upsdebugx(1, "shutdown.stayoff - (TYPE=%02x, SD=%u, WU=%u): %s", OutBuff[2], ups.ShutdownDelay, 0, PrintErr(ups.ErrCode));
			upslogx(LOG_ERR, "shutdown.stayoff - (TYPE=%02x, SD=%u, WU=%u)", OutBuff[2], ups.ShutdownDelay, 0) ;
			}
		return STAT_INSTCMD_HANDLED;
		}

	return STAT_INSTCMD_UNKNOWN;
}

int setvar(const char *varname, const char *val)
{
	int delay;

	if (sscanf(val, "%d", &delay) != 1)
		{
		return STAT_SET_UNKNOWN;
		}

	if (strcasecmp(varname, "ups.delay.start") == 0)
		{
		delay = CLAMP(delay, 0, MAX_START_DELAY);
		upsdebugx(1, "set 'WUDELAY': %d/%d", delay, ups.WakeUpDelay);
		ups.WakeUpDelay = delay ;
		dstate_setinfo("ups.delay.start", "%d", ups.WakeUpDelay);
		dstate_dataok();
		return STAT_SET_HANDLED;
		}

	if (strcasecmp(varname, "ups.delay.shutdown") == 0)
		{
		delay = CLAMP(delay, 0, MAX_SHUTDOWN_DELAY);
		upsdebugx(1, "set 'SDDELAY': %d/%d", delay, ups.ShutdownDelay);
		ups.ShutdownDelay = delay;
		dstate_setinfo("ups.delay.shutdown", "%d", ups.ShutdownDelay);
		dstate_dataok();
		return STAT_SET_HANDLED;
		}

	return STAT_SET_UNKNOWN;
}



void upsdrv_initinfo(void)
{
	/* Get vars from ups.conf */
	if (getval("ups.delay.shutdown")) {
		ups.ShutdownDelay = CLAMP(atoi(getval("ups.delay.shutdown")), 0, MAX_SHUTDOWN_DELAY);
	}
	else {
		ups.ShutdownDelay = 120;	/* Shutdown delay in seconds */
	}

	if (getval("ups.delay.start")) {
		ups.WakeUpDelay = CLAMP(atoi(getval("ups.delay.start")), 0, MAX_START_DELAY);
	}
	else {
		ups.WakeUpDelay = 10;	/* WakeUp delay in seconds */
	}

	if (detect_hardware() == -1)
		{
		fatalx(EXIT_FAILURE,
		       "Unable to detect a Microdowell's  Enterprise UPS on port %s\nCheck the cable, port name and try again", device_path);
		}

	/* I set the correspondig UPS variables
	   They were read in 'detect_hardware()'
	   some other variables were set in 'detect_hardware()' */
	dstate_setinfo("ups.model", "Enterprise N%s", ups.UpsModel+3) ;
	dstate_setinfo("ups.power.nominal", "%d", atoi(ups.UpsModel+3) * 100) ;
	dstate_setinfo("ups.realpower.nominal", "%d", atoi(ups.UpsModel+3) * 60) ;

	ups.ge_2kVA = 0 ;		/* differentiate between 2 type of UPSs */
	if (atoi(ups.UpsModel+3) >= 20)
		ups.ge_2kVA = 1 ;

	dstate_setinfo("ups.type", "online-interactive") ;
	dstate_setinfo("ups.serial", "%s", ups.SerialNumber) ;
	dstate_setinfo("ups.firmware", "%d.%d (%d)", ups.FW_MajorVersion, ups.FW_MinorVersion, ups.FW_SubVersion) ;
	dstate_setinfo("ups.firmware.aux", "%d.%d %d.%d", ups.HW_MajorVersion, ups.HW_MinorVersion,
						ups.BR_MajorVersion, ups.BR_MinorVersion) ;
	dstate_setinfo("ups.mfr", "Microdowell") ;
	dstate_setinfo("ups.mfr.date", "%04d/%02d/%02d", ups.YearOfProd, ups.MonthOfProd, ups.DayOfProd) ;
	dstate_setinfo("battery.packs", "%d", ups.BatteryNumber) ;

	/* Register the available variables. */
	dstate_setinfo("ups.delay.start", "%d", ups.WakeUpDelay);
	dstate_setflags("ups.delay.start", ST_FLAG_RW | ST_FLAG_STRING);
	dstate_setaux("ups.delay.start", MAX_START_DELAY_LEN);

	dstate_setinfo("ups.delay.shutdown", "%d", ups.ShutdownDelay);
	dstate_setflags("ups.delay.shutdown", ST_FLAG_RW | ST_FLAG_STRING);
	dstate_setaux("ups.delay.shutdown", MAX_SHUTDOWN_DELAY_LEN);

	dstate_addcmd("load.on");
	dstate_addcmd("load.off");
	dstate_addcmd("shutdown.return");
	dstate_addcmd("shutdown.stayoff");


	/* Register the available instant commands. */
/*	dstate_addcmd("test.battery.start");
	dstate_addcmd("test.battery.stop");
	dstate_addcmd("shutdown.stop");
	dstate_addcmd("beeper.toggle");
	*/

	/* set handlers */
	upsh.instcmd = instcmd ;
	upsh.setvar = setvar;
}





void upsdrv_shutdown(void)
{
	unsigned char OutBuff[20] ;
	unsigned char InpBuff[260] ;
	unsigned char *p ;
	unsigned char BatteryFlag=0 ;

	OutBuff[0] = CMD_GET_STATUS ;   /* get UPS status */
	if ((p = CmdSerial(OutBuff, LEN_GET_STATUS, InpBuff)) != NULL)
		{
		p += 3 ;	/* 'p' points to received data */

		status_init();	/* reset status flags */

		/* store last UPS status */
		ups.StatusUPS = (int)p[0] | ((int)p[1]<<8) | ((int)p[2]<<16) | ((int)p[3]<<24) ;
		ups.ShortStatus = (int)p[0] | ((int)p[1]<<8) ;
		upsdebugx(1, "ups.StatusUPS: %08lX", ups.StatusUPS);
		upsdebugx(1, "ups.ShortStatus: %04X", ups.ShortStatus);

		/* on battery? */
		if (p[0] & 0x01)
			BatteryFlag = 1 ;	/* YES */
		upsdebugx(3, "get 'Get Status': %s", PrintErr(ups.ErrCode));
		}
	else
		{
		upsdebugx(1, "get 'Get Status': %s", PrintErr(ups.ErrCode));
		/* upslogx(LOG_ERR, "get 'Get Status': %s", PrintErr(ups.ErrCode)); */
		}


	/* Send SHUTDOWN command */
	OutBuff[0] = CMD_SD_ONESHOT ;	/* Send SHUTDOWN command */
	OutBuff[1] = 0xFF ;				/* shutdown on ALL ports */

	/* is the UPS on battery? */
	if (BatteryFlag)
		OutBuff[2] = 0x02 ;	/* Type of shutdown (BATTERY MODE) */
	else
		OutBuff[2] = 0x01 ;	/* Type of shutdown (ONLINE) */

	if (ups.ShutdownDelay < 6)
		ups.ShutdownDelay = 6 ;

	OutBuff[3] = (ups.ShutdownDelay >> 8) & 0xFF ;	/* SDDELAY (MSB)	Shutdown value (seconds) */
	OutBuff[4] = (ups.ShutdownDelay & 0xFF) ;			/* SDDELAY (LSB) */
	OutBuff[5] = (ups.WakeUpDelay >> 16) & 0xFF ;	/* WUDELAY (MSB)	Wakeup value (seconds) */
	OutBuff[6] = (ups.WakeUpDelay >> 8) & 0xFF ;		/* WUDELAY (...) */
	OutBuff[7] = (ups.WakeUpDelay & 0xFF ) ;			/* WUDELAY (LSB) */

	if ((p = CmdSerial(OutBuff, LEN_SD_ONESHOT, InpBuff)) != NULL)
		{
		upsdebugx(2, "Shutdown command(TYPE=%02x, SD=%u, WU=%u): %s", OutBuff[2], ups.ShutdownDelay, ups.WakeUpDelay, PrintErr(ups.ErrCode));
		}
	else
		{
		/* command not sent: print error code */
		upsdebugx(1, "Shutdown command(TYPE=%02x, SD=%u, WU=%u): %s", OutBuff[2], ups.ShutdownDelay, ups.WakeUpDelay, PrintErr(ups.ErrCode));
		upslogx(LOG_ERR, "Shutdown command(SD=%u, WU=%u): %s", ups.ShutdownDelay, ups.WakeUpDelay, PrintErr(ups.ErrCode));
		}
}


void upsdrv_help(void)
{
}

/* list flags and values that you want to receive via -x */
void upsdrv_makevartable(void)
{
	/* allow '-x xyzzy' */
	/* addvar(VAR_FLAG, "xyzzy", "Enable xyzzy mode"); */

	/* allow '-x foo=<some value>' */
	addvar(VAR_VALUE, "ups.delay.shutdown", "Override shutdown delay (120s)");
	addvar(VAR_VALUE, "ups.delay.start", "Override restart delay (10s)");
}

void upsdrv_initups(void)
{
	upsfd = ser_open(device_path) ;

	ser_set_speed(upsfd, device_path, B19200) ;

	/* need to clear RTS and DTR: otherwise with default cable, communication will be problematic
	   It is the same as removing pin7 from cable (pin 7 is needed for Plug&Play compatibility) */
	ser_set_dtr(upsfd, 0);
	ser_set_rts(upsfd, 0);

	usleep(10000) ; /* small delay (1/100 s)) */
}

void upsdrv_cleanup(void)
{
	/* free(dynamic_mem); */
	ser_close(upsfd, device_path) ;
}
