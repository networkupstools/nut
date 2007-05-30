/*
 * al175.c - NUT support for Eltek AL175 alarm module.
 *           AL175 shall be in COMLI mode.
 *
 * Copyright (C) 2004 Marine & Bridge Navigation Systems <http://mns.spb.ru>
 * Copyright (C) 2004 Kirill Smelkov <kirr@mns.spb.ru>
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


#include <stddef.h>
#include <ctype.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#include "main.h"
#include "serial.h"
#include "al175.h"

#define DEBUG	1

#define	STX	0x02
#define	ETX	0x03
#define	ACK	0x06

typedef	unsigned char	byte_t;

#if DEBUG
#define XTRACE(fmt, ...) do {					\
	upsdebugx(1, "%s: " fmt, __FUNCTION__ __VA_ARGS__);	\
} while (0)
#else
#define XTRACE(fmt, ...) do {} while (0)
#endif

/************
 * RAW DATA *
 ************/

/**
 * raw_data buffer representation
 */
typedef struct {
    byte_t     *buf;		//!< the whole buffer address
    unsigned    buf_size;	//!< the whole buffer size

    byte_t     *begin;		//!< begin of content
    byte_t     *end;		//!< one-past-end of content
} raw_data_t;

/**
 * constant raw_data buffer representation
 */
#if 0
typedef struct {
    const byte_t *buf;		//!< the whole buffer address
    unsigned      buf_size;	//!< the whole buffer size

    const byte_t *begin;	//!< begin of content
    const byte_t *end;		//!< one-past-end of content
} const_raw_data_t;
#else
typedef	raw_data_t	const_raw_data_t;
#endif


/**
 * alloca raw_data buffer
 * @param  size the size in bytes
 * @return alloca'ed memory as raw_data
 */
#define raw_alloca(size)		\
({					\
	raw_data_t data;		\
					\
	data.buf       = alloca(size);	\
	data.buf_size  = size;		\
					\
	data.begin     = data.buf;	\
	data.end       = data.buf;	\
					\
	data;				\
})

/**
 * xmalloc raw buffer
 * @param  size	the size in bytes
 * @return xmalloc'ed memory as raw_data
 */
raw_data_t raw_xmalloc(size_t size)
{
	raw_data_t data = {
		.buf       = xmalloc(size),
		.buf_size  = size,
	
	/* XXX: how to handle this?
		.begin     = .buf,
		.end	   = .buf
	*/
	};

	data.begin = data.buf;
	data.end   = data.buf;

	return data;
}

/**
 * free raw_data buffer
 * @param  buf	the buffer to be freed
 */
void raw_free(raw_data_t *buf)
{
	free(buf->buf);

	buf->buf      = NULL;
	buf->buf_size = 0;
	buf->begin    = NULL;
	buf->end      = NULL;
}


/* taken from www.asciitable.com */
static const char* ascii_symb[] = {
	"NUL",  /*  0x00    */
	"SOH",  /*  0x01    */
	"STX",  /*  0x02    */
	"ETX",  /*  0x03    */
	"EOT",  /*  0x04    */
	"ENQ",  /*  0x05    */
	"ACK",  /*  0x06    */
	"BEL",  /*  0x07    */
	"BS",   /*  0x08    */
	"TAB",  /*  0x09    */
	"LF",   /*  0x0A    */
	"VT",   /*  0x0B    */
	"FF",   /*  0x0C    */
	"CR",   /*  0x0D    */
	"SO",   /*  0x0E    */
	"SI",   /*  0x0F    */
	"DLE",  /*  0x10    */
	"DC1",  /*  0x11    */
	"DC2",  /*  0x12    */
	"DC3",  /*  0x13    */
	"DC4",  /*  0x14    */
	"NAK",  /*  0x15    */
	"SYN",  /*  0x16    */
	"ETB",  /*  0x17    */
	"CAN",  /*  0x18    */
	"EM",   /*  0x19    */
	"SUB",  /*  0x1A    */
	"ESC",  /*  0x1B    */
	"FS",   /*  0x1C    */
	"GS",   /*  0x1D    */
	"RS",   /*  0x1E    */
	"US"    /*  0x1F    */
};

/**
 * dump raw_data buffer to file stream
 * @param  out  output stream
 * @param  buf  the buffer to dump
 */
void raw_dump(FILE *out, const_raw_data_t buf)
{
	byte_t *p;

	fprintf(out, "( ");

	for (p=buf.begin; p!=buf.end; ++p)
		if (*p < 0x20)
			fprintf(out, "%4s ", ascii_symb[*p]);
		else
			fprintf(out, "'%c' ", *p);

	fprintf(out, ")\t[ ");


	for (p=buf.begin; p!=buf.end; ++p)
		fprintf(out, "0x%02x ", *p);

	fprintf(out, "]\n");

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
	int  id;        //!< Id[1:2]
	int  stamp;     //!< Stamp[3]
	int  type;      //!< Mess Type[4]
} msg_head_t;

/**
 * COMLI IO header info
 * @see 1. INTRODUCTION
 */
typedef struct {
	unsigned  addr;	//!< Addr[5:8]
	unsigned  len;	//!< NOB[9:10]
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
        msg_head_t  msg;        //!< message header [1:4]
	io_head_t   io;		//!< io header [5:10]
} comli_head_t;



/******************
 * MISC UTILITIES *
 ******************/

/**
 * convert hex string to int
 * @param  head  input string
 * @param  count string length
 * @return parsed value (>=0) if success, -1 on error
 */
static long from_hex(const char *head, unsigned len)
{
	long val=0;

	while (len-- != 0) {
		int ch = *head;

		if (!isxdigit(ch))
			return -1;	/* wrong character	*/

		val *= 0x10;

		ch = toupper(ch);	/* ? locale ?		*/

		if (isdigit(ch))
			val += (ch-'0');
		else
			val += 0x0a + (ch-'A');

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
static byte_t compute_bcc(const void *buf, size_t count)
{
	byte_t bcc=0;
	unsigned i;

	for (i=0; i<count; ++i)
		bcc ^= *((byte_t *)buf + i);

	return bcc;
}


/* XXX: not optimal */
/**
 * revers bits in a byte from right to left and vice-versa
 * @see 6. CODING AND DECODING OF REGISTER VALUES
 */
#define REVERSE_BITS(x)	(\
		( ((x) & 0x80) >> 7 )		|	\
		( ((x) & 0x40) >> 5 )		|	\
		( ((x) & 0x20) >> 3 )		|	\
		( ((x) & 0x10) >> 1 )		|	\
		( ((x) & 0x08) << 1 )		|	\
		( ((x) & 0x04) << 3 )		|	\
		( ((x) & 0x02) << 5 )		|	\
		( ((x) & 0x01) << 7 )	)

/**
 * reverse bits in a buffer
 * @param   buf     buffer address
 * @param   count   no. of bytes in the buffer
 */
static void reverse_bits(void *buf, size_t count)
{
	byte_t *mem = buf;

	while (count!=0) {
		*mem = REVERSE_BITS(*mem);
                ++mem;
		--count;
	}
}


/********************************************************************/

/*
 * communication basics
 *
 * ME	(Monitor Equipment)
 * PRS	(? Power System ? )
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
 * @param   count   amount of data bytes in the sentence]
 *
 * @note: the data are copied into the sentence "as-is", that is no conversion is done.
 *  if the coller want to reevrse bits it is necessary to call reverse_bits(...) prior
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
	snprintf(out+1, 10+1, "%02X%1i%1i%04X%02X", h->msg.id, h->msg.stamp, h->msg.type, h->io.addr, h->io.len);

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
	comli_head_t h = {
                .msg = {
		        .id	= 0x14,
		        .stamp	= 1,
		        .type	= 2,
                },

		.io = {
			.addr	= addr,
			.len	= count,
		},
	};

	return comli_prepare(dest, &h, NULL, 0);
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
static void al_prep_activate(raw_data_t *dest, int cmd, int subcmd, int pr1, int pr2, int pr3)
{
	comli_head_t h = {
                .msg = {
		        .id	= 0x14,
		        .stamp	= 1,
		        .type	= 0,
                },

		.io = {
			.addr	= 0x4500,
			.len	= 8,
		},
	};

	char data[8+1];

	data[0] = cmd;          // XXX: shall be ASCII here
	data[1] = subcmd;

	snprintf(data+2, 6+1, "%2X%2X%2X", pr1, pr2, pr3);

	return comli_prepare(dest, &h, data, 8);
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
static int comli_check_frame(const_raw_data_t f)
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
		XTRACE("wrong size");
		return -1;		/* wrong size	*/
        }

	if (reply_head[1]!='0' || reply_head[2]!='0')  {
		XTRACE("wrong id");
		return -1;		/* wrong id	*/
	}

	if (reply_head[3]!='1')  {
		XTRACE("wrong stamp");
		return -1;		/* wrong stamp	*/
	}

	if (reply_head[4]!='0')  {
		XTRACE("wrong type");
		return -1;		/* wrong type	*/
	}

	io_addr = from_hex(&reply_head[5], 4);
	if (io_addr==-1UL)  {
		XTRACE("wrong addr");
		return -1;		/* wrong addr	*/
	}

	io_len = from_hex(&reply_head[9], 2);
	if (io_len==-1UL)   {
		XTRACE("wrong nob");
		return -1;		/* wrong NOB	*/
	}

	if (io_len > IO_LEN_MAX) {
		XTRACE("nob too big");
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
static int al_parse_reply(io_head_t *io_head, raw_data_t *io_buf, const_raw_data_t raw_reply)
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
	const byte_t *reply = raw_reply.begin - 1;

	/* 1: extract header and parse it */
	const_raw_data_t raw_reply_head = raw_reply;

	if (raw_reply_head.begin + 10 <= raw_reply_head.end)
		raw_reply_head.end = raw_reply_head.begin + 10;

	err = al_parse_reply_head(io_head, raw_reply_head);
	if (err==-1)
		return -1;


	/* 2: process data */
	reply = raw_reply.begin - 1;

	if ( (raw_reply.end - raw_reply.begin) != (ptrdiff_t)(10 + io_head->len))  {
		XTRACE("corrupt sentence");
		return -1;		/* corrupt sentence	*/
	}


	/* extract the data */
	if (io_buf->buf_size < io_head->len)	{  // XXX: maybe write from io_buf->begin ?
		XTRACE("too much data to fit in io_buf.");
		return -1;		/* too much data to fit in io_buf	*/
	}

	io_buf->begin = io_buf->buf;		// XXX: see ^^^
	io_buf->end   = io_buf->begin;

	for (i=0; i<io_head->len; ++i)
		*(io_buf->end++) = reply[11+i];

	reverse_bits(io_buf->begin, (io_buf->end - io_buf->begin) );

#if DEBUG
        fprintf(stderr, "rx_buf:\t\t");
        raw_dump(stderr, *io_buf);
#endif

	return 0;	/* all ok */
}


/**
 * check acknowledge from PRS
 * @see 5. ACKNOWLEDGE FROM PRS
 * @param   raw_ack  raw acknowledge from PRS to check  
 * @return  0 on success, -1 on error    
 */
static int al_check_ack(const_raw_data_t raw_ack)
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
		XTRACE("wrong size");
                return -1;		/* wrong size	*/
	}

	if (ack[1]!='0' || ack[2]!='0')  {
		XTRACE("wrong id");
		return -1;		/* wrong id	*/
	}

	/* the following in not mandated. it is just said it will be
	 * "same as one received". but we always send '1' (0x31) as stamp
	 * (see 4. MESSAGE TYPE 0 (ACTIVATE COMMAND). Hence, stamp checking
	 * is hardcoded here.
	 */
	if (ack[3]!='1')  {
		XTRACE("wrong stamp");
		return -1;		/* wrong stamp	*/
	}

	if (ack[4]!='1')  {
		XTRACE("wrong type");
		return -1;		/* wrong type	*/
	}

        if (ack[4]!=ACK)  {
		XTRACE("wrong ack");
                return -1;		/* wrong ack	*/
	}


	return 0;
}





/******************************************************************/


/**********
 * SERIAL *
 **********/

static void alarm_handler(int sig)
{
	/* just do nothing */
	XTRACE("timeout...\n");		/* XXX: doing so is wrong from signal handler... */
}

/* clear any flow control (stolen from powercom.c) */
static void ser_disable_flow_control (void)
{
	struct termios tio;
	
	tcgetattr (upsfd, &tio);
	
	tio.c_iflag &= ~ (IXON | IXOFF);
	tio.c_cc[VSTART] = _POSIX_VDISABLE;
	tio.c_cc[VSTOP] = _POSIX_VDISABLE;
				
	upsdebugx(2, "Flow control disable");

	/* disable any flow control */
	tcsetattr(upsfd, TCSANOW, &tio);
}

static void flush_rx_queue()
{
	ser_flush_in(upsfd, "", 1/*verbose*/);
}

/**
 * transmit frame to PRS
 *
 * @param  frame  the frame to tansmit
 * @return 0 (ok) -1 (error)
 */
static int tx(const_raw_data_t frame)
{
	int err;

#if DEBUG
	fprintf(stderr, "tx:\t\t");
	raw_dump(stderr, frame);
#endif

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

static int get_char(char *ch)
{
	/* 999 here means infinity.
	 * all timeouts are processed via alarm(2)
	 */
	return ser_get_char(upsfd, ch, 999, 0);
}

static int get_buf(char *buf, size_t len)
{
	return ser_get_buf_len(upsfd, buf, len, 999, 0);
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
		if (err==-1)
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

	/* 1:  STX  */
	err = scan_for(STX);
	if (err==-1)
		return -1;


	ack = raw_alloca(8);
	*(ack.end++) = STX;

 
	/* 2:  ID1 ID2 STAMP MSG_TYPE ACK ETX BCC */
	err = get_buf(ack.end, 7);
	if (err!=7)
		return -1;

	ack.end += 7;

	/* frame constructed - let's verify it */
#if DEBUG
	fprintf(stderr, "rx:\t\t");
	raw_dump(stderr, ack);
#endif

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
	int err;
	raw_data_t reply_head;
	raw_data_t reply;

	/* 1:  STX  */
	err = scan_for(STX);
	if (err==-1)
		return -1;

	 reply_head = raw_alloca(11);
	 *(reply_head.end++) = STX;


	/* 2:  ID1 ID2 STAMP MSG_TYPE ADDR1 ADDR2 ADDR3 ADDR4 LEN1 LEN2 */
	err = get_buf(reply_head.end, 10);
	if (err!=10)
		return -1;

	reply_head.end += 10;

#if DEBUG
	fprintf(stderr, "rx_head:\t");
	raw_dump(stderr, reply_head);
#endif


	/* 3:  check header, extract IO info */
	reply_head.begin += 1;	/* temporarily strip STX */

	err = al_parse_reply_head(io, reply_head);
	if (err==-1)
		return -1;

	reply_head.begin -= 1;  /* restore STX */

#if DEBUG
	fprintf(stderr, "io: %x/%x\n", io->addr, io->len);
#endif

	/* 4:  allocate space for full reply and copy header there */
	reply = raw_alloca(11/*head*/ + io->len/*data*/ + 2/*ETX BCC*/);

	memcpy(reply.end, reply_head.begin, (reply_head.end - reply_head.begin));
	reply.end += (reply_head.end - reply_head.begin);
	
	/* 5:  receive tail of the frame */
	err = get_buf(reply.end, io->len + 2);
	if (err!=(int)(io->len+2)) {
		XTRACE("rx_tail failed");
		return -1;
	}

	reply.end += io->len + 2;


	/* frame constructed, let's verify it */
#if DEBUG
	fprintf(stderr, "rx:\t\t");
	raw_dump(stderr, reply);
#endif

	/* generic layout */
	err = comli_check_frame(reply);
	if (err==-1) {
		XTRACE("corrupt frame");
		return -1;
	}

	/* shrink frame */
	reply.begin += 1;
	reply.end   -= 2;


	// XXX: a bit of processing duplication here
	return al_parse_reply(io, io_buf, reply);
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
static int al175_do(int cmd, int subcmd, int pr1, int pr2, int pr3)
{
	int err;
	raw_data_t CTRL_frame = raw_alloca(512);

	al_prep_activate(&CTRL_frame, cmd, subcmd, pr1, pr2, pr3);

	flush_rx_queue();		/*  DROP  */

	err = tx(CTRL_frame);		/*  TX    */
	if (err==-1)
		return -1;

	
	return recv_command_ack();	/*  RX    */
}


/**
 * 'READ REGISTER'
 *
 */
static int al175_read(byte_t *dst, unsigned addr, size_t count)
{
	int err;
	raw_data_t REQ_frame = raw_alloca(512);
	raw_data_t rx_data;
	io_head_t io;

	al_prep_read_req(&REQ_frame, addr, count);

	flush_rx_queue();		/*  DROP  */

	err = tx(REQ_frame);		/*  TX    */
	if (err==-1)
		return -1;


	rx_data.buf	 = dst;
	rx_data.buf_size = count;
	rx_data.begin	 = dst;
	rx_data.end	 = dst;

	err = recv_register_data(&io, &rx_data);
	if (err==-1)
		return -1;

        if (rx_data.begin != dst)   // XXX: paranoia
            return -1;

        if ((rx_data.end - rx_data.begin) != (int)count)
            return -1;

	if ( (io.addr != addr) || (io.len != count) ) {
		XTRACE("io_head mismatch");
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

// XXX: ?
typedef int mm_t;	/* minutes */
typedef int VV_t;	/* voltage */

#define	Z1  , 0
#define Z2  , 0, 0
#define	Z3  , 0, 0, 0

#define	ACT static __attribute__((used)) int

ACT	TOGGLE_PRS_ONOFF	()		{ return al175_do(0x81, 0x80			Z3);	}
ACT	CANCEL_BOOST		()		{ return al175_do(0x82, 0x80			Z3);	}
ACT	STOP_BATTERY_TEST	()		{ return al175_do(0x83, 0x80			Z3);	}
ACT	START_BATTERY_TEST	(VV_t EndVolt, unsigned Minutes)
						{ return al175_do(0x83, 0x81, EndVolt, Minutes	Z1);	}

ACT	SET_FLOAT_VOLTAGE	(VV_t v)		{ return al175_do(0x87, 0x80, v			Z2);	}
ACT	SET_BOOST_VOLTAGE	(VV_t v)		{ return al175_do(0x87, 0x81, v			Z2);	}
ACT	SET_HIGH_BATTERY_LIMIT	(VV_t Vhigh)	{ return al175_do(0x87, 0x82, Vhigh		Z2);	}
ACT	SET_LOW_BATTERY_LIMIT	(VV_t Vlow)	{ return al175_do(0x87, 0x83, Vlow		Z2);	}

ACT	SET_DISCONNECT_LEVEL_AND_DELAY
				(VV_t level, mm_t delay)
						{ return al175_do(0x87, 0x84, level, delay	Z1);	}

ACT	RESET_ALARMS		()		{ return al175_do(0x88, 0x80			Z3);	}
ACT	CHANGE_COMM_PROTOCOL	()		{ return al175_do(0x89, 0x80			Z3);	}
ACT	SET_VOLTAGE_AT_ZERO_T	(VV_t v)		{ return al175_do(0x8a, 0x80, v			Z2);	}
ACT	SET_SLOPE_AT_ZERO_T	(VV_t mv_per_degree)
						{ return al175_do(0x8a, 0x81, mv_per_degree	Z2);	}

ACT	SET_MAX_TCOMP_VOLTAGE	(VV_t v)		{ return al175_do(0x8a, 0x82, v			Z2);	}
ACT	SET_MIN_TCOMP_VOLTAGE	(VV_t v)		{ return al175_do(0x8a, 0x83, v			Z2);	}
ACT	SWITCH_TEMP_COMP	(int on)	{ return al175_do(0x8b, 0x80, on		Z2);	}

// XXX: ?
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
	/* char temp[256]; */

	byte_t x4000[9];		/* registers from 0x4000 to 0x4040 inclusive */
	byte_t x4048[2];		/* 0x4048 - 0x4050	*/
	byte_t x4100[8];		/* 0x4100 - 0x4138	*/
	byte_t x4180[8];		/* 0x4180 - 0x41b8	*/
	byte_t x4300[2];		/* 0x4300 - 0x4308	*/
	int err;

	double batt_current = 0.0;


	fprintf(stderr, "\nUPDATEINFO\n");
	alarm(3);

#define	RECV(reg) do { \
	err = al175_read(x ## reg, 0x ## reg, sizeof(x ## reg));	\
	if (err==-1) {							\
		dstate_datastale();					\
		alarm(0);						\
		return;							\
	}								\
} while (0)

	RECV(4000);
	RECV(4048);
	RECV(4100);
	RECV(4180);
	RECV(4300);


        status_init();

#if 0					
/* non conformant with NUT naming */
				// 0x4000   DIGITAL INPUT 1-8
	dstate_setinfo("load.fuse",		(x4000[0] & 0x80) ? "OK" : "BLOWN");
	dstate_setinfo("battery.fuse",		(x4000[0] & 0x40) ? "OK" : "BLOWN");
	dstate_setinfo("symalarm.fuse",		(x4000[0] & 0x20) ? "OK" : "BLOWN");

				// 0x4008   BATTERY INFORMATION
	dstate_setinfo("battery.contactor",	(x4000[1] & 0x80) ? "XX" : "YY");	// FIXME
	dstate_setinfo("load.contactor",	(x4000[1] & 0x40) ? "XX" : "YY");	// FIXME
	dstate_setinfo("lvd.contactor",		(x4000[1] & 0x20) ? "XX" : "YY");	// FIXME
#endif
        if (x4000[1] & 0x01)	// battery test running
            status_set("TEST");

        // TODO: others from 0x4008

				// 0x4010   NOT USED
				// 0x4018   NOT USED

        switch (x4000[4]) {	// 0x4020   MAINS VOLTAGE STATUS
            case 0:     status_set("OL");   break;
            case 1:     status_set("OB");   break;
            case 2:     status_set("NOT_APPLICABLE");   break;

            default:    status_set("OFF");  // XXX: "not applicable" == OFF ?
        }

				// 0x4028   SYSTEM ON OFF STATUS
	// XXX: 0x4028 is broken? (it reads as 0x55)
	switch (x4000[5]) {
	    case 0:                         break;
	    case 1:	status_set("OFF");  break;

	    default:    status_set("UNKNOWN...");
	}

        switch (x4000[6]) {	// 0x4030   BATTERY TEST FAIL
            case 0:     dstate_setinfo("ups.test.result", "OK");
	                break;

            case 1:     status_set("RB");
			dstate_setinfo("ups.test.result", "FAIL");
	                break;  // XXX: RB == remove battery?
/* AQU note: the below must be adapted! */
            default:    status_set("?T");
        }
        switch (x4000[7]) {	// 0x4038   BATTERY VOLTAGE STATUS
            case 0:     /* normal */        break;
            case 1:     status_set("LB");   break;
#if 0
/* non conformant to ups.status values */
            case 2:     status_set("HB");   break;

            default:    status_set("?B");
#endif
            default:    break;
	}
	switch (x4000[8]) {	// 0x4040   POS./NEG. BATT. CURRENT
	    case 0:	batt_current = +1.0;	break;	// positive
	    case 1:	batt_current = -1.0;	break;	// negative

	    default:    batt_current = 0.0;		// shouldn't happen
	}

	switch (x4048[0]) {	// 0x4048   BOOST STATUS
	    case 0:	/* no boost */;		break;
	    case 1:	status_set("BOOST");	break;
#if 0
/* non conformant to ups.status values */
	    default:	status_set("BOO??");
#endif
            default:    break;
	}

	{
	    const char *v;

	    switch (x4048[1]) {	// 0x4050   SYSTEM VOLTAGE STAT.
		case 0:	v = "48";	break;
		case 1: v = "24";	break;
		case 2: v = "12";	break;
		case 3: v = "26";	break;
		case 4: v = "60";	break;

		default: v = "??V";
	    }

	    dstate_setinfo("output.voltage.nominal",    "%s", v);
	}


				// 0x4100   BATTERY VOLTAGE REF
	dstate_setinfo("battery.voltage.nominal",	"%.2f", d16(x4100+0));

				// 0x4110   BOOST VOLTAGE REF
	dstate_setinfo("input.transfer.boost.low",	"%.2f", d16(x4100+2));	// XXX: boost.high ?

				// 0x4120   HIGH BATT VOLT REF		XXX
				// 0x4130   LOW  BATT VOLT REF		XXX
	
				// 0x4180   FLOAT VOLTAGE		XXX
				// 0x4190   BATT CURRENT
	batt_current *= d16(x4180+2);
	dstate_setinfo("battery.current",		"%.2f", batt_current);

				// 0x41b0   LOAD CURRENT (output.current in NUT)
	dstate_setinfo("output.current",		"%.2f", d16(x4180+6));

				// 0x4300   BATTERY TEMPERATURE
	dstate_setinfo("battery.temperature",		"%.2f", d16(x4300+0));
				

        status_commit();

        upsdebugx(1, "STATUS: %s", dstate_getinfo("ups.status"));
        dstate_dataok();


	/* out: */
	alarm(0);
	return;

}

void upsdrv_shutdown(void)
{
	/* tell the UPS to shut down, then return - DO NOT SLEEP HERE */

	/* maybe try to detect the UPS here, but try a shutdown even if
	   it doesn't respond at first if possible */

	/* replace with a proper shutdown function */
	fatalx(EXIT_FAILURE, "shutdown not supported");	/* TODO: implement... */

	/* you may have to check the line status since the commands
	   for toggling power are frequently different for OL vs. OB */

	/* OL: this must power cycle the load if possible */

	/* OB: the load must remain off until the power returns */
}

static int instcmd_worker(const char *cmdname)
{
	/*
	 * test.battary.start
	 * test.battary.stop
	 */

	if (!strcasecmp(cmdname, "test.battery.start")) {
		START_BATTERY_TEST(24, 1);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "test.battery.stop")) {
		STOP_BATTERY_TEST();
		return STAT_INSTCMD_HANDLED;
	}

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s]", cmdname);
	return STAT_INSTCMD_UNKNOWN;
}

static int instcmd(const char *cmdname, const char *extra)
{
	int err;

        upsdebugx(1, "INSTCMD: %s", cmdname);

	alarm(5);
	err = instcmd_worker(cmdname);
	alarm(0);

	return err;
}

void upsdrv_help(void)
{
}

/* list flags and values that you want to receive via -x */
void upsdrv_makevartable(void)
{
	/* allow '-x xyzzy' */
	/* addvar(VAR_FLAG, "xyzzy", "Enable xyzzy mode"); */

	/* allow '-x foo=<some value>' */
	/* addvar(VAR_VALUE, "foo", "Override foo setting"); */
}

void upsdrv_banner(void)
{
	printf("Network UPS Tools - Eltek AL175/COMLI support module %s (%s)\n\n", 
		DRV_VERSION, UPS_VERSION);
	/*
	 * This driver does not support upsdrv_shutdown(), which makes
	 * it not very useful in a real world application. This alone
	 * warrants 'experimental' status, but for the below mentioned
	 * reasons (to name a few), it's flagged 'broken' instead.
	 *
	 * - ‘return’ with a value, in function returning void (2x)
	 * - anonymous variadic macros were introduced in C99
	 * - C++ style comments are not allowed in ISO C90
	 * - ISO C forbids braced-groups within expressions (5x)
	 * - ISO C90 forbids specifying subobject to initialize (16x)
	 * - ISO C99 requires rest arguments to be used (18x)
	 * - initializer element is not computable at load time (4x)
	 *
	 * In short, there is a lot of rewriting to be done. Not the
	 * whole world is a Linux box with the latest gcc on it.
	 */
	broken_driver = 1;
}

void upsdrv_initups(void)
{
	signal(SIGALRM, alarm_handler);
	alarm(0);

	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, B9600);

	ser_disable_flow_control();

	/* probe ups type */

	/* to get variables and flags from the command line, use this:
	 *
	 * first populate with upsdrv_buildvartable above, then...
	 *
	 *                   set flag foo : /bin/driver -x foo
	 * set variable 'cable' to '1234' : /bin/driver -x cable=1234
	 *
	 * to test flag foo in your code:
	 *
	 * 	if (testvar("foo"))
	 * 		do_something();
	 *
	 * to show the value of cable:
	 *
	 *      if ((cable == getval("cable")))
	 *		printf("cable is set to %s\n", cable);
	 *	else
	 *		printf("cable is not set!\n");
	 *
	 * don't use NULL pointers - test the return result first!
	 */

	/* the upsh handlers can't be done here, as they get initialized
	 * shortly after upsdrv_initups returns to main.
	 */

	/* don't try to detect the UPS here */
}

void upsdrv_cleanup(void)
{
	/* free(dynamic_mem); */
	ser_close(upsfd, device_path);
}


void upsdrv_initinfo(void)
{
	/* try to detect the UPS here - call fatal_with_errno(EXIT_FAILURE, ) if it fails */

	dstate_setinfo("driver.version.internal", "%s", DRV_VERSION);
	dstate_setinfo("ups.mfr", "Eltek");
	dstate_setinfo("ups.model", "AL175");
        /* ... */

        /* instant commands */
	dstate_addcmd ("test.battery.start");
	dstate_addcmd ("test.battery.stop");
        /* XXX: more? */

	upsh.instcmd = instcmd;
}


/* vim: set ts=8 noet sts=8 sw=8 */
