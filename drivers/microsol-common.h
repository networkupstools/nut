/* microsol-common.h -  common framework for Microsol Solis-based UPS hardware

   Copyright (C) 2004  Silvino B. Magalhaes    <sbm2yk@gmail.com>
                 2019  Roberto Panerai Velloso <rvelloso@gmail.com>
                 2021  Ygor A. S. Regados      <ygorre@tutanota.com>

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

   2021/03/19 - Version 0.70 - Initial release, based on solis driver

*/

#ifndef INCLUDED_MICROSOL_COMMON_H
#define INCLUDED_MICROSOL_COMMON_H

typedef unsigned int bool_t;

#define PACKET_SIZE 25

/* buffers */
extern unsigned char received_packet[PACKET_SIZE];

/* Identification */
extern const char *model_name;
extern unsigned int ups_model;
extern bool_t input_220v, output_220v;

/* logical */
extern bool_t detected;
extern bool_t line_unpowered, overheat, overload;
extern bool_t critical_battery, inverter_working;
/*extern bool_t recharging;*/ /* microsol-apc.c has its own copy */

/* Input group */
extern double input_voltage, input_current, input_frequency;
extern double input_minimum_voltage, input_maximum_voltage, input_nominal_voltage;
extern double input_low_limit, input_high_limit;

/* Output group */
extern double output_voltage, output_current, output_frequency;

/* Battery group */
extern int battery_extension;
extern double battery_voltage, battery_charge;
extern double temperature;

/* Power group */
extern double apparent_power, real_power, ups_load;
extern int load_power_factor, nominal_power, ups_power_factor;

extern void scan_received_pack_model_specific(void);
extern void set_ups_model(void);
extern bool_t ups_model_defined(void);

#endif				/* INCLUDED_MICROSOL_COMMON_H */
