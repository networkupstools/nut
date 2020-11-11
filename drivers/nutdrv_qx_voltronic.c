/* nutdrv_qx_voltronic.c - Subdriver for Voltronic Power UPSes
 *
 * Copyright (C)
 *   2013 Daniele Pezzini <hyouko@gmail.com>
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
 *
 */

#include "main.h"
#include "nutdrv_qx.h"

#include "nutdrv_qx_voltronic.h"

#define VOLTRONIC_VERSION "Voltronic 0.06"

/* Support functions */
static int	voltronic_claim(void);
static void	voltronic_makevartable(void);
static void	voltronic_massive_unskip(const int protocol);

/* Range/enum functions */
static int	voltronic_batt_low(char *value, const size_t len);
static int	voltronic_bypass_volt_max(char *value, const size_t len);
static int	voltronic_bypass_volt_min(char *value, const size_t len);
static int	voltronic_bypass_freq_max(char *value, const size_t len);
static int	voltronic_bypass_freq_min(char *value, const size_t len);
static int	voltronic_eco_freq_min(char *value, const size_t len);
static int	voltronic_eco_freq_max(char *value, const size_t len);

/* Preprocess functions */
static int	voltronic_process_setvar(item_t *item, char *value, const size_t valuelen);
static int	voltronic_process_command(item_t *item, char *value, const size_t valuelen);
static int	voltronic_capability(item_t *item, char *value, const size_t valuelen);
static int	voltronic_capability_set(item_t *item, char *value, const size_t valuelen);
static int	voltronic_capability_set_nonut(item_t *item, char *value, const size_t valuelen);
static int	voltronic_capability_reset(item_t *item, char *value, const size_t valuelen);
static int	voltronic_eco_volt(item_t *item, char *value, const size_t valuelen);
static int	voltronic_eco_volt_range(item_t *item, char *value, const size_t valuelen);
static int	voltronic_eco_freq(item_t *item, char *value, const size_t valuelen);
static int	voltronic_bypass(item_t *item, char *value, const size_t valuelen);
static int	voltronic_batt_numb(item_t *item, char *value, const size_t valuelen);
static int	voltronic_batt_runtime(item_t *item, char *value, const size_t valuelen);
static int	voltronic_protocol(item_t *item, char *value, const size_t valuelen);
static int	voltronic_fault(item_t *item, char *value, const size_t valuelen);
static int	voltronic_warning(item_t *item, char *value, const size_t valuelen);
static int	voltronic_mode(item_t *item, char *value, const size_t valuelen);
static int	voltronic_status(item_t *item, char *value, const size_t valuelen);
static int	voltronic_output_powerfactor(item_t *item, char *value, const size_t valuelen);
static int	voltronic_serial_numb(item_t *item, char *value, const size_t valuelen);
static int	voltronic_outlet(item_t *item, char *value, const size_t valuelen);
static int	voltronic_outlet_delay(item_t *item, char *value, const size_t valuelen);
static int	voltronic_outlet_delay_set(item_t *item, char *value, const size_t valuelen);
static int	voltronic_p31b(item_t *item, char *value, const size_t valuelen);
static int	voltronic_p31b_set(item_t *item, char *value, const size_t valuelen);
static int	voltronic_p31g(item_t *item, char *value, const size_t valuelen);
static int	voltronic_p31g_set(item_t *item, char *value, const size_t valuelen);
static int	voltronic_phase(item_t *item, char *value, const size_t valuelen);
static int	voltronic_phase_set(item_t *item, char *value, const size_t valuelen);
static int	voltronic_parallel(item_t *item, char *value, const size_t valuelen);

/* Capability vars */
static char	*bypass_alarm,
		*battery_alarm,
		*bypass_when_off,
		*alarm_control,
		*converter_mode,
		*eco_mode,
		*battery_open_status_check,
		*bypass_forbidding,
		*site_fault_detection,
		*advanced_eco_mode,
		*constant_phase_angle,
		*limited_runtime_on_battery;

/* ups.conf settings */
static int	max_bypass_volt,
		min_bypass_volt,
		battery_number,
		output_phase_angle,
		work_range_type;
static double	max_bypass_freq,
		min_bypass_freq;


/* == Ranges/enums == */

/* Range for ups.delay.start */
static info_rw_t	voltronic_r_ondelay[] = {
	{ "0", 0 },
	{ "599940", 0 },
	{ "", 0 }
};

/* Range for ups.delay.shutdown */
static info_rw_t	voltronic_r_offdelay[] = {
	{ "12", 0 },
	{ "5940", 0 },
	{ "", 0 }
};

/* Enumlist for output phase angle */
static info_rw_t	voltronic_e_phase[] = {
	{ "000", 0 },
	{ "120", 0 },
	{ "180", 0 },
	{ "240", 0 },
	{ "", 0 }
};

/* Range for battery low voltage */
static info_rw_t	voltronic_r_batt_low[] = {
	{ "20", 0 },
	{ "24", voltronic_batt_low },
	{ "28", voltronic_batt_low },
	{ "", 0 }
};

/* Preprocess range value for battery low voltage */
static int	voltronic_batt_low(char *value, const size_t len)
{
	int		val = strtol(value, NULL, 10);
	const char	*ovn = dstate_getinfo("output.voltage.nominal"),
			*ocn = dstate_getinfo("output.current.nominal");
	NUT_UNUSED_VARIABLE(len);

	if (!ovn || !ocn) {
		upsdebugx(2, "%s: unable to get the value of output voltage nominal/output current nominal", __func__);
		return -1;
	}

	if ((strtol(ovn, NULL, 10) * strtol(ocn, NULL, 10)) < 1000) {

		if (val == 24)
			return 0;
		else
			return -1;

	} else {

		if (val == 28)
			return 0;
		else
			return -1;
	}
}

/* Range for outlet.n.delay.shutdown */
static info_rw_t	voltronic_r_outlet_delay[] = {
	{ "0", 0 },
	{ "59940", 0 },
	{ "", 0 }
};

/* Enumlist for device grid working range type */
static info_rw_t	voltronic_e_work_range[] = {
	{ "Appliance", 0 },	/* 00 */
	{ "UPS", 0 },		/* 01 */
	{ "", 0 }
};

/* Enumlist for battery type */
static info_rw_t	voltronic_e_batt_type[] = {
	{ "Li", 0 },		/* 00 */
	{ "Flooded", 0 },	/* 01 */
	{ "AGM", 0 },		/* 02 */
	{ "", 0 }
};

/* Range for number of battery packs */
static info_rw_t	voltronic_r_batt_packs[] = {
	{ "1", 0 },
	{ "99", 0 },
	{ "", 0 }
};

/* Range for number of batteries */
static info_rw_t	voltronic_r_batt_numb[] = {
	{ "1", 0 },
	{ "9", 0 },
	{ "", 0 }
};

/* Range for Bypass Mode maximum voltage */
static info_rw_t	voltronic_r_bypass_volt_max[] = {
	{ "60", voltronic_bypass_volt_max },	/* P09 */
	{ "115", voltronic_bypass_volt_max },	/* P02/P03/P10/P13/P14/P99 ivn<200 */
	{ "120", voltronic_bypass_volt_max },	/* P01 ivn<200 */
	{ "132", voltronic_bypass_volt_max },	/* P99 ivn<200 */
	{ "138", voltronic_bypass_volt_max },	/* P02/P03/P10/P13/P14 ivn<200 */
	{ "140", voltronic_bypass_volt_max },	/* P01 ivn<200, P09 */
	{ "230", voltronic_bypass_volt_max },	/* P01 ivn>=200 */
	{ "231", voltronic_bypass_volt_max },	/* P02/P03/P10/P13/P14/P99 ivn>=200 */
	{ "261", voltronic_bypass_volt_max },	/* P99 ivn>=200 */
	{ "264", voltronic_bypass_volt_max },	/* P01 ivn>=200 */
	{ "276", voltronic_bypass_volt_max },	/* P02/P03/P10/P13/P14 ivn>=200 */
	{ "", 0 }
};

/* Preprocess range value for Bypass Mode maximum voltage */
static int	voltronic_bypass_volt_max(char *value, const size_t len)
{
	int		protocol = strtol(dstate_getinfo("ups.firmware.aux")+1, NULL, 10),
			val = strtol(value, NULL, 10),
			ivn;
	const char	*involtnom = dstate_getinfo("input.voltage.nominal");
	NUT_UNUSED_VARIABLE(len);

	if (!involtnom) {
		upsdebugx(2, "%s: unable to get input.voltage.nominal", __func__);
		return -1;
	}

	ivn = strtol(involtnom, NULL, 10);

	switch (val)
	{
	case 60:	/* P09 */

		if (protocol == 9)
			return 0;

		break;

	case 115:	/* P02/P03/P10/P13/P14/P99 ivn<200 */

		if (ivn >= 200)
			return -1;

		if (protocol == 2 || protocol == 3 || protocol == 10 || protocol == 13 || protocol == 14 || protocol == 99)
			return 0;

		break;

	case 120:	/* P01 ivn<200 */

		if (ivn >= 200)
			return -1;

		if (protocol == 1)
			return 0;

		break;

	case 132:	/* P99 ivn<200 */

		if (ivn >= 200)
			return -1;

		if (protocol == 99)
			return 0;

		break;

	case 138:	/* P02/P03/P10/P13/P14 ivn<200 */

		if (ivn >= 200)
			return -1;

		if (protocol == 2 || protocol == 3 || protocol == 10 || protocol == 13 || protocol == 14)
			return 0;

		break;

	case 140:	/* P01 ivn<200, P09 */

		if (protocol == 9)
			return 0;

		if (ivn >= 200)
			return -1;

		if (protocol == 1)
			return 0;

		break;

	case 230:	/* P01 ivn>=200 */

		if (ivn < 200)
			return -1;

		if (protocol == 1)
			return 0;

		break;

	case 231:	/* P02/P03/P10/P13/P14/P99 ivn>=200 */

		if (ivn < 200)
			return -1;

		if (protocol == 2 || protocol == 3 || protocol == 10 || protocol == 13 || protocol == 14 || protocol == 99)
			return 0;

		break;

	case 261:	/* P99 ivn>=200 */

		if (ivn < 200)
			return -1;

		if (protocol == 99)
			return 0;

		break;

	case 264:	/* P01 ivn>=200 */

		if (ivn < 200)
			return -1;

		if (protocol == 1)
			return 0;

		break;

	case 276:	/* P02/P03/P10/P13/P14 ivn>=200 */

		if (ivn < 200)
			return -1;

		if (protocol == 2 || protocol == 3 || protocol == 10 || protocol == 13 || protocol == 14)
			return 0;

		break;

	default:

		upsdebugx(2, "%s: unknown value (%s)", __func__, value);
		break;

	}

	return -1;
}

/* Range for Bypass Mode minimum voltage */
static info_rw_t	voltronic_r_bypass_volt_min[] = {
	{ "50", voltronic_bypass_volt_min },	/* P99 ivn<200 */
	{ "55", voltronic_bypass_volt_min },	/* P02/P03/P10/P13/P14 ivn<200 */
	{ "60", voltronic_bypass_volt_min },	/* P09 */
	{ "85", voltronic_bypass_volt_min },	/* P01/P99 ivn<200 */
	{ "104", voltronic_bypass_volt_min },	/* P02/P03/P10/P13/P14 ivn<200 */
	{ "110", voltronic_bypass_volt_min },	/* P02/P03/P10/P13/P14 ivn>=200 */
	{ "115", voltronic_bypass_volt_min },	/* P01 ivn<200 */
	{ "140", voltronic_bypass_volt_min },	/* P09 */
	{ "149", voltronic_bypass_volt_min },	/* P99 ivn>=200 */
	{ "170", voltronic_bypass_volt_min },	/* P01 ivn>=200 */
	{ "209", voltronic_bypass_volt_min },	/* P02/P03/P10/P13/P14/P99 ivn>=200 */
	{ "220", voltronic_bypass_volt_min },	/* P01 ivn>=200 */
	{ "", 0 }
};

/* Preprocess range value for Bypass Mode minimum voltage */
static int	voltronic_bypass_volt_min(char *value, const size_t len)
{
	int		protocol = strtol(dstate_getinfo("ups.firmware.aux")+1, NULL, 10),
			val = strtol(value, NULL, 10),
			ivn;
	const char	*involtnom = dstate_getinfo("input.voltage.nominal");
	NUT_UNUSED_VARIABLE(len);

	if (!involtnom) {
		upsdebugx(2, "%s: unable to get input.voltage.nominal", __func__);
		return -1;
	}

	ivn = strtol(involtnom, NULL, 10);

	switch (val)
	{
	case 50:	/* P99 ivn<200 */

		if (ivn >= 200)
			return -1;

		if (protocol == 99)
			return 0;

		break;

	case 55:	/* P02/P03/P10/P13/P14 ivn<200 */

		if (ivn >= 200)
			return -1;

		if (protocol == 2 || protocol == 3 || protocol == 10 || protocol == 13 || protocol == 14)
			return 0;

		break;

	case 60:	/* P09 */
	case 140:	/* P09 */

		if (protocol == 9)
			return 0;

		break;

	case 85:	/* P01/P99 ivn<200 */

		if (ivn >= 200)
			return -1;

		if (protocol == 1 || protocol == 99)
			return 0;

		break;

	case 104:	/* P02/P03/P10/P13/P14 ivn<200 */

		if (ivn >= 200)
			return -1;

		if (protocol == 2 || protocol == 3 || protocol == 10 || protocol == 13 || protocol == 14)
			return 0;

		break;

	case 110:	/* P02/P03/P10/P13/P14 ivn>=200 */

		if (ivn < 200)
			return -1;

		if (protocol == 2 || protocol == 3 || protocol == 10 || protocol == 13 || protocol == 14)
			return 0;

		break;

	case 115:	/* P01 ivn<200 */

		if (ivn >= 200)
			return -1;

		if (protocol == 1)
			return 0;

		break;

	case 149:	/* P99 ivn>=200 */

		if (ivn < 200)
			return -1;

		if (protocol == 99)
			return 0;

		break;

	case 170:	/* P01 ivn>=200 */

		if (ivn < 200)
			return -1;

		if (protocol == 1)
			return 0;

		break;

	case 209:	/* P02/P03/P10/P13/P14/P99 ivn>=200 */

		if (ivn < 200)
			return -1;

		if (protocol == 2 || protocol == 3 || protocol == 10 || protocol == 13 || protocol == 14 || protocol == 99)
			return 0;

		break;

	case 220:	/* P01 ivn>=200 */

		if (ivn < 200)
			return -1;

		if (protocol == 1)
			return 0;

		break;

	default:

		upsdebugx(2, "%s: unknown value (%s)", __func__, value);
		break;

	}

	return -1;
}

/* Range for Bypass Mode maximum frequency */
static info_rw_t	voltronic_r_bypass_freq_max[] = {
	{ "51.0", voltronic_bypass_freq_max },	/* P01/P09/P02/P03/P10/P13/P14/P99 ofn==50.0 */
	{ "54.0", voltronic_bypass_freq_max },	/* P02/P03/P10/P13/P14/P99 ofn==50.0 */
	{ "60.0", voltronic_bypass_freq_max },	/* P01/P09 ofn==50.0 */
	{ "61.0", voltronic_bypass_freq_max },	/* P01/P09/P02/P03/P10/P13/P14/P99 ofn==60.0 */
	{ "64.0", voltronic_bypass_freq_max },	/* P02/P03/P10/P13/P14/P99 ofn==60.0 */
	{ "70.0", voltronic_bypass_freq_max },	/* P01/P09 ofn==60.0 */
	{ "", 0 }
};

/* Preprocess range value for Bypass Mode maximum frequency */
static int	voltronic_bypass_freq_max(char *value, const size_t len)
{
	int		protocol = strtol(dstate_getinfo("ups.firmware.aux")+1, NULL, 10),
			val = strtol(value, NULL, 10);
	double		ofn;
	const char	*outfreqnom = dstate_getinfo("output.frequency.nominal");
	NUT_UNUSED_VARIABLE(len);

	if (!outfreqnom) {
		upsdebugx(2, "%s: unable to get output.frequency.nominal", __func__);
		return -1;
	}

	ofn = strtod(outfreqnom, NULL);

	switch (val)
	{
	case 51:	/* P01/P09/P02/P03/P10/P13/P14/P99 ofn==50.0 */

		if (ofn != 50.0)
			return -1;

		if (protocol == 1 || protocol == 2 || protocol == 3 || protocol == 9 || protocol == 10 || protocol == 13 || protocol == 14 || protocol == 99)
			return 0;

		break;

	case 54:	/* P02/P03/P10/P13/P14/P99 ofn==50.0 */

		if (ofn != 50.0)
			return -1;

		if (protocol == 2 || protocol == 3 || protocol == 10 || protocol == 13 || protocol == 14 || protocol == 99)
			return 0;

		break;

	case 60:	/* P01/P09 ofn==50.0 */

		if (ofn != 50.0)
			return -1;

		if (protocol == 1 || protocol == 9)
			return 0;

		break;

	case 61:	/* P01/P09/P02/P03/P10/P13/P14/P99 ofn==60.0 */

		if (ofn != 60.0)
			return -1;

		if (protocol == 1 || protocol == 2 || protocol == 3 || protocol == 9 || protocol == 10 || protocol == 13 || protocol == 14 || protocol == 99)
			return 0;

		break;

	case 64:	/* P02/P03/P10/P13/P14/P99 ofn==60.0 */

		if (ofn != 60.0)
			return -1;

		if (protocol == 2 || protocol == 3 || protocol == 10 || protocol == 13 || protocol == 14 || protocol == 99)
			return 0;

		break;

	case 70:	/* P01/P09 ofn==60.0 */

		if (ofn != 60.0)
			return -1;

		if (protocol == 1 || protocol == 9)
			return 0;

		break;

	default:

		upsdebugx(2, "%s: unknown value (%s)", __func__, value);
		break;

	}

	return -1;
}

/* Range for Bypass Mode minimum frequency */
static info_rw_t	voltronic_r_bypass_freq_min[] = {
	{ "40.0", voltronic_bypass_freq_min },	/* P01/P09 ofn==50.0 */
	{ "46.0", voltronic_bypass_freq_min },	/* P02/P03/P10/P13/P14/P99 ofn==50.0 */
	{ "49.0", voltronic_bypass_freq_min },	/* P01/P09/P02/P03/P10/P13/P14/P99 ofn==50.0 */
	{ "50.0", voltronic_bypass_freq_min },	/* P01/P09 ofn==60.0 */
	{ "56.0", voltronic_bypass_freq_min },	/* P02/P03/P10/P13/P14/P99 ofn==60.0 */
	{ "59.0", voltronic_bypass_freq_min },	/* P01/P09/P02/P03/P10/P13/P14/P99 ofn==60.0 */
	{ "", 0 }
};

/* Preprocess range value for Bypass Mode minimum frequency */
static int	voltronic_bypass_freq_min(char *value, const size_t len)
{
	int		protocol = strtol(dstate_getinfo("ups.firmware.aux")+1, NULL, 10),
			val = strtol(value, NULL, 10);
	double		ofn;
	const char	*outfreqnom = dstate_getinfo("output.frequency.nominal");
	NUT_UNUSED_VARIABLE(len);

	if (!outfreqnom) {
		upsdebugx(2, "%s: unable to get output.frequency.nominal", __func__);
		return -1;
	}

	ofn = strtod(outfreqnom, NULL);

	switch (val)
	{
	case 40:	/* P01/P09 ofn==50.0 */

		if (ofn != 50.0)
			return -1;

		if (protocol == 1 || protocol == 9)
			return 0;

		break;

	case 46:	/* P02/P03/P10/P13/P14/P99 ofn==50.0 */

		if (ofn != 50.0)
			return -1;

		if (protocol == 2 || protocol == 3 || protocol == 10 || protocol == 13 || protocol == 14 || protocol == 99)
			return 0;

		break;

	case 49:	/* P01/P09/P02/P03/P10/P13/P14/P99 ofn==50.0 */

		if (ofn != 50.0)
			return -1;

		if (protocol == 1 || protocol == 2 || protocol == 3 || protocol == 9 || protocol == 10 || protocol == 13 || protocol == 14 || protocol == 99)
			return 0;

		break;

	case 50:	/* P01/P09 ofn==60.0 */

		if (ofn != 60.0)
			return -1;

		if (protocol == 1 || protocol == 9)
			return 0;

		break;

	case 56:	/* P02/P03/P10/P13/P14/P99 ofn==60.0 */

		if (ofn != 60.0)
			return -1;

		if (protocol == 2 || protocol == 3 || protocol == 10 || protocol == 13 || protocol == 14 || protocol == 99)
			return 0;

		break;

	case 59:	/* P01/P09/P02/P03/P10/P13/P14/P99 ofn==60.0 */

		if (ofn != 60.0)
			return -1;

		if (protocol == 1 || protocol == 2 || protocol == 3 || protocol == 9 || protocol == 10 || protocol == 13 || protocol == 14 || protocol == 99)
			return 0;

		break;

	default:

		upsdebugx(2, "%s: unknown value (%s)", __func__, value);
		break;

	}

	return -1;
}

/* Range for Eco Mode maximum voltage: filled at runtime by voltronic_eco_volt */
static info_rw_t	voltronic_r_eco_volt_max[] = {
	{ "", 0 },
	{ "", 0 },
	{ "", 0 }
};

/* Range for ECO Mode minimum voltage: filled at runtime by voltronic_eco_volt */
static info_rw_t	voltronic_r_eco_volt_min[] = {
	{ "", 0 },
	{ "", 0 },
	{ "", 0 }
};

/* Range for ECO Mode minimum frequency */
static info_rw_t	voltronic_r_eco_freq_min[] = {
	{ "40.0", voltronic_eco_freq_min },	/* P01/P09 ofn==50.0 */
	{ "46.0", voltronic_eco_freq_min },	/* P02/P03/P10/P13/P14/P99 ofn==50.0 */
	{ "47.0", voltronic_eco_freq_min },	/* P01/P09 ofn==50.0 */
	{ "48.0", voltronic_eco_freq_min },	/* P02/P03/P10/P13/P14/P99 ofn==50.0 */
	{ "50.0", voltronic_eco_freq_min },	/* P01/P09 ofn==60.0 */
	{ "56.0", voltronic_eco_freq_min },	/* P02/P03/P10/P13/P14/P99 ofn==60.0 */
	{ "57.0", voltronic_eco_freq_min },	/* P01/P09 ofn==60.0 */
	{ "58.0", voltronic_eco_freq_min },	/* P02/P03/P10/P13/P14/P99 ofn==60.0 */
	{ "", 0 }
};

/* Preprocess range value for ECO Mode minimum frequency */
static int	voltronic_eco_freq_min(char *value, const size_t len)
{
	int		protocol = strtol(dstate_getinfo("ups.firmware.aux")+1, NULL, 10),
			val = strtol(value, NULL, 10);
	double		ofn;
	const char	*outfreqnom = dstate_getinfo("output.frequency.nominal");
	NUT_UNUSED_VARIABLE(len);

	if (!outfreqnom) {
		upsdebugx(2, "%s: unable to get output.frequency.nominal", __func__);
		return -1;
	}

	ofn = strtod(outfreqnom, NULL);

	switch (val)
	{
	case 40:	/* P01/P09 ofn==50.0 */

		if (ofn != 50.0)
			return -1;

		if (protocol == 1 || protocol == 9)
			return 0;

		break;

	case 46:	/* P02/P03/P10/P13/P14/P99 ofn==50.0 */

		if (ofn != 50.0)
			return -1;

		if (protocol == 2 || protocol == 3 || protocol == 10 || protocol == 13 || protocol == 14 || protocol == 99)
			return 0;

		break;

	case 47:	/* P01/P09 ofn==50.0 */

		if (ofn != 50.0)
			return -1;

		if (protocol == 1 || protocol == 9)
			return 0;

		break;

	case 48:	/* P02/P03/P10/P13/P14/P99 ofn==50.0 */

		if (ofn != 50.0)
			return -1;

		if (protocol == 2 || protocol == 3 || protocol == 10 || protocol == 13 || protocol == 14 || protocol == 99)
			return 0;

		break;

	case 50:	/* P01/P09 ofn==60.0 */

		if (ofn != 60.0)
			return -1;

		if (protocol == 1 || protocol == 9)
			return 0;

		break;

	case 56:	/* P02/P03/P10/P13/P14/P99 ofn==60.0 */

		if (ofn != 60.0)
			return -1;

		if (protocol == 2 || protocol == 3 || protocol == 10 || protocol == 13 || protocol == 14 || protocol == 99)
			return 0;

		break;

	case 57:	/* P01/P09 ofn==60.0 */

		if (ofn != 60.0)
			return -1;

		if (protocol == 1 || protocol == 9)
			return 0;

		break;

	case 58:	/* P02/P03/P10/P13/P14/P99 ofn==60.0 */

		if (ofn != 60.0)
			return -1;

		if (protocol == 2 || protocol == 3 || protocol == 10 || protocol == 13 || protocol == 14 || protocol == 99)
			return 0;

		break;

	default:

		upsdebugx(2, "%s: unknown value (%s)", __func__, value);
		break;

	}

	return -1;
}

/* Range for ECO Mode maximum frequency */
static info_rw_t	voltronic_r_eco_freq_max[] = {
	{ "52.0", voltronic_eco_freq_max },	/* P02/P03/P10/P13/P14/P99 ofn==50.0 */
	{ "53.0", voltronic_eco_freq_max },	/* P01/P09 ofn==50.0 */
	{ "54.0", voltronic_eco_freq_max },	/* P02/P03/P10/P13/P14/P99 ofn==50.0 */
	{ "60.0", voltronic_eco_freq_max },	/* P01/P09 ofn==50.0 */
	{ "62.0", voltronic_eco_freq_max },	/* P02/P03/P10/P13/P14/P99 ofn==60.0 */
	{ "63.0", voltronic_eco_freq_max },	/* P01/P09 ofn==60.0 */
	{ "64.0", voltronic_eco_freq_max },	/* P02/P03/P10/P13/P14/P99 ofn==60.0 */
	{ "70.0", voltronic_eco_freq_max },	/* P01/P09 ofn==60.0 */
	{ "", 0 }
};

/* Preprocess range value for ECO Mode maximum frequency */
static int	voltronic_eco_freq_max(char *value, const size_t len)
{
	int		protocol = strtol(dstate_getinfo("ups.firmware.aux")+1, NULL, 10),
			val = strtol(value, NULL, 10);
	double		ofn;
	const char	*outfreqnom = dstate_getinfo("output.frequency.nominal");
	NUT_UNUSED_VARIABLE(len);

	if (!outfreqnom) {
		upsdebugx(2, "%s: unable to get output.frequency.nominal", __func__);
		return -1;
	}

	ofn = strtod(outfreqnom, NULL);

	switch (val)
	{
	case 52:	/* P02/P03/P10/P13/P14/P99 ofn==50.0 */

		if (ofn != 50.0)
			return -1;

		if (protocol == 2 || protocol == 3 || protocol == 10 || protocol == 13 || protocol == 14 || protocol == 99)
			return 0;

		break;

	case 53:	/* P01/P09 ofn==50.0 */

		if (ofn != 50.0)
			return -1;

		if (protocol == 1 || protocol == 9)
			return 0;

		break;

	case 54:	/* P02/P03/P10/P13/P14/P99 ofn==50.0 */

		if (ofn != 50.0)
			return -1;

		if (protocol == 2 || protocol == 3 || protocol == 10 || protocol == 13 || protocol == 14 || protocol == 99)
			return 0;

		break;

	case 60:	/* P01/P09 ofn==50.0 */

		if (ofn != 50.0)
			return -1;

		if (protocol == 1 || protocol == 9)
			return 0;

		break;

	case 62:	/* P02/P03/P10/P13/P14/P99 ofn==60.0 */

		if (ofn != 60.0)
			return -1;

		if (protocol == 2 || protocol == 3 || protocol == 10 || protocol == 13 || protocol == 14 || protocol == 99)
			return 0;

		break;

	case 63:	/* P01/P09 ofn==60.0 */

		if (ofn != 60.0)
			return -1;

		if (protocol == 1 || protocol == 9)
			return 0;

		break;

	case 64:	/* P02/P03/P10/P13/P14/P99 ofn==60.0 */

		if (ofn != 60.0)
			return -1;

		if (protocol == 2 || protocol == 3 || protocol == 10 || protocol == 13 || protocol == 14 || protocol == 99)
			return 0;

		break;

	case 70:	/* P01/P09 ofn==60.0 */

		if (ofn != 60.0)
			return -1;

		if (protocol == 1 || protocol == 9)
			return 0;

		break;

	default:

		upsdebugx(2, "%s: unknown value (%s)", __func__, value);
		break;

	}

	return -1;
}

/* Enumlist for UPS capabilities that have a NUT var */
static info_rw_t	voltronic_e_cap[] = {
	{ "no", 0 },
	{ "yes", 0 },
	{ "", 0 }
};

/* Enumlist for NONUT capabilities */
static info_rw_t	voltronic_e_cap_nonut[] = {
	{ "enabled", 0 },
	{ "disabled", 0 },
	{ "", 0 }
};


/* == qx2nut lookup table == */
static item_t	voltronic_qx2nut[] = {

	/* Query UPS for protocol
	 * > [QPI\r]
	 * < [(PI00\r]
	 *    012345
	 *    0
	 */

	{ "ups.firmware.aux",		0,	NULL,	"QPI\r",	"",	6,	'(',	"",	1,	4,	"%s",	QX_FLAG_STATIC,	NULL,	NULL,	voltronic_protocol },

	/* Query UPS for ratings
	 * > [QRI\r]
	 * < [(230.0 004 024.0 50.0\r]
	 *    0123456789012345678901
	 *    0         1         2
	 */

	{ "output.voltage.nominal",	0,	NULL,	"QRI\r",	"",	22,	'(',	"",	1,	5,	"%.1f",	QX_FLAG_STATIC,		NULL,	NULL,	NULL },
	{ "output.current.nominal",	0,	NULL,	"QRI\r",	"",	22,	'(',	"",	7,	9,	"%.0f",	QX_FLAG_STATIC,		NULL,	NULL,	NULL },
	{ "battery.voltage.nominal",	0,	NULL,	"QRI\r",	"",	22,	'(',	"",	11,	15,	"%.1f",	QX_FLAG_SEMI_STATIC,	NULL,	NULL,	NULL },	/* as *per battery pack*: the value will change when the number of batteries is changed (battery_number through BATNn) */
	{ "output.frequency.nominal",	0,	NULL,	"QRI\r",	"",	22,	'(',	"",	17,	20,	"%.1f",	QX_FLAG_STATIC,		NULL,	NULL,	NULL },

	/* Query UPS for ratings
	 * > [QMD\r]
	 * < [(#######OLHVT1K0 ###1000 80 1/1 230 230 02 12.0\r]	<- Some UPS may reply with spaces instead of hashes
	 *    012345678901234567890123456789012345678901234567
	 *    0         1         2         3         4
	 */

	{ "device.model",		0,	NULL,	"QMD\r",	"",	48,	'(',	"",	1,	15,	"%s",	QX_FLAG_STATIC | QX_FLAG_TRIM,	NULL,	NULL,	NULL },
	{ "ups.power.nominal",		0,	NULL,	"QMD\r",	"",	48,	'(',	"",	17,	23,	"%s",	QX_FLAG_STATIC | QX_FLAG_TRIM,	NULL,	NULL,	NULL },
	{ "output.powerfactor",		0,	NULL,	"QMD\r",	"",	48,	'(',	"",	25,	26,	"%.1f",	QX_FLAG_STATIC,	NULL,	NULL,	voltronic_output_powerfactor },
	{ "input.phases",		0,	NULL,	"QMD\r",	"",	48,	'(',	"",	28,	28,	"%.0f",	QX_FLAG_STATIC,	NULL,	NULL,	NULL },
	{ "output.phases",		0,	NULL,	"QMD\r",	"",	48,	'(',	"",	30,	30,	"%.0f",	QX_FLAG_STATIC,	NULL,	NULL,	NULL },
	{ "input.voltage.nominal",	0,	NULL,	"QMD\r",	"",	48,	'(',	"",	32,	34,	"%.1f",	QX_FLAG_STATIC,	NULL,	NULL,	NULL },
	{ "output.voltage.nominal",	0,	NULL,	"QMD\r",	"",	48,	'(',	"",	36,	38,	"%.1f",	QX_FLAG_STATIC,	NULL,	NULL,	NULL },	/* redundant with value from QRI */
/*	{ "battery_number",		ST_FLAG_RW,	voltronic_r_batt_numb,	"QMD\r",	"",	48,	'(',	"",	40,	41,	"%d",	QX_FLAG_SEMI_STATIC | QX_FLAG_RANGE | QX_FLAG_NONUT,	NULL,	NULL,	voltronic_batt_numb },	*//* redundant with value from QBV */
/*	{ "battery.voltage.nominal",	0,	NULL,	"QMD\r",	"",	48,	'(',	"",	43,	46,	"%.1f",	QX_FLAG_STATIC,	NULL,	NULL,	NULL },	*//* as *per battery* vs *per pack* reported by QRI */

	/* Query UPS for ratings
	 * > [F\r]
	 * < [#220.0 000 024.0 50.0\r]
	 *    0123456789012345678901
	 *    0         1         2
	 */

	{ "input.voltage.nominal",	0,	NULL,	"F\r",	"",	22,	'#',	"",	1,	5,	"%.1f",	QX_FLAG_STATIC,	NULL,	NULL,	NULL },
	{ "input.current.nominal",	0,	NULL,	"F\r",	"",	22,	'#',	"",	7,	9,	"%.1f",	QX_FLAG_STATIC,	NULL,	NULL,	NULL },
	{ "battery.voltage.nominal",	0,	NULL,	"F\r",	"",	22,	'#',	"",	11,	15,	"%.1f",	QX_FLAG_STATIC,	NULL,	NULL,	NULL },
	{ "input.frequency.nominal",	0,	NULL,	"F\r",	"",	22,	'#',	"",	17,	20,	"%.1f",	QX_FLAG_STATIC,	NULL,	NULL,	NULL },

	/* Query UPS for manufacturer
	 * > [QMF\r]
	 * < [(#######BOH\r]	<- I don't know if it has a fixed length (-> so min length = ( + \r = 2). Hashes may be replaced by spaces
	 *    012345678901
	 *    0         1
	 */

	{ "device.mfr",		0,	NULL,	"QMF\r",	"",	2,	'(',	"",	1,	0,	"%s",	QX_FLAG_STATIC | QX_FLAG_TRIM,	NULL,	NULL,	NULL },

	/* Query UPS for firmware version
	 * > [QVFW\r]
	 * < [(VERFW:00322.02\r]
	 *    0123456789012345
	 *    0         1
	 */

	{ "ups.firmware",	0,	NULL,	"QVFW\r",	"",	16,	'(',	"",	7,	14,	"%s",	QX_FLAG_STATIC,	NULL,	NULL,	NULL },

	/* Query UPS for serial number
	 * > [QID\r]
	 * < [(12345679012345\r]	<- As far as I know it hasn't a fixed length -> min length = ( + \r = 2
	 *    0123456789012345
	 *    0         1
	 */

	{ "device.serial",	0,	NULL,	"QID\r",	"",	2,	'(',	"",	1,	0,	"%s",	QX_FLAG_STATIC,	NULL,	NULL,	voltronic_serial_numb },

	/* Query UPS for vendor infos
	 * > [I\r]
	 * < [#-------------   ------     VT12046Q  \r]
	 *    012345678901234567890123456789012345678
	 *    0         1         2         3
	 */

	{ "device.mfr",		0,	NULL,	"I\r",	"",	39,	'#',	"",	1,	15,	"%s",	QX_FLAG_STATIC | QX_FLAG_TRIM,	NULL,	NULL,	NULL },
	{ "device.model",	0,	NULL,	"I\r",	"",	39,	'#',	"",	17,	26,	"%s",	QX_FLAG_STATIC | QX_FLAG_TRIM,	NULL,	NULL,	NULL },
	{ "ups.firmware",	0,	NULL,	"I\r",	"",	39,	'#',	"",	28,	37,	"%s",	QX_FLAG_STATIC | QX_FLAG_TRIM,	NULL,	NULL,	NULL },

	/* Query UPS for status
	 * > [QGS\r]
	 * < [(234.9 50.0 229.8 50.0 000.0 000 369.1 ---.- 026.5 ---.- 018.8 100000000001\r]
	 *    0123456789012345678901234567890123456789012345678901234567890123456789012345
	 *    0         1         2         3         4         5         6         7
	 */

	{ "input.voltage",	0,	NULL,	"QGS\r",	"",	76,	'(',	"",	1,	5,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "input.frequency",	0,	NULL,	"QGS\r",	"",	76,	'(',	"",	7,	10,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "output.voltage",	0,	NULL,	"QGS\r",	"",	76,	'(',	"",	12,	16,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "output.frequency",	0,	NULL,	"QGS\r",	"",	76,	'(',	"",	18,	21,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "output.current",	0,	NULL,	"QGS\r",	"",	76,	'(',	"",	23,	27,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "ups.load",		0,	NULL,	"QGS\r",	"",	76,	'(',	"",	29,	31,	"%.0f",	0,	NULL,	NULL,	NULL },
/*	{ "unknown.1",		0,	NULL,	"QGS\r",	"",	76,	'(',	"",	33,	37,	"%.1f",	0,	NULL,	NULL,	NULL },	*//* Unknown */
/*	{ "unknown.2",		0,	NULL,	"QGS\r",	"",	76,	'(',	"",	39,	43,	"%.1f",	0,	NULL,	NULL,	NULL },	*//* Unknown */
	{ "battery.voltage",	0,	NULL,	"QGS\r",	"",	76,	'(',	"",	45,	49,	"%.2f",	0,	NULL,	NULL,	NULL },
/*	{ "unknown.3",		0,	NULL,	"QGS\r",	"",	76,	'(',	"",	51,	55,	"%.1f",	0,	NULL,	NULL,	NULL },	*//* Unknown */
	{ "ups.temperature",	0,	NULL,	"QGS\r",	"",	76,	'(',	"",	57,	61,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "ups.type",		0,	NULL,	"QGS\r",	"",	76,	'(',	"",	63,	64,	"%s",	QX_FLAG_SEMI_STATIC,	NULL,	NULL,	voltronic_status },
	{ "ups.status",		0,	NULL,	"QGS\r",	"",	76,	'(',	"",	65,	65,	"%s",	QX_FLAG_QUICK_POLL,	NULL,	NULL,	voltronic_status },	/* Utility Fail (Immediate) */
	{ "ups.status",		0,	NULL,	"QGS\r",	"",	76,	'(',	"",	66,	66,	"%s",	QX_FLAG_QUICK_POLL,	NULL,	NULL,	voltronic_status },	/* Battery Low */
	{ "ups.status",		0,	NULL,	"QGS\r",	"",	76,	'(',	"",	67,	67,	"%s",	QX_FLAG_QUICK_POLL,	NULL,	NULL,	voltronic_status },	/* Bypass/Boost or Buck Active */
	{ "ups.alarm",		0,	NULL,	"QGS\r",	"",	76,	'(',	"",	67,	67,	"%s",	0,			NULL,	NULL,	voltronic_status },	/* Bypass/Boost or Buck Active */
	{ "ups.alarm",		0,	NULL,	"QGS\r",	"",	76,	'(',	"",	68,	68,	"%s",	0,			NULL,	NULL,	voltronic_status },	/* UPS Fault */
/*	{ "unknown.4",		0,	NULL,	"QGS\r",	"",	76,	'(',	"",	69,	69,	"%s",	0,			NULL,	NULL,	voltronic_status },	*//* Unknown */
	{ "ups.status",		0,	NULL,	"QGS\r",	"",	76,	'(',	"",	70,	70,	"%s",	QX_FLAG_QUICK_POLL,	NULL,	NULL,	voltronic_status },	/* Test in Progress */
	{ "ups.status",		0,	NULL,	"QGS\r",	"",	76,	'(',	"",	71,	71,	"%s",	QX_FLAG_QUICK_POLL,	NULL,	NULL,	voltronic_status },	/* Shutdown Active */
	{ "ups.beeper.status",	0,	NULL,	"QGS\r",	"",	76,	'(',	"",	72,	72,	"%s",	0,			NULL,	NULL,	voltronic_status },	/* Beeper status - ups.beeper.status */
/*	{ "unknown.5",		0,	NULL,	"QGS\r",	"",	76,	'(',	"",	73,	73,	"%s",	0,			NULL,	NULL,	voltronic_status },	*//* Unknown */
/*	{ "unknown.6",		0,	NULL,	"QGS\r",	"",	76,	'(',	"",	74,	74,	"%s",	0,			NULL,	NULL,	voltronic_status },	*//* Unknown */

	/* Query UPS for actual working mode
	 * > [QMOD\r]
	 * < [(S\r]
	 *    012
	 *    0
	 */

	{ "ups.alarm",		0,	NULL,	"QMOD\r",	"",	3,	'(',	"",	1,	1,	"%s",	0,	NULL,	NULL,	voltronic_mode },
	{ "ups.status",		0,	NULL,	"QMOD\r",	"",	3,	'(',	"",	1,	1,	"%s",	0,	NULL,	NULL,	voltronic_mode },

	/* Query UPS for faults and their type. Unskipped when a fault is found in 12bit flag of QGS, otherwise you'll get a fake reply.
	 * > [QFS\r]
	 * < [(OK\r] <- No fault
	 *    0123
	 *    0
	 * < [(14 212.1 50.0 005.6 49.9 006 010.6 343.8 ---.- 026.2 021.8 01101100\r] <- Fault type + Short status
	 *    012345678901234567890123456789012345678901234567890123456789012345678
	 *    0         1         2         3         4         5         6
	 */

	{ "ups.alarm",		0,	NULL,	"QFS\r",	"",	4,	'(',	"",	1,	2,	"%s",	QX_FLAG_SKIP,	NULL,	NULL,	voltronic_fault },

	/* Query UPS for warnings and their type
	 * > [QWS\r]
	 * < [(0000000100000000000000000000000000000000000000000000000000000000\r]
	 *    012345678901234567890123456789012345678901234567890123456789012345
	 *    0         1         2         3         4         5         6
	 */

	{ "ups.alarm",		0,	NULL,	"QWS\r",	"",	66,	'(',	"",	1,	64,	"%s",	0,	NULL,	NULL,	voltronic_warning },

	/* Query UPS for actual infos about battery
	 * > [QBV\r]
	 * < [(026.5 02 01 068 255\r]
	 *    012345678901234567890
	 *    0         1         2
	 */

	{ "battery.voltage",	0,		NULL,			"QBV\r",	"",	21,	'(',	"",	1,	5,	"%.2f",	0,	NULL,	NULL,	NULL },
	{ "battery_number",	ST_FLAG_RW,	voltronic_r_batt_numb,	"QBV\r",	"",	21,	'(',	"",	7,	9,	"%d",	QX_FLAG_SEMI_STATIC | QX_FLAG_RANGE | QX_FLAG_NONUT,	NULL,	NULL,	voltronic_batt_numb },	/* Number of batteries that make a pack */
	{ "battery.packs",	ST_FLAG_RW,	voltronic_r_batt_packs,	"QBV\r",	"",	21,	'(',	"",	10,	11,	"%.0f",	QX_FLAG_SEMI_STATIC | QX_FLAG_RANGE,	NULL,	NULL,	NULL },	/* Number of battery packs in parallel */
	{ "battery.charge",	0,		NULL,			"QBV\r",	"",	21,	'(',	"",	13,	15,	"%.0f",	0,	NULL,	NULL,	NULL },
	{ "battery.runtime",	0,		NULL,			"QBV\r",	"",	21,	'(',	"",	17,	19,	"%.0f",	0,	NULL,	NULL,	voltronic_batt_runtime },

	/* Query UPS for last seen min/max load level
	 * > [QLDL\r]
	 * < [(021 023\r]	<- minimum load level - maximum load level
	 *    012345678
	 *    0
	 */

	{ "output.power.minimum.percent",	0,	NULL,	"QLDL\r",	"",	9,	'(',	"",	1,	3,	"%.0f",	0,	NULL,	NULL,	NULL },
	{ "output.power.maximum.percent",	0,	NULL,	"QLDL\r",	"",	9,	'(',	"",	5,	7,	"%.0f",	0,	NULL,	NULL,	NULL },

	/* Query UPS for multi-phase voltages/frequencies
	 * > [Q3**\r]
	 * < [(123.4 123.4 123.4 123.4 123.4 123.4\r]	<- Q3PV
	 * < [(123.4 123.4 123.4 123.4 123.4 123.4\r]	<- Q3OV
	 * < [(123 123 123\r]	<- Q3PC
	 * < [(123 123 123\r]	<- Q3OC
	 * < [(123 123 123\r]	<- Q3LD
	 * < [(123.4 123.4 123.4\r]	<- Q3YV - P09 protocol
	 * < [(123.4 123.4 123.4 123.4 123.4 123.4\r]	<- Q3YV - P10/P03 protocols
	 *    0123456789012345678901234567890123456
	 *    0         1         2         3
	 *
	 * P09 = 2-phase input/2-phase output
	 * Q3PV	(Input Voltage L1 | Input Voltage L2 | Input Voltage L3 | Input Voltage L1-L2 | Input Voltage L1-L3 | Input Voltage L2-L3
	 * Q3OV	(Output Voltage L1 | Output Voltage L2 | Output Voltage L3 | Output Voltage L1-L2 | Output Voltage L1-L3 | Output Voltage L2-L3
	 * Q3PC	(Input Current L1 | Input Current L2 | Input Current L3
	 * Q3OC	(Output Current L1 | Output Current L2 | Output Current L3
	 * Q3LD	(Output Load Level L1 | Output Load Level L2 | Output Load Level L3
	 * Q3YV	(Output Bypass Voltage L1 | Output Bypass Voltage L2 | Output Bypass Voltage L3
	 *
	 * P10 = 3-phase input/3-phase output / P03 = 3-phase input/ 1-phase output
	 * Q3PV	(Input Voltage L1 | Input Voltage L2 | Input Voltage L3 | Input Voltage L1-L2 | Input Voltage L2-L3 | Input Voltage L1-L3
	 * Q3OV	(Output Voltage L1 | Output Voltage L2 | Output Voltage L3 | Output Voltage L1-L2 | Output Voltage L2-L3 | Output Voltage L1-L3
	 * Q3PC	(Input Current L1 | Input Current L2 | Input Current L3
	 * Q3OC	(Output Current L1 | Output Current L2 | Output Current L3
	 * Q3LD	(Output Load Level L1 | Output Load Level L2 | Output Load Level L3
	 * Q3YV	(Output Bypass Voltage L1 | Output Bypass Voltage L2 | Output Bypass Voltage L3 | Output Bypass Voltage L1-L2 | Output Bypass Voltage L2-L3 | Output Bypass Voltage L1-L3
	 *
	 */

	/*	From Q3PV	*/
	{ "input.L1-N.voltage",			0,	NULL,	"Q3PV\r",	"",	37,	'(',	"",	1,	5,	"%.1f",	QX_FLAG_SKIP,	NULL,	NULL,	NULL },
	{ "input.L2-N.voltage",			0,	NULL,	"Q3PV\r",	"",	37,	'(',	"",	7,	11,	"%.1f",	QX_FLAG_SKIP,	NULL,	NULL,	NULL },
	{ "input.L3-N.voltage",			0,	NULL,	"Q3PV\r",	"",	37,	'(',	"",	13,	17,	"%.1f",	QX_FLAG_SKIP,	NULL,	NULL,	NULL },
	{ "input.L1-L2.voltage",		0,	NULL,	"Q3PV\r",	"",	37,	'(',	"",	19,	23,	"%.1f",	QX_FLAG_SKIP,	NULL,	NULL,	NULL },
	{ "input.L2-L3.voltage",		0,	NULL,	"Q3PV\r",	"",	37,	'(',	"",	25,	29,	"%.1f",	QX_FLAG_SKIP,	NULL,	NULL,	NULL },
	{ "input.L1-L3.voltage",		0,	NULL,	"Q3PV\r",	"",	37,	'(',	"",	31,	35,	"%.1f",	QX_FLAG_SKIP,	NULL,	NULL,	NULL },
/*	{ "input.L1-L3.voltage",		0,	NULL,	"Q3PV\r",	"",	37,	'(',	"",	25,	29,	"%.1f",	QX_FLAG_SKIP,	NULL,	NULL,	NULL },	*//* P09 *//* Commented out because P09 should be two-phase input/output UPSes */
/*	{ "input.L2-L3.voltage",		0,	NULL,	"Q3PV\r",	"",	37,	'(',	"",	31,	35,	"%.1f",	QX_FLAG_SKIP,	NULL,	NULL,	NULL },	*//* P09 *//* Commented out because P09 should be two-phase input/output UPSes */

	/*	From Q3PC	*/
	{ "input.L1.current",			0,	NULL,	"Q3PC\r",	"",	13,	'(',	"",	1,	3,	"%.0f",	QX_FLAG_SKIP,	NULL,	NULL,	NULL },
	{ "input.L2.current",			0,	NULL,	"Q3PC\r",	"",	13,	'(',	"",	5,	7,	"%.0f",	QX_FLAG_SKIP,	NULL,	NULL,	NULL },
	{ "input.L3.current",			0,	NULL,	"Q3PC\r",	"",	13,	'(',	"",	9,	11,	"%.0f",	QX_FLAG_SKIP,	NULL,	NULL,	NULL },

	/*	From Q3OV	*/
	{ "output.L1-N.voltage",		0,	NULL,	"Q3OV\r",	"",	37,	'(',	"",	1,	5,	"%.1f",	QX_FLAG_SKIP,	NULL,	NULL,	NULL },
	{ "output.L2-N.voltage",		0,	NULL,	"Q3OV\r",	"",	37,	'(',	"",	7,	11,	"%.1f",	QX_FLAG_SKIP,	NULL,	NULL,	NULL },
	{ "output.L3-N.voltage",		0,	NULL,	"Q3OV\r",	"",	37,	'(',	"",	13,	17,	"%.1f",	QX_FLAG_SKIP,	NULL,	NULL,	NULL },
	{ "output.L1-L2.voltage",		0,	NULL,	"Q3OV\r",	"",	37,	'(',	"",	19,	23,	"%.1f",	QX_FLAG_SKIP,	NULL,	NULL,	NULL },
	{ "output.L2-L3.voltage",		0,	NULL,	"Q3OV\r",	"",	37,	'(',	"",	25,	29,	"%.1f",	QX_FLAG_SKIP,	NULL,	NULL,	NULL },
	{ "output.L1-L3.voltage",		0,	NULL,	"Q3OV\r",	"",	37,	'(',	"",	31,	35,	"%.1f",	QX_FLAG_SKIP,	NULL,	NULL,	NULL },
/*	{ "output.L1-L3.voltage",		0,	NULL,	"Q3OV\r",	"",	37,	'(',	"",	25,	29,	"%.1f",	QX_FLAG_SKIP,	NULL,	NULL,	NULL },	*//* P09 *//* Commented out because P09 should be two-phase input/output UPSes */
/*	{ "output.L2-L3.voltage",		0,	NULL,	"Q3OV\r",	"",	37,	'(',	"",	31,	35,	"%.1f",	QX_FLAG_SKIP,	NULL,	NULL,	NULL },	*//* P09 *//* Commented out because P09 should be two-phase input/output UPSes */

	/*	From Q3OC	*/
	{ "output.L1.current",			0,	NULL,	"Q3OC\r",	"",	13,	'(',	"",	1,	3,	"%.0f",	QX_FLAG_SKIP,	NULL,	NULL,	NULL },
	{ "output.L2.current",			0,	NULL,	"Q3OC\r",	"",	13,	'(',	"",	5,	7,	"%.0f",	QX_FLAG_SKIP,	NULL,	NULL,	NULL },
	{ "output.L3.current",			0,	NULL,	"Q3OC\r",	"",	13,	'(',	"",	9,	11,	"%.0f",	QX_FLAG_SKIP,	NULL,	NULL,	NULL },

	/*	From Q3LD	*/
	{ "output.L1.power.percent",		0,	NULL,	"Q3LD\r",	"",	13,	'(',	"",	1,	3,	"%.0f",	QX_FLAG_SKIP,	NULL,	NULL,	NULL },
	{ "output.L2.power.percent",		0,	NULL,	"Q3LD\r",	"",	13,	'(',	"",	5,	7,	"%.0f",	QX_FLAG_SKIP,	NULL,	NULL,	NULL },
	{ "output.L3.power.percent",		0,	NULL,	"Q3LD\r",	"",	13,	'(',	"",	9,	11,	"%.0f",	QX_FLAG_SKIP,	NULL,	NULL,	NULL },

	/*	From Q3YV	*/
	{ "output.bypass.L1-N.voltage",		0,	NULL,	"Q3YV\r",	"",	37,	'(',	"",	1,	5,	"%.1f",	QX_FLAG_SKIP,	NULL,	NULL,	NULL },
	{ "output.bypass.L2-N.voltage",		0,	NULL,	"Q3YV\r",	"",	37,	'(',	"",	7,	11,	"%.1f",	QX_FLAG_SKIP,	NULL,	NULL,	NULL },
	{ "output.bypass.L3-N.voltage",		0,	NULL,	"Q3YV\r",	"",	37,	'(',	"",	13,	17,	"%.1f",	QX_FLAG_SKIP,	NULL,	NULL,	NULL },
	{ "output.bypass.L1-N.voltage",		0,	NULL,	"Q3YV\r",	"",	19,	'(',	"",	1,	5,	"%.1f",	QX_FLAG_SKIP,	NULL,	NULL,	NULL },	/* P09 */
	{ "output.bypass.L2-N.voltage",		0,	NULL,	"Q3YV\r",	"",	19,	'(',	"",	7,	11,	"%.1f",	QX_FLAG_SKIP,	NULL,	NULL,	NULL },	/* P09 */
/*	{ "output.bypass.L3-N.voltage",		0,	NULL,	"Q3YV\r",	"",	19,	'(',	"",	13,	17,	"%.1f",	QX_FLAG_SKIP,	NULL,	NULL,	NULL },	*//* P09 *//* Commented out because P09 should be two-phase input/output UPSes */
	{ "output.bypass.L1-L2.voltage",	0,	NULL,	"Q3YV\r",	"",	37,	'(',	"",	19,	23,	"%.1f",	QX_FLAG_SKIP,	NULL,	NULL,	NULL },
	{ "output.bypass.L2-L3.voltage",	0,	NULL,	"Q3YV\r",	"",	37,	'(',	"",	25,	29,	"%.1f",	QX_FLAG_SKIP,	NULL,	NULL,	NULL },
	{ "output.bypass.L1-L3.voltage",	0,	NULL,	"Q3YV\r",	"",	37,	'(',	"",	31,	35,	"%.1f",	QX_FLAG_SKIP,	NULL,	NULL,	NULL },

	/* Query UPS for capability - total options available: 23; only those whom the UPS is capable of are reported as Enabled or Disabled
	 * > [QFLAG\r]
	 * < [(EpashcDbroegfl\r]
	 *    0123456789012345
	 *    0         1	* min length = ( + E + D + \r = 4
	 */

	{ "ups.start.auto",		ST_FLAG_RW,	voltronic_e_cap,	"QFLAG\r",	"",	4,	'(',	"",	1,	0,	"%s",	QX_FLAG_SEMI_STATIC | QX_FLAG_ENUM,	NULL,	NULL,	voltronic_capability },
	{ "battery.protection",		ST_FLAG_RW,	voltronic_e_cap,	"QFLAG\r",	"",	4,	'(',	"",	1,	0,	"%s",	QX_FLAG_SEMI_STATIC | QX_FLAG_ENUM,	NULL,	NULL,	voltronic_capability },
	{ "battery.energysave",		ST_FLAG_RW,	voltronic_e_cap,	"QFLAG\r",	"",	4,	'(',	"",	1,	0,	"%s",	QX_FLAG_SEMI_STATIC | QX_FLAG_ENUM,	NULL,	NULL,	voltronic_capability },
	{ "ups.start.battery",		ST_FLAG_RW,	voltronic_e_cap,	"QFLAG\r",	"",	4,	'(',	"",	1,	0,	"%s",	QX_FLAG_SEMI_STATIC | QX_FLAG_ENUM,	NULL,	NULL,	voltronic_capability },
	{ "outlet.0.switchable",	ST_FLAG_RW,	voltronic_e_cap,	"QFLAG\r",	"",	4,	'(',	"",	1,	0,	"%s",	QX_FLAG_SEMI_STATIC | QX_FLAG_ENUM,	NULL,	NULL,	voltronic_capability },
	/* Not available in NUT */
	{ "bypass_alarm",		0,	NULL,	"QFLAG\r",	"",	4,	'(',	"",	1,	0,	"%s",	QX_FLAG_SEMI_STATIC | QX_FLAG_NONUT,	NULL,	NULL,	voltronic_capability },
	{ "battery_alarm",		0,	NULL,	"QFLAG\r",	"",	4,	'(',	"",	1,	0,	"%s",	QX_FLAG_SEMI_STATIC | QX_FLAG_NONUT,	NULL,	NULL,	voltronic_capability },
	{ "bypass_when_off",		0,	NULL,	"QFLAG\r",	"",	4,	'(',	"",	1,	0,	"%s",	QX_FLAG_SEMI_STATIC | QX_FLAG_NONUT,	NULL,	NULL,	voltronic_capability },
	{ "alarm_control",		0,	NULL,	"QFLAG\r",	"",	4,	'(',	"",	1,	0,	"%s",	QX_FLAG_SEMI_STATIC | QX_FLAG_NONUT,	NULL,	NULL,	voltronic_capability },
	{ "converter_mode",		0,	NULL,	"QFLAG\r",	"",	4,	'(',	"",	1,	0,	"%s",	QX_FLAG_SEMI_STATIC | QX_FLAG_NONUT,	NULL,	NULL,	voltronic_capability },
	{ "eco_mode",			0,	NULL,	"QFLAG\r",	"",	4,	'(',	"",	1,	0,	"%s",	QX_FLAG_SEMI_STATIC | QX_FLAG_NONUT,	NULL,	NULL,	voltronic_capability },
	{ "battery_open_status_check",	0,	NULL,	"QFLAG\r",	"",	4,	'(',	"",	1,	0,	"%s",	QX_FLAG_SEMI_STATIC | QX_FLAG_NONUT,	NULL,	NULL,	voltronic_capability },
	{ "bypass_forbidding",		0,	NULL,	"QFLAG\r",	"",	4,	'(',	"",	1,	0,	"%s",	QX_FLAG_SEMI_STATIC | QX_FLAG_NONUT,	NULL,	NULL,	voltronic_capability },
	{ "site_fault_detection",	0,	NULL,	"QFLAG\r",	"",	4,	'(',	"",	1,	0,	"%s",	QX_FLAG_SEMI_STATIC | QX_FLAG_NONUT,	NULL,	NULL,	voltronic_capability },
	{ "advanced_eco_mode",		0,	NULL,	"QFLAG\r",	"",	4,	'(',	"",	1,	0,	"%s",	QX_FLAG_SEMI_STATIC | QX_FLAG_NONUT,	NULL,	NULL,	voltronic_capability },
	{ "constant_phase_angle",	0,	NULL,	"QFLAG\r",	"",	4,	'(',	"",	1,	0,	"%s",	QX_FLAG_SEMI_STATIC | QX_FLAG_NONUT,	NULL,	NULL,	voltronic_capability },
	{ "limited_runtime_on_battery",	0,	NULL,	"QFLAG\r",	"",	4,	'(',	"",	1,	0,	"%s",	QX_FLAG_SEMI_STATIC | QX_FLAG_NONUT,	NULL,	NULL,	voltronic_capability },

	/*   Enable	or	  Disable	or	Reset to safe default values	capability options
	 * > [PEX\r]		> [PDX\r]		> [PF\r]
	 * < [(ACK\r]		< [(ACK\r]		< [(ACK\r]
	 *    01234		   01234		   01234
	 *    0			   0			   0
	 */

	{ "ups.start.auto",		0,	voltronic_e_cap,	"P%sR\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_SETVAR | QX_FLAG_ENUM | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_capability_set },
	{ "battery.protection",		0,	voltronic_e_cap,	"P%sS\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_SETVAR | QX_FLAG_ENUM | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_capability_set },
	{ "battery.energysave",		0,	voltronic_e_cap,	"P%sG\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_SETVAR | QX_FLAG_ENUM | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_capability_set },
	{ "ups.start.battery",		0,	voltronic_e_cap,	"P%sC\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_SETVAR | QX_FLAG_ENUM | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_capability_set },
	{ "outlet.0.switchable",	0,	voltronic_e_cap,	"P%sJ\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_SETVAR | QX_FLAG_ENUM | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_capability_set },
	/* Not available in NUT */
	{ "reset_to_default",		0,	NULL,			"PF\r",		"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_SETVAR | QX_FLAG_NONUT | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_capability_reset },
	{ "bypass_alarm",		0,	voltronic_e_cap_nonut,	"P%sP\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_SETVAR | QX_FLAG_ENUM | QX_FLAG_NONUT | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_capability_set_nonut },
	{ "battery_alarm",		0,	voltronic_e_cap_nonut,	"P%sB\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_SETVAR | QX_FLAG_ENUM | QX_FLAG_NONUT | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_capability_set_nonut },
	{ "bypass_when_off",		0,	voltronic_e_cap_nonut,	"P%sO\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_SETVAR | QX_FLAG_ENUM | QX_FLAG_NONUT | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_capability_set_nonut },
	{ "alarm_control",		0,	voltronic_e_cap_nonut,	"P%sA\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_SETVAR | QX_FLAG_ENUM | QX_FLAG_NONUT | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_capability_set_nonut },
	{ "converter_mode",		0,	voltronic_e_cap_nonut,	"P%sV\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_SETVAR | QX_FLAG_ENUM | QX_FLAG_NONUT | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_capability_set_nonut },
	{ "eco_mode",			0,	voltronic_e_cap_nonut,	"P%sE\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_SETVAR | QX_FLAG_ENUM | QX_FLAG_NONUT | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_capability_set_nonut },
	{ "battery_open_status_check",	0,	voltronic_e_cap_nonut,	"P%sD\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_SETVAR | QX_FLAG_ENUM | QX_FLAG_NONUT | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_capability_set_nonut },
	{ "bypass_forbidding",		0,	voltronic_e_cap_nonut,	"P%sF\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_SETVAR | QX_FLAG_ENUM | QX_FLAG_NONUT | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_capability_set_nonut },
	{ "site_fault_detection",	0,	voltronic_e_cap_nonut,	"P%sL\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_SETVAR | QX_FLAG_ENUM | QX_FLAG_NONUT | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_capability_set_nonut },
	{ "advanced_eco_mode",		0,	voltronic_e_cap_nonut,	"P%sN\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_SETVAR | QX_FLAG_ENUM | QX_FLAG_NONUT | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_capability_set_nonut },
	{ "constant_phase_angle",	0,	voltronic_e_cap_nonut,	"P%sQ\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_SETVAR | QX_FLAG_ENUM | QX_FLAG_NONUT | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_capability_set_nonut },
	{ "limited_runtime_on_battery",	0,	voltronic_e_cap_nonut,	"P%sW\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_SETVAR | QX_FLAG_ENUM | QX_FLAG_NONUT | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_capability_set_nonut },

	/* Query UPS for programmable outlet (1-4) status
	 * > [QSK1\r]
	 * < [(1\r]	<- if outlet is on -> (1 , if off -> (0 ; (NAK -> outlet is not programmable
	 *    012
	 *    0
	 */

	{ "outlet.1.switchable",	0,	NULL,	"QSK1\r",	"",	3,	'(',	"",	1,	1,	"%s",	QX_FLAG_SEMI_STATIC | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_outlet },
	{ "outlet.1.status",		0,	NULL,	"QSK1\r",	"",	3,	'(',	"",	1,	1,	"%s",	QX_FLAG_SEMI_STATIC | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_outlet },
	{ "outlet.2.switchable",	0,	NULL,	"QSK2\r",	"",	3,	'(',	"",	1,	1,	"%s",	QX_FLAG_SEMI_STATIC | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_outlet },
	{ "outlet.2.status",		0,	NULL,	"QSK2\r",	"",	3,	'(',	"",	1,	1,	"%s",	QX_FLAG_SEMI_STATIC | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_outlet },
	{ "outlet.3.switchable",	0,	NULL,	"QSK3\r",	"",	3,	'(',	"",	1,	1,	"%s",	QX_FLAG_SEMI_STATIC | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_outlet },
	{ "outlet.3.status",		0,	NULL,	"QSK3\r",	"",	3,	'(',	"",	1,	1,	"%s",	QX_FLAG_SEMI_STATIC | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_outlet },
	{ "outlet.4.switchable",	0,	NULL,	"QSK4\r",	"",	3,	'(',	"",	1,	1,	"%s",	QX_FLAG_SEMI_STATIC | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_outlet },
	{ "outlet.4.status",		0,	NULL,	"QSK4\r",	"",	3,	'(',	"",	1,	1,	"%s",	QX_FLAG_SEMI_STATIC | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_outlet },

	/* Query UPS for programmable outlet n (1-4) delay time before it shuts down the load when on battery mode
	 * > [QSKT1\r]
	 * < [(008\r]	<- if delay time is set (by PSK[1-4]n) it'll report n (minutes) otherwise it'll report (NAK (also if outlet is not programmable)
	 *    01234
	 *    0
	 */

	{ "outlet.1.delay.shutdown",	ST_FLAG_RW,	voltronic_r_outlet_delay,	"QSKT1\r",	"",	5,	'(',	"",	1,	3,	"%.0f",	QX_FLAG_SEMI_STATIC | QX_FLAG_RANGE | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_outlet_delay },
	{ "outlet.2.delay.shutdown",	ST_FLAG_RW,	voltronic_r_outlet_delay,	"QSKT2\r",	"",	5,	'(',	"",	1,	3,	"%.0f",	QX_FLAG_SEMI_STATIC | QX_FLAG_RANGE | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_outlet_delay },
	{ "outlet.3.delay.shutdown",	ST_FLAG_RW,	voltronic_r_outlet_delay,	"QSKT3\r",	"",	5,	'(',	"",	1,	3,	"%.0f",	QX_FLAG_SEMI_STATIC | QX_FLAG_RANGE | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_outlet_delay },
	{ "outlet.4.delay.shutdown",	ST_FLAG_RW,	voltronic_r_outlet_delay,	"QSKT4\r",	"",	5,	'(',	"",	1,	3,	"%.0f",	QX_FLAG_SEMI_STATIC | QX_FLAG_RANGE | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_outlet_delay },

	/* Set delay time for programmable outlets
	 * > [PSK1nnn\r]	n = 0..9
	 * < [(ACK\r]
	 *    01234
	 *    0
	 */

	{ "outlet.1.delay.shutdown",	0,	voltronic_r_outlet_delay,	"PSK1%03d\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_SETVAR | QX_FLAG_RANGE | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_outlet_delay_set },
	{ "outlet.2.delay.shutdown",	0,	voltronic_r_outlet_delay,	"PSK2%03d\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_SETVAR | QX_FLAG_RANGE | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_outlet_delay_set },
	{ "outlet.3.delay.shutdown",	0,	voltronic_r_outlet_delay,	"PSK3%03d\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_SETVAR | QX_FLAG_RANGE | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_outlet_delay_set },
	{ "outlet.4.delay.shutdown",	0,	voltronic_r_outlet_delay,	"PSK4%03d\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_SETVAR | QX_FLAG_RANGE | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_outlet_delay_set },

	/* Query UPS for ECO Mode voltage limits
	 * > [QHE\r]
	 * < [(242 218\r]
	 *    012345678
	 *    0
	 */

	{ "input.transfer.high",	ST_FLAG_RW,	voltronic_r_eco_volt_max,	"QHE\r",	"",	9,	'(',	"",	1,	3,	"%.0f",	QX_FLAG_SEMI_STATIC | QX_FLAG_RANGE | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_eco_volt },
	{ "input.transfer.low",		ST_FLAG_RW,	voltronic_r_eco_volt_min,	"QHE\r",	"",	9,	'(',	"",	5,	7,	"%.0f",	QX_FLAG_SEMI_STATIC | QX_FLAG_RANGE | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_eco_volt },
	{ "input.transfer.low.min",	0,		NULL,				"QHE\r",	"",	9,	'(',	"",	5,	7,	"%.0f",	QX_FLAG_SEMI_STATIC | QX_FLAG_SKIP,			NULL,	NULL,	voltronic_eco_volt_range },
	{ "input.transfer.low.max",	0,		NULL,				"QHE\r",	"",	9,	'(',	"",	5,	7,	"%.0f",	QX_FLAG_SEMI_STATIC | QX_FLAG_SKIP,			NULL,	NULL,	voltronic_eco_volt_range },
	{ "input.transfer.high.min",	0,		NULL,				"QHE\r",	"",	9,	'(',	"",	1,	3,	"%.0f",	QX_FLAG_SEMI_STATIC | QX_FLAG_SKIP,			NULL,	NULL,	voltronic_eco_volt_range },
	{ "input.transfer.high.max",	0,		NULL,				"QHE\r",	"",	9,	'(',	"",	1,	3,	"%.0f",	QX_FLAG_SEMI_STATIC | QX_FLAG_SKIP,			NULL,	NULL,	voltronic_eco_volt_range },

	/* Set ECO Mode voltage limits
	 * > [HEHnnn\r]		> [HELnnn\r]		n = 0..9
	 * < [(ACK\r]		< [(ACK\r]
	 *    01234		   01234
	 *    0			   0
	 */

	{ "input.transfer.high",	0,	voltronic_r_eco_volt_max,	"HEH%03.0f\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_SETVAR | QX_FLAG_RANGE | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_process_setvar },
	{ "input.transfer.low",		0,	voltronic_r_eco_volt_min,	"HEL%03.0f\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_SETVAR | QX_FLAG_RANGE | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_process_setvar },

	/* Query UPS for ECO Mode frequency limits
	 * > [QFRE\r]
	 * < [(53.0 47.0\r]
	 *    01234567890
	 *    0         1
	 */

	{ "input.frequency.high",	ST_FLAG_RW,	voltronic_r_eco_freq_max,	"QFRE\r",	"",	11,	'(',	"",	1,	4,	"%.1f",	QX_FLAG_SEMI_STATIC | QX_FLAG_RANGE | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_eco_freq },
	{ "input.frequency.low",	ST_FLAG_RW,	voltronic_r_eco_freq_min,	"QFRE\r",	"",	11,	'(',	"",	6,	9,	"%.1f",	QX_FLAG_SEMI_STATIC | QX_FLAG_RANGE | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_eco_freq },

	/* Set ECO Mode frequency limits
	 * > [FREHnn.n\r]	> [FRELnn.n\r]		n = 0..9
	 * < [(ACK\r]		< [(ACK\r]
	 *    01234		   01234
	 *    0			   0
	 */

	{ "input.frequency.high",	0,	voltronic_r_eco_freq_max,	"FREH%04.1f\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_SETVAR | QX_FLAG_RANGE | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_process_setvar },
	{ "input.frequency.low",	0,	voltronic_r_eco_freq_min,	"FREL%04.1f\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_SETVAR | QX_FLAG_RANGE | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_process_setvar },

	/* Query UPS for Bypass Mode voltage limits
	 * > [QBYV\r]
	 * < [(264 170\r]
	 *    012345678
	 *    0
	 */

	{ "max_bypass_volt",	ST_FLAG_RW,	voltronic_r_bypass_volt_max,	"QBYV\r",	"",	9,	'(',	"",	1,	3,	"%.0f",	QX_FLAG_SEMI_STATIC | QX_FLAG_RANGE | QX_FLAG_NONUT | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_bypass },
	{ "min_bypass_volt",	ST_FLAG_RW,	voltronic_r_bypass_volt_min,	"QBYV\r",	"",	9,	'(',	"",	5,	7,	"%.0f",	QX_FLAG_SEMI_STATIC | QX_FLAG_RANGE | QX_FLAG_NONUT | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_bypass },

	/* Set Bypass Mode voltage limits
	 * > [PHVnnn\r]		> [PLVnnn\r]		n = 0..9
	 * < [(ACK\r]		< [(ACK\r]
	 *    01234		   01234
	 *    0			   0
	 */

	{ "max_bypass_volt",	0,	voltronic_r_bypass_volt_max,	"PHV%03.0f\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_SETVAR | QX_FLAG_RANGE | QX_FLAG_NONUT | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_process_setvar },
	{ "min_bypass_volt",	0,	voltronic_r_bypass_volt_min,	"PLV%03.0f\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_SETVAR | QX_FLAG_RANGE | QX_FLAG_NONUT | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_process_setvar },

	/* Query UPS for Bypass Mode frequency limits
	 * > [QBYF\r]
	 * < [(53.0 47.0\r]
	 *    01234567890
	 *    0         1
	 */

	{ "max_bypass_freq",	ST_FLAG_RW,	voltronic_r_bypass_freq_max,	"QBYF\r",	"",	11,	'(',	"",	1,	4,	"%.1f",	QX_FLAG_SEMI_STATIC | QX_FLAG_RANGE | QX_FLAG_NONUT | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_bypass },
	{ "min_bypass_freq",	ST_FLAG_RW,	voltronic_r_bypass_freq_min,	"QBYF\r",	"",	11,	'(',	"",	6,	9,	"%.1f",	QX_FLAG_SEMI_STATIC | QX_FLAG_RANGE | QX_FLAG_NONUT | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_bypass },

	/* Set Bypass Mode frequency limits
	 * > [PGFnn.n\r]	> [PSFnn.n\r]		n = 0..9
	 * < [(ACK\r]		< [(ACK\r]
	 *    01234		   01234
	 *    0			   0
	 */

	{ "max_bypass_freq",	0,	voltronic_r_bypass_freq_max,	"PGF%04.1f\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_SETVAR | QX_FLAG_RANGE | QX_FLAG_NONUT | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_process_setvar },
	{ "min_bypass_freq",	0,	voltronic_r_bypass_freq_min,	"PSF%04.1f\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_SETVAR | QX_FLAG_RANGE | QX_FLAG_NONUT | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_process_setvar },

	/* Set number of batteries that make a pack to n (integer, 1-9). NOTE: changing the number of batteries will change the UPS's estimation on battery charge/runtime
	 * > [BATNn\r]
	 * < [(ACK\r]
	 *    01234
	 *    0
	 */

	{ "battery_number",	0,	voltronic_r_batt_numb,	"BATN%1.0f\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_SETVAR | QX_FLAG_RANGE | QX_FLAG_NONUT,	NULL,	NULL,	voltronic_process_setvar },

	/* Set number of battery packs in parallel to n (integer, 01-99). NOTE: changing the number of battery packs will change the UPS's estimation on battery charge/runtime
	 * > [BATGNn\r]
	 * < [(ACK\r]
	 *    01234
	 *    0
	 */

	{ "battery.packs",	0,	voltronic_r_batt_packs,	"BATGN%02.0f\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_SETVAR | QX_FLAG_RANGE,	NULL,	NULL,	voltronic_process_setvar },

	/* Query UPS for battery type (Only P31)
	 * > [QBT\r]
	 * < [(01\r]	<- 00="Li", 01="Flooded" or 02="AGM"
	 *    0123
	 *    0
	 */

	{ "battery.type",	ST_FLAG_RW,	voltronic_e_batt_type,	"QBT\r",	"",	4,	'(',	"",	1,	2,	"%s",	QX_FLAG_SEMI_STATIC | QX_FLAG_ENUM | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_p31b },

	/* Set battery type (Only P31)
	 * > [PBTnn\r]		nn = 00/01/02
	 * < [(ACK\r]
	 *    01234
	 *    0
	 */

	{ "battery.type",	0,	voltronic_e_batt_type,	"PBT%02.0f\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_SETVAR | QX_FLAG_ENUM | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_p31b_set },

	/* Query UPS for device grid working range (Only P31)
	 * > [QGR\r]
	 * < [(01\r]	<- 00=Appliance, 01=UPS
	 *    0123
	 *    0
	 */

	{ "work_range_type",	ST_FLAG_RW,	voltronic_e_work_range,	"QGR\r",	"",	4,	'(',	"",	1,	2,	"%s",	QX_FLAG_SEMI_STATIC | QX_FLAG_ENUM | QX_FLAG_NONUT | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_p31g },

	/* Set device grid working range type (Only P31)
	 * > [PBTnn\r]		nn = 00/01
	 * < [(ACK\r]
	 *    01234
	 *    0
	 */

	{ "work_range_type",	0,	voltronic_e_work_range,	"PGR%02.0f\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_SETVAR | QX_FLAG_ENUM | QX_FLAG_NONUT | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_p31g_set },

	/* Query UPS for battery low voltage
	 * > [RE0\r]
	 * < [#20\r]
	 *    012
	 *    0
	 */

	{ "battery.voltage.low",	ST_FLAG_RW,	voltronic_r_batt_low,	"RE0\r",	"",	3,	'#',	"",	1,	2,	"%.1f",	QX_FLAG_SEMI_STATIC | QX_FLAG_RANGE,	NULL,	NULL,	NULL },

	/* Set voltage for battery low to n (integer, 20..24/20..28). NOTE: changing the battery low voltage will change the UPS's estimation on battery charge/runtime
	 * > [W0En\r]
	 * < [(ACK\r]
	 *    01234
	 *    0
	 */

	{ "battery.voltage.low",	0,	voltronic_r_batt_low,	"W0E%02.0f\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_SETVAR | QX_FLAG_RANGE,	NULL,	NULL,	voltronic_process_setvar },

	/* Query UPS for Phase Angle
	 * > [QPD\r]
	 * < [(000 120\r]	<- Input Phase Angle - Output Phase Angle
	 *    012345678
	 *    0
	 */

	{ "input_phase_angle",		0,		NULL,			"QPD\r",	"",	9,	'(',	"",	1,	3,	"%03d",	QX_FLAG_SEMI_STATIC | QX_FLAG_NONUT,			NULL,	NULL,	voltronic_phase },
	{ "output_phase_angle",		ST_FLAG_RW,	voltronic_e_phase,	"QPD\r",	"",	9,	'(',	"",	5,	7,	"%03d",	QX_FLAG_SEMI_STATIC | QX_FLAG_ENUM | QX_FLAG_NONUT,	NULL,	NULL,	voltronic_phase },

	/* Set output phase angle
	 * > [PPDn\r]		n = (000, 120, 180 or 240)
	 * < [(ACK\r]
	 *    01234
	 *    0
	 */

	{ "output_phase_angle",		0,	voltronic_e_phase,	"PPD%03.0f\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_SETVAR | QX_FLAG_ENUM | QX_FLAG_NONUT | QX_FLAG_SKIP,	NULL,	NULL,	voltronic_phase_set },

	/* Query UPS for master/slave for a system of UPSes in parallel
	 * > [QPAR\r]
	 * < [(001\r]	<- 001 for master UPS, 002 and 003 for slave UPSes
	 *    01234
	 *    0
	 */

	{ "voltronic_parallel",		0,	NULL,	"QPAR\r",	"",	5,	'(',	"",	1,	3,	"%s",	QX_FLAG_STATIC | QX_FLAG_NONUT,	NULL,	NULL,	voltronic_parallel },

	/* Query UPS for ??
	 * > [QBDR\r]
	 * < [(1234\r]	<- unknown reply - My UPS (NAK at me
	 *    012345
	 *    0
	 */

	{ "unknown.7",		0,	NULL,	"QBDR\r",	"",	5,	'(',	"",	1,	0,	"%s",	QX_FLAG_STATIC | QX_FLAG_NONUT | QX_FLAG_SKIP,	NULL,	NULL,	NULL },

	/* Instant commands */
	{ "load.off",			0,	NULL,	"SOFF\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },
	{ "load.on",			0,	NULL,	"SON\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },

	{ "shutdown.return",		0,	NULL,	"S%s\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	voltronic_process_command },
	{ "shutdown.stayoff",		0,	NULL,	"S%sR0000\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	voltronic_process_command },
	{ "shutdown.stop",		0,	NULL,	"CS\r",		"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },

	{ "test.battery.start",		0,	NULL,	"T%s\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	voltronic_process_command },
	{ "test.battery.start.deep",	0,	NULL,	"TL\r",		"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },
	{ "test.battery.start.quick",	0,	NULL,	"T\r",		"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },
	{ "test.battery.stop",		0,	NULL,	"CT\r",		"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },

	{ "beeper.toggle",		0,	NULL,	"BZ%s\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	voltronic_process_command },
	/* Enable/disable beeper: unskipped if the UPS can control alarm (capability) */
	{ "beeper.enable",		0,	NULL,	"PEA\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_CMD | QX_FLAG_SKIP,	NULL,	NULL,	NULL },
	{ "beeper.disable",		0,	NULL,	"PDA\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_CMD | QX_FLAG_SKIP,	NULL,	NULL,	NULL },

	/* Outlet control: unskipped if the outlets are manageable */
	{ "outlet.1.load.off",		0,	NULL,	"SKOFF1\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_CMD | QX_FLAG_SKIP,	NULL,	NULL,	NULL },
	{ "outlet.1.load.on",		0,	NULL,	"SKON1\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_CMD | QX_FLAG_SKIP,	NULL,	NULL,	NULL },
	{ "outlet.2.load.off",		0,	NULL,	"SKOFF2\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_CMD | QX_FLAG_SKIP,	NULL,	NULL,	NULL },
	{ "outlet.2.load.on",		0,	NULL,	"SKON2\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_CMD | QX_FLAG_SKIP,	NULL,	NULL,	NULL },
	{ "outlet.3.load.off",		0,	NULL,	"SKOFF3\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_CMD | QX_FLAG_SKIP,	NULL,	NULL,	NULL },
	{ "outlet.3.load.on",		0,	NULL,	"SKON3\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_CMD | QX_FLAG_SKIP,	NULL,	NULL,	NULL },
	{ "outlet.4.load.off",		0,	NULL,	"SKOFF4\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_CMD | QX_FLAG_SKIP,	NULL,	NULL,	NULL },
	{ "outlet.4.load.on",		0,	NULL,	"SKON4\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_CMD | QX_FLAG_SKIP,	NULL,	NULL,	NULL },

	/* Bypass: unskipped if the UPS is capable of ECO Mode */
	{ "bypass.start",		0,	NULL,	"PEE\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_CMD | QX_FLAG_SKIP,	NULL,	NULL,	NULL },
	{ "bypass.stop",		0,	NULL,	"PDE\r",	"",	5,	'(',	"",	1,	3,	NULL,	QX_FLAG_CMD | QX_FLAG_SKIP,	NULL,	NULL,	NULL },

	/* Server-side settable vars */
	{ "ups.delay.start",		ST_FLAG_RW,	voltronic_r_ondelay,	NULL,		"",	0,	0,	"",	0,	0,	DEFAULT_ONDELAY,	QX_FLAG_ABSENT | QX_FLAG_SETVAR | QX_FLAG_RANGE,	NULL,	NULL,	voltronic_process_setvar },
	{ "ups.delay.shutdown",		ST_FLAG_RW,	voltronic_r_offdelay,	NULL,		"",	0,	0,	"",	0,	0,	DEFAULT_OFFDELAY,	QX_FLAG_ABSENT | QX_FLAG_SETVAR | QX_FLAG_RANGE,	NULL,	NULL,	voltronic_process_setvar },

	/* End of structure. */
	{ NULL,		0,	NULL,	NULL,	"",	0,	0,	"",	0,	0,	NULL,	0,	NULL,	NULL,	NULL }
};


/* == Testing table == */
#ifdef TESTING
static testing_t	voltronic_testing[] = {
	{ "QGS\r",	"(234.9 50.0 229.8 50.0 000.0 00A 369.1 ---.- 026.5 ---.- 018.8 100000000001\r",	-1 },
	{ "QPI\r",	"(PI01\r",	-1 },
	{ "QRI\r",	"(230.0 004 024.0 50.0\r",	-1 },
	{ "QMF\r",	"(#####VOLTRONIC\r",	-1 },
	{ "I\r",	"#-------------   ------     VT12046Q  \r",	-1 },
	{ "F\r",	"#220.0 000 024.0 50.0\r",	-1 },
	{ "QMD\r",	"(#######OLHVT1K0 ###1000 80 2/2 230 230 02 12.0\r",	-1 },
	{ "QFS\r",	"(14 212.1 50.0 005.6 49.9 006 010.6 343.8 ---.- 026.2 021.8 01101100\r",	-1 },
	{ "QMOD\r",	"(S\r",	-1 },
	{ "QVFW\r",	"(VERFW:00322.02\r",	-1 },
	{ "QID\r",	"(685653211455\r",	-1 },
	{ "QBV\r",	"(026.5 02 01 068 255\r",	-1 },
	{ "QFLAG\r",	"(EpashcjDbroegfl\r",	-1 },
	{ "QWS\r",	"(0000000000000000000000000000000000000000000000000000000001000001\r",	-1 },
	{ "QHE\r",	"(242 218\r",	-1 },
	{ "QBYV\r",	"(264 170\r",	-1 },
	{ "QBYF\r",	"(53.0 47.0\r",	-1 },
	{ "QSK1\r",	"(1\r",	-1 },
	{ "QSK2\r",	"(0\r",	-1 },
	{ "QSK3\r",	"(1\r",	-1 },
	{ "QSK4\r",	"(NAK\r",	-1 },
	{ "QSKT1\r",	"(008\r",	-1 },
	{ "QSKT2\r",	"(012\r",	-1 },
	{ "QSKT3\r",	"(NAK\r",	-1 },
	{ "QSKT4\r",	"(007\r",	-1 },
	{ "RE0\r",	"#20\r",	-1 },
	{ "W0E24\r",	"(ACK\r",	-1 },
	{ "PF\r",	"(ACK\r",	-1 },
	{ "PEA\r",	"(ACK\r",	-1 },
	{ "PDR\r",	"(NAK\r",	-1 },
	{ "HEH250\r",	"(ACK\r",	-1 },
	{ "HEL210\r",	"(ACK\r",	-1 },
	{ "PHV260\r",	"(NAK\r",	-1 },
	{ "PLV190\r",	"(ACK\r",	-1 },
	{ "PGF51.0\r",	"(NAK\r",	-1 },
	{ "PSF47.5\r",	"(ACK\r",	-1 },
	{ "BATN2\r",	"(ACK\r",	-1 },
	{ "BATGN04\r",	"(ACK\r",	-1 },
	{ "QBT\r",	"(01\r",	-1 },
	{ "PBT02\r",	"(ACK\r",	-1 },
	{ "QGR\r",	"(00\r",	-1 },
	{ "PGR01\r",	"(ACK\r",	-1 },
	{ "PSK1008\r",	"(ACK\r",	-1 },
	{ "PSK3987\r",	"(ACK\r",	-1 },
	{ "PSK2009\r",	"(ACK\r",	-1 },
	{ "PSK4012\r",	"(ACK\r",	-1 },
	{ "Q3PV\r",	"(123.4 456.4 789.4 012.4 323.4 223.4\r",	-1 },
	{ "Q3OV\r",	"(253.4 163.4 023.4 143.4 103.4 523.4\r",	-1 },
	{ "Q3OC\r",	"(109 069 023\r",	-1 },
	{ "Q3LD\r",	"(005 033 089\r",	-1 },
	{ "Q3YV\r",	"(303.4 245.4 126.4 222.4 293.4 321.4\r",	-1 },
	{ "Q3PC\r",	"(002 023 051\r",	-1 },
	{ "SOFF\r",	"(NAK\r",	-1 },
	{ "SON\r",	"(ACK\r",	-1 },
	{ "T\r",	"(NAK\r",	-1 },
	{ "TL\r",	"(ACK\r",	-1 },
	{ "CS\r",	"(ACK\r",	-1 },
	{ "CT\r",	"(NAK\r",	-1 },
	{ "BZOFF\r",	"(ACK\r",	-1 },
	{ "BZON\r",	"(ACK\r",	-1 },
	{ "S.3R0002\r",	"(ACK\r",	-1 },
	{ "S02R0024\r",	"(NAK\r",	-1 },
	{ "S.5\r",	"(ACK\r",	-1 },
	{ "T.3\r",	"(ACK\r",	-1 },
	{ "T02\r",	"(NAK\r",	-1 },
	{ "SKON1\r",	"(ACK\r",	-1 },
	{ "SKOFF1\r",	"(NAK\r",	-1 },
	{ "SKON2\r",	"(ACK\r",	-1 },
	{ "SKOFF2\r",	"(ACK\r",	-1 },
	{ "SKON3\r",	"(NAK\r",	-1 },
	{ "SKOFF3\r",	"(ACK\r",	-1 },
	{ "SKON4\r",	"(NAK\r",	-1 },
	{ "SKOFF4\r",	"(NAK\r",	-1 },
	{ "QPAR\r",	"(003\r",	-1 },
	{ "QPD\r",	"(000 240\r",	-1 },
	{ "PPD120\r",	"(ACK\r",	-1 },
	{ "QLDL\r",	"(005 080\r",	-1 },
	{ "QBDR\r",	"(1234\r",	-1 },
	{ "QFRE\r",	"(50.0 00.0\r",	-1 },
	{ "FREH54.0\r",	"(ACK\r",	-1 },
	{ "FREL47.0\r",	"(ACK\r",	-1 },
	{ "PEP\r",	"(ACK\r",	-1 },
	{ "PDP\r",	"(ACK\r",	-1 },
	{ "PEB\r",	"(ACK\r",	-1 },
	{ "PDB\r",	"(ACK\r",	-1 },
	{ "PER\r",	"(NAK\r",	-1 },
	{ "PDR\r",	"(NAK\r",	-1 },
	{ "PEO\r",	"(ACK\r",	-1 },
	{ "PDO\r",	"(ACK\r",	-1 },
	{ "PEA\r",	"(ACK\r",	-1 },
	{ "PDA\r",	"(ACK\r",	-1 },
	{ "PES\r",	"(ACK\r",	-1 },
	{ "PDS\r",	"(ACK\r",	-1 },
	{ "PEV\r",	"(ACK\r",	-1 },
	{ "PDV\r",	"(ACK\r",	-1 },
	{ "PEE\r",	"(ACK\r",	-1 },
	{ "PDE\r",	"(ACK\r",	-1 },
	{ "PEG\r",	"(ACK\r",	-1 },
	{ "PDG\r",	"(NAK\r",	-1 },
	{ "PED\r",	"(ACK\r",	-1 },
	{ "PDD\r",	"(ACK\r",	-1 },
	{ "PEC\r",	"(ACK\r",	-1 },
	{ "PDC\r",	"(NAK\r",	-1 },
	{ "PEF\r",	"(NAK\r",	-1 },
	{ "PDF\r",	"(ACK\r",	-1 },
	{ "PEJ\r",	"(NAK\r",	-1 },
	{ "PDJ\r",	"(ACK\r",	-1 },
	{ "PEL\r",	"(ACK\r",	-1 },
	{ "PDL\r",	"(ACK\r",	-1 },
	{ "PEN\r",	"(ACK\r",	-1 },
	{ "PDN\r",	"(ACK\r",	-1 },
	{ "PEQ\r",	"(ACK\r",	-1 },
	{ "PDQ\r",	"(ACK\r",	-1 },
	{ "PEW\r",	"(NAK\r",	-1 },
	{ "PDW\r",	"(ACK\r",	-1 },
	{ NULL }
};
#endif	/* TESTING */


/* == Support functions == */

/* This function allows the subdriver to "claim" a device: return 1 if the device is supported by this subdriver, else 0. */
static int	voltronic_claim(void)
{

	/* We need at least QGS and QPI to run this subdriver */

	item_t	*item = find_nut_info("input.voltage", 0, 0);

	/* Don't know what happened */
	if (!item)
		return 0;

	/* No reply/Unable to get value */
	if (qx_process(item, NULL))
		return 0;

	/* Unable to process value */
	if (ups_infoval_set(item) != 1)
		return 0;

	/* UPS Protocol */
	item = find_nut_info("ups.firmware.aux", 0, 0);

	/* Don't know what happened */
	if (!item) {
		dstate_delinfo("input.voltage");
		return 0;
	}

	/* No reply/Unable to get value */
	if (qx_process(item, NULL)) {
		dstate_delinfo("input.voltage");
		return 0;
	}

	/* Unable to process value/Protocol out of range */
	if (ups_infoval_set(item) != 1) {
		dstate_delinfo("input.voltage");
		return 0;
	}

	return 1;

}

/* Subdriver-specific flags/vars */
static void	voltronic_makevartable(void)
{
	/* Capability vars */
	addvar(VAR_FLAG, "reset_to_default", "Reset capability options and their limits to safe default values");
	addvar(VAR_VALUE, "bypass_alarm", "Alarm (BEEP!) at Bypass Mode [enabled/disabled]");
	addvar(VAR_VALUE, "battery_alarm", "Alarm (BEEP!) at Battery Mode [enabled/disabled]");
	addvar(VAR_VALUE, "bypass_when_off", "Bypass when the UPS is Off [enabled/disabled]");
	addvar(VAR_VALUE, "alarm_control", "Alarm (BEEP!) Control [enabled/disabled]");
	addvar(VAR_VALUE, "converter_mode", "Converter Mode [enabled/disabled]");
	addvar(VAR_VALUE, "eco_mode", "ECO Mode [enabled/disabled]");
	addvar(VAR_VALUE, "battery_open_status_check", "Battery Open Status Check [enabled/disabled]");
	addvar(VAR_VALUE, "bypass_forbidding", "Bypass not allowed (Bypass Forbidding) [enabled/disabled]");
	addvar(VAR_VALUE, "site_fault_detection", "Site fault detection [enabled/disabled]");
	addvar(VAR_VALUE, "advanced_eco_mode", "Advanced ECO Mode [enabled/disabled]");
	addvar(VAR_VALUE, "constant_phase_angle", "Constant Phase Angle Function (Output and input phase angles are not equal) [enabled/disabled]");
	addvar(VAR_VALUE, "limited_runtime_on_battery", "Limited runtime on battery mode [enabled/disabled]");

	/* Bypass Mode frequency/voltage limits */
	addvar(VAR_VALUE, "max_bypass_volt", "Maximum voltage for Bypass Mode");
	addvar(VAR_VALUE, "min_bypass_volt", "Minimum voltage for Bypass Mode");
	addvar(VAR_VALUE, "max_bypass_freq", "Maximum frequency for Bypass Mode");
	addvar(VAR_VALUE, "min_bypass_freq", "Minimum frequency for Bypass Mode");

	/* Device grid working range type for P31 UPSes */
	addvar(VAR_VALUE, "work_range_type", "Device grid working range for P31 UPSes [Appliance/UPS]");

	/* Output phase angle */
	addvar(VAR_VALUE, "output_phase_angle", "Change output phase angle to the provided value [000, 120, 180, 240]°");

	/* Number of batteries */
	addvar(VAR_VALUE, "battery_number", "Set number of batteries that make a pack to n (integer, 1-9)");

	/* For testing purposes */
	addvar(VAR_FLAG, "testing", "If invoked the driver will exec also commands that still need testing");
}

/* Unskip vars according to protocol used by the UPS */
static void	voltronic_massive_unskip(const int protocol)
{
	item_t	*item;

	for (item = voltronic_qx2nut; item->info_type != NULL; item++) {

		if (!item->command)
			continue;

		if (	/* == Multiphase UPSes == */
			/* P09 */
			(protocol == 9 && (
			/*	(!strcasecmp(item->info_type, "input.L1-L3.voltage") && item->from == 25) ||	*//* Not unskipped because P09 should be 2-phase input/output */
			/*	(!strcasecmp(item->info_type, "input.L2-L3.voltage") && item->from == 31) ||	*//* Not unskipped because P09 should be 2-phase input/output */
			/*	(!strcasecmp(item->info_type, "output.L1-L3.voltage") && item->from == 25) ||	*//* Not unskipped because P09 should be 2-phase input/output */
			/*	(!strcasecmp(item->info_type, "output.L2-L3.voltage") && item->from == 31) ||	*//* Not unskipped because P09 should be 2-phase input/output */
				(!strcasecmp(item->info_type, "output.bypass.L1-N.voltage") && item->answer_len == 19) ||
				(!strcasecmp(item->info_type, "output.bypass.L2-N.voltage") && item->answer_len == 19)/* ||
				(!strcasecmp(item->info_type, "output.bypass.L3-N.voltage") && item->answer_len == 19)	*//* Not unskipped because P09 should be 2-phase input/output */
			)) ||
			/* P10 */
			(protocol == 10 && (
				!strcasecmp(item->info_type, "output.L3-N.voltage") ||
				(!strcasecmp(item->info_type, "output.L2-L3.voltage") && item->from == 25) ||
				(!strcasecmp(item->info_type, "output.L1-L3.voltage") && item->from == 31) ||
				(!strcasecmp(item->info_type, "output.bypass.L1-N.voltage") && item->answer_len == 37) ||
				(!strcasecmp(item->info_type, "output.bypass.L2-N.voltage") && item->answer_len == 37) ||
				(!strcasecmp(item->info_type, "output.bypass.L3-N.voltage") && item->answer_len == 37) ||
				!strcasecmp(item->info_type, "output.bypass.L1-L2.voltage") ||
				!strcasecmp(item->info_type, "output.bypass.L2-L3.voltage") ||
				!strcasecmp(item->info_type, "output.bypass.L1-L3.voltage") ||
				!strcasecmp(item->info_type, "output.L3.current") ||
				!strcasecmp(item->info_type, "output.L3.power.percent")
			)) ||
			/* P09-P10 */
			((protocol == 9 || protocol == 10) && (
				!strcasecmp(item->info_type, "output.L1-N.voltage") ||
				!strcasecmp(item->info_type, "output.L2-N.voltage") ||/*
				!strcasecmp(item->info_type, "output.L3-N.voltage") ||	*//* Not unskipped because P09 should be 2-phase input/output */
				!strcasecmp(item->info_type, "output.L1-L2.voltage") ||
				!strcasecmp(item->info_type, "output.L1.current") ||
				!strcasecmp(item->info_type, "output.L2.current") ||/*
				!strcasecmp(item->info_type, "output.L3.current") ||	*//* Not unskipped because P09 should be 2-phase input/output */
				!strcasecmp(item->info_type, "output.L1.power.percent") ||
				!strcasecmp(item->info_type, "output.L2.power.percent")/* ||
				!strcasecmp(item->info_type, "output.L3.power.percent")	*//* Not unskipped because P09 should be 2-phase input/output */
			)) ||
			/* P03-P09-P10 */
			((protocol == 3 || protocol == 9 || protocol == 10) && (
				!strcasecmp(item->info_type, "input.L1-N.voltage") ||
				!strcasecmp(item->info_type, "input.L2-N.voltage") ||/*
				!strcasecmp(item->info_type, "input.L3-N.voltage") ||*//* Not unskipped because P09 should be 2-phase input/output */
				!strcasecmp(item->info_type, "input.L1-L2.voltage") ||
				!strcasecmp(item->info_type, "input.L1.current") ||
				!strcasecmp(item->info_type, "input.L2.current")/* ||
				!strcasecmp(item->info_type, "input.L3.current")*//* Not unskipped because P09 should be 2-phase input/output */
			)) ||
			/* P03-P10 */
			((protocol == 3 || protocol == 10) && (
				!strcasecmp(item->info_type, "input.L3-N.voltage") ||
				(!strcasecmp(item->info_type, "input.L2-L3.voltage") && item->from == 25) ||
				(!strcasecmp(item->info_type, "input.L1-L3.voltage") && item->from == 31) ||
				!strcasecmp(item->info_type, "input.L3.current")
			)) ||
			/* == P31 battery type/device grid working range == */
			(protocol == 31 && (
				!strcasecmp(item->info_type, "battery.type") ||
				(!strcasecmp(item->info_type, "work_range_type") && !(item->qxflags & QX_FLAG_SETVAR)) ||
				(!strcasecmp(item->info_type, "work_range_type") && (item->qxflags & QX_FLAG_SETVAR) && getval(item->info_type))
			)) ||
			/* == ByPass limits: all but P00/P08/P31 == */
			(protocol != 0 && protocol != 8 && protocol != 31 && (
				(!strcasecmp(item->info_type, "max_bypass_volt") && !(item->qxflags & QX_FLAG_SETVAR)) ||
				(!strcasecmp(item->info_type, "min_bypass_volt") && !(item->qxflags & QX_FLAG_SETVAR)) ||
				(!strcasecmp(item->info_type, "max_bypass_freq") && !(item->qxflags & QX_FLAG_SETVAR)) ||
				(!strcasecmp(item->info_type, "min_bypass_freq") && !(item->qxflags & QX_FLAG_SETVAR))
			)) ||
			/* == Reset capabilities options to safe default values == */
			(!strcasecmp(item->info_type, "reset_to_default") && testvar("reset_to_default")) ||
			/* == QBDR (unknown) == */
			(!strcasecmp(item->info_type, "unknown.7") && testvar("testing"))
		) {

				item->qxflags &= ~QX_FLAG_SKIP;

		}

	}
}


/* == Preprocess functions == */

/* *SETVAR(/NONUT)* Preprocess setvars */
static int	voltronic_process_setvar(item_t *item, char *value, const size_t valuelen)
{
	double	val;

	if (!strlen(value)) {
		upsdebugx(2, "%s: value not given for %s", __func__, item->info_type);
		return -1;
	}

	val = strtod(value, NULL);

	if (!strcasecmp(item->info_type, "ups.delay.start")) {

		/* Truncate to minute */
		val -= ((int)val % 60);

		snprintf(value, valuelen, "%.0f", val);

		return 0;

	} else if (!strcasecmp(item->info_type, "ups.delay.shutdown")) {

		/* Truncate to nearest setable value */
		if (val < 60) {
			val -= ((int)val % 6);
		} else {
			val -= ((int)val % 60);
		}

		snprintf(value, valuelen, "%.0f", val);

		return 0;

	} else if (!strcasecmp(item->info_type, "max_bypass_freq")) {

		if (val == max_bypass_freq) {
			upslogx(LOG_INFO, "%s is already set to %.1f", item->info_type, val);
			return -1;
		}

	} else if (!strcasecmp(item->info_type, "min_bypass_freq")) {

		if (val == min_bypass_freq) {
			upslogx(LOG_INFO, "%s is already set to %.1f", item->info_type, val);
			return -1;
		}

	} else if (!strcasecmp(item->info_type, "max_bypass_volt")) {

		if (val == max_bypass_volt) {
			upslogx(LOG_INFO, "%s is already set to %.0f", item->info_type, val);
			return -1;
		}

	} else if (!strcasecmp(item->info_type, "min_bypass_volt")) {

		if (val == min_bypass_volt) {
			upslogx(LOG_INFO, "%s is already set to %.0f", item->info_type, val);
			return -1;
		}

	} else if (!strcasecmp(item->info_type, "battery_number")) {

		if (val == battery_number) {
			upslogx(LOG_INFO, "%s is already set to %.0f", item->info_type, val);
			return -1;
		}

	}

	snprintf(value, valuelen, item->command, val);

	return 0;
}

/* *CMD* Preprocess instant commands */
static int	voltronic_process_command(item_t *item, char *value, const size_t valuelen)
{
	char	buf[SMALLBUF] = "";

	if (!strcasecmp(item->info_type, "shutdown.return")) {

		/* Sn: Shutdown after n minutes and then turn on when mains is back
		 * SnRm: Shutdown after n minutes and then turn on after m minutes
		 * Accepted values for n: .2 -> .9 , 01 -> 99
		 * Accepted values for m: 0001 -> 9999 */

		int	offdelay = strtol(dstate_getinfo("ups.delay.shutdown"), NULL, 10),
			ondelay = strtol(dstate_getinfo("ups.delay.start"), NULL, 10) / 60;

		if (ondelay == 0) {

			if (offdelay < 60) {
				snprintf(buf, sizeof(buf), ".%d", offdelay / 6);
			} else {
				snprintf(buf, sizeof(buf), "%02d", offdelay / 60);
			}

		} else if (offdelay < 60) {

			snprintf(buf, sizeof(buf), ".%dR%04d", offdelay / 6, ondelay);

		} else {

			snprintf(buf, sizeof(buf), "%02dR%04d", offdelay / 60, ondelay);

		}

	} else if (!strcasecmp(item->info_type, "shutdown.stayoff")) {

		/* SnR0000
		 * Shutdown after n minutes and stay off
		 * Accepted values for n: .2 -> .9 , 01 -> 99 */

		int	offdelay = strtol(dstate_getinfo("ups.delay.shutdown"), NULL, 10);

		if (offdelay < 60) {
			snprintf(buf, sizeof(buf), ".%d", offdelay / 6);
		} else {
			snprintf(buf, sizeof(buf), "%02d", offdelay / 60);
		}

	} else if (!strcasecmp(item->info_type, "test.battery.start")) {

		/* Accepted values for test time: .2 -> .9 (.2=12sec ..), 01 -> 99 (minutes)
		 * -> you have to invoke test.battery.start + number of seconds [12..5940] */

		int	delay;

		if (strlen(value) != strspn(value, "0123456789")) {
			upslogx(LOG_ERR, "%s: non numerical value [%s]", item->info_type, value);
			return -1;
		}

		delay = strlen(value) > 0 ? strtol(value, NULL, 10) : 600;

		if ((delay < 12) || (delay > 5940)) {
			upslogx(LOG_ERR, "%s: battery test time '%d' out of range [12..5940] seconds", item->info_type, delay);
			return -1;
		}

		/* test time < 1 minute */
		if (delay < 60) {

			delay = delay / 6;
			snprintf(buf, sizeof(buf), ".%d", delay);

		/* test time > 1 minute */
		} else {

			delay = delay / 60;
			snprintf(buf, sizeof(buf), "%02d", delay);

		}

	} else if (!strcasecmp(item->info_type, "beeper.toggle")) {

		const char	*beeper_status = dstate_getinfo("ups.beeper.status");

		/* If the UPS is beeping then we can call BZOFF; if we previously set BZOFF we can call BZON, provided that the beeper is not disabled */

		/* The UPS can disable/enable alarm (from UPS capability) */
		if (alarm_control) {

			if (!strcmp(beeper_status, "enabled")) {

				snprintf(buf, sizeof(buf), "OFF");

			} else if (!strcmp(beeper_status, "muted")) {

				snprintf(buf, sizeof(buf), "ON");

			/* Beeper disabled */
			} else {

				upslogx(LOG_INFO, "The beeper is already disabled");
				return -1;

			}

		/* The UPS can't disable/enable alarm (from UPS capability) */
		} else {

			if (!strcmp(beeper_status, "enabled")) {

				snprintf(buf, sizeof(buf), "OFF");

			} else if (!strcmp(beeper_status, "disabled")) {

				snprintf(buf, sizeof(buf), "ON");

			}

		}

	} else {

		/* Don't know what happened */
		return -1;

	}

	snprintf(value, valuelen, item->command, buf);

	return 0;
}

/* UPS capabilities */
static int	voltronic_capability(item_t *item, char *value, const size_t valuelen)
{
	char	rawval[SMALLBUF], *enabled, *disabled, *val = NULL, *saveptr = NULL;
	item_t	*unskip;

	snprintf(rawval, sizeof(rawval), "%s", item->value);

	enabled = strtok_r(rawval+1, "D", &saveptr);
	disabled = strtok_r(NULL, "\0", &saveptr);

	if (!enabled && !disabled) {
		upsdebugx(2, "%s: parsing failed", __func__);
		return -1;
	}

	enabled = enabled ? enabled : "";
	disabled = disabled ? disabled : "";

	/* NONUT items */
	if (!strcasecmp(item->info_type, "bypass_alarm")) {

		if (strchr(enabled, 'p')) {
			val = bypass_alarm = "enabled";
		} else if (strchr(disabled, 'p')) {
			val = bypass_alarm = "disabled";
		}

	} else if (!strcasecmp(item->info_type, "battery_alarm")) {

		if (strchr(enabled, 'b')) {
			val = battery_alarm = "enabled";
		} else if (strchr(disabled, 'b')) {
			val = battery_alarm = "disabled";
		}

	} else if (!strcasecmp(item->info_type, "bypass_when_off")) {

		if (strchr(enabled, 'o')) {
			val = bypass_when_off = "enabled";
		} else if (strchr(disabled, 'o')) {
			val = bypass_when_off = "disabled";
		}

	} else if (!strcasecmp(item->info_type, "alarm_control")) {

		if (strchr(item->value, 'a')) {

			if (strchr(enabled, 'a')) {

				const char	*beeper = dstate_getinfo("ups.beeper.status");

				val = alarm_control = "enabled";

				if (!beeper || strcmp(beeper, "muted")) {
					dstate_setinfo("ups.beeper.status", "enabled");
				}

			} else if (strchr(disabled, 'a')) {

				val = alarm_control = "disabled";
				dstate_setinfo("ups.beeper.status", "disabled");

			}

			/* Unskip beeper.{enable,disable} instcmds */
			unskip = find_nut_info("beeper.enable", QX_FLAG_CMD, 0);

			/* Don't know what happened */
			if (!unskip)
				return -1;

			unskip->qxflags &= ~QX_FLAG_SKIP;

			unskip = find_nut_info("beeper.disable", QX_FLAG_CMD, 0);

			/* Don't know what happened */
			if (!unskip)
				return -1;

			unskip->qxflags &= ~QX_FLAG_SKIP;

		}

	} else if (!strcasecmp(item->info_type, "converter_mode")) {

		if (strchr(enabled, 'v')) {
			val = converter_mode = "enabled";
		} else if (strchr(disabled, 'v')) {
			val = converter_mode = "disabled";
		}

	} else if (!strcasecmp(item->info_type, "eco_mode")) {

		if (strchr(item->value, 'e')) {

			if (strchr(enabled, 'e')) {
				val = eco_mode = "enabled";
			} else if (strchr(disabled, 'e')) {
				val = eco_mode = "disabled";
			}

			/* Unskip bypass.{start,stop} instcmds */
			unskip = find_nut_info("bypass.start", QX_FLAG_CMD, 0);

			/* Don't know what happened */
			if (!unskip)
				return -1;

			unskip->qxflags &= ~QX_FLAG_SKIP;

			unskip = find_nut_info("bypass.stop", QX_FLAG_CMD, 0);

			/* Don't know what happened */
			if (!unskip)
				return -1;

			unskip->qxflags &= ~QX_FLAG_SKIP;

			/* Unskip input.transfer.{high,low} */
			unskip = find_nut_info("input.transfer.high", QX_FLAG_SEMI_STATIC, 0);

			/* Don't know what happened */
			if (!unskip)
				return -1;

			unskip->qxflags &= ~QX_FLAG_SKIP;

			unskip = find_nut_info("input.transfer.low", QX_FLAG_SEMI_STATIC, 0);

			/* Don't know what happened */
			if (!unskip)
				return -1;

			unskip->qxflags &= ~QX_FLAG_SKIP;

			/* Unskip input.frequency.{high,low} */
			unskip = find_nut_info("input.frequency.high", QX_FLAG_SEMI_STATIC, 0);

			/* Don't know what happened */
			if (!unskip)
				return -1;

			unskip->qxflags &= ~QX_FLAG_SKIP;

			unskip = find_nut_info("input.frequency.low", QX_FLAG_SEMI_STATIC, 0);

			/* Don't know what happened */
			if (!unskip)
				return -1;

			unskip->qxflags &= ~QX_FLAG_SKIP;

		}

	} else if (!strcasecmp(item->info_type, "battery_open_status_check")) {

		if (strchr(enabled, 'd')) {
			val = battery_open_status_check = "enabled";
		} else if (strchr(disabled, 'd')) {
			val = battery_open_status_check = "disabled";
		}

	} else if (!strcasecmp(item->info_type, "bypass_forbidding")) {

		if (strchr(enabled, 'f')) {
			val = bypass_forbidding = "enabled";
		} else if (strchr(disabled, 'f')) {
			val = bypass_forbidding = "disabled";
		}

	} else if (!strcasecmp(item->info_type, "site_fault_detection")) {

		if (strchr(enabled, 'l')) {
			val = site_fault_detection = "enabled";
		} else if (strchr(disabled, 'l')) {
			val = site_fault_detection = "disabled";
		}

	} else if (!strcasecmp(item->info_type, "advanced_eco_mode")) {

		if (strchr(enabled, 'n')) {
			val = advanced_eco_mode = "enabled";
		} else if (strchr(disabled, 'n')) {
			val = advanced_eco_mode = "disabled";
		}

	} else if (!strcasecmp(item->info_type, "constant_phase_angle")) {

		if (strchr(enabled, 'q')) {
			val = constant_phase_angle = "enabled";
		} else if (strchr(disabled, 'q')) {
			val = constant_phase_angle = "disabled";
		}

	} else if (!strcasecmp(item->info_type, "limited_runtime_on_battery")) {

		if (strchr(enabled, 'w')) {
			val = limited_runtime_on_battery = "enabled";
		} else if (strchr(disabled, 'w')) {
			val = limited_runtime_on_battery = "disabled";
		}

/*	} else if (!strcasecmp(item->info_type, "")) {

		if (strchr(enabled, 'h')) {	unknown/unused
		} else if (strchr(disabled, 'h')) { }

	} else if (!strcasecmp(item->info_type, "")) {

		if (strchr(enabled, 't')) {	unknown/unused
		} else if (strchr(disabled, 't')) { }

	} else if (!strcasecmp(item->info_type, "")) {

		if (strchr(enabled, 'k')) {	unknown/unused
		} else if (strchr(disabled, 'k')) { }

	} else if (!strcasecmp(item->info_type, "")) {

		if (strchr(enabled, 'i')) {	unknown/unused
		} else if (strchr(disabled, 'i')) { }

	} else if (!strcasecmp(item->info_type, "")) {

		if (strchr(enabled, 'm')) {	unknown/unused
		} else if (strchr(disabled, 'm')) { }

	} else if (!strcasecmp(item->info_type, "")) {

		if (strchr(enabled, 'z')) {	unknown/unused
		} else if (strchr(disabled, 'z')) { }
*/
	/* Items with a NUT variable */
	} else if (!strcasecmp(item->info_type, "ups.start.auto")) {

		if (strchr(enabled, 'r')) {
			val = "yes";
		} else if (strchr(disabled, 'r')) {
			val = "no";
		}

	} else if (!strcasecmp(item->info_type, "battery.protection")) {

		if (strchr(enabled, 's')) {
			val = "yes";
		} else if (strchr(disabled, 's')) {
			val = "no";
		}

	} else if (!strcasecmp(item->info_type, "battery.energysave")) {

		if (strchr(enabled, 'g')) {
			val = "yes";
		} else if (strchr(disabled, 'g')) {
			val = "no";
		}

	} else if (!strcasecmp(item->info_type, "ups.start.battery")) {

		if (strchr(enabled, 'c')) {
			val = "yes";
		} else if (strchr(disabled, 'c')) {
			val = "no";
		}

	} else if (!strcasecmp(item->info_type, "outlet.0.switchable")) {

		if (strchr(enabled, 'j')) {

			int	i;
			char	buf[SMALLBUF];

			val = "yes";

			/* Unskip outlet.n.{switchable,status} */
			for (i = 1; i <= 4; i++) {

				snprintf(buf, sizeof(buf), "outlet.%d.switchable", i);

				unskip = find_nut_info(buf, 0, 0);

				/* Don't know what happened */
				if (!unskip)
					return -1;

				unskip->qxflags &= ~QX_FLAG_SKIP;

				snprintf(buf, sizeof(buf), "outlet.%d.status", i);

				unskip = find_nut_info(buf, 0, 0);

				/* Don't know what happened */
				if (!unskip)
					return -1;

				unskip->qxflags &= ~QX_FLAG_SKIP;

			}

		} else if (strchr(disabled, 'j')) {
			val = "no";
		}

	}

	/* UPS doesn't have that capability */
	if (!val)
		return -1;

	snprintf(value, valuelen, item->dfl, val);

	/* This item doesn't have a NUT var and we were not asked by the user to change its value */
	if ((item->qxflags & QX_FLAG_NONUT) && !getval(item->info_type))
		return 0;

	/* Unskip setvar */
	unskip = find_nut_info(item->info_type, QX_FLAG_SETVAR, 0);

	/* Don't know what happened */
	if (!unskip)
		return -1;

	unskip->qxflags &= ~QX_FLAG_SKIP;

	return 0;
}

/* *SETVAR* Set UPS capability options */
static int	voltronic_capability_set(item_t *item, char *value, const size_t valuelen)
{
	if (!strcasecmp(value, "yes")) {
		snprintf(value, valuelen, item->command, "E");
		return 0;
	}

	if (!strcasecmp(value, "no")) {
		snprintf(value, valuelen, item->command, "D");
		return 0;
	}

	/* At this point value should have been already checked against enum so this shouldn't happen.. however.. */
	upslogx(LOG_ERR, "%s: given value [%s] is not acceptable. Enter either 'yes' or 'no'.", item->info_type, value);

	return -1;
}

/* *SETVAR/NONUT* Change UPS capability according to user configuration in ups.conf */
static int	voltronic_capability_set_nonut(item_t *item, char *value, const size_t valuelen)
{
	const char	*match = NULL;
	int		i;
	const struct {
		const char	*type;	/* Name of the option */
		const char	*match;	/* Value reported by the UPS */
	} capability[] = {
		{ "bypass_alarm",		bypass_alarm },
		{ "battery_alarm",		battery_alarm },
		{ "bypass_when_off",		bypass_when_off },
		{ "alarm_control",		alarm_control },
		{ "converter_mode",		converter_mode },
		{ "eco_mode",			eco_mode },
		{ "battery_open_status_check",	battery_open_status_check },
		{ "bypass_forbidding",		bypass_forbidding },
		{ "site_fault_detection",	site_fault_detection },
		{ "advanced_eco_mode",		advanced_eco_mode },
		{ "constant_phase_angle",	constant_phase_angle },
		{ "limited_runtime_on_battery",	limited_runtime_on_battery },
		{ NULL }
	};

	for (i = 0; capability[i].type; i++) {

		if (strcasecmp(item->info_type, capability[i].type))
			continue;

		match = capability[i].match;

		break;

	}

	/* UPS doesn't have that capability */
	if (!match)
		return -1;

	/* At this point value should have been already checked by nutdrv_qx's own setvar so this shouldn't happen.. however.. */
	if (!strcasecmp(value, match)) {
		upslogx(LOG_INFO, "%s is already %s", item->info_type, match);
		return -1;
	}

	if (!strcasecmp(value, "disabled")) {
		snprintf(value, valuelen, item->command, "D");
	} else if (!strcasecmp(value, "enabled")) {
		snprintf(value, valuelen, item->command, "E");
	} else {
		/* At this point value should have been already checked against enum so this shouldn't happen.. however.. */
		upslogx(LOG_ERR, "%s: [%s] is not within acceptable values [enabled/disabled]", item->info_type, value);
		return -1;
	}

	return 0;
}

/* *SETVAR/NONUT* Reset capability options and their limits to safe default values */
static int	voltronic_capability_reset(item_t *item, char *value, const size_t valuelen)
{
	/* Nothing to do */
	if (!testvar("reset_to_default"))
		return -1;

	/* UPS capability options can be reset only when the UPS is in 'Standby Mode' (=OFF) (from QMOD) */
	if (!(qx_status() & STATUS(OFF))) {
		upslogx(LOG_ERR, "%s: UPS capability options can be reset only when the UPS is in Standby Mode (i.e. ups.status = 'OFF').", item->info_type);
		return -1;
	}

	snprintf(value, valuelen, "%s", item->command);

	return 0;
}

/* Voltage limits for ECO Mode */
static int	voltronic_eco_volt(item_t *item, char *value, const size_t valuelen)
{
	const int	protocol = strtol(dstate_getinfo("ups.firmware.aux")+1, NULL, 10);
	int		ovn;
	const char	*outvoltnom;
	char		buf[SMALLBUF];
	item_t		*unskip;
	/* Range of accepted values for maximum voltage for ECO Mode */
	struct {
		int	lower;	/* Lower limit */
		int	upper;	/* Upper limit */
	} max;
	/* Range of accepted values for minimum voltage for ECO Mode */
	struct {
		int	lower;	/* Lower limit */
		int	upper;	/* Upper limit */
	} min;

	if (strspn(item->value, "0123456789 .") != strlen(item->value)) {
		upsdebugx(2, "%s: non numerical value [%s: %s]", __func__, item->info_type, item->value);
		return -1;
	}

	snprintf(value, valuelen, item->dfl, strtod(item->value, NULL));

	outvoltnom = dstate_getinfo("output.voltage.nominal");

	/* Since query for ratings (QRI) is not mandatory to run this driver, skip next steps if we can't get the value of output voltage nominal */
	if (!outvoltnom) {
		upsdebugx(2, "%s: unable to get output voltage nominal", __func__);
		/* We return 0 since we have the value and all's ok: simply we can't set its range so we won't unskip SETVAR item and .{min,max} */
		return 0;
	}

	ovn = strtol(outvoltnom, NULL, 10);

	/* For P01/P09 */
	if (protocol == 1 || protocol == 9) {

		if (ovn >= 200) {
			min.lower = ovn - 24;
			min.upper = ovn - 7;
			max.lower = ovn + 7;
			max.upper = ovn + 24;
		} else {
			min.lower = ovn - 12;
			min.upper = ovn - 3;
			max.lower = ovn + 3;
			max.upper = ovn + 12;

		}

	/* For P02/P03/P10/P13/P14/P99 */
	} else if (protocol == 2 || protocol == 3 || protocol == 10 || protocol == 13 || protocol == 14 || protocol == 99) {

		if (ovn >= 200) {
			min.lower = ovn - 24;
			min.upper = ovn - 11;
			max.lower = ovn + 11;
			max.upper = ovn + 24;
		} else {
			min.lower = ovn - 12;
			min.upper = ovn - 5;
			max.lower = ovn + 5;
			max.upper = ovn + 12;
		}

	/* ECO mode not supported */
	} else {
		upsdebugx(2, "%s: the UPS doesn't seem to support ECO Mode", __func__);
		/* We return 0 since we have the value and all's ok: simply we can't set its range so we won't unskip SETVAR item and .{min,max} */
		return 0;
	}

	if (!strcasecmp(item->info_type, "input.transfer.high")) {

		/* Fill voltronic_r_eco_volt_max */
		snprintf(item->info_rw[0].value, sizeof(item->info_rw[0].value), "%d", max.lower);
		snprintf(item->info_rw[1].value, sizeof(item->info_rw[1].value), "%d", max.upper);


	} else if (!strcasecmp(item->info_type, "input.transfer.low")) {

		/* Fill voltronic_r_eco_volt_min */
		snprintf(item->info_rw[0].value, sizeof(item->info_rw[0].value), "%d", min.lower);
		snprintf(item->info_rw[1].value, sizeof(item->info_rw[1].value), "%d", min.upper);

	}

	/* Unskip input.transfer.{high,low}.{min,max} */
	snprintf(buf, sizeof(buf), "%s.min", item->info_type);

	unskip = find_nut_info(buf, QX_FLAG_SEMI_STATIC, 0);

	/* Don't know what happened */
	if (!unskip)
		return -1;

	unskip->qxflags &= ~QX_FLAG_SKIP;

	snprintf(buf, sizeof(buf), "%s.max", item->info_type);

	unskip = find_nut_info(buf, QX_FLAG_SEMI_STATIC, 0);

	/* Don't know what happened */
	if (!unskip)
		return -1;

	unskip->qxflags &= ~QX_FLAG_SKIP;

	/* Unskip input.transfer.{high,low} setvar */
	unskip = find_nut_info(item->info_type, QX_FLAG_SETVAR, 0);

	/* Don't know what happened */
	if (!unskip)
		return -1;

	unskip->qxflags &= ~QX_FLAG_SKIP;

	return 0;
}

/* Voltage limits for ECO Mode (max, min) */
static int	voltronic_eco_volt_range(item_t *item, char *value, const size_t valuelen)
{
	char	*buf;
	int	i;
	item_t	*from;

	if (!strcasecmp(item->info_type, "input.transfer.low.min")) {

		buf = "input.transfer.low";
		i = 0;

	} else if (!strcasecmp(item->info_type, "input.transfer.low.max")) {

		buf = "input.transfer.low";
		i = 1;

	} else if (!strcasecmp(item->info_type, "input.transfer.high.min")) {

		buf = "input.transfer.high";
		i = 0;

	} else if (!strcasecmp(item->info_type, "input.transfer.high.max")) {

		buf = "input.transfer.high";
		i = 1;

	} else {

		/* Don't know what happened */
		return -1;

	}

	from = find_nut_info(buf, QX_FLAG_SEMI_STATIC, 0);

	/* Don't know what happened */
	if (!from)
		return -1;

	/* Value is set at runtime by voltronic_eco_volt, so if it's still unset something went wrong */
	if (!strlen(from->info_rw[i].value))
		return -1;

	snprintf(value, valuelen, "%s", from->info_rw[i].value);

	return 0;
}

/* Frequency limits for ECO Mode */
static int	voltronic_eco_freq(item_t *item, char *value, const size_t valuelen)
{
	item_t	*unskip;

	if (strspn(item->value, "0123456789 .") != strlen(item->value)) {
		upsdebugx(2, "%s: non numerical value [%s: %s]", __func__, item->info_type, item->value);
		return -1;
	}

	snprintf(value, valuelen, item->dfl, strtod(item->value, NULL));

	/* Unskip input.transfer.{high,low} setvar */
	unskip = find_nut_info(item->info_type, QX_FLAG_SETVAR, 0);

	/* Don't know what happened */
	if (!unskip)
		return -1;

	unskip->qxflags &= ~QX_FLAG_SKIP;

	return 0;
}

/* *NONUT* Voltage/frequency limits for Bypass Mode */
static int	voltronic_bypass(item_t *item, char *value, const size_t valuelen)
{
	item_t	*unskip;
	double	val;

	if (strspn(item->value, "0123456789 .") != strlen(item->value)) {
		upsdebugx(2, "%s: non numerical value [%s: %s]", __func__, item->info_type, item->value);
		return -1;
	}

	if (!strcasecmp(item->info_type, "max_bypass_volt")) {

		val = max_bypass_volt = strtol(item->value, NULL, 10);

	} else if (!strcasecmp(item->info_type, "min_bypass_volt")) {

		val = min_bypass_volt = strtol(item->value, NULL, 10);

	} else if (!strcasecmp(item->info_type, "max_bypass_freq")) {

		val = max_bypass_freq = strtod(item->value, NULL);

	} else if (!strcasecmp(item->info_type, "min_bypass_freq")) {

		val = min_bypass_freq = strtod(item->value, NULL);

	} else {

		/* Don't know what happened */
		return -1;

	}

	snprintf(value, valuelen, item->dfl, val);

	/* No user-provided value to change.. */
	if (!getval(item->info_type))
		return 0;

	/* Unskip {min,max}_bypass_volt setvar */
	unskip = find_nut_info(item->info_type, QX_FLAG_SETVAR, 0);

	/* Don't know what happened */
	if (!unskip)
		return -1;

	unskip->qxflags &= ~QX_FLAG_SKIP;

	return 0;
}

/* *NONUT* Number of batteries */
static int	voltronic_batt_numb(item_t *item, char *value, const size_t valuelen)
{
	item_t	*unskip;

	if (strspn(item->value, "0123456789 .") != strlen(item->value)) {
		upsdebugx(2, "%s: non numerical value [%s: %s]", __func__, item->info_type, item->value);
		return -1;
	}

	battery_number = strtol(item->value, NULL, 10);

	snprintf(value, valuelen, item->dfl, battery_number);

	/* No user-provided value to change.. */
	if (!getval(item->info_type))
		return 0;

	/* Unskip battery_number setvar */
	unskip = find_nut_info("battery_number", QX_FLAG_SETVAR, 0);

	/* Don't know what happened */
	if (!unskip)
		return -1;

	unskip->qxflags &= ~QX_FLAG_SKIP;

	return 0;
}

/* Battery runtime */
static int	voltronic_batt_runtime(item_t *item, char *value, const size_t valuelen)
{
	double	runtime;

	if (strspn(item->value, "0123456789 .") != strlen(item->value)) {
		upsdebugx(2, "%s: non numerical value [%s: %s]", __func__, item->info_type, item->value);
		return -1;
	}

	/* Battery runtime is reported by the UPS in minutes, NUT expects seconds */
	runtime = strtod(item->value, NULL) * 60;

	snprintf(value, valuelen, item->dfl, runtime);

	return 0;
}

/* Protocol used by the UPS */
static int	voltronic_protocol(item_t *item, char *value, const size_t valuelen)
{
	int	protocol;

	if (strncasecmp(item->value, "PI", 2)) {
		upsdebugx(2, "%s: invalid start characters [%.2s]", __func__, item->value);
		return -1;
	}

	/* Here we exclude non numerical value and other non accepted protocols (hence the restricted comparison target) */
	if (strspn(item->value+2, "0123489") != strlen(item->value+2)) {
		upslogx(LOG_ERR, "Protocol [%s] is not supported by this driver", item->value);
		return -1;
	}

	protocol = strtol(item->value+2, NULL, 10);

	switch (protocol)
	{
	case 0:
	case 1:
	case 2:
	case 3:
	case 8:
	case 9:
	case 10:
	case 13:
	case 14:
	case 31:
	case 99:

		break;

	default:

		upslogx(LOG_ERR, "Protocol [PI%02d] is not supported by this driver", protocol);
		return -1;

	}

	snprintf(value, valuelen, "P%02d", protocol);

	/* Unskip vars according to protocol */
	voltronic_massive_unskip(protocol);

	return 0;
}

/* Fault reported by the UPS:
 * When the UPS is queried for status (QGS), if it reports a fault (6th bit of 12bit flag of the reply to QGS set to 1), the driver unskips the QFS item in qx2nut array: this function processes the reply to QFS query */
static int	voltronic_fault(item_t *item, char *value, const size_t valuelen)
{
	int	protocol = strtol(dstate_getinfo("ups.firmware.aux")+1, NULL, 10);

	char	alarm[LARGEBUF]; /* can sprintf() SMALLBUF plus markup into here */

	upslogx(LOG_INFO, "Checking for faults..");

	if (!strcasecmp(item->value, "OK")) {
		snprintf(value, valuelen, item->dfl, "No fault found");
		upslogx(LOG_INFO, "%s", value);
		item->qxflags |= QX_FLAG_SKIP;
		return 0;
	}

	if ((strspn(item->value, "0123456789ABC") != 2) || ((item->value[0] != '1') && (strspn(item->value+1, "0123456789") != 1))) {

		snprintf(alarm, sizeof(alarm), "Unknown fault [%s]", item->value);

	/* P31 UPSes */
	} else if (protocol == 31) {

		if (strpbrk(item->value+1, "ABC")) {

			snprintf(alarm, sizeof(alarm), "Unknown fault [%s]", item->value);

		} else {

			switch (strtol(item->value, NULL, 10))
			{
			case 1:

				strcpy(alarm, "Fan failure.");
				break;

			case 2:

				strcpy(alarm, "Over temperature fault.");
				break;

			case 3:

				strcpy(alarm, "Battery voltage is too high.");
				break;

			case 4:

				strcpy(alarm, "Battery voltage too low.");
				break;

			case 5:

				strcpy(alarm, "Inverter relay short-circuited.");
				break;

			case 6:

				strcpy(alarm, "Inverter voltage over maximum value.");
				break;

			case 7:

				strcpy(alarm, "Overload fault.");
				update_status("OVER");
				break;

			case 8:

				strcpy(alarm, "Bus voltage exceeds its upper limit.");
				break;

			case 9:

				strcpy(alarm, "Bus soft start fail.");
				break;

			case 10:

				strcpy(alarm, "Unknown fault [Fault code: 10]");
				break;

			case 51:

				strcpy(alarm, "Over current fault.");
				break;

			case 52:

				strcpy(alarm, "Bus voltage below its under limit.");
				break;

			case 53:

				strcpy(alarm, "Inverter soft start fail.");
				break;

			case 54:

				strcpy(alarm, "Self test fail.");
				break;

			case 55:

				strcpy(alarm, "Output DC voltage exceeds its upper limit.");
				break;

			case 56:

				strcpy(alarm, "Battery open fault.");
				break;

			case 57:

				strcpy(alarm, "Current sensor fault.");
				break;

			case 58:

				strcpy(alarm, "Battery short.");
				break;

			case 59:

				strcpy(alarm, "Inverter voltage below its lower limit.");
				break;

			default:

				snprintf(alarm, sizeof(alarm), "Unknown fault [%s]", item->value);
				break;

			}

		}

	/* All other UPSes */
	} else {

		switch (strtol(item->value, NULL, 10))
		{
		case 1:

			switch (item->value[1])
			{
			case 'A':

				strcpy(alarm, "L1 inverter negative power out of acceptable range.");
				break;

			case 'B':

				strcpy(alarm, "L2 inverter negative power out of acceptable range.");
				break;

			case 'C':

				strcpy(alarm, "L3 inverter negative power out of acceptable range.");
				break;

			default:

				strcpy(alarm, "Bus voltage not within default setting.");
				break;

			}

			break;

		case 2:

			strcpy(alarm, "Bus voltage over maximum value.");
			break;

		case 3:

			strcpy(alarm, "Bus voltage below minimum value.");
			break;

		case 4:

			strcpy(alarm, "Bus voltage differences out of acceptable range.");
			break;

		case 5:

			strcpy(alarm, "Bus voltage of slope rate drops too fast.");
			break;

		case 6:

			strcpy(alarm, "Over current in PFC input inductor.");
			break;

		case 11:

			strcpy(alarm, "Inverter voltage not within default setting.");
			break;

		case 12:

			strcpy(alarm, "Inverter voltage over maximum value.");
			break;

		case 13:

			strcpy(alarm, "Inverter voltage below minimum value.");
			break;

		case 14:

			strcpy(alarm, "Inverter short-circuited.");
			break;

		case 15:

			strcpy(alarm, "L2 phase inverter short-circuited.");
			break;

		case 16:

			strcpy(alarm, "L3 phase inverter short-circuited.");
			break;

		case 17:

			strcpy(alarm, "L1L2 inverter short-circuited.");
			break;

		case 18:

			strcpy(alarm, "L2L3 inverter short-circuited.");
			break;

		case 19:

			strcpy(alarm, "L3L1 inverter short-circuited.");
			break;

		case 21:

			strcpy(alarm, "Battery SCR short-circuited.");
			break;

		case 22:

			strcpy(alarm, "Line SCR short-circuited.");
			break;

		case 23:

			strcpy(alarm, "Inverter relay open fault.");
			break;

		case 24:

			strcpy(alarm, "Inverter relay short-circuited.");
			break;

		case 25:

			strcpy(alarm, "Input and output wires oppositely connected.");
			break;

		case 26:

			strcpy(alarm, "Battery oppositely connected.");
			break;

		case 27:

			strcpy(alarm, "Battery voltage is too high.");
			break;

		case 28:

			strcpy(alarm, "Battery voltage too low.");
			break;

		case 29:

			strcpy(alarm, "Failure for battery fuse being open-circuited.");
			break;

		case 31:

			strcpy(alarm, "CAN-bus communication fault.");
			break;

		case 32:

			strcpy(alarm, "Host signal circuit fault.");
			break;

		case 33:

			strcpy(alarm, "Synchronous signal circuit fault.");
			break;

		case 34:

			strcpy(alarm, "Synchronous pulse signal circuit fault.");
			break;

		case 35:

			strcpy(alarm, "Parallel cable disconnected.");
			break;

		case 36:

			strcpy(alarm, "Load unbalanced.");
			break;

		case 41:

			strcpy(alarm, "Over temperature fault.");
			break;

		case 42:

			strcpy(alarm, "Communication failure between CPUs in control board.");
			break;

		case 43:

			strcpy(alarm, "Overload fault.");
			update_status("OVER");
			break;

		case 44:

			strcpy(alarm, "Fan failure.");
			break;

		case 45:

			strcpy(alarm, "Charger failure.");
			break;

		case 46:

			strcpy(alarm, "Model fault.");
			break;

		case 47:

			strcpy(alarm, "MCU communication fault.");
			break;

		default:

			snprintf(alarm, sizeof(alarm), "Unknown fault [%s]", item->value);
			break;

		}

	}

	snprintf(value, valuelen, item->dfl, alarm);
	upslogx(LOG_INFO, "Fault found: %s", alarm);

	item->qxflags |= QX_FLAG_SKIP;

	return 0;
}

/* Warnings reported by the UPS */
static int	voltronic_warning(item_t *item, char *value, const size_t valuelen)
{
	char	warn[SMALLBUF] = "", unk[SMALLBUF] = "", bitwarns[SMALLBUF] = "", warns[4096] = "";
	int	i;

	if (strspn(item->value, "01") != strlen(item->value)) {
		upsdebugx(2, "%s: invalid reply from the UPS [%s]", __func__, item->value);
		return -1;
	}

	/* No warnings */
	if (strspn(item->value, "0") == strlen(item->value)) {
		return 0;
	}

	snprintf(value, valuelen, "UPS warnings:");

	for (i = 0; i < (int)strlen(item->value); i++) {

		int	u = 0;

		if (item->value[i] == '1') {

			switch (i)
			{
			case 0:

				strcpy(warn, "Battery disconnected.");
				break;

			case 1:

				strcpy(warn, "Neutral not connected.");
				break;

			case 2:

				strcpy(warn, "Site fault.");
				break;

			case 3:

				strcpy(warn, "Phase sequence incorrect.");
				break;

			case 4:

				strcpy(warn, "Phase sequence incorrect in bypass.");
				break;

			case 5:

				strcpy(warn, "Input frequency unstable in bypass.");
				break;

			case 6:

				strcpy(warn, "Battery overcharged.");
				break;

			case 7:

				strcpy(warn, "Low battery.");
				update_status("LB");
				break;

			case 8:

				strcpy(warn, "Overload alarm.");
				update_status("OVER");
				break;

			case 9:

				strcpy(warn, "Fan alarm.");
				break;

			case 10:

				strcpy(warn, "EPO enabled.");
				break;

			case 11:

				strcpy(warn, "Unable to turn on UPS.");
				break;

			case 12:

				strcpy(warn, "Over temperature alarm.");
				break;

			case 13:

				strcpy(warn, "Charger alarm.");
				break;

			case 14:

				strcpy(warn, "Remote auto shutdown.");
				break;

			case 15:

				strcpy(warn, "L1 input fuse not working.");
				break;

			case 16:

				strcpy(warn, "L2 input fuse not working.");
				break;

			case 17:

				strcpy(warn, "L3 input fuse not working.");
				break;

			case 18:

				strcpy(warn, "Positive PFC abnormal in L1.");
				break;

			case 19:

				strcpy(warn, "Negative PFC abnormal in L1.");
				break;

			case 20:

				strcpy(warn, "Positive PFC abnormal in L2.");
				break;

			case 21:

				strcpy(warn, "Negative PFC abnormal in L2.");
				break;

			case 22:

				strcpy(warn, "Positive PFC abnormal in L3.");
				break;

			case 23:

				strcpy(warn, "Negative PFC abnormal in L3.");
				break;

			case 24:

				strcpy(warn, "Abnormal in CAN-bus communication.");
				break;

			case 25:

				strcpy(warn, "Abnormal in synchronous signal circuit.");
				break;

			case 26:

				strcpy(warn, "Abnormal in synchronous pulse signal circuit.");
				break;

			case 27:

				strcpy(warn, "Abnormal in host signal circuit.");
				break;

			case 28:

				strcpy(warn, "Male connector of parallel cable not connected well.");
				break;

			case 29:

				strcpy(warn, "Female connector of parallel cable not connected well.");
				break;

			case 30:

				strcpy(warn, "Parallel cable not connected well.");
				break;

			case 31:

				strcpy(warn, "Battery connection not consistent in parallel systems.");
				break;

			case 32:

				strcpy(warn, "AC connection not consistent in parallel systems.");
				break;

			case 33:

				strcpy(warn, "Bypass connection not consistent in parallel systems.");
				break;

			case 34:

				strcpy(warn, "UPS model types not consistent in parallel systems.");
				break;

			case 35:

				strcpy(warn, "Capacity of UPSes not consistent in parallel systems.");
				break;

			case 36:

				strcpy(warn, "Auto restart setting not consistent in parallel systems.");
				break;

			case 37:

				strcpy(warn, "Battery cell over charge.");
				break;

			case 38:

				strcpy(warn, "Battery protection setting not consistent in parallel systems.");
				break;

			case 39:

				strcpy(warn, "Battery detection setting not consistent in parallel systems.");
				break;

			case 40:

				strcpy(warn, "Bypass not allowed setting not consistent in parallel systems.");
				break;

			case 41:

				strcpy(warn, "Converter setting not consistent in parallel systems.");
				break;

			case 42:

				strcpy(warn, "High loss point for frequency in bypass mode not consistent in parallel systems.");
				break;

			case 43:

				strcpy(warn, "Low loss point for frequency in bypass mode not consistent in parallel systems.");
				break;

			case 44:

				strcpy(warn, "High loss point for voltage in bypass mode not consistent in parallel systems.");
				break;

			case 45:

				strcpy(warn, "Low loss point for voltage in bypass mode not consistent in parallel systems.");
				break;

			case 46:

				strcpy(warn, "High loss point for frequency in AC mode not consistent in parallel systems.");
				break;

			case 47:

				strcpy(warn, "Low loss point for frequency in AC mode not consistent in parallel systems.");
				break;

			case 48:

				strcpy(warn, "High loss point for voltage in AC mode not consistent in parallel systems.");
				break;

			case 49:

				strcpy(warn, "Low loss point for voltage in AC mode not consistent in parallel systems.");
				break;

			case 50:

				strcpy(warn, "Warning for locking in bypass mode after 3 consecutive overloads within 30 min.");
				break;

			case 51:

				strcpy(warn, "Warning for three-phase AC input current unbalance.");
				break;

			case 52:

				strcpy(warn, "Warning for a three-phase input current unbalance detected in battery mode.");
				break;

			case 53:

				strcpy(warn, "Warning for Inverter inter-current unbalance.");
				break;

			case 54:

				strcpy(warn, "Programmable outlets cut off pre-alarm.");
				break;

			case 55:

				strcpy(warn, "Warning for Battery replace.");
				update_status("RB");
				break;

			case 56:

				strcpy(warn, "Abnormal warning on input phase angle.");
				break;

			case 57:

				strcpy(warn, "Warning!! Cover of maintain switch is open.");
				break;

			case 61:

				strcpy(warn, "EEPROM operation error.");
				break;

			default:

				snprintf(warn, sizeof(warn), "Unknown warning from UPS [bit: #%02d]", i + 1);
				u++;
				break;

			}

			upslogx(LOG_INFO, "Warning from UPS: %s", warn);

			if (u) {	/* Unknown warnings */

				snprintfcat(unk, sizeof(unk), ", #%02d", i + 1);

			} else {	/* Known warnings */

				if (strlen(warns) > 0) {

					/* For too long warnings (total) */
					snprintfcat(bitwarns, sizeof(bitwarns), ", #%02d", i + 1);

					/* For warnings (total) not too long */
					snprintfcat(warns, sizeof(warns), " %s", warn);

				} else {

					snprintf(bitwarns, sizeof(bitwarns), "Known (see log or manual) [bit: #%02d", i + 1);
					snprintf(warns, sizeof(warns), "%s", warn);

				}

			}
		}
	}

	/* There's some known warning, at least */
	if (strlen(warns) > 0) {

		/* We have both known and unknown warnings */
		if (strlen(unk) > 0) {

			/* Appending unknown ones to known ones; removing leading comma from unk - 'explicit' */
			snprintfcat(warns, sizeof(warns), " Unknown warnings [bit:%s]", unk+1);

			/* Appending unknown ones to known ones; removing leading comma from unk - 'cryptic' */
			snprintfcat(bitwarns, sizeof(bitwarns), "]; Unknown warnings [bit:%s]", unk+1);

		/* We have only known warnings */
		} else {

			snprintfcat(bitwarns, sizeof(bitwarns), "]");

		}

	/* We have only unknown warnings */
	} else if (strlen(unk) > 0) {

		/* Removing leading comma from unk */
		snprintf(warns, sizeof(warns), "Unknown warnings [bit:%s]", unk+1);
		strcpy(bitwarns, warns);

	} else {

		/* Don't know what happened */
		upsdebugx(2, "%s: failed to process warnings", __func__);
		return -1;

	}

	/* If grand total of warnings doesn't exceed value of alarm (=ST_MAX_VALUE_LEN) minus some space (32) for other alarms.. */
	if ((ST_MAX_VALUE_LEN - 32) > strlen(warns)) {
		/* ..then be explicit.. */
		snprintfcat(value, valuelen, " %s", warns);
	/* ..otherwise.. */
	} else {
		/* ..be cryptic */
		snprintfcat(value, valuelen, " %s", bitwarns);
	}

	return 0;
}

/* Working mode reported by the UPS */
static int	voltronic_mode(item_t *item, char *value, const size_t valuelen)
{
	char	*status = NULL, *alarm = NULL;

	switch (item->value[0])
	{
	case 'P':

		alarm = "UPS is going ON";
		break;

	case 'S':

		status = "OFF";
		break;

	case 'Y':

		status = "BYPASS";
		break;

	case 'L':

		status = "OL";
		break;

	case 'B':

		status = "!OL";
		break;

	case 'T':

		status = "CAL";
		break;

	case 'F':

		alarm = "Fault reported by UPS.";
		break;

	case 'E':

		alarm = "UPS is in ECO Mode.";
		break;

	case 'C':

		alarm = "UPS is in Converter Mode.";
		break;

	case 'D':

		alarm = "UPS is shutting down!";
		status = "FSD";
		break;

	default:

		upsdebugx(2, "%s: invalid reply from the UPS [%s]", __func__, item->value);
		return -1;

	}

	if (alarm && !strcasecmp(item->info_type, "ups.alarm")) {

		snprintf(value, valuelen, item->dfl, alarm);

	} else if (status && !strcasecmp(item->info_type, "ups.status")) {

		snprintf(value, valuelen, item->dfl, status);

	}

	return 0;
}

/* Process status bits */
static int	voltronic_status(item_t *item, char *value, const size_t valuelen)
{
	char	*val = "";

	if (strspn(item->value, "01") != strlen(item->value)) {
		upsdebugx(3, "%s: unexpected value %s@%d->%s", __func__, item->value, item->from, item->value);
		return -1;
	}

	switch (item->from)
	{
	case 63:	/* UPS Type - ups.type */

		{
			int	type = strtol(item->value, NULL, 10);

			if (!type)		/* 00 -> Offline */
				val = "offline";
			else if (type == 1)	/* 01 -> Line-interactive */
				val = "line-interactive";
			else if (type == 10)	/* 10 -> Online */
				val = "online";
			else {
				upsdebugx(2, "%s: invalid type [%s: %s]", __func__, item->info_type, item->value);
				return -1;
			}
		}

		break;

	case 65:	/* Utility Fail (Immediate) - ups.status */

		if (item->value[0] == '1')
			val = "!OL";
		else
			val = "OL";
		break;

	case 66:	/* Battery Low - ups.status */

		if (item->value[0] == '1')
			val = "LB";
		else
			val = "!LB";
		break;

	case 67:	/* Bypass/Boost or Buck Active - ups.{status,alarm} */

		if (item->value[0] == '1') {

			double	vi, vo;

			vi = strtod(dstate_getinfo("input.voltage"), NULL);
			vo = strtod(dstate_getinfo("output.voltage"), NULL);

			if (vo < 0.5 * vi) {
				upsdebugx(2, "%s: output voltage too low", __func__);
				return -1;
			}

			if (vo < 0.95 * vi) {
				val = "TRIM";
			} else if (vo < 1.05 * vi) {

				int	prot = strtol(dstate_getinfo("ups.firmware.aux")+1, NULL, 10);

				if (!prot || prot == 8) {	/* ups.alarm */

					if (!strcasecmp(item->info_type, "ups.alarm"))
						val = "UPS is in AVR Mode.";

				} else {	/* ups.status */

					if (!strcasecmp(item->info_type, "ups.status"))
						val = "BYPASS";

				}

			} else if (vo < 1.5 * vi) {
				val = "BOOST";
			} else {
				upsdebugx(2, "%s: output voltage too high", __func__);
				return -1;
			}

		}

		break;

	case 68:	/* UPS Fault - ups.alarm */

		if (item->value[0] == 1) {

			item_t	*faultitem;

			for (faultitem = voltronic_qx2nut; faultitem->info_type != NULL; faultitem++) {

				if (!faultitem->command)
					continue;

				if (!strcasecmp(faultitem->command, "QFS\r")) {
					faultitem->qxflags &= ~QX_FLAG_SKIP;
					break;
				}

			}

			val = "UPS Fault!";

		}

		break;

/*	case 69:	*//* unknown */
/*		break;*/

	case 70:	/* Test in Progress - ups.status */

		if (item->value[0] == '1')
			val = "CAL";
		else
			val = "!CAL";
		break;

	case 71:	/* Shutdown Active - ups.status */

		if (item->value[0] == '1')
			val = "FSD";
		else
			val = "!FSD";
		break;

	case 72:	/* Beeper status - ups.beeper.status */

		/* The UPS has the ability to enable/disable the alarm (from UPS capability) */
		if (alarm_control) {

			const char	*beeper = dstate_getinfo("ups.beeper.status");

			if (!beeper || strcasecmp(beeper, "disabled")) {

				if (item->value[0] == '0')	/* Beeper On */
					val = "enabled";
				else
					val = "muted";

			}

		/* The UPS lacks the ability to enable/disable the alarm (from UPS capability) */
		} else {

			if (item->value[0] == '0')	/* Beeper On */
				val = "enabled";
			else
				val = "disabled";

		}

		break;

/*	case 73:	*//* unknown */
/*		break;*/
/*	case 74:	*//* unknown */
/*		break;*/

	default:
		/* Don't know what happened */
		return -1;
	}

	snprintf(value, valuelen, "%s", val);

	return 0;
}

/* Output power factor */
static int	voltronic_output_powerfactor(item_t *item, char *value, const size_t valuelen)
{
	double	opf;

	if (strspn(item->value, "0123456789 .") != strlen(item->value)) {
		upsdebugx(2, "%s: non numerical value [%s: %s]", __func__, item->info_type, item->value);
		return -1;
	}

	/* UPS report a value expressed in % so -> output.powerfactor*100 e.g. opf = 0,8 -> ups = 80 */
	opf = strtod(item->value, NULL) * 0.01;

	snprintf(value, valuelen, item->dfl, opf);

	return 0;
}

/* UPS serial number */
static int	voltronic_serial_numb(item_t *item, char *value, const size_t valuelen)
{
	/* If the UPS report a 00..0 serial we'll log it but we won't store it in device.serial */
	if (strspn(item->value, "0") == strlen(item->value)) {
		upslogx(LOG_INFO, "%s: UPS reported a non-significant serial [%s]", item->info_type, item->value);
		return -1;
	}

	snprintf(value, valuelen, item->dfl, item->value);
	return 0;
}

/* Outlet status */
static int	voltronic_outlet(item_t *item, char *value, const size_t valuelen)
{
	const char	*status, *switchable;
	char		number = item->info_type[7],
			buf[SMALLBUF];
	item_t		*outlet_item;

	switch (item->value[0])
	{
	case '1':

		switchable = "yes";
		status = "on";
		break;

	case '0':

		switchable = "yes";
		status = "off";
		break;

	default:

		upsdebugx(2, "%s: invalid reply from the UPS [%s: %s]", __func__, item->info_type, item->value);
		return -1;

	}

	if (strstr(item->info_type, "switchable")) {

		snprintf(value, valuelen, item->dfl, switchable);

	} else if (strstr(item->info_type, "status")) {

		snprintf(value, valuelen, item->dfl, status);

	} else {

		/* Don't know what happened */
		return -1;

	}

	/* Unskip outlet.n.delay.shutdown */
	snprintf(buf, sizeof(buf), "outlet.%c.delay.shutdown", number);

	outlet_item = find_nut_info(buf, QX_FLAG_SEMI_STATIC, 0);

	/* Don't know what happened*/
	if (!outlet_item)
		return -1;

	outlet_item->qxflags &= ~QX_FLAG_SKIP;

	/* Unskip outlet.n.load.on */
	snprintf(buf, sizeof(buf), "outlet.%c.load.on", number);

	outlet_item = find_nut_info(buf, QX_FLAG_CMD, 0);

	/* Don't know what happened*/
	if (!outlet_item)
		return -1;

	outlet_item->qxflags &= ~QX_FLAG_SKIP;

	/* Unskip outlet.n.load.off */
	snprintf(buf, sizeof(buf), "outlet.%c.load.off", number);

	outlet_item = find_nut_info(buf, QX_FLAG_CMD, 0);

	/* Don't know what happened*/
	if (!outlet_item)
		return -1;

	outlet_item->qxflags &= ~QX_FLAG_SKIP;

	return 0;
}

/* Outlet delay time */
static int	voltronic_outlet_delay(item_t *item, char *value, const size_t valuelen)
{
	char	number = item->info_type[7],
		buf[SMALLBUF];
	double	val;
	item_t	*setvar_item;

	if (strspn(item->value, "0123456789 .") != strlen(item->value)) {
		upsdebugx(2, "%s: non numerical value [%s: %s]", __func__, item->info_type, item->value);
		return -1;
	}

	/* UPS reports minutes, NUT expects seconds */
	val = strtod(item->value, NULL) * 60;

	snprintf(value, valuelen, item->dfl, val);

	/* Unskip outlet.n.delay.shutdown setvar */
	snprintf(buf, sizeof(buf), "outlet.%c.delay.shutdown", number);

	setvar_item = find_nut_info(buf, QX_FLAG_SETVAR, 0);

	/* Don't know what happened*/
	if (!setvar_item)
		return -1;

	setvar_item->qxflags &= ~QX_FLAG_SKIP;

	return 0;
}

/* *SETVAR* Outlet delay time */
static int	voltronic_outlet_delay_set(item_t *item, char *value, const size_t valuelen)
{
	int	delay = strtol(value, NULL, 10);

	/* From seconds to minute */
	delay = delay / 60;

	snprintf(value, valuelen, item->command, delay);

	return 0;
}

/* Type of battery */
static int	voltronic_p31b(item_t *item, char *value, const size_t valuelen)
{
	int	val;

	if ((item->value[0] != '0') || (strspn(item->value+1, "012") != 1)) {

		upsdebugx(2, "%s: invalid battery type reported by the UPS [%s]", __func__, item->value);
		return -1;

	}

	val = strtol(item->value, NULL, 10);

	snprintf(value, valuelen, item->dfl, item->info_rw[val].value);

	return 0;
}

/* *SETVAR* Type of battery */
static int	voltronic_p31b_set(item_t *item, char *value, const size_t valuelen)
{
	int	i;

	for (i = 0; strlen(item->info_rw[i].value) > 0; i++) {

		if (!strcasecmp(item->info_rw[i].value, value))
			break;

	}

	/* At this point value should already be checked against enum so this shouldn't happen.. however.. */
	if (i >= (int)(sizeof(item->info_rw) / sizeof(item->info_rw[0]))) {
		upslogx(LOG_ERR, "%s: value [%s] out of range", item->info_type, value);
		return -1;
	}

	snprintf(value, valuelen, "%d", i);

	return voltronic_process_setvar(item, value, valuelen);
}

/* *NONUT* Actual device grid working range type for P31 UPSes */
static int	voltronic_p31g(item_t *item, char *value, const size_t valuelen)
{
	int	val;

	if ((item->value[0] != '0') || (strspn(item->value+1, "01") != 1)) {

		upsdebugx(2, "%s: invalid device grid working range reported by the UPS [%s]", __func__, item->value);
		return -1;

	}

	val = strtol(item->value, NULL, 10);

	snprintf(value, valuelen, item->dfl, item->info_rw[val].value);
	work_range_type = val;

	return 0;
}

/* *SETVAR/NONUT* Device grid working range type for P31 UPSes */
static int	voltronic_p31g_set(item_t *item, char *value, const size_t valuelen)
{
	int	i;

	for (i = 0; strlen(item->info_rw[i].value) > 0; i++) {

		if (!strcasecmp(item->info_rw[i].value, value))
			break;

	}

	/* At this point value should have been already checked against enum so this shouldn't happen.. however.. */
	if (i >= (int)(sizeof(item->info_rw) / sizeof(item->info_rw[0]))) {
		upslogx(LOG_ERR, "%s: value [%s] out of range", item->info_type, value);
		return -1;
	}

	if (i == work_range_type) {
		upslogx(LOG_INFO, "%s is already set to %s", item->info_type, item->info_rw[i].value);
		return -1;
	}

	snprintf(value, valuelen, "%d", i);

	return voltronic_process_setvar(item, value, valuelen);
}

/* *NONUT* UPS actual input/output phase angles */
static int	voltronic_phase(item_t *item, char *value, const size_t valuelen)
{
	int	angle;

	if (strspn(item->value, "0123456789 .") != strlen(item->value)) {
		upsdebugx(2, "%s: non numerical value [%s: %s]", __func__, item->info_type, item->value);
		return -1;
	}

	angle = strtol(item->value, NULL, 10);

	if (!strcasecmp(item->info_type, "output_phase_angle")) {

		output_phase_angle = angle;

		/* User-provided value to change.. */
		if (getval(item->info_type)) {

			item_t	*unskip;

			/* Unskip output_phase_angle setvar */
			unskip = find_nut_info(item->info_type, QX_FLAG_SETVAR, 0);

			/* Don't know what happened */
			if (!unskip)
				return -1;

			unskip->qxflags &= ~QX_FLAG_SKIP;

		}

	}

	snprintf(value, valuelen, item->dfl, angle);

	return 0;
}

/* *SETVAR/NONUT* Output phase angle */
static int	voltronic_phase_set(item_t *item, char *value, const size_t valuelen)
{
	int	i;

	for (i = 0; strlen(item->info_rw[i].value) > 0; i++) {

		if (!strcasecmp(item->info_rw[i].value, value))
			break;

	}

	/* At this point value should have been already checked against enum so this shouldn't happen.. however.. */
	if (i >= (int)(sizeof(item->info_rw) / sizeof(item->info_rw[0]))) {
		upslogx(LOG_ERR, "%s: value [%s] out of range", item->info_type, value);
		return -1;
	}

	if (strtol(item->info_rw[i].value, NULL, 10) == output_phase_angle) {
		upslogx(LOG_INFO, "%s is already set to %s", item->info_type, item->info_rw[i].value);
		return -1;
	}

	snprintf(value, valuelen, "%d", i);

	return voltronic_process_setvar(item, value, valuelen);
}

/* *NONUT* UPS is master/slave in a system of UPSes in parallel */
static int	voltronic_parallel(item_t *item, char *value, const size_t valuelen)
{
	char	*type;

	if (strlen(item->value) != strspn(item->value, "0123456789")) {
		upsdebugx(2, "%s: non numerical value [%s: %s]", __func__, item->info_type, item->value);
		return -1;
	}

	/* 001 for master UPS, 002 and 003 for slave UPSes */
	switch (strtol(item->value, NULL, 10))
	{
	case 1:

		type = "master";
		break;

	case 2:
	case 3:

		type = "slave";
		break;

	default:

		upsdebugx(2, "%s: invalid reply from the UPS [%s]", __func__, item->value);
		return -1;

	}

	snprintf(value, valuelen, "This UPS is *%s* in a system of UPSes in parallel", type);

	return 0;
}


/* == Subdriver interface == */
subdriver_t	voltronic_subdriver = {
	VOLTRONIC_VERSION,
	voltronic_claim,
	voltronic_qx2nut,
	NULL,
	NULL,
	voltronic_makevartable,
	"ACK",
	"(NAK\r",
#ifdef TESTING
	voltronic_testing,
#endif	/* TESTING */
};
