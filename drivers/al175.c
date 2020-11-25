/*
 * al175.c - NUT support for Eltek AL175 alarm module.
 *           AL175 shall be in COMLI mode.
 *
 * Copyright (C) 2004-2013 Marine & Bridge Navigation Systems <http://mns.spb.ru>
 * Author: Kirill Smelkov <kirr@mns.spb.ru>
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
 */


/*
 * - NOTE the following document is referenced in this driver:
 *
 *	TE-36862-B4 "COMLI COMMUNICATION PROTOCOL IMPLEMENTED IN PRS SYSTEMS",
 *	by Eltek A/S
 *
 *
 * - AL175 debug levels:
 *
 *	1 user-level trace (status, instcmd, etc...)
 * 	2 status decode errors
 * 	3 COMLI proto handling errors
 * 	4 raw IO trace
 *
 */

#include "main.h"
#include "serial.h"
#include "timehead.h"

#include <stddef.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>


#include "nut_stdint.h"
typedef	uint8_t byte_t;


#define DRIVER_NAME	"Eltek AL175/COMLI driver"
#define DRIVER_VERSION	"0.12"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Kirill Smelkov <kirr@mns.spb.ru>\n" \
	"Marine & Bridge Navigation Systems <http://mns.spb.ru>",
	DRV_EXPERIMENTAL,
	{ NULL }
};


#define	STX	0x02
#define	ETX	0x03
#define	ACK	0x06


/************
 * RAW DATA *
 ************/

/**
 * raw_data buffer representation
 */
typedef struct {
	byte_t     *buf;	/*!< the whole buffer address	*/
	unsigned    buf_size;	/*!< the whole buffer size	*/

	byte_t     *begin;	/*!< begin of content		*/
	byte_t     *end;	/*!< one-past-end of content	*/
} raw_data_t;


/**
 * pseudo-alloca raw_data buffer  (alloca is not in POSIX)
 * @param  varp		ptr-to local raw_data_t variable to which to alloca
 * @param  buf_array	array allocated on stack which will be used as storage
 *			(must be auto-variable)
 * @return alloca'ed memory as raw_data
 *
 * Example:
 *
 *	raw_data_t ack;
 *	byte_t     ack_buf[8];
 *
 *	raw_alloc_onstack(&ack, ack_buf);
 */
#define raw_alloc_onstack(varp, buf_array) do {		\
	(varp)->buf		= &(buf_array)[0];	\
	(varp)->buf_size	= sizeof(buf_array);	\
							\
	(varp)->begin		= (varp)->buf;		\
	(varp)->end		= (varp)->buf;		\
} while (0)


/**
 * xmalloc raw buffer
 * @param  size	size in bytes
 * @return xmalloc'ed memory as raw_data
 */
static raw_data_t raw_xmalloc(size_t size)
{
	raw_data_t data;

	data.buf	= xmalloc(size);
	data.buf_size	= size;

	data.begin	= data.buf;
	data.end	= data.buf;

	return data;
}

/**
 * free raw_data buffer
 * @param  buf	raw_data buffer to free
 */
static void raw_free(raw_data_t *buf)
{
	free(buf->buf);

	buf->buf      = NULL;
	buf->buf_size = 0;
	buf->begin    = NULL;
	buf->end      = NULL;
}


/***************************************************************************/

/***************
 * COMLI types *
 ***************/

/**
 * COMLI message header info
 * @see 1. INTRODUCTION
 */
typedef struct {
	int  id;	/*!< Id[1:2]		*/
	int  stamp;	/*!< Stamp[3]		*/
	int  type;	/*!< Mess Type[4]	*/
} msg_head_t;

/**
 * COMLI IO header info
 * @see 1. INTRODUCTION
 */
typedef struct {
	unsigned  addr;	/*!< Addr[5:8]		*/
	unsigned  len;	/*!< NOB[9:10]		*/
} io_head_t;

/**
 * maximum allowed io.len value
 */
#define	IO_LEN_MAX	0xff

/**
 * COMLI header info
 * @see 1. INTRODUCTION
 */
typedef struct {
	msg_head_t  msg;	/*!< message header [1:4]	*/
	io_head_t   io;		/*!< io header [5:10]		*/
} comli_head_t;



/******************
 * MISC UTILITIES *
 ******************/

/**
 * convert hex string to int
 * @param  head  input string
 * @param  len   string length
 * @return parsed value (>=0) if success, -1 on error
 */
static long from_hex(const byte_t *head, unsigned len)
{
	long val=0;

	while (len-- != 0) {
		int ch = *head;

		if (!isxdigit(ch))
			return -1;	/* wrong character	*/

		val *= 0x10;

		if (isdigit(ch)) {
			val += (ch-'0');
		}
		else {
			/* ch = toupper(ch)  without locale-related problems */
			if (ch < 'A')
				ch += 'A' - 'a';

			val += 0x0A + (ch-'A');
		}

		++head;
	}

	return val;
}

/**
 * compute checksum of a buffer
 * @see 10. CHECKSUM BCC
 * @param   buf     buffer address
 * @param   count   no. of bytes in the buffer
 * @return  computed checksum
 */
static byte_t compute_bcc(const byte_t *buf, size_t count)
{
	byte_t bcc=0;
	unsigned i;

	for (i=0; i<count; ++i)
		bcc ^= buf[i];

	return bcc;
}


/**
 * reverse bits in a buffer bytes from right to left
 * @see 6. CODING AND DECODING OF REGISTER VALUES
 * @param   buf     buffer address
 * @param   count   no. of bytes in the buffer
 */
static void reverse_bits(byte_t *buf, size_t count)
{
	byte_t x;

	while (count!=0) {
		x = *buf;
		x = ( (x & 0x80) >> 7 )  |
		    ( (x & 0x40) >> 5 )  |
		    ( (x & 0x20) >> 3 )  |
		    ( (x & 0x10) >> 1 )  |
		    ( (x & 0x08) << 1 )  |
		    ( (x & 0x04) << 3 )  |
		    ( (x & 0x02) << 5 )  |
		    ( (x & 0x01) << 7 );
		*buf = x;

		++buf;
		--count;
	}
}


/********************************************************************/

/*
 * communication basics
 *
 * ME	(Monitor Equipment)
 * PRS	(Power Rectifier System)  /think of it as of UPS in common speak/
 *
 * there are 2 types of transactions:
 *
 * 'ACTIVATE COMMAND'
 *	ME -> PRS		(al_prep_activate)
 *	ME <- PRS [ack]		(al_check_ack)
 *
 *
 * 'READ REGISTER'
 *	ME -> PRS		(al_prep_read_req)
 *	ME <- PRS [data]	(al_parse_reply)
 *
 */

/********************
 * COMLI primitives *
 ********************/


/************************
 * COMLI: OUTPUT FRAMES *
 ************************/

/**
 * prepare COMLI sentence
 * @see 1. INTRODUCTION
 * @param   dest    [out] where to put the result
 * @param   h       COMLI header info
 * @param   buf     data part of the sentence
 * @param   count   amount of data bytes in the sentence
 *
 * @note: the data are copied into the sentence "as-is", there is no conversion is done.
 *  if the caller wants to reverse bits it is necessary to call reverse_bits(...) prior
 *  to comli_prepare.
 */
static void comli_prepare(raw_data_t *dest, const comli_head_t *h, const void *buf, size_t count)
{
/*
 *    0     1   2      3       4     5     6     7     8       9    10    11 - - -     N-1    N
 * +-----+---------+-------+------+-------------------------+-----------+------------+-----+-----+
 * | STX | IDh IDl | Stamp | type | addr1 addr2 addr3 addr4 | NOBh NOBl | ...data... | ETX | BCC |
 * +-----+---------+-------+------+-------------------------+-----------+------------+-----+-----+
 *
 * ^                                                                                             ^
 * |                                                                                             |
 *begin                                                                                         end
 */
	byte_t *out = dest->begin;


	/* it's caller responsibility to allocate enough space.
	   else it is a bug in the program */
	if ( (out+11+count+2) > (dest->buf + dest->buf_size) )
		fatalx(EXIT_FAILURE, "too small dest in comli_prepare\n");

	out[0] = STX;
	snprintf((char *)out+1, 10+1, "%02X%1i%1i%04X%02X", h->msg.id, h->msg.stamp, h->msg.type, h->io.addr, h->io.len);

	memcpy(out+11, buf, count);
	reverse_bits(out+11, count);


	out[11+count] = ETX;
	out[12+count] = compute_bcc(out+1, 10+count+1);

	dest->end = dest->begin + (11+count+2);
}




/**
 * prepare AL175 read data request
 * @see 2. MESSAGE TYPE 2 (COMMAND SENT FROM MONITORING EQUIPMENT)
 * @param   dest    [out] where to put the result
 * @param   addr    start address of requested area
 * @param   count   no. of requested bytes
 */
static void al_prep_read_req(raw_data_t *dest, unsigned addr, size_t count)
{
	comli_head_t h;

	h.msg.id	= 0x14;
	h.msg.stamp	= 1;
	h.msg.type	= 2;

	h.io.addr	= addr;
	h.io.len	= count;

	comli_prepare(dest, &h, NULL, 0);
}


/**
 * prepare AL175 activate command
 * @see 4. MESSAGE TYPE 0 (ACTIVATE COMMAND)
 * @param   dest    [out] where to put the result
 * @param   cmd     command type        [11]
 * @param   subcmd  command subtype     [12]
 * @param   pr1     first parameter     [13:14]
 * @param   pr2     second parameter    [15:16]
 * @param   pr3     third parameter     [17:18]
 */
static void al_prep_activate(raw_data_t *dest, byte_t cmd, byte_t subcmd, uint16_t pr1, uint16_t pr2, uint16_t pr3)
{
	comli_head_t h;
	char data[8+1];

	h.msg.id	= 0x14;
	h.msg.stamp	= 1;
	h.msg.type	= 0;

	h.io.addr	= 0x4500;
	h.io.len	= 8;

	/* NOTE: doc says we should use ASCII coding here, but the actual
	 *       values are > 0x80, so we use binary coding	*/
	data[0] = cmd;
	data[1] = subcmd;

	/* FIXME? One CI testcase builder claims here that
	 *   warning: '%2X' directive output may be truncated writing
	 *   between 2 and 4 bytes into a region of size between 3 and 5
	 *   [-Wformat-truncation=]
	 * but none others do, and I can't figure out how it thinks so :/
	 */
	snprintf(data+2, 6+1, "%2X%2X%2X", pr1, pr2, pr3);

	comli_prepare(dest, &h, data, 8);
}

/***********************
 * COMLI: INPUT FRAMES *
 ***********************/

/**
 * check COMLI frame for correct layout and bcc
 * @param  f	frame to check
 *
 * @return 0 (ok)  -1 (error)
 */
static int comli_check_frame(/*const*/ raw_data_t f)
{
	int bcc;
	byte_t *tail;

	if (*f.begin!=STX)
		return -1;

	tail = f.end - 2;
	if (tail <= f.begin)
		return -1;

	if (tail[0]!=ETX)
		return -1;

	bcc = compute_bcc(f.begin+1, (f.end - f.begin) - 2/*STX & BCC*/);
	if (bcc!= tail[1])
		return -1;

	return 0;
}


/**
 * parse reply header from PRS
 * @see 3. MESSAGE TYPE 0 (REPLY FROM PRS ON MESSAGE TYPE 2)
 *
 * @param  io			[out] parsed io_header
 * @param  raw_reply_head	[in] raw reply header from PRS
 * @return 0 (ok),  -1 (error)
 *
 * @see al_parse_reply
 */
static int al_parse_reply_head(io_head_t *io, const raw_data_t raw_reply_head)
{
/*
 *    0     1   2      3       4     5     6     7     8       9    10
 * +-----+---------+-------+------+-------------------------+-----------+-----------+
 * | STX | IDh IDl | Stamp | type | addr1 addr2 addr3 addr4 | NOBh NOBl | ......... |
 * +-----+---------+-------+------+-------------------------+-----------+-----------+
 *
 *       ^                                                              ^
 *       |                                                              |
 *     begin							       end
 */

	unsigned long io_addr, io_len;
	const byte_t *reply_head = raw_reply_head.begin - 1;

	if ( (raw_reply_head.end - raw_reply_head.begin) != 10)  {
		upsdebugx(3, "%s: wrong size\t(%i != 10)", __func__, (int)(raw_reply_head.end - raw_reply_head.begin));
		return -1;		/* wrong size	*/
	}

	if (reply_head[1]!='0' || reply_head[2]!='0')  {
		upsdebugx(3, "%s: wrong id\t('%c%c' != '00')", __func__, reply_head[1], reply_head[2]);
		return -1;		/* wrong id	*/
	}

	if (reply_head[3]!='1')  {
		upsdebugx(3, "%s: wrong stamp\t('%c' != '1')", __func__, reply_head[3]);
		return -1;		/* wrong stamp	*/
	}

	if (reply_head[4]!='0')  {
		upsdebugx(3, "%s: wrong type\t('%c' != '0')", __func__, reply_head[4]);
		return -1;		/* wrong type	*/
	}

	io_addr = from_hex(&reply_head[5], 4);
	if (io_addr==-1UL)  {
		upsdebugx(3, "%s: invalid addr\t('%c%c%c%c')", __func__, reply_head[5],reply_head[6],reply_head[7],reply_head[8]);
		return -1;		/* wrong addr	*/
	}

	io_len = from_hex(&reply_head[9], 2);
	if (io_len==-1UL)   {
		upsdebugx(3, "%s: invalid nob\t('%c%c')", __func__, reply_head[9],reply_head[10]);
		return -1;		/* wrong NOB	*/
	}

	if (io_len > IO_LEN_MAX) {
		upsdebugx(3, "nob too big\t(%lu > %i)", io_len, IO_LEN_MAX);
		return -1;		/* too much data claimed */
	}

	io->addr = io_addr;
	io->len  = io_len;


	return 0;
}


/**
 * parse reply from PRS
 * @see 3. MESSAGE TYPE 0 (REPLY FROM PRS ON MESSAGE TYPE 2)
 * @param  io_head	[out] parsed io_header
 * @param  io_buf	[in] [out] raw_data where to place incoming data (see ...data... below)
 * @param  raw_reply	raw reply from PRS to check
 * @return  0 (ok), -1 (error)
 *
 * @see al_parse_reply_head
 */
static int al_parse_reply(io_head_t *io_head, raw_data_t *io_buf, /*const*/ raw_data_t raw_reply)
{
/*
 *    0     1   2      3       4     5     6     7     8       9    10    11 - - -     N-1    N
 * +-----+---------+-------+------+-------------------------+-----------+------------+-----+-----+
 * | STX | IDh IDl | Stamp | type | addr1 addr2 addr3 addr4 | NOBh NOBl | ...data... | ETX | BCC |
 * +-----+---------+-------+------+-------------------------+-----------+------------+-----+-----+
 *
 *       ^                                                                           ^
 *       |                                                                           |
 *     begin                                                                        end
 */

	int err;
	unsigned i;
	const byte_t *reply = NULL;

	/* 1: extract header and parse it */
	/*const*/ raw_data_t raw_reply_head = raw_reply;

	if (raw_reply_head.begin + 10 <= raw_reply_head.end)
		raw_reply_head.end = raw_reply_head.begin + 10;

	err = al_parse_reply_head(io_head, raw_reply_head);
	if (err==-1)
		return -1;


	/* 2: process data */
	reply = raw_reply.begin - 1;

	if ( (raw_reply.end - raw_reply.begin) != (ptrdiff_t)(10 + io_head->len))  {
		upsdebugx(3, "%s: corrupt sentence\t(%i != %i)",
				__func__, (int)(raw_reply.end - raw_reply.begin), 10 + io_head->len);
		return -1;		/* corrupt sentence	*/
	}


	/* extract the data */
	if (io_buf->buf_size < io_head->len)	{
		upsdebugx(3, "%s: too much data to fit in io_buf\t(%u > %u)",
				__func__, io_head->len, io_buf->buf_size);
		return -1;		/* too much data to fit in io_buf	*/
	}

	io_buf->begin = io_buf->buf;
	io_buf->end   = io_buf->begin;

	for (i=0; i<io_head->len; ++i)
		*(io_buf->end++) = reply[11+i];

	reverse_bits(io_buf->begin, (io_buf->end - io_buf->begin) );

	upsdebug_hex(3, "\t\t--> payload", io_buf->begin, (io_buf->end - io_buf->begin));

	return 0;	/* all ok */
}


/**
 * check acknowledge from PRS
 * @see 5. ACKNOWLEDGE FROM PRS
 * @param   raw_ack  raw acknowledge from PRS to check
 * @return  0 on success, -1 on error
 */
static int al_check_ack(/*const*/ raw_data_t raw_ack)
{
/*
 *    0     1   2      3       4     5     6     7
 * +-----+---------+-------+------+-----+-----+-----+
 * | STX | IDh IDl | Stamp | type | ACK | ETX | BCC |
 * +-----+---------+-------+------+-----+-----+-----+
 *
 *       ^                              ^
 *       |                              |
 *     begin                           end
 */

	const byte_t *ack = raw_ack.begin - 1;

	if ( (raw_ack.end - raw_ack.begin) !=5)  {
		upsdebugx(3, "%s: wrong size\t(%i != 5)", __func__, (int)(raw_ack.end - raw_ack.begin));
		return -1;		/* wrong size	*/
	}

	if (ack[1]!='0' || ack[2]!='0')  {
		upsdebugx(3, "%s: wrong id\t('%c%c' != '00')", __func__, ack[1], ack[2]);
		return -1;		/* wrong id	*/
	}

	/* the following in not mandated. it is just said it will be
	 * "same as one received". but we always send '1' (0x31) as stamp
	 * (see 4. MESSAGE TYPE 0 (ACTIVATE COMMAND). Hence, stamp checking
	 * is hardcoded here.
	 */
	if (ack[3]!='1')  {
		upsdebugx(3, "%s: wrong stamp\t('%c' != '1')", __func__, ack[3]);
		return -1;		/* wrong stamp	*/
	}

	if (ack[4]!='1')  {
		upsdebugx(3, "%s: wrong type\t('%c' != '1')", __func__, ack[4]);
		return -1;		/* wrong type	*/
	}

	if (ack[5]!=ACK)  {
		upsdebugx(3, "%s: wrong ack\t(0x%02X != 0x%02X)", __func__, ack[5], ACK);
		return -1;		/* wrong ack	*/
	}


	return 0;
}





/******************************************************************/


/**********
 * SERIAL *
 **********/

/* clear any flow control (copy from powercom.c) */
static void ser_disable_flow_control (void)
{
	struct termios tio;

	tcgetattr (upsfd, &tio);

	tio.c_iflag &= ~ (IXON | IXOFF);
	tio.c_cc[VSTART] = _POSIX_VDISABLE;
	tio.c_cc[VSTOP] = _POSIX_VDISABLE;

	upsdebugx(4, "Flow control disable");

	/* disable any flow control */
	tcsetattr(upsfd, TCSANOW, &tio);
}

static void flush_rx_queue()
{
	ser_flush_in(upsfd, "", /*verbose=*/nut_debug_level);
}

/**
 * transmit frame to PRS
 *
 * @param  dmsg   debug message prefix
 * @param  frame  the frame to tansmit
 * @return 0 (ok) -1 (error)
 */
static int tx(const char *dmsg, /*const*/ raw_data_t frame)
{
	int err;

	upsdebug_ascii(3, dmsg, frame.begin, (frame.end - frame.begin));

	err = ser_send_buf(upsfd, frame.begin, (frame.end - frame.begin) );
	if (err==-1) {
		upslogx(LOG_ERR, "failed to send frame to PRS: %s", strerror(errno));
		return -1;
	}

	if (err != (frame.end - frame.begin)) {
		upslogx(LOG_ERR, "sent incomplete frame to PRS");
		return -1;
	}

	return 0;
}

/***********
 * CHATTER *
 ***********/

static time_t T_io_begin;	/* start of current I/O transaction */
static int    T_io_timeout;	/* in seconds */

/* start new I/O transaction with maximum time limit */
static void io_new_transaction(int timeout)
{
	T_io_begin = time(NULL);
	T_io_timeout = timeout;
}

/**
 * get next character from input stream
 *
 * @param  ch   ptr-to where store result
 *
 * @return -1 (error)  0 (timeout)  >0 (got it)
 *
 */
static int get_char(char *ch)
{
	time_t now = time(NULL);
	long rx_timeout;

	rx_timeout = T_io_timeout - (now - T_io_begin);
	/* negative rx_timeout -> time already out */
	if (rx_timeout < 0)
		return 0;
	return ser_get_char(upsfd, ch, rx_timeout, 0);
}


/**
 * get next characters from input stream
 *
 * @param   buf ptr-to output buffer
 * @param   len buffer length
 *
 * @return -1 (error)  0 (timeout)  >0 (no. of characters actually read)
 *
 */
static int get_buf(byte_t *buf, size_t len)
{
	time_t now = time(NULL);
	long rx_timeout;

	rx_timeout = T_io_timeout - (now - T_io_begin);
	/* negative rx_timeout -> time already out */
	if (rx_timeout < 0)
		return 0;
	return ser_get_buf_len(upsfd, buf, len, rx_timeout, 0);
}

/**
 * scan incoming bytes for specific character
 *
 * @return 0 (got it) -1 (error)
 */
static int scan_for(char c)
{
	char in;
	int  err;

	while (1) {
		err = get_char(&in);
		if (err==-1 || err==0 /*timeout*/)
			return -1;

		if (in==c)
			break;
	}

	return 0;
}


/**
 * receive 'activate command' ACK from PRS
 *
 * @return 0 (ok) -1 (error)
 */
static int recv_command_ack()
{
	int err;
	raw_data_t ack;
	byte_t     ack_buf[8];

	/* 1:  STX  */
	err = scan_for(STX);
	if (err==-1)
		return -1;


	raw_alloc_onstack(&ack, ack_buf);
	*(ack.end++) = STX;


	/* 2:  ID1 ID2 STAMP MSG_TYPE ACK ETX BCC */
	err = get_buf(ack.end, 7);
	if (err!=7)
		return -1;

	ack.end += 7;

	/* frame constructed - let's verify it */
	upsdebug_ascii(3, "rx (ack):\t\t", ack.begin, (ack.end - ack.begin));

	/* generic layout */
	err = comli_check_frame(ack);
	if (err==-1)
		return -1;

	/* shrink frame */
	ack.begin += 1;
	ack.end   -= 2;

	return al_check_ack(ack);
}

/**
 * receive 'read register' data from PRS
 * @param  io		[out] io header of received data
 * @param  io_buf	[in] [out] where to place incoming data
 *
 * @return 0 (ok) -1 (error)
 */
static int recv_register_data(io_head_t *io, raw_data_t *io_buf)
{
	int err, ret;
	raw_data_t reply_head;
	raw_data_t reply;

	byte_t	   reply_head_buf[11];

	/* 1:  STX  */
	err = scan_for(STX);
	if (err==-1)
		return -1;

	raw_alloc_onstack(&reply_head, reply_head_buf);
	*(reply_head.end++) = STX;


	/* 2:  ID1 ID2 STAMP MSG_TYPE ADDR1 ADDR2 ADDR3 ADDR4 LEN1 LEN2 */
	err = get_buf(reply_head.end, 10);
	if (err!=10)
		return -1;

	reply_head.end += 10;

	upsdebug_ascii(3, "rx (head):\t", reply_head.begin, (reply_head.end - reply_head.begin));


	/* 3:  check header, extract IO info */
	reply_head.begin += 1;	/* temporarily strip STX */

	err = al_parse_reply_head(io, reply_head);
	if (err==-1)
		return -1;

	reply_head.begin -= 1;  /* restore STX */

	upsdebugx(4, "\t\t--> addr: 0x%x  len: 0x%x", io->addr, io->len);

	/* 4:  allocate space for full reply and copy header there */
	reply = raw_xmalloc(11/*head*/ + io->len/*data*/ + 2/*ETX BCC*/);

	memcpy(reply.end, reply_head.begin, (reply_head.end - reply_head.begin));
	reply.end += (reply_head.end - reply_head.begin);

	/* 5:  receive tail of the frame */
	err = get_buf(reply.end, io->len + 2);
	if (err!=(int)(io->len+2)) {
		upsdebugx(4, "rx_tail failed, err=%i (!= %i)", err, io->len+2);
		ret = -1; goto out;
	}

	reply.end += io->len + 2;


	/* frame constructed, let's verify it */
	upsdebug_ascii(3, "rx (head+data):\t", reply.begin, (reply.end - reply.begin));

	/* generic layout */
	err = comli_check_frame(reply);
	if (err==-1) {
		upsdebugx(3, "%s: corrupt frame", __func__);
		ret = -1; goto out;
	}

	/* shrink frame */
	reply.begin += 1;
	reply.end   -= 2;


	/* XXX: a bit of processing duplication here	*/
	ret = al_parse_reply(io, io_buf, reply);

out:
	raw_free(&reply);
	return ret;
}


/*****************************************************************/

/*********************
 * AL175: DO COMMAND *
 *********************/

/**
 * do 'ACTIVATE COMMAND'
 *
 * @return 0 (ok)  -1 (error)
 */
static int al175_do(byte_t cmd, byte_t subcmd, uint16_t pr1, uint16_t pr2, uint16_t pr3)
{
	int err;
	raw_data_t CTRL_frame;
	byte_t	   CTRL_frame_buf[512];

	raw_alloc_onstack(&CTRL_frame, CTRL_frame_buf);
	al_prep_activate(&CTRL_frame, cmd, subcmd, pr1, pr2, pr3);

	flush_rx_queue();		        /*  DROP  */

	err = tx("tx (ctrl):\t", CTRL_frame);	/*  TX    */
	if (err==-1)
		return -1;


	return recv_command_ack();	        /*  RX    */
}


/**
 * 'READ REGISTER'
 *
 */
static int al175_read(byte_t *dst, unsigned addr, size_t count)
{
	int err;
	raw_data_t REQ_frame;
	raw_data_t rx_data;
	io_head_t io;

	byte_t	REQ_frame_buf[512];

	raw_alloc_onstack(&REQ_frame, REQ_frame_buf);
	al_prep_read_req(&REQ_frame, addr, count);

	flush_rx_queue();			/*  DROP  */

	err = tx("tx (req):\t", REQ_frame);	/*  TX    */
	if (err==-1)
		return -1;


	rx_data.buf	 = dst;
	rx_data.buf_size = count;
	rx_data.begin	 = dst;
	rx_data.end	 = dst;

	err = recv_register_data(&io, &rx_data);
	if (err==-1)
		return -1;

	if ((rx_data.end - rx_data.begin) != (int)count)
		return -1;

	if ( (io.addr != addr) || (io.len != count) ) {
		upsdebugx(3, "%s: io_head mismatch\t(%x,%x != %x,%x)",
				__func__, io.addr, io.len, addr,
				(unsigned int)count);
		return -1;
	}


	return 0;
}

/*************
 * NUT STUFF *
 *************/

/****************************
 * ACTIVATE COMMANDS table
 *
 * see 8. ACTIVATE COMMANDS
 */

typedef int mm_t;	/* minutes */
typedef int VV_t;	/* voltage */

#define	Z1  , 0
#define Z2  , 0, 0
#define	Z3  , 0, 0, 0

#define	ACT int

/* Declare to keep compiler happy even if some routines below are not used currently */
ACT	TOGGLE_PRS_ONOFF	();
ACT	CANCEL_BOOST		();
ACT	STOP_BATTERY_TEST	();
ACT	START_BATTERY_TEST	(VV_t EndVolt, unsigned Minutes);
ACT	SET_FLOAT_VOLTAGE	(VV_t v);
ACT	SET_BOOST_VOLTAGE	(VV_t v);
ACT	SET_HIGH_BATTERY_LIMIT	(VV_t Vhigh);
ACT	SET_LOW_BATTERY_LIMIT	(VV_t Vlow);
ACT	SET_DISCONNECT_LEVEL_AND_DELAY	(VV_t level, mm_t delay);
ACT	RESET_ALARMS		();
ACT	CHANGE_COMM_PROTOCOL	();
ACT	SET_VOLTAGE_AT_ZERO_T	(VV_t v);
ACT	SET_SLOPE_AT_ZERO_T	(VV_t mv_per_degree);
ACT	SET_MAX_TCOMP_VOLTAGE	(VV_t v);
ACT	SET_MIN_TCOMP_VOLTAGE	(VV_t v);
ACT	SWITCH_TEMP_COMP	(int on);
ACT	SWITCH_SYM_ALARM	();

/* Implement */
ACT	TOGGLE_PRS_ONOFF	()		{ return al175_do(0x81, 0x80			Z3);	}
ACT	CANCEL_BOOST		()		{ return al175_do(0x82, 0x80			Z3);	}
ACT	STOP_BATTERY_TEST	()		{ return al175_do(0x83, 0x80			Z3);	}
ACT	START_BATTERY_TEST	(VV_t EndVolt, unsigned Minutes)
						{ return al175_do(0x83, 0x81, EndVolt, Minutes	Z1);	}

ACT	SET_FLOAT_VOLTAGE	(VV_t v)	{ return al175_do(0x87, 0x80, v			Z2);	}
ACT	SET_BOOST_VOLTAGE	(VV_t v)	{ return al175_do(0x87, 0x81, v			Z2);	}
ACT	SET_HIGH_BATTERY_LIMIT	(VV_t Vhigh)	{ return al175_do(0x87, 0x82, Vhigh		Z2);	}
ACT	SET_LOW_BATTERY_LIMIT	(VV_t Vlow)	{ return al175_do(0x87, 0x83, Vlow		Z2);	}

ACT	SET_DISCONNECT_LEVEL_AND_DELAY
				(VV_t level, mm_t delay)
						{ return al175_do(0x87, 0x84, level, delay	Z1);	}

ACT	RESET_ALARMS		()		{ return al175_do(0x88, 0x80			Z3);	}
ACT	CHANGE_COMM_PROTOCOL	()		{ return al175_do(0x89, 0x80			Z3);	}
ACT	SET_VOLTAGE_AT_ZERO_T	(VV_t v)	{ return al175_do(0x8a, 0x80, v			Z2);	}
ACT	SET_SLOPE_AT_ZERO_T	(VV_t mv_per_degree)
						{ return al175_do(0x8a, 0x81, mv_per_degree	Z2);	}

ACT	SET_MAX_TCOMP_VOLTAGE	(VV_t v)	{ return al175_do(0x8a, 0x82, v			Z2);	}
ACT	SET_MIN_TCOMP_VOLTAGE	(VV_t v)	{ return al175_do(0x8a, 0x83, v			Z2);	}
ACT	SWITCH_TEMP_COMP	(int on)	{ return al175_do(0x8b, 0x80, on		Z2);	}

ACT	SWITCH_SYM_ALARM	()		{ return al175_do(0x8c, 0x80			Z3);	}


/**
 * extract double value from a word
 */
static double d16(byte_t data[2])
{
	return (data[1] + 0x100*data[0]) / 100.0;
}

void upsdrv_updateinfo(void)
{
	/* int flags; */

	byte_t x4000[9];		/* registers from 0x4000 to 0x4040 inclusive */
	byte_t x4048[2];		/* 0x4048 - 0x4050	*/
	byte_t x4100[8];		/* 0x4100 - 0x4138	*/
	byte_t x4180[8];		/* 0x4180 - 0x41b8	*/
	byte_t x4300[2];		/* 0x4300 - 0x4308	*/
	int err;

	double batt_current = 0.0;


	upsdebugx(4, " ");
	upsdebugx(4, "UPDATEINFO");
	upsdebugx(4, "----------");
	io_new_transaction(/*timeout=*/3);

#define	RECV(reg) do { \
	err = al175_read(x ## reg, 0x ## reg, sizeof(x ## reg));	\
	if (err==-1) {							\
		dstate_datastale();					\
		return;							\
	}								\
} while (0)

	RECV(4000);
	RECV(4048);
	RECV(4100);
	RECV(4180);
	RECV(4300);


	status_init();

	/* XXX non conformant with NUT naming & not well understood what they mean */
#if 0
				/* 0x4000   DIGITAL INPUT 1-8	*/
	dstate_setinfo("load.fuse",		(x4000[0] & 0x80) ? "OK" : "BLOWN");
	dstate_setinfo("battery.fuse",		(x4000[0] & 0x40) ? "OK" : "BLOWN");
	dstate_setinfo("symalarm.fuse",		(x4000[0] & 0x20) ? "OK" : "BLOWN");

				/* 0x4008   BATTERY INFORMATION	*/
	dstate_setinfo("battery.contactor",	(x4000[1] & 0x80) ? "XX" : "YY");	/* FIXME */
	dstate_setinfo("load.contactor",	(x4000[1] & 0x40) ? "XX" : "YY");	/* FIXME */
	dstate_setinfo("lvd.contactor",		(x4000[1] & 0x20) ? "XX" : "YY");	/* FIXME */
#endif
	if (x4000[0] & 0x40){
		dstate_setinfo("battery.fuse",	"FAIL");
		status_set("RB");
	}else{
		dstate_setinfo("battery.fuse",	"OK");
	}

	if (x4000[0] & 0x20){
		dstate_setinfo("battery.symmetry",	"FAIL");
		status_set("RB");
	}else{
		dstate_setinfo("battery.symmetry",	"OK");
	}

	if (x4000[1] & 0x01)	/* battery test running	*/
		status_set("TEST");

	/* TODO: others from 0x4008 */

				/* 0x4010   NOT USED	*/
				/* 0x4018   NOT USED	*/

	switch (x4000[4]) {	/* 0x4020   MAINS VOLTAGE STATUS	*/
		case 0:     status_set("OL");   break;
		case 1:     status_set("OB");   break;

		case 2:     /* doc: "not applicable" */
		default:
			upsdebugx(2, "%s: invalid mains voltage status\t(%i)", __func__, x4000[4]);
	}

				/* 0x4028   SYSTEM ON OFF STATUS	*/
	switch (x4000[5]) {
		case 0: /* system on */     break;
		case 1:	status_set("OFF");  break;

		default:
			upsdebugx(2, "%s: invalid system on/off status\t(%i)", __func__, x4000[5]);
	}

	switch (x4000[6]) {	/* 0x4030   BATTERY TEST FAIL	*/
		case 0:     dstate_setinfo("ups.test.result", "OK");
			break;

		case 1:     status_set("RB");
			dstate_setinfo("ups.test.result", "FAIL");
			break;

		default:
			upsdebugx(2, "%s: invalid battery test fail\t(%i)", __func__, x4000[6]);
	}
	switch (x4000[7]) {	/* 0x4038   BATTERY VOLTAGE STATUS	*/
		case 0:     /* normal */	break;
		case 1:     status_set("LB");	break;
		case 2:     status_set("HB");   break;

		default:
			upsdebugx(2, "%s: invalid battery voltage status\t(%i)", __func__, x4000[7]);
	}
	switch (x4000[8]) {	/* 0x4040   POS./NEG. BATT. CURRENT	*/
		case 0:	batt_current = +1.0;	break;	/* positive	*/
		case 1:	batt_current = -1.0;	break;	/* negative	*/

		default:
			upsdebugx(2, "%s: invalid pos/neg battery current\t(%i)", __func__, x4000[8]);
	}

	switch (x4048[0]) {	/* 0x4048   BOOST STATUS	*/
		case 0:	/* no boost */;		break;
		case 1:	status_set("BOOST");	break;

		default:
			upsdebugx(2, "%s: invalid boost status\t(%i)", __func__, x4048[0]);
	}

	{
		const char *v=NULL;

		switch (x4048[1]) {	/* 0x4050   SYSTEM VOLTAGE STAT.	*/
			case 0:	v = "48";	break;
			case 1: v = "24";	break;
			case 2: v = "12";	break;
			case 3: v = "26";	break;
			case 4: v = "60";	break;

			default:
				upsdebugx(2, "%s: invalid system voltage status\t(%i)", __func__, x4048[1]);
		}

		if (v)
			dstate_setinfo("output.voltage.nominal",    "%s", v);
	}


				/* 0x4100   BATTERY VOLTAGE REF	*/
	dstate_setinfo("battery.voltage.nominal",	"%.2f", d16(x4100+0));

				/* 0x4110   BOOST VOLTAGE REF	*/
	dstate_setinfo("input.transfer.boost.low",	"%.2f", d16(x4100+2));	/* XXX: boost.high ?	*/

				/* 0x4120   HIGH BATT VOLT REF		XXX	*/
				/* 0x4130   LOW  BATT VOLT REF		XXX	*/

				/* 0x4180   FLOAT VOLTAGE		XXX	*/
				/* 0x4190   BATT CURRENT			*/
	batt_current *= d16(x4180+2);
	dstate_setinfo("battery.current",		"%.2f", batt_current);

				/* 0x41b0   LOAD CURRENT (output.current in NUT)	*/
	dstate_setinfo("output.current",		"%.2f", d16(x4180+6));

				/* 0x4300   BATTERY TEMPERATURE	*/
	dstate_setinfo("battery.temperature",		"%.2f", d16(x4300+0));


	status_commit();

	upsdebugx(1, "STATUS: %s", dstate_getinfo("ups.status"));
	dstate_dataok();


	/* out: */
	return;

}

void upsdrv_shutdown(void)
	__attribute__((noreturn));

void upsdrv_shutdown(void)
{
	/* TODO use TOGGLE_PRS_ONOFF for shutdown */

	/* tell the UPS to shut down, then return - DO NOT SLEEP HERE */

	/* maybe try to detect the UPS here, but try a shutdown even if
	   it doesn't respond at first if possible */

	/* replace with a proper shutdown function */
	fatalx(EXIT_FAILURE, "shutdown not supported");

	/* you may have to check the line status since the commands
	   for toggling power are frequently different for OL vs. OB */

	/* OL: this must power cycle the load if possible */

	/* OB: the load must remain off until the power returns */
}


static int instcmd(const char *cmdname, const char *extra)
{
	int err;

	upsdebugx(1, "INSTCMD: %s", cmdname);

	io_new_transaction(/*timeout=*/5);

	/*
	 * test.battery.start
	 * test.battery.stop
	 */

	if (!strcasecmp(cmdname, "test.battery.start")) {
		err = START_BATTERY_TEST(24, 1);
		return (!err ? STAT_INSTCMD_HANDLED : STAT_INSTCMD_FAILED);
	}

	if (!strcasecmp(cmdname, "test.battery.stop")) {
		err = STOP_BATTERY_TEST();
		return (!err ? STAT_INSTCMD_HANDLED : STAT_INSTCMD_FAILED);
	}

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s] [%s]", cmdname, extra);
	return STAT_INSTCMD_UNKNOWN;
}

/* no help */
void upsdrv_help(void)
{
}

/* no -x flags */
void upsdrv_makevartable(void)
{
}

void upsdrv_initups(void)
{
	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, B9600);

	ser_disable_flow_control();
}

void upsdrv_cleanup(void)
{
	ser_close(upsfd, device_path);
}


void upsdrv_initinfo(void)
{
	/* TODO issue short io with UPS to detect it's presence */
	/* try to detect the UPS here - call fatal_with_errno(EXIT_FAILURE, ) if it fails */

	dstate_setinfo("ups.mfr", "Eltek");
	dstate_setinfo("ups.model", "AL175");
	/* ... */

	/* instant commands */
	dstate_addcmd ("test.battery.start");
	dstate_addcmd ("test.battery.stop");
	/* TODO rest instcmd(s) */

	upsh.instcmd = instcmd;
}
