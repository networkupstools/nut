/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: t; -*-
 * 
 * megatec_usb.c: usb communication layer for Megatec protocol based UPSes
 *
 * Copyright (C) Andrey Lelikov <nut-driver@lelik.org>
 *
 * megatec_usb.c created on 3-Oct-2006
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
#include "libusb.h"

#include <stdio.h>
#include <limits.h>
#include <string.h>

/*
    This is a communication driver for "USB HID" UPS-es which use proprietary
usb-to-serial converter and speak megatec protocol. Usually these are cheap
models and usb-to-serial converter is a huge oem hack - HID tables are bogus,
device has no UPS reports, etc. 
    This driver has a table of all known devices which has pointers to device-
specific communication functions (namely send a string to UPS and read a string
from it). Driver takes care of detection, opening a usb device, string
formatting etc. So in order to add support for another usb-to-serial device one
only needs to implement device-specific get/set functions and add an entry into
KnownDevices table.

*/

static communication_subdriver_t    *usb = &usb_subdriver;
static usb_dev_handle               *udev=NULL;
static HIDDevice                    hiddevice;

static int comm_usb_recv(char *buffer,size_t buffer_len,char endchar,const char *ignchars);

typedef struct 
{
    uint16_t        vid;
    uint16_t        pid;
    int (*get_data)(char *buffer,int buffer_size);
    int (*set_data)(const char *str);
} usb_ups_t;

usb_ups_t   *usb_ups_device = NULL;

/*
    All devices known to this driver go here
    along with their set/get routines
*/

static int get_data_agiler(char *buffer,int buffer_size);
static int set_data_agiler(const char *str);

static usb_ups_t KnownDevices[]={
    { 0x05b8, 0x0000, get_data_agiler, set_data_agiler },
    { .vid=0 }     /* end of list */
};

static int comm_usb_match(HIDDevice *d, void *privdata)
{
    usb_ups_t   *p;
    
    for (p=KnownDevices;p->vid!=0;p++)
    {
        if ( (p->vid==d->VendorID) && (p->pid==d->ProductID) ) 
        {
            usb_ups_device = p;
            return 1;
        }
    }

    p = (usb_ups_t*)privdata;

    if (NULL!=p)
    {
        if ( (p->vid==d->VendorID) && (p->pid==d->ProductID) ) 
        {
            usb_ups_device = p;
            return 1;
        }
    }

    return 0;
}

static int comm_usb_open(const char *param)
{
    HIDDeviceMatcher_t  match;
    static usb_ups_t    param_arg;
    const char*         p;
    int                 ret,i;
    union  _u {
        unsigned char   report_desc[4096];
        char            flush_buf[256];
    } u;

    memset(&match,0,sizeof(match));
    match.match_function = &comm_usb_match;

    if (0!=strcmp(param,"auto"))
    {
        param_arg.vid = (uint16_t) strtoul(param,NULL,16);
        p = strchr(param,':');
        if (NULL!=p)
        {
            param_arg.pid = (uint16_t) strtoul(p+1,NULL,16);
        } else {
            param_arg.vid = 0;
        }

        // pure heuristics - assume this unknown device speaks agiler protocol
        param_arg.get_data = get_data_agiler;
        param_arg.set_data = set_data_agiler;

        if (0!=param_arg.vid)
        {
            match.privdata = &param_arg;
        } else {
            upslogx(LOG_ERR, 
                "comm_usb_open: invalid usb device specified, must be \"auto\" or \"vid:pid\"");
            return -1;
        }
    }

    ret = usb->open(&udev,&hiddevice,&match,u.report_desc,MODE_OPEN);
    if (ret<0) 
        return ret;

    // flush input buffers
    for (i=0;i<10;i++)
    {
        if (comm_usb_recv(u.flush_buf,sizeof(u.flush_buf),0,NULL)<1) break;
    }

    return 0;
}

static void comm_usb_close(const char *param)
{
    usb->close(udev);
}

static int comm_usb_send(const char *fmt,...)
{
    char    buf[128];
    size_t  len;
    va_list	ap;

    if (NULL==udev) 
        return -1;

    va_start(ap, fmt);

    len = vsnprintf(buf, sizeof(buf), fmt, ap);

    va_end(ap);

    if ((len < 1) || (len >= (int) sizeof(buf)))
    {
        upslogx(LOG_WARNING, "comm_usb_send: vsnprintf needed more "
            "than %d bytes", (int)sizeof(buf));
        buf[sizeof(buf)-1]=0;
    }

    return usb_ups_device->set_data(buf);
}

static int comm_usb_recv(char *buffer,size_t buffer_len,char endchar,const char *ignchars)
{
    int len;
    char *src,*dst,c;
    
    if (NULL==udev) 
        return -1;

    len = usb_ups_device->get_data(buffer,buffer_len);
    if (len<0)
        return len;

    dst = buffer;

    for (src=buffer;src!=(buffer+len);src++)
    {
        c = *src;

        if ( (c==endchar) || (c==0) ) {
            break;
        }

        if (NULL!=strchr(ignchars,c)) continue;

        *(dst++) = c;
    }

    // terminate string if we have space
    if (dst!=(buffer+len))
    {
        *dst = 0;
    }

    return (dst-buffer);
}

static megatec_comm_t comm_usb = 
{
    .name = "usb",
    .open = &comm_usb_open,
    .close = &comm_usb_close,
    .send = &comm_usb_send,
    .recv = &comm_usb_recv
};

megatec_comm_t *comm = & comm_usb;


/************** minidrivers go after this point **************************/


/*
    Agiler seraial-to-usb device.

    Protocol was reverse-engineered from Windows driver
    HID tables are complitely bogus
    Data is transferred out as one 8-byte packet with report ID 0
    Data comes in as 6 8-byte reports per line , padded with zeroes
    All constants are hardcoded in windows driver
*/

#define AGILER_REPORT_SIZE      8
#define AGILER_REPORT_COUNT     6
#define AGILER_TIMEOUT          5000

static int set_data_agiler(const char *str)
{
    unsigned char report_buf[AGILER_REPORT_SIZE];

    if (strlen(str)>AGILER_REPORT_SIZE)
    {
        upslogx(LOG_ERR, 
            "set_data_agiler: output string too large");
        return -1;
    }

    memset(report_buf,0,sizeof(report_buf));
    memcpy(report_buf,str,strlen(str));

    return usb->set_report(udev,0,report_buf,sizeof(report_buf));
}

static int get_data_agiler(char *buffer,int buffer_size)
{
    int i,len;
    char    buf[AGILER_REPORT_SIZE*AGILER_REPORT_COUNT+1];

    memset(buf,0,sizeof(buf));

    for (i=0;i<AGILER_REPORT_COUNT;i++)
    {
        len = usb->get_interrupt(udev,buf+i*AGILER_REPORT_SIZE,AGILER_REPORT_SIZE,AGILER_TIMEOUT);
        if (len!=AGILER_REPORT_SIZE) {
            if (len<0) len=0;
            buf[i*AGILER_REPORT_SIZE+len]=0;
            break;
        }
    }

    len = strlen(buf);

    if (len > buffer_size)
    {
        upslogx(LOG_ERR, 
            "get_data_agiler: input buffer too small");
        len = buffer_size;
    }

    memcpy(buffer,buf,len);
    return len;
}

/* EOF - megatec_usb.c */
