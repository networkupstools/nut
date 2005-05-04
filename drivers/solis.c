/* solis.c - driver for Microsol Solis UPS hardware

   Copyright (C) 2004  Silvino B. Magalhães  <sbm2yk@gmail.com>

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

   2004/10/10 - Version 0.10 - Initial release
   2004/10/20 - Version 0.20 - add Battery information in driver
   2004/10/26 - Version 0.30 - add commands and test shutdown
   2004/10/30 - Version 0.40 - add model data structs

   Microsol contributed with UPS Solis 1.2 HS 1.2 KVA for my tests.

   http://www.microsol.com.br

*/

#define DRV_VERSION "0.40"

#include <stdio.h>
#include <time.h>
#include "main.h"
#include "serial.h"
#include "solis.h"

#define false 0
#define true 1
#define ENDCHAR 13	/* replies end with CR */
/* solis commands */
#define CMD_UPSCONT 0xCC
#define CMD_SHUT    0xDD
#define CMD_SHUTRET 0xDE
#define CMD_EVENT   0xCE
#define CMD_DUMP    0xCD

/* comment out on english language */
// #define PORTUGUESE

#ifdef PORTUGUESE
#define M_UNKN     "Modêlo solis desconhecido\n"
#define NO_SOLIS   "Solis não detectado! abortando ...\n"
#define UPS_DATE   "Data no UPS %4d/%02d/%02d\n"
#define SYS_DATE   "Data do Sistema %4d/%02d/%02d dia da semana %s\n"
#define ERR_PACK   "Pacote errado\n"
#define NO_EVENT   "Não há eventos\n"
#define UPS_TIME   "Hora interna UPS %0d:%02d:%02d\n"
#define PRG_DAYS   "Shutdown Programavel Dom  Seg  Ter  Qua  Qui  Sex  Sab\n"
#define PRG_ONON   "Programação shutdown ativa\n"
#define TIME_OFF   "UPS Hora desligar %02d:%02d\n"
#define TIME_ON    "UPS Hora ligar %02d:%02d\n"
#define PRG_ONOF   "Programação shutdown desativada\n"
#else
#define M_UNKN     "Unknown solis model\n"
#define NO_SOLIS   "Solis not detected! aborting ...\n"
#define UPS_DATE   "UPS Date %4d/%02d/%02d\n"
#define SYS_DATE   "System Date %4d/%02d/%02d day of week %s\n"
#define ERR_PACK   "Wrong package\n"
#define NO_EVENT   "No events\n"
#define UPS_TIME   "UPS internal Time %0d:%02d:%02d\n"
#define PRG_DAYS   "Programming Shutdown Sun  Mon  Tue  Wed  Thu  fri  Sat\n"
#define PRG_ONON   "Shutdown programming ative\n"
#define TIME_OFF   "UPS Time power off %02d:%02d\n"
#define TIME_ON    "UPS Time power on %02d:%02d\n"
#define PRG_ONOF   "Shutdown programming not atived\n"
#endif

#define FMT_DAYS   "                      %d    %d    %d    %d    %d    %d    %d\n"

/* print UPS internal variables */
static void prnInfo( void )
{

	int sun=0, mon=0, tue=0, wed=0, thu=0, fri=0, sat=0;

	printf( UPS_DATE, Year, Month, Day );
	printf( SYS_DATE, anon, mesn, dian, seman );

	printf( UPS_TIME, ihour, imin, isec);

	tue = ( ( DaysOnWeek & 0x40 ) == 0x40 );
	wed = ( ( DaysOnWeek & 0x20 ) == 0x20 );
	thu = ( ( DaysOnWeek & 0x10 ) == 0x10 );
 	fri = ( ( DaysOnWeek & 0x08 ) == 0x08 );
	sat = ( ( DaysOnWeek & 0x04 ) == 0x04 );
	sun = ( ( DaysOnWeek & 0x02 ) == 0x02 );
	mon = ( ( DaysOnWeek & 0x01 ) == 0x01 );

	if( isprogram ){
		printf( PRG_ONON );
		printf( TIME_ON, lhour, lmin);
		printf( TIME_OFF, dhour, dmin);
		printf( PRG_DAYS );
		printf( FMT_DAYS, sun, mon, tue, wed, thu, fri, sat);
	}
	else
		printf( PRG_ONOF );

}

/* is today shutdown day */
static int IsToday( unsigned char dweek, int nweek)
{

	switch ( nweek )
	{
	case 0: // sunday
		return ( ( ( dweek & 0x02 ) == 0x02 ) );
	case 1:
		return ( ( ( dweek & 0x01 ) == 0x01 ) );
	case 2:
		return ( ( ( dweek & 0x40 ) == 0x40 ) );
	case 3:
		return ( ( ( dweek & 0x20 ) == 0x20 ) );
	case 4:
		return ( ( ( dweek & 0x10 ) == 0x10 ) );
	case 5:
		return ( ( ( dweek & 0x08 ) == 0x08 ) );
	case 6: // saturday
		return ( ( ( dweek & 0x04 ) == 0x04 ) );
	}
	return 0;
}

static void AutonomyCalc( int iauto ) /* all models */
{
	int indice, indd, lim, min, max, inf, sup, indc, bx, ipo =0;

	bx = bext[iauto];
	indice = RecPack[3];
	indd = indice - 139;
	if( UtilPower > 20 )
		ipo = ( UtilPower - 51 ) / 100;

	indc = auton[iauto].maxi;

	if( ipo > indc )
		return;

	min = auton[iauto].minc[ipo];
	inf = min - 1;
	max = auton[iauto].maxc[ipo];
	lim = max - 139;
	sup = max + 1;

	if(  UtilPower <= 20 )
	{
		Autonomy = 170;
		maxauto = 170;
	}
	else
	{
		maxauto = auton[iauto].mm[ipo][lim];
		if( indice > inf && indice < sup )
			Autonomy = auton[iauto].mm[ipo][indd];
		else
		{
			if(  indice > max ) Autonomy = maxauto;
			if(  indice < min ) Autonomy = 0;
		}
	}

	if(  BattExtension > 0 && iauto < 4 )
		Autonomy = ( Autonomy * ( BattExtension + bx ) * 1.0 / bx );

}

static void ScanReceivePack( void )
{
	int aux, im, ov = 0;

	/* model independent data */

	Year = ( RecPack[ 19 ] & 0x0F ) + BASE_YEAR;
	Month = ( RecPack[ 19 ] & 0xF0 ) >> 4;
	Day = ( RecPack[ 18 ] & 0x1F );

	/*  Days of week in UPS shutdown programming */
	DaysOnWeek = RecPack[ 17 ];

	/* time for programming UPS off */
	dhour = RecPack[15];
	dmin  = RecPack[16];
	/* time for programming UPS on */
	lhour = RecPack[13];
	lmin  = RecPack[14];
	/* UPS internal time */
	ihour = RecPack[11];
	imin  = RecPack[10];
	isec  = RecPack[9];

	if( ( ( 0x01  & RecPack[ 20 ] ) == 0x01 ) )
		Out220 = 1;
	CriticBatt = ( ( 0x04  & RecPack[ 20 ] ) == 0x04 );
	InversorOn = ( ( 0x08 & RecPack[ 20 ] ) == 0x08 );
	SuperHeat = ( ( 0x10  & RecPack[ 20 ] ) == 0x10 );
	SourceFail = ( ( 0x20  & RecPack[ 20 ] ) == 0x20 );
	OverCharge = ( ( 0x80  & RecPack[ 20 ] ) == 0x80 );

	if( ( ( 0x40  & RecPack[ 20 ] ) == 0x40 ) )
		InputValue = 1;
	else
		InputValue = 0;
	Temperature = ( 0x7F & RecPack[ 4 ]);
	if(  ( ( 0x80  & RecPack[ 4 ] ) == 0x80 ) )
	Temperature = Temperature - 128;

	/* model dependent data */

	im = inds[imodel];
	ov = Out220;

	if(  RecPack[ 6 ] >= 194 )
		InVoltage = RecPack[ 6 ] * ctab[imodel].m_involt194[0] + ctab[imodel].m_involt194[1];
	else
		InVoltage = RecPack[ 6 ] * ctab[imodel].m_involt193[0] + ctab[imodel].m_involt193[1];

	BattVoltage = RecPack[ 3 ] * ctab[imodel].m_battvolt[0] + ctab[imodel].m_battvolt[1];

	NominalPower = nompow[im];
	if(  SourceFail ) {
		OutVoltage = RecPack[ 1 ] * ctab[imodel].m_outvolt_i[ov][0] + ctab[imodel].m_outvolt_i[ov][1];
		OutCurrent = RecPack[ 5 ] * ctab[imodel].m_outcurr_i[ov][0] + ctab[imodel].m_outcurr_i[ov][1];
		AppPower = ( RecPack[ 5 ] * RecPack[ 1 ] ) * ctab[imodel].m_appp_i[ov][0] + ctab[imodel].m_appp_i[ov][1];
		UtilPower = ( RecPack[ 7 ] + RecPack[ 8 ] * 256 ) * ctab[imodel].m_utilp_i[ov][0] + ctab[imodel].m_utilp_i[ov][1];
		InCurrent = 0;
	}
	else {
	OutVoltage = RecPack[ 1 ] * ctab[imodel].m_outvolt_s[ov][0] + ctab[imodel].m_outvolt_s[ov][1];
	OutCurrent = RecPack[ 5 ] * ctab[imodel].m_outcurr_s[ov][0] + ctab[imodel].m_outcurr_s[ov][1];
	AppPower = ( RecPack[ 5 ] * RecPack[ 1 ] ) * ctab[imodel].m_appp_s[ov][0] + ctab[imodel].m_appp_s[ov][1];
	UtilPower = ( RecPack[ 7 ] + RecPack[ 8 ] * 256 ) * ctab[imodel].m_utilp_s[ov][0] + ctab[imodel].m_utilp_s[ov][1];
	InCurrent = ( ctab[imodel].m_incurr[0] * 1.0 / BattVoltage ) - ( AppPower * 1.0 / ctab[imodel].m_incurr[1] )
		+ OutCurrent *( OutVoltage * 1.0 / InVoltage );
	}

	aux = ( RecPack[ 21 ] + RecPack[ 22 ] * 256 );
	if( aux > 0 )
		InFreq = ctab[imodel].m_infreq * 1.0 / aux;
	else
		InFreq = 0;
  
	/* input voltage offset */
	if( InVoltage < InVolt_offset ) { /* all is equal 30 */
		InFreq = 0;
		InVoltage = 0;
		InCurrent = 0;
	}

	/*  app power offset */
	if( AppPower < ctab[imodel].m_appp_offset ) {
		AppPower = 0;
		UtilPower = 0;
		ChargePowerFactor = 0;
		OutCurrent = 0;
	}

	if( im < 3 )
		AutonomyCalc( im );
	else {
		if(  BattExtension == 80 )
			AutonomyCalc( im + 1 );
		else
			AutonomyCalc( im );
	}

	/* model independent data */

	batcharge = ( Autonomy / maxauto ) * 100.0;
	upscharge = ( AppPower / NominalPower ) * 100.0;

	if (batcharge > 100.0)
		batcharge = 100.0;
   
	OutFreq = 60;
	if( !( InversorOn ) ) {
		OutVoltage = 0;
		OutFreq = 0;
	}

	if(  ( !( SourceFail ) && InversorOn ) )
		OutFreq = InFreq;
   
	if(  AppPower <= 0 ) /* charge pf */
		ChargePowerFactor = 0;
	else {
		if( AppPower == 0 )
			ChargePowerFactor = 100;
		else
			ChargePowerFactor = (( UtilPower / AppPower) * 100 );
	if(  ChargePowerFactor > 100 )
		ChargePowerFactor = 100;
	}
   
	if( SourceFail && SourceLast ) /* first time failure */
		FailureFlag = true;

	/* source return */
	if( !( SourceFail ) && !( SourceLast ) ) {
		SourceReturn = true;
		ser_flush_in(upsfd,"",0);    /* clean port */
	}
   
	if( !( SourceFail ) == SourceLast ) {
		SourceReturn = false;
		FailureFlag = false;
	}
   
	SourceLast = !( SourceFail );

	/* Autonomy */
   
	if( ( Autonomy < 5 ) )
		LowBatt = true;
	else
		LowBatt = false;
                                                                                                                             
	UpsPowerFactor = 700;
   
	/* input 110V or 220v */
	if(  ( InputValue == 0 ) ) {
		InDownLim = 75;
		InUpLim = 150;
		NomInVolt = 110;
	}
	else {
		InDownLim = 150;
		InUpLim = 300;
		NomInVolt = 220;
	}

	/* output volage 220V or 110V */
	if( Out220 ) {
		OutDownLim = 190;
		OutUpLim = 250;
		NomOutVolt = 220;
	}
	else {
		OutDownLim = 100;
		OutUpLim = 140;
		NomOutVolt = 110;
	}
   
	if( SourceFail )  /* source status */
		InputStatus = 2;
	else
		InputStatus = 1;
   
	if( InversorOn )  /* output status */
		OutputStatus = 1;
	else
		OutputStatus = 2;
  
	if( OverCharge )
		OutputStatus = 3;
  
	if( CriticBatt ) /* battery status */
		BattStatus = 4;
	else
		BattStatus = 1;
   
	SourceEvents = 0;

	if( FailureFlag )
		SourceEvents = 1;
	if( SourceReturn )
		SourceEvents = 2;
   
	/* verify Inversor */
	if( Flag_inversor ) {
		InversorOnLast = InversorOn;
		Flag_inversor = false;
	}

   
	OutputEvents = 0;
	if( InversorOn && !( InversorOnLast ) )
		OutputEvents = 26;
	if( InversorOnLast && !( InversorOn ) )
		OutputEvents = 27;
	InversorOnLast = InversorOn;
	if( SuperHeat && !( SuperHeatLast ) )
		OutputEvents = 12;
	if( SuperHeatLast && !( SuperHeat ) )
		OutputEvents = 13;
	SuperHeatLast = SuperHeat;
	if( OverCharge && !( OverChargeLast ) )
		OutputEvents = 10;
	if( OverChargeLast && !( OverCharge ) )
		OutputEvents = 11;
	OverChargeLast = OverCharge;

	BattEvents = 0;
	CriticBattLast = CriticBatt;

}

static void
CommReceive(const char *bufptr,  int size)
{
	int i, CheckSum, i_end;

	if(  ( size==25 ) )
		Waiting = 0;

	switch( Waiting ) {
	/* normal package */
	case 0:
	{
	if(  size == 25 ) {
		i_end = 25;
		for( i = 0 ; i < i_end ; ++i ) {
			RecPack[i] = *bufptr;
			bufptr++;
			}
	    
		/* CheckSum verify */
		CheckSum = 0;
		i_end = 23;
		for( i = 0 ; i < i_end ; ++i )
			CheckSum = RecPack[ i ] + CheckSum;
		CheckSum = CheckSum % 256;
		
		ser_flush_in(upsfd,"",0); /* clean port */

		/* correct package */
		if( ( (RecPack[0] & 0xF0) == 0xA0 )
			&& ( RecPack[ 24 ] == 254 )
			&& ( RecPack[ 23 ] == CheckSum ) ) {

			if(!(detected)) {
				SolisModel = (int) (RecPack[0] & 0x0F);
				if( SolisModel < 13 )
				imodel = SolisModel - 10; /* 10 = 0, 11 = 1 */
			else
				imodel = SolisModel - 11; /* 13 = 2, 14 = 3, 15 = 4 */
			detected = true;
			}

			switch( SolisModel )
			{
			case 10:
			case 11:
			case 12:
			case 13:
			case 14:
			case 15:
			{
				ScanReceivePack();
				break;
			}
			default:
			{
				printf( M_UNKN );
				break;
			}
			}
		}

		}
	
	break;
	}
       
	case 1:
	{
		/* dumping package nothing to do yet */
		Waiting = 0;
		break;
	}

	}

	Waiting =0;

}

static void getbaseinfo(void)
{
	unsigned char  temp[256];
#ifdef PORTUGUESE
	char diassemana[7][4]={"Dom", "Seg", "Ter", "Qua", "Qui", "Sex", "Sab"};
#else
	char DaysOfWeek[7][4]={"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
#endif
	char    mycmd[8]; // , ch;
	unsigned char Pacote[25];
	int  i, j=0, tam, tpac=25;

	time_t *tmt;
	struct tm *now;
	tmt  = ( time_t * ) malloc( sizeof( time_t ) );
	time( tmt );
	now = localtime( tmt );
	dian = now->tm_mday;
	mesn = now->tm_mon+1;
	anon = now->tm_year+1900;
	weekn = now->tm_wday;

#ifdef PORTUGUESE
	strcpy( seman, diassemana[weekn] );
#else	
	strcpy( seman, DaysOfWeek[weekn] );
#endif

	if( testvar("battext"))
		BattExtension = atoi(getval("battext"));

	if( testvar("prgshut"))
		isprogram = atoi(getval("prgshut"));

	/* dummy read attempt to sync - throw it out */
	sprintf(mycmd,"%c%c",CMD_UPSCONT, ENDCHAR);
	ser_send(upsfd, mycmd);
	/* ser_send_char(upsfd, CMD_UPSCONT); // send the character */

	/* trying detect solis model */
	while ( ( !detected ) && ( j < 20 ) ) {
		temp[0] = 0; // flush temp buffer
		tam = ser_get_buf_len(upsfd, temp, tpac, 3, 0);
		if( tam == 25 ) {
		for( i = 0 ; i < tam ; i++ )
		    Pacote[i] = temp[i];
		}

		j++;
		if( tam == 25)
			CommReceive(Pacote, tam);
		else
			CommReceive(temp, tam);
	} /* while end */

	if( (!detected) ) {
		printf( NO_SOLIS );
		upsdrv_cleanup();
		exit(0);
	}

	switch( SolisModel )
	{
	case 10:
	case 11:
	case 12:
		{
		strcpy(Model, "Solis 1.0");
		break;
		}
	case 13:
		{
		strcpy(Model, "Solis 1.5");
		break;
		}
	case 14:
		{
		strcpy(Model, "Solis 2.0");
		break;
		}
	case 15:
		{
		strcpy(Model, "Solis 3.0");
		break;
		}
	}

	if( isprogram )
	  {
	    if( dmin < 5 )
	      {
		if( dhour > 1 )
		  hourshut = dhour - 1;
		else
		  hourshut = 23;
		minshut = 60 - ( 5 -dmin );
	      }
	    else
	      minshut = dmin - 5;
	  }

	/* manufacturer */
	dstate_setinfo("ups.mfr", "%s", "Microsol");

	dstate_setinfo("ups.model", "%s", Model);
	dstate_setinfo("input.transfer.low", "%03.1f", InDownLim);
	dstate_setinfo("input.transfer.high", "%03.1f", InUpLim);

	dstate_addcmd("shutdown.return");	/* CMD_SHUTRET */
	dstate_addcmd("shutdown.stayoff");	/* CMD_SHUT */

	printf("Detected %s on %s\n", dstate_getinfo("ups.model"), device_path);

	prnInfo();

}

static void getupdateinfo(void)
{
	unsigned char  temp[256];
	int tam, isday, hourn, minn;

	/* time update and programable shutdown block */
	time_t *tmt;
	struct tm *now;
	tmt  = ( time_t * ) malloc( sizeof( time_t ) );
	time( tmt );
	now = localtime( tmt );
	hourn = now->tm_hour;
	minn = now->tm_min;
	weekn = now->tm_wday;

	if( isprogram ) {
		isday = IsToday( DaysOnWeek, weekn );
		if( ( dhour == hourshut ) && ( minshut >= dmin ) && isday )
			progshut = 1;
	}	

	/* programable shutdown end block */

	pacsize = 25;

	/* get update package */
	temp[0] = 0; /* flush temp buffer */
	tam = ser_get_buf_len(upsfd, temp, pacsize, 3, 0);

	CommReceive(temp, tam);

}

static int instcmd(const char *cmdname, const char *extra)
{

	if (!strcasecmp(cmdname, "shutdown.return")) {
		// shutdown and restart
		ser_send_char(upsfd, CMD_SHUTRET); // 0xDE
		// ser_send_char(upsfd, ENDCHAR);
		return STAT_INSTCMD_HANDLED;
	}
	
	if (!strcasecmp(cmdname, "shutdown.stayoff")) {
		// shutdown now (one way)
		ser_send_char(upsfd, CMD_SHUT); // 0xDD
		// ser_send_char(upsfd, ENDCHAR);
		return STAT_INSTCMD_HANDLED;
	}

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s]", cmdname);
	return STAT_INSTCMD_UNKNOWN;

}

void upsdrv_initinfo(void)
{
	getbaseinfo();

	upsh.instcmd = instcmd;
	dstate_setinfo("driver.version.internal", "%s", DRV_VERSION);
}

void upsdrv_updateinfo(void)
{

	getupdateinfo(); /* new package for updates */

	dstate_setinfo("output.voltage", "%03.1f", OutVoltage);
	dstate_setinfo("input.voltage", "%03.1f", InVoltage);
	dstate_setinfo("battery.voltage", "%02.1f", BattVoltage);
	dstate_setinfo("battery.charge", "%03.1f", batcharge);

	status_init();

	if (!SourceFail )
		status_set("OL");		/* on line */
	else
		status_set("OB");		/* on battery */
	
	if (Autonomy < 5 )
		status_set("LB");		/* low battery */

	if( progshut )
		status_set("LB");		/* low battery but is a force shutdown */
	
	status_commit();
	
	dstate_setinfo("ups.temperature", "%2.2f", Temperature);
	dstate_setinfo("input.frequency", "%2.1f", InFreq);
	dstate_setinfo("ups.load", "%03.1f", upscharge);

	dstate_dataok();

}

/* power down the attached load immediately */
void upsdrv_shutdown(void)
{

	/* basic idea: find out line status and send appropriate command */
	/* on battery: send normal shutdown, ups will return by itself on utility */
	/* on line: send shutdown+return, ups will cycle and return soon */

	if (!SourceFail) {    /* on line */

		printf("On line, sending shutdown+return command...\n");
		ser_send_char(upsfd, CMD_SHUTRET );
		/* ser_send_char(upsfd, ENDCHAR); */
	}
	else {
		printf("On battery, sending normal shutdown command...\n");
		ser_send_char(upsfd, CMD_SHUT);
		/* ser_send_char(upsfd, ENDCHAR); */
	}
	
}

void upsdrv_help(void)
{

	printf("\nSolis options\n");
	printf(" Battery Extension\n");
	printf("  battext = 80\n");
	printf(" Programable power off\n");
	printf("  prgshut = 1  (activate programable power off)\n");
	printf(" Uses Solis Monitor for setting date-time power off\n");
  
}

void upsdrv_makevartable(void)
{

	addvar(VAR_VALUE, "battext", "Battery Extension (0-80)min");
	addvar(VAR_VALUE, "prgshut", "Programable power off (0-1)");

}

void upsdrv_banner(void)
{
	printf("Network UPS Tools - Microsol Solis UPS driver %s (%s)\n", 
		DRV_VERSION, UPS_VERSION);
        printf("by Silvino Magalhaes for Microsol - sbm2yk@gmail.com\n\n");
}

void upsdrv_initups(void)
{
	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, B9600);
}

void upsdrv_cleanup(void)
{
	ser_close(upsfd, device_path);
}
