/*
 * riello.c: driver core for Riello protocol based UPSes
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

#include <string.h>
#include <stdint.h>

#include "main.h"
#include "riello.h"

static uint8_t foundheader = 0;
uint16_t buf_ptr_length;

uint8_t wait_packet = 0;
uint8_t foundnak = 0;
uint8_t foundbadcrc = 0;

uint8_t commbyte;
uint8_t requestSENTR;

static unsigned char LAST_DATA[6];

uint16_t riello_calc_CRC(uint8_t type, uint8_t *buff, uint16_t size, uint8_t checksum)
{
	uint16_t i;
	uint16_t pom, CRC_Word;

	CRC_Word = 0;
	switch (type) {
		case DEV_RIELLOSENTRY:
			CRC_Word = 0;
			if (size == 101) {
				for (i=0; i<100; i++)
					CRC_Word += buff[i];
			}
			else {
				for (i=0; i<size; i++)
					CRC_Word += buff[i];
			}
			break;
		case DEV_RIELLOGPSER:
			if (checksum) {
				buff++;
				size--;
				CRC_Word = 0x554D;
				while(size--) {
					pom  = (CRC_Word ^ *buff) & 0x00ff;
					pom  = (pom ^ (pom << 4)) & 0x00ff;
					/* Thanks to &0xff above, pom is at most 255 --
					 * so shifted by 8 bits is still uint16_t range
					 */
					pom  = (uint16_t)(pom << 8);
					pom ^= (pom << 3);
					pom ^= (pom >> 4);
					CRC_Word = (CRC_Word >> 8) ^ pom;
					buff++;
				}
			}
			else {
				CRC_Word = 0;
				for (i=1; i<size; i++)
					CRC_Word += buff[i];
			}
			break;
	}
	return(CRC_Word);
}

void riello_create_crc(uint8_t type, uint8_t *buff, uint16_t size, uint8_t checksum)
{
	uint16_t CRC_Word;

	CRC_Word = riello_calc_CRC(type, buff, size, checksum);

	if (type == DEV_RIELLOGPSER) {
		buff[size++] = (uint8_t) ((CRC_Word/4096)+0x30);
		buff[size++] = (uint8_t) (((CRC_Word%4096)/256)+0x30);
		buff[size++] = (uint8_t) ((((CRC_Word%4096)%256)/16)+0x30);
		buff[size] = (uint8_t) ((((CRC_Word%4096)%256)%16)+0x30);
	}
}

uint8_t riello_test_crc(uint8_t type, uint8_t *buff, uint16_t size, uint8_t checksum)
{
	uint16_t suma, CRC_Word;

	switch (type) {
		case DEV_RIELLOSENTRY:
			CRC_Word = riello_calc_CRC(type, buff, size-2, 0);

			suma = buff[size-2] + buff[size-1]*256;
			if (suma != CRC_Word)
				return(1);
			break;
		case DEV_RIELLOGPSER:
			CRC_Word = riello_calc_CRC(type, buff, size-5, checksum);

			suma = (buff[size-5]-0x30)*4096;
			suma += (buff[size-4]-0x30)*256;
			suma += (buff[size-3]-0x30)*16;
			suma += (buff[size-2]-0x30);
			if (suma != CRC_Word)
				return(1);
			break;
	}
	return(0);
}

uint8_t riello_test_bit(uint8_t *basic_address, uint8_t bit)
{
	uint8_t posuv, offset;
	uint8_t var, value;

	if (basic_address == NULL)
		return(0);

	posuv = bit/8;
	offset = bit%8;
	var = *(basic_address+posuv);
	if (var & (1 << offset))
		value = 1;
	else
		value = 0;
	return(value);
}

uint8_t riello_prepare_gi(uint8_t* buffer)
{
	uint8_t buf_ptr;

	buffer[0] = 0x2;
	buffer[1] = 0x20;
	buffer[2] = 0x22;
	buffer[3] = 'G';
	buffer[4] = 'I';
	buffer[5] = '0';
	buffer[6] = '0';

	buf_ptr = 7;
	riello_create_crc(DEV_RIELLOGPSER, buffer, buf_ptr, 0);
	buf_ptr = buf_ptr+4;

	buffer[buf_ptr++] = 0x3;

	return buf_ptr;
}

uint8_t riello_prepare_gn(uint8_t* buffer, uint8_t gpser_error_control)
{
	uint8_t buf_ptr;

	buffer[0] = 0x2;
	buffer[1] = 0x20;
	buffer[2] = 0x22;
	buffer[3] = 'G';
	buffer[4] = 'N';
	buffer[5] = '0';
	buffer[6] = '0';

	buf_ptr = 7;
	riello_create_crc(DEV_RIELLOGPSER, buffer, buf_ptr, gpser_error_control);
	buf_ptr = buf_ptr+4;

	buffer[buf_ptr++] = 0x3;

	return buf_ptr;
}

uint8_t riello_prepare_rs(uint8_t* buffer, uint8_t gpser_error_control)
{
	uint8_t buf_ptr;

	buffer[0] = 0x2;
	buffer[1] = 0x20;
	buffer[2] = 0x22;
	buffer[3] = 'R';
	buffer[4] = 'S';
	buffer[5] = '0';
	buffer[6] = '0';

	buf_ptr = 7;
	riello_create_crc(DEV_RIELLOGPSER, buffer, buf_ptr, gpser_error_control);
	buf_ptr = buf_ptr+4;

	buffer[buf_ptr++] = 0x3;

	return buf_ptr;
}

uint8_t riello_prepare_re(uint8_t* buffer, uint8_t gpser_error_control)
{
	uint8_t buf_ptr;

	buffer[0] = 0x2;
	buffer[1] = 0x20;
	buffer[2] = 0x22;
	buffer[3] = 'R';
	buffer[4] = 'E';
	buffer[5] = '0';
	buffer[6] = '0';

	buf_ptr = 7;
	riello_create_crc(DEV_RIELLOGPSER, buffer, buf_ptr, gpser_error_control);
	buf_ptr = buf_ptr+4;

	buffer[buf_ptr++] = 0x3;

	return buf_ptr;
}

uint8_t riello_prepare_rc(uint8_t* buffer, uint8_t gpser_error_control)
{
	uint8_t buf_ptr;

	buffer[0] = 0x2;
	buffer[1] = 0x20;
	buffer[2] = 0x22;
	buffer[3] = 'R';
	buffer[4] = 'C';
	buffer[5] = '0';
	buffer[6] = '0';

	buf_ptr = 7;
	riello_create_crc(DEV_RIELLOGPSER, buffer, buf_ptr, gpser_error_control);
	buf_ptr = buf_ptr+4;

	buffer[buf_ptr++] = 0x3;

	return buf_ptr;
}

uint8_t riello_prepare_cs(uint8_t* buffer, uint8_t gpser_error_control, uint16_t delay)
{
	uint8_t buf_ptr;

	buffer[0] = 0x2;
	buffer[1] = 0x20;
	buffer[2] = 0x22;
	buffer[3] = 'C';
	buffer[4] = 'S';
	buffer[5] = '0';
	buffer[6] = '4';
	buffer[7] = (uint8_t) ((delay/4096)+0x30);
	buffer[8] = (uint8_t) (((delay%4096)/256)+0x30);
	buffer[9] = (uint8_t) ((((delay%4096)%256)/16)+0x30);
	buffer[10] = (uint8_t) ((((delay%4096)%256)%16)+0x30);

	buf_ptr = 11;
	riello_create_crc(DEV_RIELLOGPSER, buffer, buf_ptr, gpser_error_control);
	buf_ptr = buf_ptr+4;

	buffer[buf_ptr++] = 0x3;

	return buf_ptr;
}

uint8_t riello_prepare_cr(uint8_t* buffer, uint8_t gpser_error_control, uint16_t delay)
{
	uint8_t buf_ptr;

	buffer[0] = 0x2;
	buffer[1] = 0x20;
	buffer[2] = 0x22;
	buffer[3] = 'C';
	buffer[4] = 'R';
	buffer[5] = '0';
	buffer[6] = '8';
	buffer[7] = '0';
	buffer[8] = '0';
	buffer[9] = '0';
	buffer[10] = '0';
	buffer[11] = (uint8_t) ((delay/4096)+0x30);
	buffer[12] = (uint8_t) (((delay%4096)/256)+0x30);
	buffer[13] = (uint8_t) ((((delay%4096)%256)/16)+0x30);
	buffer[14] = (uint8_t) ((((delay%4096)%256)%16)+0x30);

	buf_ptr = 15;
	riello_create_crc(DEV_RIELLOGPSER, buffer, buf_ptr, gpser_error_control);
	buf_ptr = buf_ptr+4;

	buffer[buf_ptr++] = 0x3;

	return buf_ptr;
}

uint8_t riello_prepare_cd(uint8_t* buffer, uint8_t gpser_error_control)
{
	uint8_t buf_ptr;

	buffer[0] = 0x2;
	buffer[1] = 0x20;
	buffer[2] = 0x22;
	buffer[3] = 'C';
	buffer[4] = 'D';
	buffer[5] = '0';
	buffer[6] = '0';

	buf_ptr = 7;
	riello_create_crc(DEV_RIELLOGPSER, buffer, buf_ptr, gpser_error_control);
	buf_ptr = buf_ptr+4;

	buffer[buf_ptr++] = 0x3;

	return buf_ptr;
}

uint8_t riello_prepare_tp(uint8_t* buffer, uint8_t gpser_error_control)
{
	uint8_t buf_ptr;

	buffer[0] = 0x2;
	buffer[1] = 0x20;
	buffer[2] = 0x22;
	buffer[3] = 'T';
	buffer[4] = 'P';
	buffer[5] = '0';
	buffer[6] = '0';

	buf_ptr = 7;
	riello_create_crc(DEV_RIELLOGPSER, buffer, buf_ptr, gpser_error_control);
	buf_ptr = buf_ptr+4;

	buffer[buf_ptr++] = 0x3;

	return buf_ptr;
}

uint8_t riello_prepare_tb(uint8_t* buffer, uint8_t gpser_error_control)
{
	uint8_t buf_ptr;

	buffer[0] = 0x2;
	buffer[1] = 0x20;
	buffer[2] = 0x22;
	buffer[3] = 'T';
	buffer[4] = 'B';
	buffer[5] = '0';
	buffer[6] = '3';
	buffer[7] = '0';
	buffer[8] = '0';
	buffer[9] = '5';

	buf_ptr = 10;
	riello_create_crc(DEV_RIELLOGPSER, buffer, buf_ptr, gpser_error_control);
	buf_ptr = buf_ptr+4;

	buffer[buf_ptr++] = 0x3;

	return buf_ptr;
}

uint8_t riello_prepare_shutsentr(uint8_t* buffer, uint16_t delay)
{
	buffer[0] = 176;
	buffer[1] = 6;
	buffer[2] = (uint8_t)(delay % 256);
	buffer[3] = delay / 256;
	buffer[4] = buffer[0] + buffer[1] + buffer[2] + buffer[3];

	return 5;
}

uint8_t riello_prepare_cancelsentr(uint8_t* buffer)
{
	buffer[0] = 176;
	buffer[1] = 5;
	buffer[2] = 0;
	buffer[3] = 0;
	buffer[4] = buffer[0] + buffer[1] + buffer[2] + buffer[3];

	return 5;
}

uint8_t riello_prepare_setrebsentr(uint8_t* buffer, uint16_t delay)
{
	buffer[0] = 176;
	buffer[1] = 2;
	buffer[2] = (uint8_t)(delay % 256);
	buffer[3] = delay / 256;
	buffer[4] = buffer[0] + buffer[1] + buffer[2] + buffer[3];

	return 5;
}

uint8_t riello_prepare_rebsentr(uint8_t* buffer, uint16_t delay)
{
	buffer[0] = 176;
	buffer[1] = 1;
	buffer[2] = (uint8_t)(delay % 256);
	buffer[3] = delay / 256;
	buffer[4] = buffer[0] + buffer[1] + buffer[2] + buffer[3];

	return 5;
}

uint8_t riello_prepare_tbsentr(uint8_t* buffer)
{
	buffer[0] = 176;
	buffer[1] = 4;
	buffer[2] = 0;
	buffer[3] = 0;
	buffer[4] = buffer[0] + buffer[1] + buffer[2] + buffer[3];

	return 5;
}

void riello_parse_gi(uint8_t* buffer, TRielloData* data)
{
	memcpy(data->Identification, &buffer[7], 16);
	data->Identification[16] = 0;
	memcpy(data->ModelStr, &buffer[23], 16);
	data->ModelStr[16] = 0;
	data->ModelStr[15] = 0;
	memcpy(data->Version, &buffer[39], 12);
	data->Version[12] = 0;
	memcpy(data->Identif_bytes, &buffer[51], 12);
	data->Identif_bytes[11] = 0;
	data->NumBat = data->Identif_bytes[7] - 0x30;
}

void riello_parse_gn(uint8_t* buffer, TRielloData* data)
{
	uint16_t pom_word;
	uint32_t pom_long;
	uint8_t j;

	j = 7;
	pom_long = (buffer[j++]-0x30)*65536;
	pom_long += (buffer[j++]-0x30)*4096;
	pom_long += (buffer[j++]-0x30)*256;
	pom_long += (buffer[j++]-0x30)*16;
	pom_long += (buffer[j++]-0x30);

	if (data->Identif_bytes[0] != '1')
		pom_long/=100;
	assert (pom_long < UINT16_MAX);
	data->NomPowerKVA = (uint16_t)pom_long;

	pom_long = (buffer[j++]-0x30)*65536;
	pom_long += (buffer[j++]-0x30)*4096;
	pom_long += (buffer[j++]-0x30)*256;
	pom_long += (buffer[j++]-0x30)*16;
	pom_long += (buffer[j++]-0x30);

	if (data->Identif_bytes[0] != '1')
		pom_long/=100;
	assert (pom_long < UINT16_MAX);
	data->NomPowerKW = (uint16_t)pom_long;

	pom_word = (buffer[j++]-0x30)*256;
	pom_word += (buffer[j++]-0x30)*16;
	pom_word += (buffer[j++]-0x30);
	data->NomUbat = pom_word;

	pom_word = (buffer[j++]-0x30)*256;
	pom_word += (buffer[j++]-0x30)*16;
	pom_word += (buffer[j++]-0x30);
	data->NomBatCap = pom_word;

	pom_word = (buffer[j++]-0x30)*256;
	pom_word += (buffer[j++]-0x30)*16;
	pom_word += (buffer[j++]-0x30);
	data->NominalUout = pom_word;

	pom_word = (buffer[j++]-0x30)*256;
	pom_word += (buffer[j++]-0x30)*16;
	pom_word += (buffer[j++]-0x30);
	data->NomFout = pom_word;
}

void riello_parse_rs(uint8_t* buffer, TRielloData* data, uint8_t numread)
{
	uint16_t pom_word;
	uint8_t j;

	j = 7;
	memcpy(data->StatusCode, &buffer[j], 5);
	data->StatusCode[5] = 0;
	data->Boost = riello_test_bit(&buffer[j], 9);
	data->Buck = riello_test_bit(&buffer[j], 8);
	data->LockUPS = riello_test_bit(&buffer[j], 2);
	j+=5;

	pom_word = (buffer[j++]-0x30)*256;
	pom_word += (buffer[j++]-0x30)*16;
	pom_word += (buffer[j++]-0x30);
	data->Finp = pom_word;

	pom_word = (buffer[j++]-0x30)*256;
	pom_word += (buffer[j++]-0x30)*16;
	pom_word += (buffer[j++]-0x30);
	data->Uinp1 = pom_word;

	pom_word = (buffer[j++]-0x30)*256;
	pom_word += (buffer[j++]-0x30)*16;
	pom_word += (buffer[j++]-0x30);
	if (!riello_test_bit(&data->StatusCode[0], 3))
		pom_word = 0;
	data->Fout = pom_word;

	pom_word = (buffer[j++]-0x30)*256;
	pom_word += (buffer[j++]-0x30)*16;
	pom_word += (buffer[j++]-0x30);
	data->Uout1 = pom_word;

	pom_word = (buffer[j++]-0x30)*16;
	pom_word += (buffer[j++]-0x30);
	data->Pout1 = pom_word;

	pom_word = (buffer[j++]-0x30)*256;
	pom_word += (buffer[j++]-0x30)*16;
	pom_word += (buffer[j++]-0x30);
	data->Fbypass = pom_word;

	pom_word = (buffer[j++]-0x30)*256;
	pom_word += (buffer[j++]-0x30)*16;
	pom_word += (buffer[j++]-0x30);
	data->Ubypass1 = pom_word;

	pom_word = (buffer[j++]-0x30)*4096;
	pom_word += (buffer[j++]-0x30)*256;
	pom_word += (buffer[j++]-0x30)*16;
	pom_word += (buffer[j++]-0x30);
	data->Ubat = pom_word;

	pom_word = (buffer[j++]-0x30)*16;
	pom_word += (buffer[j++]-0x30);
	data->BatCap = pom_word;

	pom_word = (buffer[j++]-0x30)*256;
	pom_word += (buffer[j++]-0x30)*16;
	pom_word += (buffer[j++]-0x30);
	data->BatTime = pom_word;
	if (data->BatTime == 0xfff)
		data->BatTime = 0xffff;

	pom_word = (buffer[j++]-0x30)*16;
	pom_word += (buffer[j++]-0x30);
	data->Tsystem = pom_word;

	if (numread > 42) {
		pom_word = (buffer[j++]-0x30)*256;
		pom_word += (buffer[j++]-0x30)*16;
		pom_word += (buffer[j++]-0x30);
		data->Uinp2 = pom_word;
		pom_word = (buffer[j++]-0x30)*256;
		pom_word += (buffer[j++]-0x30)*16;
		pom_word += (buffer[j++]-0x30);
		data->Uinp3 = pom_word;
	}
	else {
		data->Uinp2 = 0;
		data->Uinp3 = 0;
	}

	if (numread > 48) {
		pom_word = (buffer[j++]-0x30)*256;
		pom_word += (buffer[j++]-0x30)*16;
		pom_word += (buffer[j++]-0x30);
		data->Uout2 = pom_word;
		pom_word = (buffer[j++]-0x30)*16;
		pom_word += (buffer[j++]-0x30);
		data->Pout2 = pom_word;
		pom_word = (buffer[j++]-0x30)*256;
		pom_word += (buffer[j++]-0x30)*16;
		pom_word += (buffer[j++]-0x30);
		data->Ubypass2 = pom_word;
		pom_word = (buffer[j++]-0x30)*256;
		pom_word += (buffer[j++]-0x30)*16;
		pom_word += (buffer[j++]-0x30);
		data->Uout3 = pom_word;
		pom_word = (buffer[j++]-0x30)*16;
		pom_word += (buffer[j++]-0x30);
		data->Pout3 = pom_word;
		pom_word = (buffer[j++]-0x30)*256;
		pom_word += (buffer[j++]-0x30)*16;
		pom_word += (buffer[j++]-0x30);
		data->Ubypass3 = pom_word;
	}
	else {
		data->Uout2 = 0;
		data->Pout2 = 0;
		data->Ubypass2 = 0;
		data->Uout3 = 0;
		data->Pout3 = 0;
		data->Ubypass3 = 0;
	}
}

void riello_parse_re(uint8_t* buffer, TRielloData* data)
{
	uint16_t pom_word;
	uint32_t pom_long;
	uint8_t j;

	j = 23;
	data->Iinp1 = 0xFFFF;
	data->Iinp2 = 0xFFFF;
	data->Iinp3 = 0xFFFF;

	pom_word = (buffer[j++]-0x30)*4096;
	pom_word += (buffer[j++]-0x30)*256;
	pom_word += (buffer[j++]-0x30)*16;
	pom_word += (buffer[j++]-0x30);
	data->Iout1 = pom_word;

	pom_word = (buffer[j++]-0x30)*4096;
	pom_word += (buffer[j++]-0x30)*256;
	pom_word += (buffer[j++]-0x30)*16;
	pom_word += (buffer[j++]-0x30);
	data->Iout2 = pom_word;

	pom_word = (buffer[j++]-0x30)*4096;
	pom_word += (buffer[j++]-0x30)*256;
	pom_word += (buffer[j++]-0x30)*16;
	pom_word += (buffer[j++]-0x30);
	data->Iout3 = pom_word;

	pom_long = (buffer[j++]-0x30)*65536;
	pom_long += (buffer[j++]-0x30)*4096;
	pom_long += (buffer[j++]-0x30)*256;
	pom_long += (buffer[j++]-0x30)*16;
	pom_long += (buffer[j++]-0x30);
	data->Pout1W = pom_word;

	pom_long = (buffer[j++]-0x30)*65536;
	pom_long += (buffer[j++]-0x30)*4096;
	pom_long += (buffer[j++]-0x30)*256;
	pom_long += (buffer[j++]-0x30)*16;
	pom_long += (buffer[j++]-0x30);
	data->Pout2W = pom_word;

	pom_long = (buffer[j++]-0x30)*65536;
	pom_long += (buffer[j++]-0x30)*4096;
	pom_long += (buffer[j++]-0x30)*256;
	pom_long += (buffer[j++]-0x30)*16;
	pom_long += (buffer[j++]-0x30);
	data->Pout3W = pom_word;

	pom_long = (buffer[j++]-0x30)*65536;
	pom_long += (buffer[j++]-0x30)*4096;
	pom_long += (buffer[j++]-0x30)*256;
	pom_long += (buffer[j++]-0x30)*16;
	pom_long += (buffer[j++]-0x30);
	data->Pout1VA = pom_word;

	pom_long = (buffer[j++]-0x30)*65536;
	pom_long += (buffer[j++]-0x30)*4096;
	pom_long += (buffer[j++]-0x30)*256;
	pom_long += (buffer[j++]-0x30)*16;
	pom_long += (buffer[j++]-0x30);
	data->Pout2VA = pom_word;

	pom_long = (buffer[j++]-0x30)*65536;
	pom_long += (buffer[j++]-0x30)*4096;
	pom_long += (buffer[j++]-0x30)*256;
	pom_long += (buffer[j++]-0x30)*16;
	pom_long += (buffer[j++]-0x30);
	data->Pout3VA = pom_word;
}

void riello_parse_rc(uint8_t* buffer, TRielloData* data)
{
	uint8_t j, i;

	j = 7;
	for (i = 0; i < 22; i++, j+=2) {
		data->StatusCodeT[i] = (char)(buffer[j+1]-0x30);
		data->StatusCodeT[i] |= ((buffer[j]-0x30) << 4);
	}
	data->StatusCodeT[23] = 0;
	data->StatusCodeT[24] = 0;
}

void riello_parse_sentr(uint8_t* buffer, TRielloData* data)
{
	uint32_t pom;

	data->Model = buffer[2]+256*buffer[3];
	if (data->Model < 3000) {
		if ((data->Model % 10) >= 4) {
			if (buffer[100] & 0x01)
				requestSENTR = SENTR_EXT176;
			else
				requestSENTR = SENTR_ALSO240;
		}
		else
			requestSENTR = SENTR_ONLY192;
		data->NomPowerKVA = data->Model/10;
	}
	else {
		if (((data->Model-3000) % 10) >= 4) {
			if (buffer[100] & 0x01)
				requestSENTR = SENTR_EXT176;
			else
				requestSENTR = SENTR_ALSO240;
		}
		else
			requestSENTR = SENTR_ONLY192;
		data->NomPowerKVA = (data->Model-3000)/10;
	}

	if (buffer[76] & 0x08)
		data->NomPowerKW = ((data->NomPowerKVA * 1000) * 9 / 10) / 1000;
	else
		data->NomPowerKW = ((data->NomPowerKVA * 1000) * 8 / 10) / 1000;

	data->NomPowerKVA *= 1000;
	data->NomPowerKW *= 1000;
	if (data->Model < 3000)
		data->Identif_bytes[0] = '3';
	else
		data->Identif_bytes[0] = '4';
	data->Identif_bytes[1] = '3';
	data->Identif_bytes[6] = '2';

	data->SWversion = buffer[4]+256*buffer[5];

	data->Version[0] = (char)(uint8_t)(48 + ((data->SWversion / 1000) % 10));
	data->Version[1] = (char)(uint8_t)(48 + ((data->SWversion / 100) % 10));
	data->Version[2] = '.';
	data->Version[3] = (char)(uint8_t)(48 + ((data->SWversion / 10) % 10));
	data->Version[4] = (char)(uint8_t)(48 + (data->SWversion % 10));

	if (data->Model < 3000)
		pom = data->Model*100;
	else
		pom = (data->Model-3000)*100;

	if (buffer[0] == SENTR_EXT176) {
		data->Uinp1 = (buffer[117]+buffer[118]*256)/10;
		data->Uinp2 = (buffer[119]+buffer[120]*256)/10;
		data->Uinp3 = (buffer[121]+buffer[122]*256)/10;
		data->Iinp1 = (buffer[123]+buffer[124]*256)/10;
		data->Iinp2 = (buffer[125]+buffer[126]*256)/10;
		data->Iinp3 = (buffer[127]+buffer[128]*256)/10;
		data->Finp = buffer[41]+256*buffer[42];

		data->Uout1 = (buffer[135]+buffer[136]*256)/10;
		data->Uout2 = (buffer[137]+buffer[138]*256)/10;
		data->Uout3 = (buffer[139]+buffer[140]*256)/10;
		data->Iout1 = (buffer[141]+buffer[142]*256)/10;
		data->Iout2 = (buffer[143]+buffer[144]*256)/10;
		data->Iout3 = (buffer[145]+buffer[146]*256)/10;

		data->Pout1 = buffer[62];
		data->Pout2 = buffer[63];
		data->Pout3 = buffer[64];
		data->Ipout1 = buffer[65]*3;
	}
	else {
		data->Uinp1 = buffer[35]*230/100;
		data->Uinp2 = buffer[36]*230/100;
		data->Uinp3 = buffer[37]*230/100;
		/* TODO: Range-check the casts to uint16_t? */
		data->Iinp1 = (uint16_t)(((pom/690)*buffer[38])/100);
		data->Iinp2 = (uint16_t)(((pom/690)*buffer[39])/100);
		data->Iinp3 = (uint16_t)(((pom/690)*buffer[40])/100);
		data->Finp = buffer[41]+256*buffer[42];

		if (buffer[79] & 0x80) {
			data->Uout1 = buffer[59]*2;
			data->Uout2 = buffer[60]*2;
			data->Uout3 = buffer[61]*2;
		}
		else {
			data->Uout1 = buffer[59];
			data->Uout2 = buffer[60];
			data->Uout3 = buffer[61];
		}

		if (buffer[73]) {
			/* FIXME: Wondering how the addition below works for uint8_t[] buffer... */
			/* TODO: Range-check the casts to uint16_t? */
			if (buffer[73] < 100)
				buffer[73]+=256;
			if (data->Model < 3000) /* singlephase */
				data->Iout1 = (uint16_t)(((pom/buffer[73])*buffer[62])/100);
			else
				data->Iout1 = (uint16_t)(((pom/buffer[73])*buffer[62])/100/3);
			data->Iout2 = (uint16_t)(((pom/buffer[73])*buffer[63])/100/3);
			data->Iout3 = (uint16_t)(((pom/buffer[73])*buffer[64])/100/3);
		}
		else {
			data->Iout1 = 0;
			data->Iout2 = 0;
			data->Iout3 = 0;
		}

		if ((data->Model & 0x0007) < 4) {
			data->Iout1 *= 0.9;
			data->Iout2 *= 0.9;
			data->Iout3 *= 0.9;
		}

		data->Pout1 = buffer[62];
		data->Pout2 = buffer[63];
		data->Pout3 = buffer[64];
		data->Ipout1 = buffer[65]*3;
	}

	if (data->Model < 3000) {
		data->Ipout2 = 0;
		data->Ipout3 = 0;
	}
	else {
		data->Ipout2 = buffer[66]*3;
		data->Ipout3 = buffer[67]*3;
	}

	data->Fout = buffer[68]+256*buffer[69];

	data->BatTime = buffer[6]+256*buffer[7];
	data->BatCap = buffer[8];

	if ((buffer[0] == SENTR_ALSO240) || (buffer[0] == SENTR_EXT176)) {
		if (buffer[100] & 0x80)
			data->Ubat = buffer[43]+256*buffer[44];
		else {
			if (buffer[44] < buffer[43])
				data->Ubat = buffer[44]*2;
			else
				data->Ubat = buffer[43]*2;
		}
	}
	else
		data->Ubat = buffer[43]+256*buffer[44];

	data->Ubat *= 10;
	data->Ibat = buffer[45]+256*buffer[46];
	if (!buffer[47])
		data->Ibat = data->Ibat*10;

	data->Tsystem = buffer[48];
	data->NomBatCap = buffer[74]+256*buffer[75];

	switch (buffer[0]) {
		case SENTR_EXT176:
			data->Ubypass1 = (buffer[129]+buffer[130]*256)/10;
			data->Ubypass2 = (buffer[131]+buffer[132]*256)/10;
			data->Ubypass3 = (buffer[133]+buffer[134]*256)/10;
			data->Fbypass = buffer[57]+256*buffer[58];
			break;
		case SENTR_ALSO240:
			data->Ubypass1 = buffer[51]*2;
			data->Ubypass2 = buffer[53]*2;
			data->Ubypass3 = buffer[55]*2;
			data->Fbypass = buffer[57]+256*buffer[58];
			break;
		default:
			data->Ubypass1 = buffer[51]+256*buffer[52];
			data->Ubypass2 = buffer[53]+256*buffer[54];
			data->Ubypass3 = buffer[55]+256*buffer[56];
			data->Fbypass = buffer[57]+256*buffer[58];
			break;
	}

	data->StatusCode[0] = 0x08;
	data->StatusCode[1] = 0x00;
	data->StatusCode[2] = 0x00;
	data->StatusCode[3] = 0x00;
	data->StatusCode[4] = 0x00;

	/* Overload 	if (riello_test_bit(&DevData.StatusCode[4], 2)) */
	if (buffer[31] & 128)
		data->StatusCode[4] |= 0x04;

	/* Bypass 		if (riello_test_bit(&DevData.StatusCode[1], 3)) */
	if (((buffer[31] & 2) || (riello_test_bit(&buffer[32], 0)) || (riello_test_bit(&buffer[32], 12)) ||
		(riello_test_bit(&buffer[32], 13)) || (riello_test_bit(&buffer[32], 14))) && (!(buffer[34] & 8)))
		data->StatusCode[1] |= 0x08;

	/* AC Fail  	if (riello_test_bit(&DevData.StatusCode[0], 1)) */
	if (buffer[31] & 8)
		data->StatusCode[0] |= 0x02;

	/* LowBatt	  if ((riello_test_bit(&DevData.StatusCode[0], 1)) && (riello_test_bit(&DevData.StatusCode[0], 0))) */
	if (buffer[31] & 16) {
		if (buffer[31] & 8) {
			data->StatusCode[0] |= 0x02;
			data->StatusCode[0] |= 0x01;
		}
	}

	/* Standby 		if (!riello_test_bit(&DevData.StatusCode[0], 3)) */
	if ((buffer[22] & 2) || (buffer[34] & 4) || (buffer[34] & 8))
		data->StatusCode[0] &= 0xF7;

	/* Battery bad (Replace battery) 		if (riello_test_bit(&DevData.StatusCode[2], 0)) */
	if (buffer[31] & 0x40)
		data->StatusCode[2] |= 0x01;
}

void riello_init_serial()
{
	wait_packet = 1;
	buf_ptr_length = 0;
	foundbadcrc = 0;
	foundnak = 0;
}

uint8_t riello_header(uint8_t type, uint8_t a, uint8_t* length)
{
	LAST_DATA[0] = LAST_DATA[1];
	LAST_DATA[1] = LAST_DATA[2];
	LAST_DATA[2] = LAST_DATA[3];
	LAST_DATA[3] = LAST_DATA[4];
	LAST_DATA[4] = LAST_DATA[5];
	LAST_DATA[5] = (uint8_t) a;

	switch (type) {
		case DEV_RIELLOSENTRY:
			if (((LAST_DATA[4]>=192) && (LAST_DATA[5]==103)) ||
				((LAST_DATA[4]==176) && (LAST_DATA[5]==164))) {
				*length = LAST_DATA[5];
				return(1);
			}
			break;
		case DEV_RIELLOGPSER:
			if ((buf_ptr_length==0) && (LAST_DATA[5]>0x20) && (LAST_DATA[4]==0x2))
				return(1);
			break;
	}
	return(0);
}

uint8_t riello_tail(uint8_t type, uint8_t length)
{
	uint8_t number;

	switch (type) {
		case DEV_RIELLOSENTRY:
			number = length;

			if (buf_ptr_length >= number)
				return(1);
			break;
		case DEV_RIELLOGPSER:
			if (LAST_DATA[5] == 0x03)
				return(1);
			break;
	}
	return(0);
}

uint8_t riello_test_nak(uint8_t type, uint8_t* buffer)
{
	switch (type) {
		case DEV_RIELLOGPSER:
			if (buffer[3] == 0x15)
				return(1);
			break;
	}
	return(0);
}

void riello_parse_serialport(uint8_t typedev, uint8_t* buffer, uint8_t checksum)
{
	static uint8_t actual_char, int_i;
	static uint8_t length;

	actual_char = commbyte;

	if ((riello_header(typedev, actual_char, &length)) && (!foundheader)) {
		upsdebugx(5,"Header detected: LAST_DATA:%X,%X,%X,%X,%X,%X  buf_ptr:%i  \n\r",
					LAST_DATA[0], LAST_DATA[1], LAST_DATA[2],
					LAST_DATA[3], LAST_DATA[4], LAST_DATA[5], buf_ptr_length);

		foundheader = 1;
		buf_ptr_length = 1;
		memset(buffer, 0, BUFFER_SIZE);
		buffer[0] = LAST_DATA[4];
	}

	if ((foundheader) && (buf_ptr_length < BUFFER_SIZE))
		buffer[buf_ptr_length++] = actual_char;

	if ((foundheader) && (riello_tail(typedev, length))) {
		upsdebugx(5,"\n\rEnd detected: LAST_DATA:%X,%X,%X,%X,%X,%X  buf_ptr:%i  \n\r",
					LAST_DATA[0], LAST_DATA[1], LAST_DATA[2],
					LAST_DATA[3], LAST_DATA[4], LAST_DATA[5], buf_ptr_length);

		foundheader = 0;

		for (int_i=0; int_i<6; int_i++)
			LAST_DATA[int_i] = 0;

		if (riello_test_nak(typedev, buffer)) {
			wait_packet = 0;
			foundnak = 1;
		}

		if (riello_test_crc(typedev, buffer, buf_ptr_length, checksum)) {
			wait_packet = 0;
			foundbadcrc = 1;
		}
		else {
			wait_packet = 0;
			foundbadcrc = 0;
		}
	}
}
