/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: t; -*-
 * 
 * megatec_ser.c: serial communication layer for Megatec protocol based UPSes
 *
 * Copyright (C) Andrey Lelikov <nut-driver@lelik.org>
 *
 * megatec_ser.c created on 3-Oct-2006
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

#include "main.h"
#include "megatec.h"
#include "serial.h"

#include <stdio.h>
#include <limits.h>
#include <string.h>

#define SEND_PACE    100000  /* 100ms interval between chars */
#define READ_TIMEOUT 2       /* 2 seconds timeout on read */

static int comm_ser_open(const char *param)
{
    upsfd = ser_open(param);
    ser_set_speed(upsfd, device_path, B2400);
    return (upsfd<0) ? -1 : 0;
}

static void comm_ser_close(const char *param)
{
    ser_close(upsfd, param);
}

static int comm_ser_send(const char *fmt,...)
{
    char    buf[128];
    size_t  len;
    va_list	ap;

    va_start(ap, fmt);

    len = vsnprintf(buf, sizeof(buf), fmt, ap);

    va_end(ap);

    if ((len < 1) || (len >= (int) sizeof(buf)))
        upslogx(LOG_WARNING, "comm_ser_send: vsnprintf needed more "
            "than %d bytes", (int)sizeof(buf));

    return ser_send_buf_pace(upsfd, SEND_PACE, (unsigned char*)buf, len);
}

static int comm_ser_recv(char *buffer,size_t buffer_len,char endchar,const char *ignchars)
{
    return ser_get_line(upsfd, buffer, buffer_len, endchar, ignchars, READ_TIMEOUT, 0);
}

static megatec_comm_t comm_ser = 
{
    .name = "serial",
    .open = &comm_ser_open,
    .close = &comm_ser_close,
    .send = &comm_ser_send,
    .recv = &comm_ser_recv
};

megatec_comm_t *comm = & comm_ser;

/* EOF - megatec_ser.c */
