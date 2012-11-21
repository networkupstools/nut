/*
 * riello.h: defines/macros for Riello protocol based UPSes
 *
 * A document describing the protocol implemented by this driver can be
 * found online at "http://www.networkupstools.org/protocols/megatec.html".
 *
 * Copyright (C) 2012 - Elio Parisi <e.parisi@riello-ups.com>
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
 * Reference of the derivative work: blazer driver   
 */

#ifndef dev_dataH
#define dev_dataH

#ifdef UNIX

#ifndef DWORD
#define DWORD unsigned int
#endif
#ifndef ulong1
#define ulong1 unsigned int
#endif

#else

#ifndef DWORD
#define DWORD unsigned long
#endif
#ifndef ulong1
#define ulong1 unsigned long
#endif

#endif

#ifndef BYTE
#define BYTE unsigned char
#endif

#ifndef WORD
#define WORD unsigned short int
#endif

#ifndef WIN32
#ifndef INT1
#define INT1 signed short int
#endif
#endif

#define CTRL_RETRIES 50
#define CTRL_TIMEOUT 100

#define USB_ENDPOINT_IN 0x80
#define USB_ENDPOINT_OUT 0x00

#define MAX_READ_WRITE (16 * 1024) 

#define USB_WRITE_DELAY 200

#define MAXTRIES 3

#define DEV_RIELLOSENTRY 14
#define DEV_RIELLOGPSER 21

#define LENGTH_GI 68
#define LENGTH_GN 34
#define LENGTH_RS_MM 42
#define LENGTH_RS_TM 48
#define LENGTH_RS_TT 64
#define LENGTH_RE 70
#define LENGTH_RC 56
#define LENGTH_DEF 12

#define SENTR_EXT176 176
#define SENTR_ALSO240 240
#define SENTR_ONLY192 192

typedef struct {
	WORD SWversion;
	WORD Model;
	WORD Uinp1;
	WORD Uinp2;
	WORD Uinp3;
	WORD Iinp1;
	WORD Iinp2;
	WORD Iinp3;
	WORD Finp;

	WORD Uout1;
	WORD Uout2;
	WORD Uout3;
	WORD Iout1;
	WORD Iout2;
	WORD Iout3;
	WORD Pout1;
	WORD Pout2;
	WORD Pout3;
	WORD Ipout1;
	WORD Ipout2;
	WORD Ipout3;
	WORD Fout;

	WORD BatTime;
	WORD BatCap;
	WORD Ubat;
	WORD Ibat;

	WORD Tsystem;
	WORD NomBatCap;

	WORD Ubypass1;
	WORD Ubypass2;
	WORD Ubypass3;
	WORD Fbypass;
	WORD LockUPS;

	BYTE AlarmCode[4];
	char AlarmCodeT[12];
	BYTE StatusCode[12];
	char StatusCodeT[42];

	char Identification[18];
	char ModelStr[18];
	char Version[14];

	WORD NomPowerKVA;
	WORD NomPowerKW;
	WORD NomUbat;
	WORD NumBat;

	WORD UbatPerc;

	WORD NominalUout;

	WORD Boost;
	WORD Buck;

	BYTE Identif_bytes[12];
	WORD NomFout;

	DWORD Pout1VA;
	DWORD Pout2VA;
	DWORD Pout3VA;
	DWORD Pout1W;
	DWORD Pout2W;
	DWORD Pout3W;
} TRielloData;

/* CRC and Checksum functions */
WORD riello_calc_CRC(BYTE type, BYTE *buff, WORD size, BYTE checksum);
void riello_vytvor_crc(BYTE type, BYTE *buff, WORD size, BYTE checksum);
BYTE riello_test_crc(BYTE type, BYTE *buff, WORD size, BYTE chacksum);
BYTE riello_test_bit(BYTE *basic_address, BYTE bit);

/* send GPSER command functions */
BYTE riello_prepare_gi(BYTE* buffer);
BYTE riello_prepare_gn(BYTE* buffer, BYTE gpser_error_control);
BYTE riello_prepare_rs(BYTE* buffer, BYTE gpser_error_control);
BYTE riello_prepare_re(BYTE* buffer, BYTE gpser_error_control);
BYTE riello_prepare_rc(BYTE* buffer, BYTE gpser_error_control);
BYTE riello_prepare_cs(BYTE* buffer, BYTE gpser_error_control, WORD delay);
BYTE riello_prepare_cr(BYTE* buffer, BYTE gpser_error_control, WORD delay);
BYTE riello_prepare_cd(BYTE* buffer, BYTE gpser_error_control);
BYTE riello_prepare_tp(BYTE* buffer, BYTE gpser_error_control);
BYTE riello_prepare_tb(BYTE* buffer, BYTE gpser_error_control);

/* send SENTR command functions */
BYTE riello_prepare_shutsentr(BYTE* buffer, WORD delay);
BYTE riello_prepare_cancelsentr(BYTE* buffer);
BYTE riello_prepare_setrebsentr(BYTE* buffer, WORD delay);
BYTE riello_prepare_rebsentr(BYTE* buffer, WORD delay);
BYTE riello_prepare_tbsentr(BYTE* buffer);

/* parse GPSER ups responses */
void riello_parse_gi(BYTE* buffer, TRielloData* data);
void riello_parse_gn(BYTE* buffer, TRielloData* data);
void riello_parse_rs(BYTE* buffer, TRielloData* data, BYTE numread);
void riello_parse_re(BYTE* buffer, TRielloData* data);
void riello_parse_rc(BYTE* buffer, TRielloData* data);

/* parse SENTR ups responses */
void riello_parse_sentr(BYTE* buffer, TRielloData* data);

/* communication functions */
void riello_init_serial();
int riello_header(BYTE type, int a, int* length);
int riello_koniec(BYTE type, int length);
int riello_prislo_nak(BYTE type, BYTE* buffer);
void riello_spracuj_port(BYTE typedev, BYTE* buffer, BYTE checksum);
void riello_comm_setup(const char *port);

#endif 
