/* common_voltronic-crc.h - Common CRC routines for Voltronic Power devices
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

#ifndef COMMON_VOLTRONIC_CRC_H
#define COMMON_VOLTRONIC_CRC_H

/* NOTE
 * ----
 * The provided functions implement a simple 2-bytes non-reflected CRC (polynomial: 0x1021) as used in some Voltronic Power devices (LF, CR and '(' are 'escaped').
 *
 * Apart from the basic functions (that require you to know the exact length, *len*, of what you feed them) to compute CRC and check whether a given byte array, *input*, is CRC-valid or not, some helper functions are provided, that automagically calculate the length of the part of a given *input* (of max size *inputlen*) to be used.
 * When using one of these functions (that, having been developed to work with Voltronic Power devices, expect a CR-terminated string), take into consideration that:
 * - *input* is considered first as a CR-terminated byte array that may contain inner '\0's and its length is calculated till the position of the expected CR;
 * - if a CR cannot be found, then, and only then, *input* is considered as a null-terminated byte string and its length calculated accordingly.
 * Therefore, the presence of one (or more) CR automatically:
 * - discards whatever stands after (the first CR);
 * - includes whatever stands before (the first CR).
 * So, as a CR takes precedence, if you know *input* is not (or may be not) CR-terminated, make sure the remaining bytes after the end of the string till *inputlen*-length are all 0'ed (or at least don't contain any CR), if you don't want to risk them being included.
 * Also, if you know *input* may contain inner CRs, you better use one of the functions that don't try to guess the part of *input* to be used and cook up instead your own routines using only the provided basic functions to compute and check CRC.
 *
 * Don't forget, too, that CRC may contain a '\0' and hence null-terminated strings will be fooled. */

#include <string.h>

/* Compute CRC of a given *input* till the length of *len* bytes.
 * Return:
 * - the CRC computed from *len* bytes of *input*. */
unsigned short	common_voltronic_crc_compute(const char *input, const size_t len);

/* Compute CRC (till the first CR, if any, or till the end of the string) of *input* (of max size *inputlen*).
 * Please note that *input* must be:
 * - at least 1 byte long (not counting the optional trailing CR and the terminating '\0' byte);
 * - either a valid null-terminated byte string or CR-terminated.
 * Return:
 * - -1, on failure (i.e: *input* not fulfilling the aforementioned conditions);
 * - the CRC computed from *input*, on success. */
unsigned short	common_voltronic_crc_calc(const char *input, const size_t inputlen);

/* Compute CRC (till the first CR, if any, or till the end of the string) of *input* (of max size *inputlen*), then write to *output* (of max size *outputlen*):
 * - *input* (minus the trailing CR, if any);
 * - the computed 2-bytes CRC;
 * - a trailing CR (if present in *input*).
 * Please note that:
 * - *input* must be at least 1 byte long (not counting the optional trailing CR and the terminating '\0' byte) and either be a valid null-terminated byte string or CR-terminated;
 * - *output* must have space, to accomodate the computed 2-bytes CRC, for at least 2 bytes more (not counting the trailing, reserved, byte for the terminating '\0') than the actually used size of *input*.
 * Return:
 * - -1, on failure (i.e: either *input* or *output* not fulfilling the aforementioned conditions);
 * - the number of bytes written to *output*, on success. */
int	common_voltronic_crc_calc_and_add(const char *input, const size_t inputlen, char *output, const size_t outputlen);

/* Compute CRC (till the first CR, if any, or till the end of the string) of *input* (of max size *inputlen*), then add to it the computed 2-bytes CRC (before the trailing CR, if present).
 * Please note that *input* must:
 * - be at least 1 byte long (not counting the optional trailing CR and the terminating '\0' byte);
 * - either be a valid null-terminated byte string or CR-terminated;
 * - have space, to accomodate the computed 2-bytes CRC, for at least 2 bytes more (not counting the trailing, reserved, byte for the terminating '\0') than the actually used size.
 * Return:
 * - -1, on failure (i.e: *input* not fulfilling the aforementioned conditions);
 * - the number of bytes that make up the modified *input*, on success. */
int	common_voltronic_crc_calc_and_add_m(char *input, const size_t inputlen);

/* Check *input* (of max size *inputlen*) CRC.
 * Please note that *input* must be:
 * - at least 3 bytes long (not counting the optional trailing CR and the terminating '\0' byte);
 * - either a valid null-terminated byte string or CR-terminated.
 * Return:
 * - -1, on failure (i.e: *input* not fulfilling the aforementioned conditions or not CRC-validated);
 * - 0, on success (i.e.: *input* successfully validated). */
int	common_voltronic_crc_check(const char *input, const size_t inputlen);

/* Check *input* (of max size *inputlen*) CRC and copy *input*, purged of the CRC, to *output* (of max size *outputlen*).
 * Please note that:
 * - *input* must be at least 3 bytes long (not counting the optional trailing CR and the terminating '\0' byte) and either be a valid null-terminated byte string or CR-terminated;
 * - *output* must have space for at least 2 bytes less (not counting the trailing, reserved, byte for the terminating '\0') than the actually used size of *input*.
 * Return:
 * - -1, on failure (i.e: either *input* or *output* not fulfilling the aforementioned conditions or *input* not CRC-validated);
 * - the number of bytes written to *output*, on success. */
int	common_voltronic_crc_check_and_remove(const char *input, const size_t inputlen, char *output, const size_t outputlen);

/* Check *input* (of max size *inputlen*) CRC and remove it from *input*.
 * Please note that *input* must be:
 * - at least 3 bytes long (not counting the optional trailing CR and the terminating '\0' byte);
 * - either a valid null-terminated byte string or CR-terminated.
 * Return:
 * - -1, on failure;
 * - the number of bytes that make up the modified *input*, on success. */
int	common_voltronic_crc_check_and_remove_m(char *input, const size_t inputlen);

#endif	/* COMMON_VOLTRONIC_CRC_H */
