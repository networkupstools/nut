/* ydn23.h - helper functions for YDN23 (YD/T1363) serial protocol.

   Copyright (C) 2024  Gong Zhile <goodspeed@mailo.cat>

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

/* BACKGROUND: YDN23 (YD/T1363) is a serial protocol standarized by China
   Communications Standards Association. The biggest adpoter is Emerson Network
   Power. And, in fact, Emerson Network Power did involve in the drafting.

   The protocol is identified as `Dian4 Zong3 Xie2 Yi4' in Chinese and rarely
   referred as `YDN23' or `YD/T1363' in English documents. The standard is
   written in Chinese, identified as YD/T 1363.3-2014 (also YD∕T 1363.3-2023),
   with the English title being `Specification of supervision system for power,
   air condition and envirnoment -- Part 3: Intelligent equipment communication
   power'.
*/

/* TESTING: search indicates several Vertiv (Formly, Emerson) devices are
   using this protocol.

   A UPS can by probed with the command (GET_DEV_ADDR):

     # cat /dev/ttyX & printf "~21012A500000FDA4\x0D" > /dev/ttyX

   Response (Whitespaces are only for formating):

     ~ 21 01 2A 00 0000 [] FDA9 [\x0D]
     ^ ^  ^  ^  ^  ^    ^  ^    ^
     | |  |  |  |  |    |  |    | EOI
     | |  |  |  |  |    |  | CHKSUM
     | |  |  |  |  |    | INFO (No Data)
     | |  |  |  |  | LENGTH (No Data)
     | |  |  |  | RTN/CID2 (Return Value = 0x00, OK)
     | |  |  | CID1 (Device Type = 0x2A, UPS)
     | |  | ADR (Device Address = 0x01)
     | | VER (Protocol Version = 0x21)
     | SOI

   `liebert-gxe' is the driver implimented for Liebert GXE Series UPS. The
   standard defined UPS data frames and registers and it's similar to the
   protocol manual of Liebert GXE Series UPS. If the above test works, you can
   try `liebert-gxe' driver. Good Luck ;-)
*/

#ifndef YDN23_H_SEEN
#define YDN23_H_SEEN 1

#include "serial.h"

#ifndef htole16
# ifdef WORDS_BIGENDIAN
# define htole16(x) ((((x) >> 8) | ((x) << 8)))
#else	/* WORDS_BIGENDIAN */
# define htole16(x) (x)
# endif	/* WORDS_BIGENDIAN */
#endif	/* htole16 */

/* X_F: Float-point Data, X_D: Fixed-point Data */
enum YDN23_COMMAND_ID {
	YDN23_CMD_FIRST = 0,
	YDN23_GET_ANALOG_DATA_F,
	YDN23_GET_ANALOG_DATA_D,
	YDN23_GET_ONOFF_DATA,
	YDN23_GET_WARNING_DATA,
	YDN23_REMOTE_COMMAND,
	YDN23_GET_SYS_PARAM_F,
	YDN23_GET_SYS_PARAM_D,
	YDN23_SET_SYS_PARAM_F,
	YDN23_SET_SYS_PARAM_D,
	YDN23_GET_SYS_HIST_F,
	YDN23_GET_SYS_HIST_D,
	YDN23_GET_WARN_HIST,
	YDN23_GET_TIME,
	YDN23_SET_TIME,
	YDN23_GET_PROTO_VER,
	YDN23_GET_DEV_ADDR,
	YDN23_GET_VENDOR_INFO,
	YDN23_CMD_LAST,
};

/* YDN23 Protocol frame has two command ID.
   CID1: Device Type (2A for UPS)
   CID2: Action	(41~51 Standarized, 80~EF Customized)
*/

static const char YDN23_COMMANDS[YDN23_CMD_LAST][4] = {
	{'0','0','0','0'},	/* RESERVED */
	{'2','A','4','1'},	/* YDN23_GET_ANALOG_DATA_F */
	{'2','A','4','2'},	/* YDN23_GET_ANALOG_DATA_D */
	{'2','A','4','3'},	/* YDN23_GET_ONOFF_DATA */
	{'2','A','4','4'},	/* YDN23_GET_WARNING_DATA */
	{'2','A','4','5'},	/* YDN23_REMOTE_COMMAND */
	{'2','A','4','6'},	/* YDN23_GET_SYS_PARAM_F */
	{'2','A','4','7'},	/* YDN23_GET_SYS_PARAM_D */
	{'2','A','4','8'},	/* YDN23_SET_SYS_PARAM_F */
	{'2','A','4','9'},	/* YDN23_SET_SYS_PARAM_D */
	{'2','A','4','A'},	/* YDN23_GET_SYS_HIST_F */
	{'2','A','4','B'},	/* YDN23_GET_SYS_HIST_D */
	{'2','A','4','C'},	/* YDN23_GET_WARN_HIST */
	{'2','A','4','D'},	/* YDN23_GET_TIME */
	{'2','A','4','E'},	/* YDN23_SET_TIME */
	{'2','A','4','F'},	/* YDN23_GET_PROTO_VER */
	{'2','A','5','0'},	/* YDN23_GET_DEV_ADDR */
	{'2','A','5','1'},	/* YDN23_GET_VENDOR_INFO */
};

#ifndef YDN23_FRAME_INFO_SIZE
# define YDN23_FRAME_INFO_SIZE 128
#endif	/* YDN23_FRAME_INFO_SIZE */

static const char *YDN23_RTN_VALS[] = {
	"OK",
	"Bad VER",
	"Bad CHKSUM",
	"Bad LCHKSUM",
	"Invalid CID2",
	"Bad Command Format",
	"Bad Data",
};

#define YDN23_RTN_TO_STR(rtn)							\
	(((size_t) rtn) < sizeof(YDN23_RTN_VALS)/sizeof(*YDN23_RTN_VALS) ? YDN23_RTN_VALS[(rtn)] : "Unknown RTN")

#pragma pack(push, 1)
struct ydn23_frame {
	char	SOI;		/* Start of Infomation, 0x7e */
	char	VER[2];		/* Protocol Version */
	char	ADR[2];		/* Device Address */
	char	CID[4];		/* Command ID 1&2 */
	char	LEN[4];		/* Message Length, LCHKSUM &LENID */
	char	INFO[YDN23_FRAME_INFO_SIZE]; /* Command Info, Data Info */
	char	CHKSUM[4];	/* Checksum */
	char	EOI;		/* End of Infomation, 0x0d */

	size_t infolen;
};
#pragma pack(pop)

static inline void ydn23_lchecksum(uint16_t dlen, char *out)
{
	uint8_t	lenchk = 0;
	uint16_t	lelen = htole16(dlen);
	/* GCC complains uint16_t fits 7-bytes buffer, store here. */
	char	fbuf[7];

	/* Sum all four 4 bits */
	lenchk += lelen & 0x000f;
	lenchk += (lelen & 0x00f0) >> 4;
	lenchk += (lelen & 0x0f00) >> 8;
	lenchk += (lelen & 0xf000) >> 12;

	lenchk %= 16;
	lenchk = ~lenchk + 1;

	snprintf(fbuf, 7, "%04X", (uint16_t) (lelen | lenchk << 12));
	memcpy(out, fbuf, 5);
}

static inline void ydn23_checksum(const char *buf, size_t len, char* out)
{
	size_t	i;
	uint32_t	sum = 0;
	/* GCC complains uint16_t fits 7-bytes buffer, store here. */
	char	fbuf[7];

	for (i = 0; i < len; i++)
		sum += buf[i];
	sum %= 65536;

	snprintf(fbuf, 7, "%04X", (unsigned int)((uint16_t) ~sum + 1));
	memcpy(out, fbuf, 5);
}

#define YDN23_FRAME_CLEAR(framep) memset((framep), 0, sizeof(struct ydn23_frame))

/* Get the address to a register in the frame. Since all the binary data will
   be presented in INFO as hexidecimal string, the macro multiplies actual reg
   address by 2 to get the address in INFO. However, *_from_hex functions still
   take the hexidecimal string length (e.g. 1 byte reg = 2 bytes hex, dlen = 2).
*/
#define YDN23_FRAME_REG(frame, reg) ((frame).INFO + 2 * reg)

static inline int ydn23_val_from_hex(char *buf, size_t dlen)
{
	char	valbuf[16];

	if (dlen > 15)
		return 0;

	memcpy(valbuf, buf, dlen);
	valbuf[dlen] = '\0';
	return strtol(valbuf, NULL, 16);
}

static inline void ydn23_substr_from_hex(
	char *substr, size_t len,
	char *dbuf, size_t dlen)
{
	int	val;
	size_t	i;

	for (i = 0; i < dlen; i += 2) {
		val = ydn23_val_from_hex(dbuf+i, 2);
		if (val == 0x20) {
			substr[i/2] = '\0';
			return;
		}
		if (i/2 > len-1)
			break;
		substr[i/2] = val;
	}

	substr[i/2] = '\0';
	upslogx(LOG_DEBUG, "ydn23_substr_from_hex: %s", substr);
}

static inline void ydn23_frame_init(
	struct ydn23_frame *frame,
	enum YDN23_COMMAND_ID cmdi,
	const char *ver,
	const char *addr,
	const char *dptr,
	size_t dlen)
{
	if (dlen > YDN23_FRAME_INFO_SIZE) {
		upslogx(LOG_WARNING,
			"frame not big enough, required %d got %zu, truncated",
			YDN23_FRAME_INFO_SIZE, dlen);
		dlen = YDN23_FRAME_INFO_SIZE;
	}

	frame->infolen = dlen;
	memset(frame->INFO, 0, sizeof(frame->INFO));

	frame->SOI = 0x7e;
	memcpy(frame->VER, ver, sizeof(frame->VER));
	memcpy(frame->ADR, addr, sizeof(frame->ADR));
	memcpy(frame->CID, YDN23_COMMANDS[cmdi], sizeof(frame->CID));
	ydn23_lchecksum(dlen, frame->LEN);
	memcpy(frame->INFO, dptr, dlen);
	ydn23_checksum(frame->VER, frame->CHKSUM - frame->VER, frame->CHKSUM);
	frame->EOI = 0x0d;

	upsdebug_hex(6, "ydn23_frame_init", frame, sizeof(*frame));
}

static inline int ydn23_frame_send(TYPE_FD_SER fd, struct ydn23_frame *frame)
{
	int	ret;

	ser_flush_io(fd);

	ret = ser_send_buf(fd, frame, frame->INFO - &frame->SOI);
	if (ret <= 0) {
		upslogx(LOG_WARNING, "ydn23_frame_send: %s",
			ret ? strerror(errno) : "timeout");
		return ret;
	}

	if (frame->infolen) {
		ret = ser_send_buf(fd, frame->INFO, frame->infolen);
		if (ret <= 0) {
			upslogx(LOG_WARNING, "ydn23_frame_send: %s",
				ret ? strerror(errno) : "timeout");
			return ret;
		}
	}

	ret = ser_send_buf(fd, frame->CHKSUM,
			   ((char *) &frame->infolen) - frame->CHKSUM);
	if (ret <= 0) {
		upslogx(LOG_WARNING, "ydn23_frame_send: %s", ret ? strerror(errno) : "timeout");
		return ret;
	}

	return ret;
}

static inline int ydn23_frame_read(
	TYPE_FD_SER fd, struct ydn23_frame *frame,
	time_t d_sec, useconds_t d_usec)
{
	char	clearlen[5];
	int	ret, rtn;

	clearlen[4] = '\0';
	YDN23_FRAME_CLEAR(frame);

	ret = ser_get_buf(fd, frame, sizeof(*frame), d_sec, d_usec);
	if (ret <= 0) {
		upslogx(LOG_WARNING, "ydn23_frame_read: %s", ret ? strerror(errno) : "timeout");
		return -1;
	}
	upsdebug_hex(5, "ydn23_frame_read(raw)", frame, sizeof(*frame));

	if (frame->SOI != 0x7e) {
		upslogx(LOG_WARNING, "ydn23_frame_read: bad SOI, frame dropped");
		return -1;
	}

	if ((rtn = ydn23_val_from_hex(frame->CID + 2, 2))) {
		upslogx(LOG_WARNING,
			"ydn23_frame_read: bad RTN (%s), frame dropped",
			YDN23_RTN_TO_STR(rtn));
		return -1;
	}

	memcpy(clearlen, frame->LEN, sizeof(frame->LEN));
	frame->infolen = ydn23_val_from_hex(clearlen+1, 4); /* Exclude LENID */

	if (frame->infolen > YDN23_FRAME_INFO_SIZE) {
		upslogx(LOG_WARNING,
			"ydn23_frame_read: bad LEN (%zx), frame dropped",
			frame->infolen);
		return -1;
	}

	memcpy(frame->CHKSUM, frame->INFO + frame->infolen,
		((char *) &frame->infolen) - frame->CHKSUM);
	memset(frame->INFO + frame->infolen, 0,
		((char *) &frame->infolen) - frame->CHKSUM);

	ydn23_checksum(frame->VER, frame->CHKSUM - frame->VER, clearlen);
	if (memcmp(clearlen, frame->CHKSUM, sizeof(frame->CHKSUM))) {
		upslogx(LOG_WARNING,
			"ydn23_frame_read: bad CHKSUM (%c%c%c%c, %s), frame dropped",
			frame->CHKSUM[0], frame->CHKSUM[1], frame->CHKSUM[2], frame->CHKSUM[3], clearlen);
		return -1;
	}

	upsdebug_hex(5, "ydn23_frame_read(cooked)", frame, sizeof(*frame));
	return ret;
}

#endif	/* YDN23_H_SEEN */
