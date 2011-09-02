/*  nut-ipmi.h - Abstract IPMI interface, to allow using different IPMI backends
 *
 *  Copyright (C) 2011 - Arnaud Quette <arnaud.quette@free.fr>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

/* Abstract structure to store information */
typedef struct IPMIDevice_s {
	int			ipmi_id;
	char*		manufacturer;		/* Manufacturer Name */
	char*		product;			/* Product Name */
	char*		serial;				/* Product serial number */
	char*		part;				/* Part Number */
	char*		date;				/* Manufacturing Date/Time */
	int			overall_capacity;	/* realpower.nominal? */
	int			input_minvoltage;
	int			input_maxvoltage;
	int			input_minfreq;
	int			input_maxfreq;
	int			voltage;			/* psu.voltage or device.voltage */
} IPMIDevice_t;

/* Generic functions, to implement in the backends */
int nutipmi_open(int ipmi_id, IPMIDevice_t *ipmi_dev);
void nutipmi_close(void);
