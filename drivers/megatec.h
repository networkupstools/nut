/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: t; -*-
 * 
 * megatec.h: support for Megatec protocol based UPSes
 *
 * Copyright (C) Carlos Rodrigues <carlos.efr at mail.telepac.pt>
 *
 * megatec.c created on 4/10/2003
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

#define DRV_VERSION "1.5"

/* comm driver */
typedef struct {
    const char  *name;
    int         (*open)(const char*param);
    void        (*close)();
    int         (*send)(const char *fmt,...);
    int         (*recv)(char *buffer,size_t buffer_len,char endchar,const char *ignchars);
}	megatec_comm_t;

extern megatec_comm_t* comm;
