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
   2005/06/30 - Version 0.41 - patch for solaris compability
   2005/07/01 - Version 0.50 - add internal e external shutdown programming
   2005/08/18 - Version 0.60 - save external shutdown programming to ups,
 			       and support new cables for solis 3
   2015/09/19 - Version 0.65 - patch for correct reading for Microsol Back-Ups BZ1200-BR
   (see the version control logs for more recent updates)

   Microsol contributed with UPS Solis 1.5 HS 1.5 KVA for my tests.

   http://www.microsol.com.br

*/

#include <ctype.h>
#include <stdio.h>
#include <math.h>
#include "main.h"
#include "serial.h"
#include "solis.h"
#include "timehead.h"

#define DRIVER_NAME	"Microsol Solis UPS driver"
#define DRIVER_VERSION	"0.65"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Silvino B. Magalhães <sbm2yk@gmail.com>",
	DRV_STABLE,
	{ NULL }
};

#define false 0
#define true 1
#define RESP_END    0xFE
#define ENDCHAR 13	/* replies end with CR */
/* solis commands */
#define CMD_UPSCONT 0xCC
#define CMD_SHUT    0xDD
#define CMD_SHUTRET 0xDE
#define CMD_EVENT   0xCE
#define CMD_DUMP    0xCD

/* comment on english language */
/* #define PORTUGUESE */

/* The following Portuguese strings are in UTF-8. */
#ifdef PORTUGUESE
#define M_UNKN     "Modêlo solis desconhecido\n"
#define NO_SOLIS   "Solis não detectado! abortando ...\n"
#define UPS_DATE   "Data no UPS %4d/%02d/%02d\n"
#define SYS_DATE   "Data do Sistema %4d/%02d/%02d dia da semana %s\n"
#define ERR_PACK   "Pacote errado\n"
#define NO_EVENT   "Não há eventos\n"
#define UPS_TIME   "Hora interna UPS %0d:%02d:%02d\n"
#define PRG_DAYS   "Shutdown Programavel Dom  Seg  Ter  Qua  Qui  Sex  Sab\n"
#define PRG_ONON   "Programação shutdown ativa externa\n"
#define PRG_ONOU   "Programação shutdown ativa interna\n"
#define TIME_OFF   "UPS Hora desligar %02d:%02d\n"
#define TIME_ON    "UPS Hora ligar %02d:%02d\n"
#define PRG_ONOF   "Programação shutdown desativada\n"
#define TODAY_DD   "Desligamento hoje as %02d:%02d\n"
#define SHUT_NOW   "Shutdown iminente!\n"
#else
#define M_UNKN     "Unknown solis model\n"
#define NO_SOLIS   "Solis not detected! aborting ...\n"
#define UPS_DATE   "UPS Date %4d/%02d/%02d\n"
#define SYS_DATE   "System Date %4d/%02d/%02d day of week %s\n"
#define ERR_PACK   "Wrong package\n"
#define NO_EVENT   "No events\n"
#define UPS_TIME   "UPS internal Time %0d:%02d:%02d\n"
#define PRG_DAYS   "Programming Shutdown Sun  Mon  Tue  Wed  Thu  Fri  Sat\n"
#define PRG_ONON   "External shutdown programming active\n"
#define PRG_ONOU   "Internal shutdown programming atcive\n"
#define TIME_OFF   "UPS Time power off %02d:%02d\n"
#define TIME_ON    "UPS Time power on %02d:%02d\n"
#define PRG_ONOF   "Shutdown programming not activated\n"
#define TODAY_DD   "Shutdown today at %02d:%02d\n"
#define SHUT_NOW   "Shutdown now!\n"
#endif

#define FMT_DAYS   "                      %d    %d    %d    %d    %d    %d    %d\n"

/* convert standard days string to firmware format */
static char* convdays( char *cop )
{

	char *stra;
	char alt[8];
	int i, ish, fim, iw;
	iw = weekn;
	if ( iw == 6)
		ish = 0;
	else
		ish = 1 + iw;

	fim = 7 - ish;
	/* rotate left only 7 bits */

	for(i=0; i < fim; i++) {
		alt[i] = cop[i+ish];
	}

	if ( ish > 0 ) {

		for(i=0; i < ish; i++) {
			alt[i+fim] = cop[i];
		}
	}

	alt[7] = 0; /* string terminator */

	stra = strdup( alt );
	return stra;
}

static int IsBinary(char ch )
{
	if( ch == '1' || ch == '0' )
		return 1;
	else
		return 0;
}

/* convert string to binary */
static int Binary( char *nome )
{

	char ch, cc;
	int cont=0, nint = 1, tobin=0;
	int ex, nbin;

	while( *nome && ( cont < 7 ) ) {
		ch = *nome;
		if( !(IsBinary( ch ) ) )
			nint = 0;
		else
		{
			if( ch == '1') {
				cc = 1;
				ex = (6 - cont);
				nbin = cc<<ex;
				tobin = tobin + nbin;
			}
		}
		nome++;
		cont++;
	}

	if( nint == 0 )
		return nint;
	else
		return tobin;
}

/* revert firmware format to standard string binary days */
static unsigned char revertdays( unsigned char dweek )
{

	char alt[8];
	unsigned char keewd;
	int i, iw;
	iw = weekn;

	/* it uses only 7 bits */

	switch ( iw )
	{
	case 0: /* sunday */
		{
		alt[0] = ( ( ( dweek & 0x20 ) == 0x20 ) );
		alt[1] = ( ( ( dweek & 0x10 ) == 0x10 ) );
		alt[2] = ( ( ( dweek & 0x08 ) == 0x08 ) );
		alt[3] = ( ( ( dweek & 0x04 ) == 0x04 ) );
		alt[4] = ( ( ( dweek & 0x02 ) == 0x02 ) );
		alt[5] = ( ( ( dweek & 0x01 ) == 0x01 ) );
		alt[6] = ( ( ( dweek & 0x40 ) == 0x40 ) );
		break;
		}
	case 1:
		{
		alt[0] = ( ( ( dweek & 0x10 ) == 0x10 ) );
		alt[1] = ( ( ( dweek & 0x08 ) == 0x08 ) );
		alt[2] = ( ( ( dweek & 0x04 ) == 0x04 ) );
		alt[3] = ( ( ( dweek & 0x02 ) == 0x02 ) );
		alt[4] = ( ( ( dweek & 0x01 ) == 0x01 ) );
		alt[5] = ( ( ( dweek & 0x40 ) == 0x40 ) );
		alt[6] = ( ( ( dweek & 0x20 ) == 0x20 ) );
		break;
		}
	case 2:
		{
		alt[0] = ( ( ( dweek & 0x08 ) == 0x08 ) );
		alt[1] = ( ( ( dweek & 0x04 ) == 0x04 ) );
		alt[2] = ( ( ( dweek & 0x02 ) == 0x02 ) );
		alt[3] = ( ( ( dweek & 0x01 ) == 0x01 ) );
		alt[4] = ( ( ( dweek & 0x40 ) == 0x40 ) );
		alt[5] = ( ( ( dweek & 0x20 ) == 0x20 ) );
		alt[6] = ( ( ( dweek & 0x10 ) == 0x10 ) );
		break;
		}
	case 3:
		{
		alt[0] = ( ( ( dweek & 0x04 ) == 0x04 ) );
		alt[1] = ( ( ( dweek & 0x02 ) == 0x02 ) );
		alt[2] = ( ( ( dweek & 0x01 ) == 0x01 ) );
		alt[3] = ( ( ( dweek & 0x40 ) == 0x40 ) );
		alt[4] = ( ( ( dweek & 0x20 ) == 0x20 ) );
		alt[5] = ( ( ( dweek & 0x10 ) == 0x10 ) );
		alt[6] = ( ( ( dweek & 0x08 ) == 0x08 ) );
		break;
		}
	case 4:
		{
		alt[0] = ( ( ( dweek & 0x02 ) == 0x02 ) );
		alt[1] = ( ( ( dweek & 0x01 ) == 0x01 ) );
		alt[2] = ( ( ( dweek & 0x40 ) == 0x40 ) );
		alt[3] = ( ( ( dweek & 0x20 ) == 0x20 ) );
		alt[4] = ( ( ( dweek & 0x10 ) == 0x10 ) );
		alt[5] = ( ( ( dweek & 0x08 ) == 0x08 ) );
		alt[6] = ( ( ( dweek & 0x04 ) == 0x04 ) );
		break;
		}
	case 5:
		{
		alt[0] = ( ( ( dweek & 0x01 ) == 0x01 ) );
		alt[1] = ( ( ( dweek & 0x40 ) == 0x40 ) );
		alt[2] = ( ( ( dweek & 0x20 ) == 0x20 ) );
		alt[3] = ( ( ( dweek & 0x10 ) == 0x10 ) );
		alt[4] = ( ( ( dweek & 0x08 ) == 0x08 ) );
		alt[5] = ( ( ( dweek & 0x04 ) == 0x04 ) );
		alt[6] = ( ( ( dweek & 0x02 ) == 0x02 ) );
		break;
		}
	case 6: /* saturday */
		{
		alt[0] = ( ( ( dweek & 0x40 ) == 0x40 ) );
		alt[1] = ( ( ( dweek & 0x20 ) == 0x20 ) );
		alt[2] = ( ( ( dweek & 0x10 ) == 0x10 ) );
		alt[3] = ( ( ( dweek & 0x08 ) == 0x08 ) );
		alt[4] = ( ( ( dweek & 0x04 ) == 0x04 ) );
		alt[5] = ( ( ( dweek & 0x02 ) == 0x02 ) );
		alt[6] = ( ( ( dweek & 0x01 ) == 0x01 ) );
		}

	}

	for(i=0; i < 7; i++) {
		if( alt[i] == 0 )
			alt[i] = '0';
		if( alt[i] == 1 )
			alt[i] = '1';
	}

	alt[7] = 0; /* string terminator */
	keewd = Binary ( alt );

	return keewd;

}

static int IsHour( char *strx, int qual )
{

	int hora=0, min = 0;

	if ((strlen(strx) != 5) || (sscanf(strx, "%d:%d", &hora, &min) != 2)) {
		return -1;
	}

	if( qual ) {
		dhour = hora;
		dmin = min;
	}
	else
	{
		lhour = hora;
		lmin = min;
	}
	return 1;

}


static void sendshut( void )
{

	int i;

	for(i=0; i < 10; i++)
	  ser_send_char(upsfd, CMD_SHUT );

	upslogx(LOG_NOTICE, "Ups shutdown command sent");
	printf("Ups shutdown command sent\n");

}

/* save config ups */
static void confups( void )
{

	int i, chks = 0;

	ConfigPack[0] = 0xCF;
	ConfigPack[1] = ihour;
	ConfigPack[2] = imin;
	ConfigPack[3] = isec;
	ConfigPack[4] = lhour;
	ConfigPack[5] = lmin;
	ConfigPack[6] = dhour;
	ConfigPack[7] = dmin;
	ConfigPack[8] = weekn << 5;
	ConfigPack[8] = ConfigPack[8] | dian;
	ConfigPack[9] = mesn << 4;
	ConfigPack[9] = ConfigPack[9] | ( anon - BASE_YEAR );
	ConfigPack[10] = DaysOffWeek;

	/* MSB zero */
	ConfigPack[10] = ConfigPack[10] & (~(0x80));

	for(i=0; i < 11; i++)
	  chks = chks + ConfigPack[i];

	ConfigPack[11] = chks % 256;

	for(i=0; i < 12; i++)
	  ser_send_char(upsfd, ConfigPack[i] );

}

/* print UPS internal variables */
static void prnInfo( void )
{

	int sunday=0, monday=0, tuesday=0, wednesday=0, thursday=0, friday=0, saturday=0;
	unsigned char dweek;

	printf( UPS_DATE, Year, Month, Day );
	printf( SYS_DATE, anon, mesn, dian, seman );

	printf( UPS_TIME, ihour, imin, isec);

	dweek = DaysStd;

	if( prgups > 0 ) {

		/* this is the string to binary standard */
		sunday = ( ( dweek & 0x40 ) == 0x40 );
		monday = ( ( dweek & 0x20 ) == 0x20 );
		tuesday = ( ( dweek & 0x10 ) == 0x10 );
		wednesday = ( ( dweek & 0x08 ) == 0x08 );
		thursday = ( ( dweek & 0x04 ) == 0x04 );
		friday = ( ( dweek & 0x02 ) == 0x02 );
		saturday = ( ( dweek & 0x01 ) == 0x01 );

		if( prgups == 3)
			printf( PRG_ONOU );
		else
			printf( PRG_ONON );
		printf( TIME_ON, lhour, lmin);
		printf( TIME_OFF, dhour, dmin);
		printf( PRG_DAYS );
		printf( FMT_DAYS, sunday, monday, tuesday, wednesday, thursday, friday, saturday);
	}
	else
		printf( PRG_ONOF );

}

/* is today shutdown day ? */
static int IsToday( unsigned char dweek, int nweek)
{

	switch ( nweek )
	{
	case 0: /* sunday */
		return ( ( ( dweek & 0x40 ) == 0x40 ) );
	case 1:
		return ( ( ( dweek & 0x20 ) == 0x20 ) );
	case 2:
		return ( ( ( dweek & 0x10 ) == 0x10 ) );
	case 3:
		return ( ( ( dweek & 0x08 ) == 0x08 ) );
	case 4:
		return ( ( ( dweek & 0x04 ) == 0x04 ) );
	case 5:
		return ( ( ( dweek & 0x02 ) == 0x02 ) );
	case 6: /* saturday */
		return ( ( ( dweek & 0x01 ) == 0x01 ) );
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

	if(  UtilPower <= 20 ) {
		Autonomy = 170;
		 maxauto = 170;
	}
	else
	{
		maxauto = auton[iauto].mm[ipo][lim];
		if( indice > inf && indice < sup ) {
			Autonomy = auton[iauto].mm[ipo][indd];
		}
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
	DaysOnWeek = RecPack[17];

	/*  Days of week if in UPS shutdown programming mode */
	if( prgups == 3 ) {
		DaysStd = revertdays( DaysOnWeek );

		/* time for programming UPS off */
		dhour = RecPack[15];
		dmin  = RecPack[16];
		/* time for programming UPS on */
		lhour = RecPack[13];
		lmin  = RecPack[14];
	}

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

	if (SolisModel != 16) {

		if(  RecPack[ 6 ] >= 194 )
			InVoltage = RecPack[ 6 ] * ctab[imodel].m_involt194[0] + ctab[imodel].m_involt194[1];
		else
			InVoltage = RecPack[ 6 ] * ctab[imodel].m_involt193[0] + ctab[imodel].m_involt193[1];
	} else {
		/* Code InVoltage for STAY1200_USB */

		if ((RecPack[20] & 0x1) == 0) { //IsOutVoltage 220

			InVoltage = RecPack[2] * ctab[imodel].m_involt193[0] + ctab[imodel].m_involt193[1];
		} else {

			InVoltage = RecPack[2] * ctab[imodel].m_involt193[0] + ctab[imodel].m_involt193[1] - 3.0;
		}
	}

	BattVoltage = RecPack[ 3 ] * ctab[imodel].m_battvolt[0] + ctab[imodel].m_battvolt[1];

	NominalPower = nompow[im];

	if(  SourceFail ) {
		OutVoltage = RecPack[ 1 ] * ctab[imodel].m_outvolt_i[ov][0] + ctab[imodel].m_outvolt_i[ov][1];
		OutCurrent = RecPack[ 5 ] * ctab[imodel].m_outcurr_i[ov][0] + ctab[imodel].m_outcurr_i[ov][1];
		AppPower = ( RecPack[ 5 ] * RecPack[ 1 ] ) * ctab[imodel].m_appp_i[ov][0] + ctab[imodel].m_appp_i[ov][1];
		UtilPower = ( RecPack[ 7 ] + RecPack[ 8 ] * 256 ) * ctab[imodel].m_utilp_i[ov][0] + ctab[imodel].m_utilp_i[ov][1];
		InCurrent = 0;
	}
	else
	{
		OutVoltage = RecPack[ 1 ] * ctab[imodel].m_outvolt_s[ov][0] + ctab[imodel].m_outvolt_s[ov][1];
		OutCurrent = RecPack[ 5 ] * ctab[imodel].m_outcurr_s[ov][0] + ctab[imodel].m_outcurr_s[ov][1];
		AppPower = ( RecPack[ 5 ] * RecPack[ 1 ] ) * ctab[imodel].m_appp_s[ov][0] + ctab[imodel].m_appp_s[ov][1];
		UtilPower = ( RecPack[ 7 ] + RecPack[ 8 ] * 256 ) * ctab[imodel].m_utilp_s[ov][0] + ctab[imodel].m_utilp_s[ov][1];
		InCurrent = ( ctab[imodel].m_incurr[0] * 1.0 / BattVoltage ) - ( AppPower * 1.0 / ctab[imodel].m_incurr[1] )
		+ OutCurrent *( OutVoltage * 1.0 / InVoltage );
	}
		if (SolisModel == 16) {

			int configRelay = (RecPack[6] & 0x38) >> 3;
			double TENSAO_SAIDA_F1_MR[8] = { 1.1549, 1.0925, 0.0, 0.0, 1.0929, 1.0885, 0.0, 0.8654262224145391 };
			double TENSAO_SAIDA_F2_MR[8] = { -6.9157, 11.026, 10.43, 0.0, -0.6109, 12.18, 0.0, 13.677};

			const double TENSAO_SAIDA_F2_MI[8] ={ 5.59, 9.47, 13.7, 0.0, 0.0, 0.0, 0.0, 0.0 };
			const double TENSAO_SAIDA_F1_MI[8] = { 7.9, 9.1, 17.6, 0.0, 0.0, 0.0, 0.0, 0.0 };

			const double corrente_saida_F1_MR = 0.12970000389100012;
			const double corrente_saida_F2_MR = 0.5387060281204546;
			/* double corrente_saida_F1_MI = 0.1372; 
			double corrente_saida_F2_MI = 0.3456; */

			if (SourceFail) {
				if (RecPack[20] == 0) {
					double a = RecPack[1] * 2;
					a /= 128.0;
					//	a = double sqrt(a);
					OutVoltage = RecPack[1] * a *  TENSAO_SAIDA_F1_MI[configRelay] + TENSAO_SAIDA_F2_MI[configRelay];

				}

			} else {


				OutCurrent = (float)(corrente_saida_F1_MR * RecPack[5] + corrente_saida_F2_MR);
				OutVoltage = RecPack[1] * TENSAO_SAIDA_F1_MR[configRelay] + TENSAO_SAIDA_F2_MR[configRelay];
				AppPower = OutCurrent * OutVoltage;



				double RealPower = (RecPack[7] + RecPack[8] * 256);

				double potVA1 = 5.968 * AppPower - 284.36;
				double potVA2 = 7.149 * AppPower - 567.18;
				double potLin = 0.1664 * RealPower + 49.182;
				double potRe = 0.1519 * RealPower + 32.644;
				if (fabs(potVA1 - RealPower) < fabs(potVA2 - RealPower)) {
					RealPower = potLin;
				} else {
					RealPower = potRe;

				}
				if (OutCurrent < 0.7) {
					RealPower = AppPower;
				}
				if (AppPower < RealPower) {
					double f = AppPower;
					AppPower = RealPower;
					RealPower = f;
				}



			}
		}

	aux = ( RecPack[ 21 ] + RecPack[ 22 ] * 256 );
	if( aux > 0 )
		InFreq = ctab[imodel].m_infreq * 1.0 / aux;

	/* Specific for STAY1200_USB */
	        if (SolisModel == 16) {
	             InFreq = ((float)(0.37 * (257 - (aux >> 8))));
	        }
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
	else
	{
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
	else
	{
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

	if( Autonomy < 5 )
		LowBatt = true;
	else
		LowBatt = false;

	UpsPowerFactor = 700;

	/* input 110V or 220v */
	if(  InputValue == 0 )  {
		InDownLim = 75;
		InUpLim = 150;
		NomInVolt = 110;
	}
	else
	{
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
	else
	{
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

	if(  size == 25 )  {
		i_end = 25;
		for( i = 0 ; i < i_end ; ++i ) {
			RecPack[i] = *bufptr;
			bufptr++;
		}
		if(nut_debug_level >= 3) {
			upsdebug_hex(3, "CommReceive: RecPack", RecPack, size);
		}

		/* CheckSum verify */
		CheckSum = 0;
		i_end = 23;
		for( i = 0 ; i < i_end ; ++i )
			CheckSum = RecPack[ i ] + CheckSum;
		CheckSum = CheckSum % 256;
		upsdebugx(4, "%s: calculated checksum = 0x%02x, RecPack[23] = 0x%02x", __func__, CheckSum, RecPack[23]);

		ser_flush_in(upsfd,"",0); /* clean port */

		/* RecPack[0] identify the model number below.
		 *  SOLIS = 1;
		 RHINO = 2;
		 STAY = 3;
		 SOLIS_LI_700 = 169;
		 SOLIS_M11 = 171;
		 SOLIS_M15 = 175;
		 SOLIS_M14 = 174;
		 SOLIS_M13 = 173;
		 SOLISDC_M14 = 201;
		 SOLISDC_M13 = 206;
		 SOLISDC_M15 = 207;
		 CABECALHO_RHINO = 194;
		 PS800 = 185;
		 STAY1200_USB = 186;
		 PS350_CII = 184;
		 PS2200 = 187;
		 PS2200_22 = 188;
		 STAY700_USB = 189;
		 BZ1500 = 190;
		 */
		if(  ( ( (RecPack[0] & 0xF0) == 0xA0 ) || (RecPack[0] & 0xF0) == 0xB0)
				&& ( RecPack[ 24 ] == 254 )
				&& ( RecPack[ 23 ] == CheckSum ) ) {

			if(!(detected)) {

				if (RecPack[0] == 186) {
					SolisModel = 16;
				} else {
					SolisModel = (int) (RecPack[0] & 0x0F);
				}
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
				case 16:      // STAY1200_USB model
					{
						ScanReceivePack();
						break;
					}
				default:
					{
						printf( M_UNKN );
						ScanReceivePack(); // Scan anyway.
						break;
					}
			}
		}
	}
}

static void getbaseinfo(void)
{
	unsigned char tmp;
#ifdef PORTUGUESE
	const char diassemana[7][4]={"Dom", "Seg", "Ter", "Qua", "Qui", "Sex", "Sab"};
#else
	const char DaysOfWeek[7][4]={"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
#endif
	char *str1, *str2, *str3, *str4, *strx;
	unsigned char Pacote[25];
	int  i, i1=0, i2=0, tam;
	const int tpac=25;

	time_t tmt;
	struct tm *now;
	time( &tmt );
	now = localtime( &tmt );
	dian = now->tm_mday;
	mesn = now->tm_mon+1;
	anon = now->tm_year+1900;
	ihour = now->tm_hour;
	imin = now->tm_min;
	isec = now->tm_sec;
        weekn = now->tm_wday;

#ifdef PORTUGUESE
	strcpy( seman, diassemana[weekn] );
#else
	strcpy( seman, DaysOfWeek[weekn] );
#endif

	if( testvar("battext"))
		BattExtension = atoi(getval("battext"));

	if( testvar("prgshut"))
		prgups = atoi(getval("prgshut"));

	if( prgups > 0 && prgups < 3 ) {
		if( testvar("daysweek") ) {
			strx = getval("daysweek");
			str1 = convdays( strx );
			DaysOnWeek = Binary( str1 );
		}

		if( testvar("daysoff") ) {
			strx = getval("daysoff");
			str2 = convdays( strx );
			DaysStd = Binary ( strx );
			DaysOffWeek = Binary( str2 );
		}

		if( testvar("houron") ) {
			str3 = getval("houron");
			i1 = IsHour( str3, 0 );
		}

		if( testvar("houroff") ) {
			str4 = getval("houroff");
			i2 =  IsHour( str4, 1 );
		}

		if( i1 == 1 && i2 == 1 && ( DaysOnWeek > 0 ) ) {
			isprogram = 1; /* prgups == 1 ou 2  */

		if( prgups == 2 )
			confups();  /* save ups config */
		}
		else
		{
			if( (i2 == 1) && ( DaysOffWeek > 0 ) ) {
				isprogram = 1;
			if( DaysOnWeek != DaysOffWeek )
				DaysOnWeek = DaysOffWeek;
		}
	}

	} /* end prgups 1 - 2 */

	/* dummy read attempt to sync - throw it out */
	upsdebugx(3, "%s: sending CMD_UPSCONT and ENDCHAR to sync", __func__);
	ser_send(upsfd, "%c%c", CMD_UPSCONT, ENDCHAR);

	/* Read until end-of-response character (0xFE): */
	for(i=0; i<tpac*3; i++) {
		ser_get_char(upsfd, &tmp, 3, 0);
		if(tmp == RESP_END)
			break;
	}

	if(tmp != RESP_END) {
		fatalx(EXIT_FAILURE, NO_SOLIS);
	} else {
		upsdebugx(4, "%s: requesting %d bytes from ser_get_buf_len()", __func__, tpac);
		tam = ser_get_buf_len(upsfd, Pacote, tpac, 3, 0);
		upsdebugx(2, "%s: received %d bytes from ser_get_buf_len()", __func__, tam);
		if(tam > 0 && nut_debug_level >= 4) {
			upsdebug_hex(4, "received from ser_get_buf_len()", Pacote, tam);
		}
		CommReceive((char *)Pacote, tam);
	}

	if( (!detected) ) {
		fatalx(EXIT_FAILURE,  NO_SOLIS );
	}

	switch( SolisModel )
	{
	case 10:

	case 11:
	case 12:
	{
		Model = "Solis 1.0";
		break;
	}
	case 13:
	{
		Model = "Solis 1.5";
		break;
	}
	case 14:
	{
		Model = "Solis 2.0";
		break;
	}
	case 15:
	{
		Model = "Solis 3.0";
		break;
	}
	case 16:
	  Model = "Microsol Back-Ups BZ1200-BR";
	  break;
	}

	/* if( isprogram ) */
	if( prgups == 1 ) {
		hourshut = dhour;
		minshut = dmin;
	}
	else
	{
		if( prgups == 2 || prgups == 3 ) { /* broadcast before firmware shutdown */
			if( dmin < 5 ) {
				if( dhour > 1 )
					hourshut = dhour - 1;
				else
					hourshut = 23;
				minshut = 60 - ( 5 - dmin );
			}
		else
		{
			hourshut = dhour;
			minshut = dmin - 5;
		}
	      }
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
	time_t tmt;
	struct tm *now;
	time( &tmt );
	now = localtime( &tmt );
	hourn = now->tm_hour;
	minn = now->tm_min;
        weekn = now->tm_wday;

	if( isprogram || prgups == 3 ) {
		if( isprogram )
			isday = IsToday( DaysStd, weekn );
		else
			isday = IsToday( DaysStd, weekn );

		if( isday )
			printf( TODAY_DD, hourshut, minshut );

		if( ( hourn == hourshut ) && ( minn >= minshut ) && isday ) {
			printf( SHUT_NOW );
			progshut = 1;
		}
	}

	/* programable shutdown end block */

	pacsize = 25;

	/* get update package */
	temp[0] = 0; /* flush temp buffer */

	upsdebugx(3, "%s: requesting %d bytes from ser_get_buf_len()", __func__, pacsize);
	tam = ser_get_buf_len(upsfd, temp, pacsize, 3, 0);

	upsdebugx(2, "%s: received %d bytes from ser_get_buf_len()", __func__, tam);
	if(tam > 0 && nut_debug_level >= 4) {
		upsdebug_hex(4, "received from ser_get_buf_len()", temp, tam);
	}

	CommReceive((char *)temp, tam);

}

static int instcmd(const char *cmdname, const char *extra)
{

	if (!strcasecmp(cmdname, "shutdown.return"))  {
		/* shutdown and restart */
		ser_send_char(upsfd, CMD_SHUTRET); /* 0xDE */
		/* ser_send_char(upsfd, ENDCHAR); */
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "shutdown.stayoff"))
	  {
	    /* shutdown now (one way) */
	    ser_send_char(upsfd, CMD_SHUT); /* 0xDD */
	    /* ser_send_char(upsfd, ENDCHAR); */
	    return STAT_INSTCMD_HANDLED;
	  }

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s]", cmdname);
	return STAT_INSTCMD_UNKNOWN;

}

void upsdrv_initinfo(void)
{
	getbaseinfo();

	upsh.instcmd = instcmd;
}

void upsdrv_updateinfo(void)
{

        getupdateinfo(); /* new package for updates */

	dstate_setinfo("output.voltage", "%03.1f", OutVoltage);
	dstate_setinfo("input.voltage", "%03.1f", InVoltage);
	dstate_setinfo("battery.voltage", "%02.1f", BattVoltage);
	dstate_setinfo("battery.charge", "%03.1f", batcharge);
	dstate_setinfo("output.current", "%03.1f", OutCurrent);
	status_init();

	if (!SourceFail )
		status_set("OL");	/* on line */
	else
		status_set("OB");	/* on battery */

	if (Autonomy < 5 )
		status_set("LB");	/* low battery */

	if( progshut )  {         /* software programable shutdown immediately */
		if( prgups == 2 )
			sendshut();       /* Ups shutdown in 4-5 minutes -- redundant Ups shutdown */

		status_set("LB");	/* no low battery but is a force shutdown */
	}

	status_commit();

	dstate_setinfo("ups.temperature", "%2.2f", Temperature);
	dstate_setinfo("input.frequency", "%2.1f", InFreq);
	dstate_setinfo("ups.load", "%03.1f", upscharge);

	dstate_dataok();

}

/*! @brief Power down the attached load immediately.
 * Basic idea: find out line status and send appropriate command.
 *  - on battery: send normal shutdown, UPS will return by itself on utility
 *  - on line: send shutdown+return, UPS will cycle and return soon.
 */
void upsdrv_shutdown(void)
{


	if (!SourceFail) {     /* on line */

		upslogx(LOG_NOTICE, "On line, sending shutdown+return command...\n");
		ser_send_char(upsfd, CMD_SHUTRET );
	}
	else
	{
		upslogx(LOG_NOTICE, "On battery, sending normal shutdown command...\n");
		ser_send_char(upsfd, CMD_SHUT);
	}

}

void upsdrv_help(void)
{

	printf("\nSolis options\n");
	printf(" Battery Extension in AH\n");
	printf("  battext = 80\n");
	printf(" Programable UPS power on/off\n");
	printf("  prgshut = 0  (default, no software programable shutdown)\n");
	printf("  prgshut = 1  (software programable shutdown without UPS power off)\n");
	printf("  prgshut = 2  (software programable shutdown with UPS power off)\n");
	printf("  prgshut = 3  (activate UPS programable power on/off)\n");
	printf(" Otherwise uses:\n");
	printf("  daysweek = 1010101  ( power on days )\n");
	printf("  daysoff = 1010101 ( power off days )\n");
	printf(" where each digit is a day from sun...sat with 0 = off and 1 = on\n");
	printf("  houron = hh:mm hh = hour 0-23 mm = minute 0-59 separated with :\n");
	printf("  houroff = hh:mm hh = hour 0-23 mm = minute 0-59 separated with :\n");
	printf(" where houron is power-on hour and houroff is shutdown and power-off hour\n");
	printf(" Uses daysweek and houron to programing and save UPS power on/off\n");
	printf(" These are valid only if prgshut = 2 or 3\n");

}

void upsdrv_makevartable(void)
{

	addvar(VAR_VALUE, "battext",  "Battery Extension (0-80)min");
	addvar(VAR_VALUE, "prgshut",  "Programable power off (0-3)");
	addvar(VAR_VALUE, "daysweek", "Days of week UPS power of/off");
	addvar(VAR_VALUE, "daysoff",  "Days of week Driver shutdown");
	addvar(VAR_VALUE, "houron",   "Power on hour (hh:mm)");
	addvar(VAR_VALUE, "houroff",  "Power off hour (hh:mm)");

}

void upsdrv_initups(void)
{
	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, B9600);

	ser_set_dtr(upsfd, 1);
	ser_set_rts(upsfd, 0);
}

void upsdrv_cleanup(void)
{
	ser_close(upsfd, device_path);
}
