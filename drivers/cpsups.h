/* cpsups.h - Lookup tables for CyberPower text protocol UPSes

   Copyright (C) 2003  Walt Holman <waltabbyh@comcast.net>
   with thanks to Russell Kroll <rkroll@exploits.org>

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

#include "main.h"
#include "serial.h"

struct {
        int begin;
        int end;
}       pollstatusmap[] = {
        {  2, 6 },              /* Input Voltage */
        {  8, 12 },             /* Output Voltage */
        { 14, 16 },             /* Current Load */
        { 18, 20 },             /* Battery Charge % */
        { 22, 24 },             /* Temperature in C */
        { 26, 30 },             /* Frequency in Hz */
        { 32, 32 },
        { 33, 33 },
        { 34, 34 },
        {  0,  0 }
};

#define POLL_UPSSTATUS  6

#define ENDCHAR  	13     	/* replies end with CR */
#define MAXTRIES 5
#define UPSDELAY 50000  /* 50 ms delay required for reliable operation */
#define SER_WAIT_SEC    3       /* allow 3.0 sec for ser_get calls */
#define SER_WAIT_USEC   10

#define CPS_STAT_CAL	8
#define CPS_STAT_LB     32
#define CPS_STAT_OB	64
#define CPS_STAT_OL	144
