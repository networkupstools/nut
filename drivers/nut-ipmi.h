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

#ifndef NUT_IPMI_H
#define NUT_IPMI_H

typedef enum {
	PSU_STATUS_UNKNOWN = 1,
	PSU_PRESENT,			/* = status OL */
	PSU_ABSENT,				/* = status stale */
	PSU_POWER_FAILURE		/* = status OFF */
} psu_status_t;

/* Abstract structure to store information */
typedef struct IPMIDevice_s {
	int			ipmi_id;				/* FRU ID */
	char*		manufacturer;			/* Manufacturer Name */
	char*		product;				/* Product Name */
	char*		serial;					/* Product serial number */
	char*		part;					/* Part Number */
	char*		date;					/* Manufacturing Date/Time */
	int			overall_capacity;		/* realpower.nominal? */
	int			input_minvoltage;
	int			input_maxvoltage;
	int			input_minfreq;
	int			input_maxfreq;
	int			voltage;				/* psu.voltage or device.voltage */
	unsigned int	sensors_count;			/* number of sensors IDs in sensors_id_list */
	unsigned int	sensors_id_list[20];	/* ID of sensors linked to this FRU */

	/* measurements... */
	int			status;					/* values from psu_status_t */
	double		input_voltage;
	double		input_current;
	double		temperature;
} IPMIDevice_t;

/* Generic functions, to implement in the backends */
int nut_ipmi_open(int ipmi_id, IPMIDevice_t *ipmi_dev);
void nut_ipmi_close(void);
int nut_ipmi_monitoring_init(void);
int nut_ipmi_get_sensors_status(IPMIDevice_t *ipmi_dev);

#endif /* NUT_IPMI_H */

