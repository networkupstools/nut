/*
 * riello.h: defines/macros for Riello protocol based UPSes
 *
 * Documents describing the protocol implemented by this driver can be
 * found online at:
 *
 *   http://www.networkupstools.org/ups-protocols/riello/PSGPSER-0104.pdf
 *   http://www.networkupstools.org/ups-protocols/riello/PSSENTR-0100.pdf
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

#ifndef NUT_RIELLO_H_SEEN
#define NUT_RIELLO_H_SEEN 1

#include <stdint.h>

#define CTRL_RETRIES 50
#define CTRL_TIMEOUT 100

#define USB_ENDPOINT_IN 0x80
#define USB_ENDPOINT_OUT 0x00

#define MAX_READ_WRITE (16 * 1024)

#define USB_WRITE_DELAY 200

#define MAXTRIES 3
#define COUNTLOST 10

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

#define BUFFER_SIZE 220

typedef struct {
	uint16_t SWversion;
	uint16_t Model;
	uint16_t Uinp1;
	uint16_t Uinp2;
	uint16_t Uinp3;
	uint16_t Iinp1;
	uint16_t Iinp2;
	uint16_t Iinp3;
	uint16_t Finp;

	uint16_t Uout1;
	uint16_t Uout2;
	uint16_t Uout3;
	uint16_t Iout1;
	uint16_t Iout2;
	uint16_t Iout3;
	uint16_t Pout1;
	uint16_t Pout2;
	uint16_t Pout3;
	uint16_t Ipout1;
	uint16_t Ipout2;
	uint16_t Ipout3;
	uint16_t Fout;

	uint16_t BatTime;
	uint16_t BatCap;
	uint16_t Ubat;
	uint16_t Ibat;

	uint16_t Tsystem;
	uint16_t NomBatCap;

	uint16_t Ubypass1;
	uint16_t Ubypass2;
	uint16_t Ubypass3;
	uint16_t Fbypass;
	uint16_t LockUPS;

	uint8_t AlarmCode[4];
	char AlarmCodeT[12];
	uint8_t StatusCode[12];
	char StatusCodeT[42];

	char Identification[18];
	char ModelStr[18];
	char Version[14];

	uint16_t NomPowerKVA;
	uint16_t NomPowerKW;
	uint16_t NomUbat;
	uint16_t NumBat;

	uint16_t UbatPerc;

	uint16_t NominalUout;

	uint16_t Boost;
	uint16_t Buck;

	uint8_t Identif_bytes[12];
	uint16_t NomFout;

	uint32_t Pout1VA;
	uint32_t Pout2VA;
	uint32_t Pout3VA;
	uint32_t Pout1W;
	uint32_t Pout2W;
	uint32_t Pout3W;
} TRielloData;

/* CRC and Checksum functions */
uint16_t riello_calc_CRC(uint8_t type, uint8_t *buff, uint16_t size, uint8_t checksum);
void riello_create_crc(uint8_t type, uint8_t *buff, uint16_t size, uint8_t checksum);
uint8_t riello_test_crc(uint8_t type, uint8_t *buff, uint16_t size, uint8_t chacksum);
uint8_t riello_test_bit(uint8_t *basic_address, uint8_t bit);

/* send GPSER command functions */
uint8_t riello_prepare_gi(uint8_t* buffer);
uint8_t riello_prepare_gn(uint8_t* buffer, uint8_t gpser_error_control);
uint8_t riello_prepare_rs(uint8_t* buffer, uint8_t gpser_error_control);
uint8_t riello_prepare_re(uint8_t* buffer, uint8_t gpser_error_control);
uint8_t riello_prepare_rc(uint8_t* buffer, uint8_t gpser_error_control);
uint8_t riello_prepare_cs(uint8_t* buffer, uint8_t gpser_error_control, uint16_t delay);
uint8_t riello_prepare_cr(uint8_t* buffer, uint8_t gpser_error_control, uint16_t delay);
uint8_t riello_prepare_cd(uint8_t* buffer, uint8_t gpser_error_control);
uint8_t riello_prepare_tp(uint8_t* buffer, uint8_t gpser_error_control);
uint8_t riello_prepare_tb(uint8_t* buffer, uint8_t gpser_error_control);

/* send SENTR command functions */
uint8_t riello_prepare_shutsentr(uint8_t* buffer, uint16_t delay);
uint8_t riello_prepare_cancelsentr(uint8_t* buffer);
uint8_t riello_prepare_setrebsentr(uint8_t* buffer, uint16_t delay);
uint8_t riello_prepare_rebsentr(uint8_t* buffer, uint16_t delay);
uint8_t riello_prepare_tbsentr(uint8_t* buffer);

/* parse GPSER ups responses */
void riello_parse_gi(uint8_t* buffer, TRielloData* data);
void riello_parse_gn(uint8_t* buffer, TRielloData* data);
void riello_parse_rs(uint8_t* buffer, TRielloData* data, uint8_t numread);
void riello_parse_re(uint8_t* buffer, TRielloData* data);
void riello_parse_rc(uint8_t* buffer, TRielloData* data);

/* parse SENTR ups responses */
void riello_parse_sentr(uint8_t* buffer, TRielloData* data);

/* communication functions */
void riello_init_serial();
uint8_t riello_header(uint8_t type, uint8_t a, uint8_t* length);
uint8_t riello_tail(uint8_t type, uint8_t length);
uint8_t riello_test_nak(uint8_t type, uint8_t* buffer);
void riello_parse_serialport(uint8_t typedev, uint8_t* buffer, uint8_t checksum);
void riello_comm_setup(const char *port);

#endif /* NUT_RIELLO_H_SEEN */
