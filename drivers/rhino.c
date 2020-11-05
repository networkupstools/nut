/* rhino.c - driver for Microsol Rhino UPS hardware

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

   2004/11/13 - Version 0.10 - Initial release
   2005/07/07 - Version 0.20 - Initial rhino commands tests
   2005/10/25 - Version 0.30 - Operational-1 release
   2005/10/26 - Version 0.40 - Operational-2 release
   2005/11/29 - Version 0.50 - rhino commands release


   http://www.microsol.com.br

*/

#include <stdio.h>
#include <math.h>

#include "main.h"
#include "serial.h"
#include "timehead.h"

#define DRIVER_NAME		"Microsol Rhino UPS driver"
#define DRIVER_VERSION	"0.51"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Silvino B. Magalhaes <sbm2yk@gmail.com>",
	DRV_STABLE,
	{ NULL }
};

#define UPSDELAY 500 /* 0.5 ms delay */

typedef int bool_t;

#define false 0
#define true 1

/* rhino commands */
#define CMD_INON    0x0001
#define CMD_INOFF   0x0002
#define CMD_SHUT    0x0004
#define CMD_OUTON   0x0003
#define CMD_OUTOFF  0x0004
#define CMD_PASSON  0x0005
#define CMD_PASSOFF 0x0006
#define CMD_UPSCONT 0x0053

/* xoff - xon protocol */
#define _SOH = 0x01; /* start of header */
#define _EOT = 0x04; /* end of transmission */
#define _ACK = 0x06; /* acknoledge (positive) */
#define _DLE = 0x10; /* data link escape */
#define _XOn = 0x11; /* transmit on */
#define _XOff = 0x13; /* transmit off */
#define _NAK = 0x15; /* negative acknoledge */
#define _SYN = 0x16; /* synchronous idle */
#define _CAN = 0x18; /* cancel */

static int const pacsize = 37; /* size of receive data package */

/* autonomy calcule */
static double  const AmpH = 40;       /* Amperes-hora da bateria */
static double  const VbatMin = 126;   /* Tensão mínina das baterias */
static double  const VbatNom = 144;   /* Tensão nominal das baterias */
static double  const FM = 0.32;       /* Fator multiplicativo de correção da autonomia */
static double  const FA = -2;         /* Fator aditivo de correção da autonomia */
static double  const ConstInt = 250;  /* Consumo interno sem o carregador */
static double  const Vin = 220;       /* Tensão de entrada */

static int Day, Month, Year;
static int dian=0, mesn=0, anon=0, weekn=0;
static int ihour,imin, isec;
/* unsigned char DaysOnWeek; */
/* char seman[4]; */

/* int FExpansaoBateria; */
/* internal variables */
/* package handshake variables */
/* int ContadorEstouro; */
static bool_t detected;
static bool_t SourceFail, Out110, RedeAnterior, OcorrenciaDeFalha;
static bool_t RetornoDaRede, SuperAquecimento, SuperAquecimentoAnterior;
static bool_t OverCharge, OldOverCharge, CriticBatt, OldCritBatt;
static bool_t Flag_inversor, BypassOn, InputOn, OutputOn;
static bool_t LowBatt, oldInversorOn;
/* data vetor from received and configuration data package - not used yet
unsigned char Dados[ 161 ]; */
/* identification group */
static int RhinoModel; /*, imodel; */
static int PotenciaNominal, PowerFactor;
/* input group */
static double AppPowerIn, UtilPowerIn, InFreq, InCurrent;
static double LimInfEntrada, LimSupEntrada, ValorNominalEntrada;
static int FatorPotEntrada;
/* output group */
static double OutVoltage, InVoltage, OutCurrent, AppPowerOut;
static double UtilPowerOut, OutFreq, LimInfSaida, LimSupSaida, ValorNominalSaida;
static int FatorPotSaida;
/* battery group */
static int Autonomy, Waiting;
static double BattVoltage, Temperature, LimInfBattSrc, LimSupBattSrc;
static double LimInfBattInv, LimSupBattInv, BattNonValue;
/* general group */
static int BoostVolt, Rendimento;
/* status group */
static unsigned char StatusEntrada, StatusSaida, StatusBateria;
/* events group */
static unsigned char EventosRede, EventosSaida, EventosBateria;
/* Grupo de Programação */

/* Methods */
static void ScanReceivePack(void);
static int AutonomyCalc( int );
static void CommReceive(const unsigned char*, int );
static void getbaseinfo(void);
static void getupdateinfo(void);

static unsigned char RecPack[37];

/* comment on english language */
/* #define PORTUGUESE */

/* The following Portuguese strings are in UTF-8. */
#ifdef PORTUGUESE
#define M_UNKN     "Modêlo rhino desconhecido\n"
#define NO_RHINO   "Rhino não detectado! abortando ...\n"
#define UPS_DATE   "Data no UPS %4d/%02d/%02d\n"
#define SYS_DATE   "Data do Sistema %4d/%02d/%02d dia da semana %s\n"
#define ERR_PACK   "Pacote errado\n"
#define NO_EVENT   "Não há eventos\n"
#define UPS_TIME   "Hora interna UPS %0d:%02d:%02d\n"
#else
#define M_UNKN     "Unknown rhino model\n"
#define NO_RHINO   "Rhino not detected! aborting ...\n"
#define UPS_DATE   "UPS Date %4d/%02d/%02d\n"
#define SYS_DATE   "System Date %4d/%02d/%02d day of week %s\n"
#define ERR_PACK   "Wrong package\n"
#define NO_EVENT   "No events\n"
#define UPS_TIME   "UPS internal Time %0d:%02d:%02d\n"
#endif

static int
AutonomyCalc( int ia ) /* all models */
{
	int result = 0;
	double auton, calc, currin;

	if( ia )
	{
		if( ( BattVoltage == 0 ) )
			result = 0;
		else
		{
					calc = ( OutVoltage * OutCurrent )* 1.0 / ( 0.08 * BattVoltage );
					auton = pow( calc, 1.18 );
					if( ( auton == 0 ) )
						result = 0;
					else
						{
							auton = 1.0 / auton;
							auton = auton * 11.07;
							calc = ( BattVoltage * 1.0 / 10 ) - 168;
							result = (int) ( auton * calc * 2.5 );
						}
				}
		}
	else
		{
			currin = ( UtilPowerOut + ConstInt ) *1.0 / Vin;
			auton = ( ( ( AmpH *1.0 / currin ) * 60 * ( ( BattVoltage - VbatMin ) * 1.0 /( VbatNom - VbatMin ) ) * FM ) + FA );
			if( ( BattVoltage > 129 ) && ( BattVoltage < 144 ) )
				result = 133;
			else
				result = (int) auton;
		}

	return result;
}

/* Treat received package */
static void
ScanReceivePack( void )
{
	/* model independent data */

	Year = RecPack[31] + ( RecPack[32] * 100 );
	Month = RecPack[30];
	Day = RecPack[29];

	/* UPS internal time */
	ihour = RecPack[26];
	imin  = RecPack[27];
	isec  = RecPack[28];

	/* Flag1 */
	/* SobreTemp        = ( ( 0x01 & RecPack[33]) = 0x01 ); */
	/* OutputOn         = ( ( 0x02 & RecPack[33]) = 0x02 ); OutputOn */
	/* InputOn          = ( ( 0x04 & RecPack[33]) = 0x04 ); InputOn */
	/* ByPassOn         = ( ( 0x08 & RecPack[33]) = 0x08 ); BypassOn */
	/* Auto_HAB         = ( ( 0x10 & RecPack[33]) = 0x10 ); */
	/* Timer_HAB        = ( ( 0x20 & RecPack[33]) = 0x20 ); */
	/* Boost_Ligado     = ( ( 0x40 & RecPack[33]) = 0x40 ); */
	/* Bateria_Desc     = ( ( 0x80 & RecPack[33]) = 0x80 ); */

	/* Flag2 */
	/* Quad_Ant_Ent     = ( ( 0x01 & RecPack[34]) = 0x01 ); */
	/* Quadratura       = ( ( 0x02 & RecPack[34]) = 0x02 ); */
	/* Termino_XMODEM   = ( ( 0x04 & RecPack[34]) = 0x04 ); */
	/* Em_Sincronismo   = ( ( 0x08 & RecPack[34]) = 0x08 ); */
	/* Out110           = ( ( 0x10 & RecPack[34]) = 0x10 ); Out110 */
	/* Exec_Beep        = ( ( 0x20 & RecPack[34]) = 0x20 ); */
	/* LowBatt          = ( ( 0x40 & RecPack[34]) = 0x40 ); LowBatt */
	/* Boost_Sobre      = ( ( 0x80 & RecPack[34]) = 0x80 ); */

	/* Flag3 */
	/* OverCharge       = ( ( 0x01 & RecPack[35]) = 0x01 ); OverCharge */
	/* SourceFail       = ( ( 0x02 & RecPack[35]) = 0x02 ); SourceFail */
	/* RedeAnterior     = ( ( 0x04 & RecPack[35]) = 0x04 ); */
	/* Cmd_Executado    = ( ( 0x08 & RecPack[35]) = 0x08 ); */
	/* Exec_Autoteste   = ( ( 0x10 & RecPack[35]) = 0x10 ); */
	/* Quad_Ant_Sai     = ( ( 0x20 & RecPack[35]) = 0x20 ); */
	/* ComandoSerial    = ( ( 0x40 & RecPack[35]) = 0x40 ); */
	/* SobreTensao      = ( ( 0x80 & RecPack[35]) = 0x80 ); */

	OutputOn = ( ( 0x02 & RecPack[33] ) == 0x02 );
	InputOn =  ( ( 0x04 & RecPack[33] ) == 0x04 );
	BypassOn = ( ( 0x08 & RecPack[33] ) == 0x08 );

	Out110 =  ( ( 0x10 & RecPack[34] ) == 0x10 );
	LowBatt = ( ( 0x40 & RecPack[34] ) == 0x40 );

	OverCharge = ( ( 0x01 & RecPack[35] ) == 0x01 );
	SourceFail = ( ( 0x02 & RecPack[35] ) == 0x02 );

	/* model dependent data read */

	PowerFactor = 800;

	if( RecPack[0] == 0xC2 )
	{
		LimInfBattSrc = 174;
		LimSupBattSrc = 192;/* 180????? carregador eh 180 (SCOPOS) */
		LimInfBattInv = 174;
		LimSupBattInv = 192;/* 170????? (SCOPOS) */
	}
	else
	{
		LimInfBattSrc = 138;
		LimSupBattSrc = 162;/* 180????? carregador eh 180 (SCOPOS) */
		LimInfBattInv = 126;
		LimSupBattInv = 156;/* 170????? (SCOPOS) */
	}

	BattNonValue = 144;
	/* VersaoInterna = "R10" + IntToStr( RecPack[1] ); */
	InVoltage = RecPack[2];
	InCurrent = RecPack[3];
	UtilPowerIn = RecPack[4] + RecPack[5] * 256;
	AppPowerIn = RecPack[6] + RecPack[7] * 256;
	FatorPotEntrada = RecPack[8];
	InFreq = ( RecPack[9] + RecPack[10] * 256 ) * 1.0 / 10;
	OutVoltage = RecPack[11];
	OutCurrent = RecPack[12];
	UtilPowerOut = RecPack[13] + RecPack[14] * 256;
	AppPowerOut = RecPack[15] + RecPack[16] * 256;
	FatorPotSaida = RecPack[17];
	OutFreq = ( RecPack[18] + RecPack[19] * 256 ) * 1.0 / 10;
	BattVoltage = RecPack[20];
	BoostVolt = RecPack[21] + RecPack[22] * 256;
	Temperature = ( 0x7F & RecPack[23] );
	Rendimento = RecPack[24];

	/* model independent data */

	if( ( BattVoltage < LimInfBattInv ) )
		CriticBatt = true;

	if( BypassOn )
		OutVoltage = ( InVoltage * 1.0 / 2 ) + 5;

	if( SourceFail && RedeAnterior ) /* falha pela primeira vez */
		OcorrenciaDeFalha = true;

	if( !( SourceFail ) && !( RedeAnterior ) ) /* retorno da rede */
		RetornoDaRede = true;

	if( !( SourceFail ) == RedeAnterior )
	{
		RetornoDaRede = false;
		OcorrenciaDeFalha = false;
	}

	RedeAnterior = !( SourceFail );

	LimInfSaida = 75;
	LimSupSaida = 150;
	ValorNominalSaida = 110;

	LimInfEntrada = 190;
	LimSupEntrada = 250;
	ValorNominalEntrada = 220;

	if( SourceFail )
	{
		StatusEntrada = 2;
		RecPack[8] = 200; /* ?????????????????????????????????? */
	}
	else
	{
		StatusEntrada = 1;
		RecPack[8] = 99; /* ??????????????????????????????????? */
	}

	if( OutputOn )     /* Output Status */
		StatusSaida = 2;
	else
		StatusSaida = 1;

	if( OverCharge )
		StatusSaida = 3;

	if( CriticBatt ) /* Battery Status */
		StatusBateria = 4;
	else
		StatusBateria = 1;

	EventosRede = 0;

	if( OcorrenciaDeFalha )
		EventosRede = 1;

	if( RetornoDaRede )
		EventosRede = 2;

	/* verify InversorOn */
	if( Flag_inversor )
	{
		oldInversorOn = InputOn;
		Flag_inversor = false;
	}

	EventosSaida = 0;
	if( InputOn && !( oldInversorOn ) )
		EventosSaida = 26;
	if( oldInversorOn && !( InputOn ) )
		EventosSaida = 27;
	oldInversorOn = InputOn;
	if( SuperAquecimento && !( SuperAquecimentoAnterior ) )
		EventosSaida = 12;
	if( SuperAquecimentoAnterior && !( SuperAquecimento ) )
		EventosSaida = 13;
	SuperAquecimentoAnterior = SuperAquecimento;
	EventosBateria = 0;
	OldCritBatt = CriticBatt;

	if( OverCharge && !( OldOverCharge ) )
		EventosSaida = 10;
	if( OldOverCharge && !( OverCharge ) )
		EventosSaida = 11;
	OldOverCharge = OverCharge;

	/* autonomy calc. */
	if( RecPack[ 0 ] == 0xC2 )
		Autonomy = AutonomyCalc( 1 );
	else
		Autonomy = AutonomyCalc( 0 );
}

static void
CommReceive(const unsigned char *bufptr, int size)
{
	int i, i_end, CheckSum, chk;

	if( ( size==37 ) )
		Waiting = 0;

	printf("CommReceive size = %d waiting = %d\n", size, Waiting );

	switch( Waiting )
	{
		/* normal package */
		case 0:
		{
			if( size == 37 )
			{
				i_end = 37;
				for( i = 0 ; i < i_end ; ++i )
				{
					RecPack[i] = *bufptr;
					bufptr++;
				}

				/* CheckSum verify */
				CheckSum = 0;
				i_end = 36;
				for( i = 0 ; i < i_end ; ++i )
				{
					chk = RecPack[ i ];
					CheckSum = CheckSum + chk;
				}

				CheckSum = CheckSum % 256;

				ser_flush_in(upsfd,"",0); /* clean port */

				/* correct package */
				if( ( RecPack[0] == 0xC0 || RecPack[0] == 0xC1 || RecPack[0] == 0xC2 || RecPack[0] == 0xC3 )
				     && ( RecPack[ 36 ] == CheckSum ) )
				{

					if(!(detected))
					{
						RhinoModel = RecPack[0];
						detected = true;
					}

					switch( RhinoModel )
					{
						case 0xC0:
						case 0xC1:
						case 0xC2:
						case 0xC3:
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
			/* dumping package, nothing to do yet */
			Waiting = 0;
			break;
		}

	}

	Waiting = 0;
}

static int
send_command( int cmd )
{
	int i, chk, checksum = 0, iend = 18, sizes = 19, ret, kount; /*, j, uc; */
	unsigned char ch, psend[sizes];

	/* mounting buffer to send */

	for(i = 0; i < iend; i++ )
	{
		if ( i == 0 )
			chk = 0x01;
		else
		{
			if( i == 1)
				chk = cmd;
			else
				chk = 0x00; /* 0x20; */
		}

		ch = chk;
		psend[i] = ch; /* psend[0 - 17] */
		if( i > 0 )    /* psend[0] not computed */
			checksum = checksum + chk;
	}

	ch = checksum;
	ch = (~( ch) ); /* not ch */
	psend[iend] = ch;

	/* send five times the command */
	kount = 0;
	while ( kount < 5 )
	{
		/* ret = ser_send_buf_pace(upsfd, UPSDELAY, psend, sizes ); */ /* optional delay */

		for(i = 0 ; i < sizes ; i++)
		{
			ret = ser_send_char( upsfd, psend[i] );
			/* usleep ( UPSDELAY ); sending without delay */
		}
		usleep( UPSDELAY ); /* delay between sent command */
		kount++;
	}
	return ret;
}

static void sendshut( void )
{
	int i;

	for(i=0; i < 30000; i++)
		usleep( UPSDELAY ); /* 15 seconds delay */

	send_command( CMD_SHUT );
	upslogx(LOG_NOTICE, "Ups shutdown command sent");
	printf("Ups shutdown command sent\n");
}

static void getbaseinfo(void)
{
	unsigned char temp[256];
	unsigned char Pacote[37];
	int tam, i, j=0;
	time_t tmt;
	struct tm *now;
	const char *Model;

	time( &tmt );
	now = localtime( &tmt );
	dian = now->tm_mday;
	mesn = now->tm_mon+1;
	anon = now->tm_year+1900;
	weekn = now->tm_wday;

	/* trying detect rhino model */
	while ( ( !detected ) && ( j < 10 ) )
	{

		temp[0] = 0; /* flush temp buffer */
		tam = ser_get_buf_len(upsfd, temp, pacsize, 3, 0);
		if( tam == 37 )
		{
			for( i = 0 ; i < tam ; i++ )
			{
				Pacote[i] = temp[i];
			}
		}

		j++;
		if( tam == 37)
			CommReceive(Pacote, tam);
		else
			CommReceive(temp, tam);
	}

	if( (!detected) )
	{
		fatalx(EXIT_FAILURE, NO_RHINO );
	}

	switch( RhinoModel )
	{
		case 0xC0:
		{
			Model = "Rhino 20.0 kVA";
			PotenciaNominal = 20000;
			break;
		}
		case 0xC1:
		{
			Model = "Rhino 10.0 kVA";
			PotenciaNominal = 10000;
			break;
		}
		case 0xC2:
		{
			Model = "Rhino 6.0 kVA";
			PotenciaNominal = 6000;
			break;
		}
		case 0xC3:
		{
			Model = "Rhino 7.5 kVA";
			PotenciaNominal = 7500;
			break;
		}
		default:
		{
			Model = "Rhino unknown model";
			PotenciaNominal = 0;
			break;
		}
	}

	/* manufacturer and model */
	dstate_setinfo("ups.mfr", "%s", "Microsol");
	dstate_setinfo("ups.model", "%s", Model);
	/*
	dstate_setinfo("input.transfer.low", "%03.1f", InDownLim); LimInfBattInv ?
	dstate_setinfo("input.transfer.high", "%03.1f", InUpLim); LimSupBattInv ?
	*/

	dstate_addcmd("shutdown.stayoff");	/* CMD_SHUT */
	/* there is no reserved words for CMD_INON and CMD_INOFF yet */
	/* dstate_addcmd("input.on"); */	/* CMD_INON    = 1 */
	/* dstate_addcmd("input.off"); */	/* CMD_INOFF   = 2 */
	dstate_addcmd("load.on");	/* CMD_OUTON   = 3 */
	dstate_addcmd("load.off");	/* CMD_OUTOFF  = 4 */
	dstate_addcmd("bypass.start");	/* CMD_PASSON  = 5 */
	dstate_addcmd("bypass.stop");	/* CMD_PASSOFF = 6 */

	printf("Detected %s on %s\n", dstate_getinfo("ups.model"), device_path);
}

static void getupdateinfo(void)
{
	unsigned char temp[256];
	int tam;

	temp[0] = 0; /* flush temp buffer */
	tam = ser_get_buf_len(upsfd, temp, pacsize, 3, 0);

	CommReceive(temp, tam);

}

static int instcmd(const char *cmdname, const char *extra)
{
	int ret = 0;

	if (!strcasecmp(cmdname, "shutdown.stayoff"))
	{
		/* shutdown now (one way) */
		/* send_command( CMD_SHUT ); */
		sendshut();
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "load.on"))
	{
		/* liga Saida */
		ret = send_command( 3 );
		if ( ret < 1 )
			upslogx(LOG_ERR, "send_command 3 failed");
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "load.off"))
	{
		/* desliga Saida */
		ret = send_command( 4 );
		if ( ret < 1 )
			upslogx(LOG_ERR, "send_command 4 failed");
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "bypass.start"))
	{
		/* liga Bypass */
		ret = send_command( 5 );
		if ( ret < 1 )
			upslogx(LOG_ERR, "send_command 5 failed");
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "bypass.stop"))
	{
		/* desliga Bypass */
		ret = send_command( 6 );
		if ( ret < 1 )
			upslogx(LOG_ERR, "send_command 6 failed");
		return STAT_INSTCMD_HANDLED;
	}

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s] [%s]", cmdname, extra);
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

	/* output and bypass tests */
	if( OutputOn )
		dstate_setinfo("outlet.switchable", "%s", "yes");
	else
		dstate_setinfo("outlet.switchable", "%s", "no");

	if( BypassOn )
		dstate_setinfo("outlet.1.switchable", "%s", "yes");
	else
		dstate_setinfo("outlet.1.switchable", "%s", "no");

	status_init();

	if (!SourceFail )
		status_set("OL");	/* on line */
	else
		status_set("OB");	/* on battery */

	if (Autonomy < 5 )
		status_set("LB");	/* low battery */

	status_commit();
	dstate_setinfo("ups.temperature", "%2.2f", Temperature);
	dstate_setinfo("input.frequency", "%2.1f", InFreq);
	dstate_dataok();
}

/* power down the attached load immediately */
void upsdrv_shutdown(void)
{
	/* basic idea: find out line status and send appropriate command */
	/* on line: send normal shutdown, ups will return by itself on utility */
	/* on battery: send shutdown+return, ups will cycle and return soon */

	if (!SourceFail)     /* on line */
	{
		printf("On line, forcing shutdown command...\n");
		send_command( CMD_SHUT );
	}
	else
	{
		printf("On battery, sending normal shutdown command...\n");
		send_command( CMD_SHUT );
	}
}

void upsdrv_help(void)
{
}

void upsdrv_makevartable(void)
{
	addvar(VAR_VALUE, "battext", "Battery Extension (0-80)min");
}

void upsdrv_initups(void)
{
	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, B19200);

	/* dtr and rts setting */
	ser_set_dtr(upsfd, 1);
	ser_set_rts(upsfd, 0);
}

void upsdrv_cleanup(void)
{
	ser_close(upsfd, device_path);
}
