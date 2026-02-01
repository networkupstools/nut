/* common_voltronic-crc.c - Common CRC routines for Voltronic Power devices
 *
 * Copyright (C)
 *   2014 Daniele Pezzini <hyouko@gmail.com>
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

#include "common.h"
#include "common_voltronic-crc.h"

/* CRC table - filled at runtime by common_voltronic_crc_init() */
static unsigned short	crc_table[256];

/* Flag (0/1) just to be sure all is properly initialized */
static int	initialized = 0;

/* Fill CRC table: this function MUST be called before anything else that needs to perform CRC operations */
static void	common_voltronic_crc_init(void)
{
	unsigned short	dividend;

	/* Already been here */
	if (initialized)
		return;

	/* Compute remainder of each possible dividend */
	for (dividend = 0; dividend < 256; dividend++) {

		unsigned short	bit;
		/* Start with dividend followed by zeros */
		unsigned long	remainder = dividend << 8;

		/* Modulo 2 division, a bit at a time */
		for (bit = 0; bit < 8; bit++)
			/* Try to divide the current data bit */
			if (remainder & 0x8000)
				remainder = (remainder << 1) ^ 0x1021;
			else
				remainder <<= 1;

		/* Store the result into the table */
		crc_table[dividend] = remainder & 0xffff;

	}

	/* Ready, now */
	initialized = 1;
}

/* See header file for details */
unsigned short	common_voltronic_crc_compute(const char *input, const size_t len)
{
	unsigned short	crc, crc_MSB, crc_LSB;
	unsigned long	remainder = 0;
	size_t		byte;

	/* Make sure all is ready */
	if (!initialized)
		common_voltronic_crc_init();

	/* Divide *input* by the polynomial, a byte at a time */
	for (byte = 0; byte < len; byte++)
		remainder = (remainder << 8) ^ crc_table[(input[byte] ^ (remainder >> 8)) & 0xff];

	/* The final remainder is the CRC */
	crc = remainder & 0xffff;

	/* Escape characters with a special meaning */

	crc_MSB = (crc >> 8) & 0xff;
	if (
		crc_MSB == 10 ||	/* LF */
		crc_MSB == 13 ||	/* CR */
		crc_MSB == 40		/* ( */
	)
		crc_MSB++;

	crc_LSB = crc & 0xff;
	if (
		crc_LSB == 10 ||	/* LF */
		crc_LSB == 13 ||	/* CR */
		crc_LSB == 40		/* ( */
	)
		crc_LSB++;

	crc = ((crc_MSB & 0xff) << 8) + crc_LSB;

	return crc;
}

/* See header file for details */
unsigned short	common_voltronic_crc_calc(const char *input, const size_t inputlen)
{
	size_t	len;
	char	*cr = (char*)memchr(input, '\r', inputlen);

	/* No CR, fall back to string length (and hope *input* doesn't contain inner '\0's) */
	if (cr == NULL)
		len = strlen(input);
	else
		len = cr - input;

	/* At least 1 byte expected */
	if (!len)
		return -1;

	/* Compute (and return) CRC */
	return common_voltronic_crc_compute(input, len);
}

/* See header file for details */
int	common_voltronic_crc_calc_and_add(const char *input, const size_t inputlen, char *output, const size_t outputlen)
{
	unsigned short	crc, crc_MSB, crc_LSB;
	size_t		len;
	char		*cr = (char*)memchr(input, '\r', inputlen);

	/* No CR, fall back to string length (and hope *input* doesn't contain inner '\0's) */
	if (cr == NULL)
		len = strlen(input);
	else
		len = cr - input;

	/* At least 1 byte expected */
	if (!len)
		return -1;

	/* To accomodate CRC, *output* must have space for at least 2 bytes more than the actually used size of *input*.
	 * Also, pretend that *input* is a valid null-terminated string and so reserve the final byte in *output* for the terminating '\0'. */
	if (
		(cr == NULL && outputlen < len + 3) ||	/* 2-bytes CRC + 1 byte for terminating '\0' */
		(cr != NULL && outputlen < len + 4)	/* 2-bytes CRC + 1 byte for trailing CR + 1 byte for terminating '\0' */
	)
		return -1;

	/* Compute CRC */
	crc = common_voltronic_crc_compute(input, len);
	crc_MSB = (crc >> 8) & 0xff;
	crc_LSB = crc & 0xff;

	/* Clear *output* */
	memset(output, 0, outputlen);

	/* Copy *input* to *output* */
	memcpy(output, input, len);

	/* Write CRC to *output* */
	output[len++] = crc_MSB;
	output[len++] = crc_LSB;

	/* Reinstate the trailing CR in *output*, if appropriate */
	if (cr != NULL)
		output[len++] = '\r';

	return (int)len;
}

/* See header file for details */
int	common_voltronic_crc_calc_and_add_m(char *input, const size_t inputlen)
{
	int	len;
	char	*buf = (char*)xcalloc(inputlen, sizeof(char));

	if (!buf)
		return -1;

	/* Compute CRC and copy *input*, with CRC added to it, to buf */
	len = common_voltronic_crc_calc_and_add(input, inputlen, buf, inputlen);

	/* Failed */
	if (len == -1) {
		free(buf);
		return -1;
	}

	/* Successfully computed CRC and copied *input*, with CRC added to it, to buf */

	/* Clear *input* */
	memset(input, 0, inputlen);

	/* Copy back buf to *input* */
	memcpy(input, buf, len);

	free(buf);
	return len;
}

/* See header file for details */
int	common_voltronic_crc_check(const char *input, const size_t inputlen)
{
	unsigned short	crc, crc_MSB, crc_LSB;
	char		*cr = (char*)memchr(input, '\r', inputlen);
	size_t		len;

	/* No CR, fall back to string length (and hope *input* doesn't contain inner '\0's) */
	if (cr == NULL)
		len = strlen(input);
	else
		len = cr - input;

	/* Minimum length: 1 byte + 2 bytes CRC -> 3 */
	if (len < 3)
		return -1;

	/* Compute CRC */
	crc = common_voltronic_crc_compute(input, len - 2);
	crc_MSB = (crc >> 8) & 0xff;
	crc_LSB = crc & 0xff;

	/* Fail */
	if (
		crc_MSB != (unsigned char)input[len - 2] ||
		crc_LSB != (unsigned char)input[len - 1]
	)
		return -1;

	/* Success */
	return 0;
}

/* See header file for details */
int	common_voltronic_crc_check_and_remove(const char *input, const size_t inputlen, char *output, const size_t outputlen)
{
	char	*cr;
	size_t	len;

	/* Failed to check *input* CRC */
	if (common_voltronic_crc_check(input, inputlen))
		return -1;

	/* *input* successfully validated -> remove CRC bytes */

	cr = (char*)memchr(input, '\r', inputlen);
	/* No CR, fall back to string length */
	if (cr == NULL)
		len = strlen(input);
	else
		len = cr - input;

	/* *output* must have space for at least 2 bytes less than the actually used size of *input*.
	 * Also, pretend that *input* is a valid null-terminated string and so reserve the final byte in *output* for the terminating '\0'. */
	len -= 2;	/* Consider 2-bytes CRC length */
	if (
		(cr == NULL && outputlen < len + 1) ||	/* 1 byte for terminating '\0' */
		(cr != NULL && outputlen < len + 2)	/* 1 byte for terminating '\0' + 1 byte for trailing CR; 2-byte CRC */
	)
		return -1;

	/* Clear *output* */
	memset(output, 0, outputlen);

	/* Copy *input* to *output* */
	memcpy(output, input, len);

	/* Reinstate the trailing CR in *output*, if appropriate */
	if (cr != NULL)
		output[len++] = '\r';

	return (int)len;
}

/* See header file for details */
int	common_voltronic_crc_check_and_remove_m(char *input, const size_t inputlen)
{
	int	len;
	char	*buf = (char*)xcalloc(inputlen, sizeof(char));

	if (!buf)
		return -1;

	/* Check CRC and copy *input*, purged of the CRC, to buf */
	len = common_voltronic_crc_check_and_remove(input, inputlen, buf, inputlen);

	/* Failed */
	if (len == -1) {
		free(buf);
		return -1;
	}

	/* Successfully checked CRC and copied *input*, purged of the CRC, to buf */

	/* Clear *input* */
	memset(input, 0, inputlen);

	/* Copy back buf to *input* */
	memcpy(input, buf, len);

	free(buf);
	return len;
}
