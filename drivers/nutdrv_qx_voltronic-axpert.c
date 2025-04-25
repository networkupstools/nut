/* nutdrv_qx_voltronic-axpert.c - Subdriver for Voltronic Power Axpert
 *
 * Copyright (C)
 *   2014 Daniele Pezzini <hyouko@gmail.com>
 *   2022 Graham Leggett <minfrin@sharp.fm>
 *   2025 Jim Klimov <jimklimov+nut@gmail.com>
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

#include "nutdrv_qx_voltronic-axpert.h"
#include "common_voltronic-crc.h"

#define VOLTRONIC_AXPERT_VERSION	"Voltronic-Axpert 0.01"

/* Support functions */
static int	voltronic_sunny_claim(void);
static void	voltronic_axpert_initinfo(void);
static void	voltronic_sunny_makevartable(void);
static int	voltronic_axpert_clear_flags(const char *varname, const unsigned long flag, const unsigned long noflag, const unsigned long clearflag);
static int	voltronic_axpert_add_flags(const char *varname, const unsigned long flag, const unsigned long noflag, const unsigned long addflag);
static int	voltronic_sunny_checksum(const char *string);
static void	voltronic_sunny_update_related_vars_limits(item_t *item, const char *value);
static int	voltronic_sunny_OEEPB(void);

/* Range/enum functions */
static int	voltronic_sunny_pv_priority_enum(char *value, const size_t len);
static int	voltronic_sunny_grid_inout_freq_max(char *value, const size_t len);
static int	voltronic_sunny_grid_inout_freq_min(char *value, const size_t len);
static int	voltronic_sunny_bc_v_bulk(char *value, const size_t len);
static int	voltronic_sunny_pv_input_volt_max(char *value, const size_t len);

/* Answer preprocess functions */
static int	voltronic_axpert_checkcrc(item_t *item, const int len);

/* Command preprocess functions */
static int      voltronic_axpert_crc(item_t *item, char *command, const size_t commandlen);
static int	voltronic_sunny_fault_query(item_t *item, char *command, const size_t commandlen);
static int	voltronic_sunny_energy_hour(item_t *item, char *command, const size_t commandlen);
static int	voltronic_sunny_energy_day(item_t *item, char *command, const size_t commandlen);
static int	voltronic_sunny_energy_month(item_t *item, char *command, const size_t commandlen);
static int	voltronic_sunny_energy_year(item_t *item, char *command, const size_t commandlen);

/* Preprocess functions */
static int      voltronic_axpert_hex_preprocess(item_t *item, char *value, const size_t valuelen);
static int	voltronic_sunny_basic_preprocess(item_t *item, char *value, const size_t valuelen);
static int	voltronic_axpert_protocol(item_t *item, char *value, const size_t valuelen);
static int	voltronic_axpert_fw(item_t *item, char *value, const size_t valuelen);
static int	voltronic_axpert_serial_numb(item_t *item, char *value, const size_t valuelen);
static int	voltronic_axpert_capability(item_t *item, char *value, const size_t valuelen);
static int	voltronic_axpert_capability_set(item_t *item, char *value, const size_t valuelen);
static int	voltronic_axpert_capability_reset(item_t *item, char *value, const size_t valuelen);
static int	voltronic_axpert_capability_set_nonut(item_t *item, char *value, const size_t valuelen);
static int	voltronic_sunny_01(item_t *item, char *value, const size_t valuelen);
static int	voltronic_sunny_01_set(item_t *item, char *value, const size_t valuelen);
static int	voltronic_sunny_pv_priority(item_t *item, char *value, const size_t valuelen);
static int	voltronic_sunny_pv_priority_set(item_t *item, char *value, const size_t valuelen);
static int	voltronic_sunny_unskip_setvar(item_t *item, char *value, const size_t valuelen);
static int      voltronic_axpert_qpiri_battery_type(item_t *item, char *value, const size_t valuelen);
static int	voltronic_axpert_qpiri_model_type(item_t *item, char *value, const size_t valuelen);
static int	voltronic_axpert_transformer(item_t *item, char *value, const size_t valuelen);
static int	voltronic_sunny_volt_nom_set(item_t *item, char *value, const size_t valuelen);
static int	voltronic_sunny_process_setvar(item_t *item, char *value, const size_t valuelen);
static int	voltronic_sunny_basic_preprocess_and_update_related_vars_limits(item_t *item, char *value, const size_t valuelen);
static int	voltronic_sunny_yymmdd(item_t *item, char *value, const size_t valuelen);
static int	voltronic_sunny_hh_mm(item_t *item, char *value, const size_t valuelen);
static int	voltronic_sunny_lst(item_t *item, char *value, const size_t valuelen);
static int	voltronic_sunny_set_limits(item_t *item, char *value, const size_t valuelen);
static int	voltronic_sunny_unskip_setvar_and_update_related_vars_limits(item_t *item, char *value, const size_t valuelen);
static int	voltronic_sunny_charger_limits_set(item_t *item, char *value, const size_t valuelen);
static int	voltronic_sunny_discharging_limits_set(item_t *item, char *value, const size_t valuelen);
static int	voltronic_sunny_hhmm(item_t *item, char *value, const size_t valuelen);
static int	voltronic_sunny_hhmm_set(item_t *item, char *value, const size_t valuelen);
static int	voltronic_sunny_hhmm_x2_set(item_t *item, char *value, const size_t valuelen);
static int	voltronic_sunny_pf(item_t *item, char *value, const size_t valuelen);
static int	voltronic_sunny_pfc_set(item_t *item, char *value, const size_t valuelen);
static int	voltronic_sunny_date(item_t *item, char *value, const size_t valuelen);
static int	voltronic_sunny_date_set(item_t *item, char *value, const size_t valuelen);
static int	voltronic_sunny_time(item_t *item, char *value, const size_t valuelen);
static int	voltronic_sunny_time_set(item_t *item, char *value, const size_t valuelen);
static int	voltronic_sunny_process_sign(item_t *item, char *value, const size_t valuelen);
static int	voltronic_axpert_status(item_t *item, char *value, const size_t valuelen);
static int	voltronic_sunny_warning(item_t *item, char *value, const size_t valuelen);
static int	voltronic_axpert_mode(item_t *item, char *value, const size_t valuelen);
static int	voltronic_sunny_batt_runtime(item_t *item, char *value, const size_t valuelen);
static int	voltronic_sunny_fault(item_t *item, char *value, const size_t valuelen);
static int	voltronic_sunny_date_skip_me(item_t *item, char *value, const size_t valuelen);
static int	voltronic_sunny_time_skip_me(item_t *item, char *value, const size_t valuelen);
static int	voltronic_sunny_skip_me(item_t *item, char *value, const size_t valuelen);
static int	voltronic_sunny_fault_status(item_t *item, char *value, const size_t valuelen);
static int	voltronic_sunny_fault_id(item_t *item, char *value, const size_t valuelen);
static int	voltronic_sunny_self_test_result(item_t *item, char *value, const size_t valuelen);


/* == Global vars == */

/* Capability vars ("enabled"/"disabled") */
static char	*bypass_alarm,
		*battery_alarm;

/* Global flags */
static int	crc = 1,		/* Whether device puts CRC in its replies or not */
		line_loss = 0,		/* Whether device has lost connection to the grid or not */
		pv_loss = 0;		/* Whether devoce has lost connection to PV or not */

static double	fw = 0;			/* Firmware version */
static int	protocol = 0,		/* Protocol used by device */
		model_id = 0,		/* Model ID */
		model_type = 0,		/* Model type */
		fault_id;		/* Fault ID */


/* == Ranges/enums/lengths == */

/* Enumlist for battery type */
static info_rw_t	voltronic_axpert_e_battery_type[] = {
	{ "AGM", 0 },				/* Type:  0 */
	{ "Flooded", 0 },			/* Type:  1 */
	{ "User", 0 },				/* Type:  2 */
	{ "", 0 }

};
/* Enumlist for device model type */
static info_rw_t	voltronic_axpert_e_model_type[] = {
	{ "Grid-tie", 0 },			/* Type:  0 */
	{ "Off-grid", 0 },			/* Type:  1 */
	{ "Hybrid", 0 },			/* Type: 10 */
	{ "Off Grid with 2 Trackers", 0 },	/* Type: 11 */
	{ "Off Grid with 3 Trackers", 0 },      /* Type: 20 */
	{ "", 0 }
};

/* Enumlist for device capabilities that have a NUT var */
static info_rw_t	voltronic_axpert_e_cap[] = {
	{ "no", 0 },
	{ "yes", 0 },
	{ "", 0 }
};

/* Enumlist for NONUT capabilities */
static info_rw_t	voltronic_axpert_e_cap_nonut[] = {
	{ "disabled", 0 },
	{ "enabled", 0 },
	{ "", 0 }
};

/* Enumlist for PV energy supply priority */
static info_rw_t	voltronic_sunny_e_pv_priority[] = {
	{ "Battery-Load", voltronic_sunny_pv_priority_enum },				/* Priority: 01; Type: Off-grid (01) */
	{ "Load-Battery", voltronic_sunny_pv_priority_enum },				/* Priority: 02; Type: Off-grid (01); Model: 150 */
	{ "Load-Battery (grid relay disconnected)", voltronic_sunny_pv_priority_enum },	/* Priority: 02; Type: Off-grid (01); Model: 151 */
	{ "Battery-Load-Grid", voltronic_sunny_pv_priority_enum },			/* Priority: 01; Type: Grid-tie with backup (10) */
	{ "Load-Battery-Grid", voltronic_sunny_pv_priority_enum },			/* Priority: 02; Type: Grid-tie with backup (10) */
	{ "Load-Grid-Battery", voltronic_sunny_pv_priority_enum },			/* Priority: 03; Type: Grid-tie with backup (10) */
	{ "", 0 }
};

/* Preprocess enum value for PV energy supply priority */
static int	voltronic_sunny_pv_priority_enum(char *value, const size_t len)
{
	NUT_UNUSED_VARIABLE(len);	/* FIXME? strncasecmp(value, expected, len) but make sure we check the whole fixed argument or it is not equal */

	switch (model_type)
	{
	case 1:		/* Off-grid */
		if (
			!strcasecmp(value, "Battery-Load") ||				/* Priority: 01 */
			!strcasecmp(value, "Load-Battery") ||				/* Priority: 02; Model: 150 */
			!strcasecmp(value, "Load-Battery (grid relay disconnected)")	/* Priority: 02; Model: 151 */
		)
			return 0;
		break;
	case 10:	/* Grid-tie with backup */
		if (
			!strcasecmp(value, "Battery-Load-Grid") ||			/* Priority: 01 */
			!strcasecmp(value, "Load-Battery-Grid") ||			/* Priority: 02 */
			!strcasecmp(value, "Load-Grid-Battery")				/* Priority: 03 */
		)
			return 0;
		break;
	case 0:		/* Grid-tie */
	case 11:	/* Self-use */
	default:
		break;
	}

	return -1;
}

/* Enumlist for nominal voltage */
static info_rw_t	voltronic_axpert_e_volt_nom[] = {
	{ "101", 0 },	/* Low voltage models */
	{ "110", 0 },	/* Low voltage models */
	{ "120", 0 },	/* Low voltage models */
	{ "127", 0 },	/* Low voltage models */
	{ "202", 0 },	/* High voltage models */
	{ "208", 0 },	/* High voltage models */
	{ "220", 0 },	/* High voltage models */
	{ "230", 0 },	/* High voltage models */
	{ "240", 0 },	/* High voltage models */
	{ "", 0 }
};

/* Enumlist for nominal frequency */
static info_rw_t	voltronic_sunny_e_freq_nom[] = {
	{ "50", 0 },
	{ "60", 0 },
	{ "", 0 }
};

/* Range for number of MPP trackers in use */
static info_rw_t	voltronic_sunny_r_mpp_number[] = {
	{ "01", 0 },
	{ "99", 0 },
	{ "", 0 }
};

/* Range for maximum grid output voltage: overwritten (if appropriate) at runtime by voltronic_sunny_set_limits() (QVFTR #1, #2) */
static info_rw_t	voltronic_sunny_r_grid_output_volt_max[] = {
	{ "240", 0 },
	{ "276", 0 },
	{ "", 0 }
};

/* Range for maximum grid input voltage: overwritten (if appropriate) at runtime by voltronic_sunny_set_limits() (QVFTR #1, #2) */
static info_rw_t	voltronic_sunny_r_grid_input_volt_max[] = {
	{ "240", 0 },
	{ "280", 0 },
	{ "", 0 }
};

/* Range for minimum grid output voltage: overwritten (if appropriate) at runtime by voltronic_sunny_set_limits() (QVFTR #3, #4) */
static info_rw_t	voltronic_sunny_r_grid_output_volt_min[] = {
	{ "176", 0 },
	{ "220", 0 },
	{ "", 0 }
};

/* Range for minimum grid input voltage: overwritten (if appropriate) at runtime by voltronic_sunny_set_limits() (QVFTR #3, #4) */
static info_rw_t	voltronic_sunny_r_grid_input_volt_min[] = {
	{ "175", 0 },
	{ "220", 0 },
	{ "", 0 }
};

/* Range for maximum grid input/output frequency: overwritten (if appropriate) at runtime by voltronic_sunny_set_limits() (QVFTR #5, #6) */
static info_rw_t	voltronic_sunny_r_grid_inout_freq_max[] = {	/* FIXME: ranges only support ints */
	{ "50.2", voltronic_sunny_grid_inout_freq_max },	/* Nominal frequency (QPIRI #2) == 50.0 */
	{ "54.8", voltronic_sunny_grid_inout_freq_max },	/* Nominal frequency (QPIRI #2) == 50.0 */
	{ "60.2", voltronic_sunny_grid_inout_freq_max },	/* Nominal frequency (QPIRI #2) == 60.0 */
	{ "64.8", voltronic_sunny_grid_inout_freq_max },	/* Nominal frequency (QPIRI #2) == 60.0 */
	{ "", 0 }
};

/* Preprocess range value for (not overwritten) maximum grid input/output frequency */
static int	voltronic_sunny_grid_inout_freq_max(char *value, const size_t len)
{
	char		*ptr;
	const int	val = strtol(value, &ptr, 10) * 10 + (*ptr == '.' ? strtol(++ptr, NULL, 10) : 0);
	double		gfn;
	const char	*gridfreqnom = dstate_getinfo("grid.frequency.nominal");

	NUT_UNUSED_VARIABLE(len);	/* FIXME? */

	if (!gridfreqnom) {
		upsdebugx(2, "%s: unable to get grid.frequency.nominal", __func__);
		return -1;
	}

	gfn = strtod(gridfreqnom, NULL);

	switch (val)
	{
	case 502:
	case 548:
		/* Nominal frequency (QPIRI #2) == 50.0 */
		if (gfn == 50.0)
			return 0;
		break;
	case 602:
	case 648:
		/* Nominal frequency (QPIRI #2) == 60.0 */
		if (gfn == 60.0)
			return 0;
		break;
	default:
		upsdebugx(2, "%s: unknown value (%s)", __func__, value);
		break;
	}

	return -1;
}

/* Range for minimum grid input/output frequency: overwritten (if appropriate) at runtime by voltronic_sunny_set_limits() (QVFTR #7, #8) */
static info_rw_t	voltronic_sunny_r_grid_inout_freq_min[] = {	/* FIXME: ranges only support ints */
	{ "45.0", voltronic_sunny_grid_inout_freq_min },	/* Nominal frequency (QPIRI #2) == 50.0 */
	{ "49.8", voltronic_sunny_grid_inout_freq_min },	/* Nominal frequency (QPIRI #2) == 50.0 */
	{ "55.0", voltronic_sunny_grid_inout_freq_min },	/* Nominal frequency (QPIRI #2) == 60.0 */
	{ "59.8", voltronic_sunny_grid_inout_freq_min },	/* Nominal frequency (QPIRI #2) == 60.0 */
	{ "", 0 }
};

/* Preprocess range value for (not overwritten) minimum grid input/output frequency */
static int	voltronic_sunny_grid_inout_freq_min(char *value, const size_t len)
{
	char		*ptr;
	const int	val = strtol(value, &ptr, 10) * 10 + (*ptr == '.' ? strtol(++ptr, NULL, 10) : 0);
	double		gfn;
	const char	*gridfreqnom = dstate_getinfo("grid.frequency.nominal");

	NUT_UNUSED_VARIABLE(len);	/* FIXME? */

	if (!gridfreqnom) {
		upsdebugx(2, "%s: unable to get grid.frequency.nominal", __func__);
		return -1;
	}

	gfn = strtod(gridfreqnom, NULL);

	switch (val)
	{
	case 450:
	case 498:
		/* Nominal frequency (QPIRI #2) == 50.0 */
		if (gfn == 50.0)
			return 0;
		break;
	case 550:
	case 598:
		/* Nominal frequency (QPIRI #2) == 60.0 */
		if (gfn == 60.0)
			return 0;
		break;
	default:
		upsdebugx(2, "%s: unknown value (%s)", __func__, value);
		break;
	}

	return -1;
}

/* Range for waiting time before grid connection: overwritten (if appropriate) at runtime by voltronic_sunny_set_limits() (QVFTR #9, #10) */
static info_rw_t	voltronic_sunny_r_grid_waiting_time[] = {
	{ "5", 0 },
	{ "999", 0 },
	{ "", 0 }
};

/* Range for maximum battery-charging current: filled at runtime by voltronic_sunny_set_limits() (QVFTR #11, #12), overwritten (if appropriate) by voltronic_sunny_update_related_vars_limits() */
static info_rw_t	voltronic_sunny_r_bc_v_floating[] = {
	{ "", 0 },
	{ "", 0 },
	{ "", 0 }
};

/* Range for maximum battery-charging current: overwritten (if appropriate) at runtime by voltronic_sunny_set_limits() (QVFTR #13, #14) and voltronic_sunny_update_related_vars_limits() */
static info_rw_t	voltronic_sunny_r_bc_c_max[] = {	/* FIXME: ranges only support ints */
	{ "0.5", 0 },
	{ "25.0", 0 },
	{ "", 0 }
};

/* Range for bulk battery-charging voltage: overwritten (if appropriate) at runtime by voltronic_sunny_set_limits() (QVFTR #25, #26) and voltronic_sunny_update_related_vars_limits() */
static info_rw_t	voltronic_sunny_r_bc_v_bulk[] = {
	{ "47", 0 },
	{ "50", voltronic_sunny_bc_v_bulk },	/* IFF FW version (QSVFW2) >= 0.3 */
	{ "57", voltronic_sunny_bc_v_bulk },	/* IFF FW version (QSVFW2) < 0.3 */
	{ "", 0 }
};

/* Preprocess range value for (not overwritten) bulk battery-charging voltage */
static int	voltronic_sunny_bc_v_bulk(char *value, const size_t len)
{
	const int	val = strtol(value, NULL, 10);

	NUT_UNUSED_VARIABLE(len);	/* FIXME? */

	switch (val)
	{
	case 50:	/* FW version (QSVFW2) >= 0.3 */
		if (fw >= 0.3)
			return 0;
		break;
	case 57:	/* FW version (QSVFW2) < 0.3 */
		if (fw < 0.3)
			return 0;
		break;
	default:
		upsdebugx(2, "%s: unknown value (%s)", __func__, value);
		break;
	}

	return -1;
}

/* Range for minimum floating battery-charging current: filled at runtime by voltronic_sunny_update_related_vars_limits() */
static info_rw_t	voltronic_sunny_r_bc_c_floating_low[] = {
	{ "0", 0 },
	{ "", 0 },
	{ "", 0 }
};

/* Range for restart battery-charging voltage: filled at runtime by voltronic_sunny_update_related_vars_limits() */
static info_rw_t	voltronic_sunny_r_bc_v_restart[] = {
	{ "0", 0 },
	{ "", 0 },
	{ "", 0 }
};

/* Range for floating battery-charging current time thershold */
static info_rw_t	voltronic_sunny_r_bc_time_threshold[] = {
	{ "0", 0 },
	{ "900", 0 },
	{ "", 0 }
};

/* Range for cut-off battery-discharging voltage when grid is not available: overwritten (if appropriate) at runtime by voltronic_sunny_update_related_vars_limits() */
static info_rw_t	voltronic_sunny_r_bd_v_cutoff_gridoff[] = {
	{ "40", 0 },
	{ "48", 0 },
	{ "", 0 }
};

/* Range for cut-off battery-discharging voltage when grid is available: overwritten (if appropriate) at runtime by voltronic_sunny_update_related_vars_limits() */
static info_rw_t	voltronic_sunny_r_bd_v_cutoff_gridon[] = {
	{ "40", 0 },
	{ "48", 0 },
	{ "", 0 }
};

/* Range for restart battery-discharging voltage when grid is unavailable: filled at runtime by voltronic_sunny_update_related_vars_limits() */
static info_rw_t	voltronic_sunny_r_bd_v_restart_gridoff[] = {
	{ "", 0 },
	{ "", 0 },
	{ "", 0 }
};

/* Range for restart battery-discharging voltage when grid is available: filled at runtime by voltronic_sunny_update_related_vars_limits() */
static info_rw_t	voltronic_sunny_r_bd_v_restart_gridon[] = {
	{ "", 0 },
	{ "", 0 },
	{ "", 0 }
};

/* Range for maximum PV input voltage: overwritten (if appropriate) at runtime by voltronic_sunny_set_limits() (QVFTR #15, #16) */
static info_rw_t	voltronic_sunny_r_pv_input_volt_max[] = {
	{ "450", 0 },
	{ "510", voltronic_sunny_pv_input_volt_max },	/* P16 */
	{ "550", voltronic_sunny_pv_input_volt_max },	/* P15 */
	{ "", 0 }
};

/* Preprocess range value for (not overwritten) maximum PV input voltage */
static int	voltronic_sunny_pv_input_volt_max(char *value, const size_t len)
{
	const int	val = strtol(value, NULL, 10);

	NUT_UNUSED_VARIABLE(len);	/* FIXME? */

	switch (val)
	{
	case 510:	/* P16 */
		if (protocol == 16)
			return 0;
		break;
	case 550:	/* P15 */
		if (protocol == 15)
			return 0;
		break;
	default:
		upsdebugx(2, "%s: unknown value (%s)", __func__, value);
		break;
	}

	return -1;
}

/* Range for minimum PV input voltage: overwritten (if appropriate) at runtime by voltronic_sunny_set_limits() (QVFTR #17, #18) */
static info_rw_t	voltronic_sunny_r_pv_input_volt_min[] = {
	{ "90", 0 },
	{ "200", 0 },
	{ "", 0 }
};

/* Range for maximum MPP voltage: overwritten (if appropriate) at runtime by voltronic_sunny_set_limits() (QVFTR #19, #20) */
static info_rw_t	voltronic_sunny_r_mpp_input_volt_max[] = {
	{ "400", 0 },
	{ "450", 0 },
	{ "", 0 }
};

/* Range for minimum MPP voltage: overwritten (if appropriate) at runtime by voltronic_sunny_set_limits() (QVFTR #21, #22) */
static info_rw_t	voltronic_sunny_r_mpp_input_volt_min[] = {
	{ "110", 0 },
	{ "200", 0 },
	{ "", 0 }
};

/* Range for maximum output power: filled(/overwritten, if appropriate) at runtime by voltronic_sunny_set_limits() (QVFTR #23, #24) and voltronic_sunny_update_related_vars_limits() */
static info_rw_t	voltronic_sunny_r_output_realpower_max[] = {
	{ "0", 0 },
	{ "", 0 },
	{ "", 0 }
};

/* Range for maximum power feeding grid: max value filled at runtime by voltronic_sunny_update_related_vars_limits() */
static info_rw_t	voltronic_sunny_r_grid_realpower_max[] = {
	{ "0", 0 },
	{ "", 0 },
	{ "", 0 }
};

/* Range for LCD sleep time (time after which LCD screen-saver starts) */
static info_rw_t	voltronic_sunny_r_lcd_sleep_time[] = {
	{ "0", 0 },	/* 00 */
	{ "2970", 0 },	/* 99 */
	{ "", 0 }
};

/* Time (hh:mm) length */
static info_rw_t	voltronic_sunny_l_hhmm[] = {
	{ "5", 0 },
	{ "", 0 }
};

/* Range for maximum grid input average voltage */
static info_rw_t	voltronic_sunny_r_grid_in_avg_volt_max[] = {
	{ "235", 0 },
	{ "265", 0 },
	{ "", 0 }
};

/* Range for grid power deviation */
static info_rw_t	voltronic_sunny_r_grid_power_deviation[] = {
	{ "0", 0 },
	{ "999", 0 },
	{ "", 0 }
};

/* Range for power factor */
static info_rw_t	voltronic_sunny_r_output_powerfactor[] = {	/* FIXME: 1. nutdrv_qx setvar+RANGE doesn't support negative values; 2. values should be divided by 100 */
	{ "-99", 0 },
	{ "-90", 0 },
	{ "90", 0 },
	{ "100", 0 },
	{ "", 0 }
};

/* Range for power percent setting */
static info_rw_t	voltronic_sunny_r_powerpercent_setting[] = {
	{ "10", 0 },
	{ "100", 0 },
	{ "", 0 }
};

/* Range for power factor_percent */
static info_rw_t	voltronic_sunny_r_powerfactor_percent[] = {
	{ "50", 0 },
	{ "100", 0 },
	{ "", 0 }
};

/* Range for power factor curve */
static info_rw_t	voltronic_sunny_r_powerfactor_curve[] = {	/* FIXME: nutdrv_qx setvar+RANGE doesn't support negative values */
	{ "-99", 0 },
	{ "-90", 0 },
	{ "", 0 }
};

/* Date (YYYY/MM/DD) length */
static info_rw_t	voltronic_sunny_l_date[] = {
	{ "10", 0 },
	{ "", 0 }
};

/* Time (hh:mm:ss) length */
static info_rw_t	voltronic_sunny_l_time[] = {
	{ "8", 0 },
	{ "", 0 }
};


/* == qx2nut lookup table == */
static item_t	voltronic_axpert_qx2nut[] = {

	/*#######################################################################################################################################################################################################################################################################################################################################################################################################*
	 *# info_type					|info_flags			|info_rw				|command			|answer	|answer	|leading|value	|from	|to	|dfl			|qxflags							|preprocess_command		|preprocess_answer			|preprocess							#*
	 *#						|				|					|				|_len	|	|	|	|	|	|			|								|				|					|								#*
	 *#######################################################################################################################################################################################################################################################################################################################################################################################################*/

	/* Query device for protocol
	 * > [QPI\r]
	 * < [(PI30\r]
	 *    012345
	 *    0
	 */

	{ "device.firmware.aux",			0,				NULL,					"QPI\r",			"",	6,	'(',	"",	1,	4,	"%s",			QX_FLAG_STATIC,							voltronic_axpert_crc,				voltronic_axpert_checkcrc,	voltronic_axpert_protocol },

	/* Query device for firmware version
	 * > [QVFW\r]
	 * < [(VERFW:00074.50\r]
	 *    0123456789012345
	 *    0         1
	 */

	{ "device.firmware",				0,				NULL,					"QVFW\r",			"",	16,	'(',	"",	7,	14,	"0X%s",			QX_FLAG_STATIC,							voltronic_axpert_crc,				voltronic_axpert_checkcrc,		voltronic_axpert_fw },

	/* Query device for main CPU processor version
	 * > [QVFW\r]
	 * < [(VERFW:00074.50\r]
	 *    0123456789012345
	 *    0         1
	 */

	{ "device.firmware.main",			0,				NULL,					"QVFW\r",			"",	16,	'(',	"",	7,	14,	"0X%s",			QX_FLAG_STATIC,							voltronic_axpert_crc,				voltronic_axpert_checkcrc,		voltronic_axpert_hex_preprocess },

	/* Query device for SCC1 CPU Firmware version
	 * > [QVFW2\r]
	 * < [(VERFW2:00000.31\r]
	 *    01234567890123456
	 *    0         1
	 */

	{ "device.firmware.scc1",			0,				NULL,					"QVFW2\r",			"",	17,	'(',	"",	8,	15,	"0X%s",			QX_FLAG_STATIC,							voltronic_axpert_crc,				voltronic_axpert_checkcrc,		voltronic_axpert_hex_preprocess },

	/* Query device for SCC2 CPU Firmware version
	 * > [QVFW3\r]
	 * < [(VERFW3:00000.31\r]
	 *    01234567890123456
	 *    0         1
	 */

	{ "device.firmware.scc2",			0,				NULL,					"QVFW3\r",			"",	17,	'(',	"",	8,	15,	"0X%s",			QX_FLAG_STATIC,							voltronic_axpert_crc,				voltronic_axpert_checkcrc,		voltronic_axpert_hex_preprocess },

	/* Query device for SCC3 CPU Firmware version
	 * > [QVFW4\r]
	 * < [(VERFW4:00000.31\r]
	 *    01234567890123456
	 *    0         1
	 */

	{ "device.firmware.scc3",			0,				NULL,					"QVFW4\r",			"",	17,	'(',	"",	8,	15,	"0X%s",			QX_FLAG_STATIC,							voltronic_axpert_crc,				voltronic_axpert_checkcrc,		voltronic_axpert_hex_preprocess },


	/* Query device for serial number
	 * > [QID\r]
	 * < [(12345679012345\r]	<- As far as I know it hasn't a fixed length -> min length = ( + \r = 2
	 *    0123456789012345
	 *    0         1
	 */

	{ "device.serial",				0,				NULL,					"QID\r",			"",	2,	'(',	"",	1,	0,	"%s",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		voltronic_axpert_serial_numb },

	/*#######################################################################################################################################################################################################################################################################################################################################################################################################################################*/

	/* Query device for capability; only those capabilities whom the device is capable of are reported as Enabled or Disabled
	 * > [QFLAG\r]
	 * < [(EakxyDbjuvz\r]
	 *    01234567890123
	 *    0		* min length = ( + E + D + \r = 4
	 *
	 * A Enable/disable silence buzzer or open buzzer
	 * B Enable/Disable overload bypass function
	 * J Enable/Disable power saving
	 * K Enable/Disable LCD display escape to default page after 1min timeout
	 * U Enable/Disable overload restart
	 * V Enable/Disable over temperature restart
	 * X Enable/Disable backlight on
	 * Y Enable/Disable alarm on when primary source interrupt
	 * Z Enable/Disable fault code record
	 * L Enable/Disable data log pop-up
	 */

	{ "battery.energysave",				ST_FLAG_RW,			voltronic_axpert_e_cap,			"QFLAG\r",			"",	4,	'(',	"",	1,	0,	"%s",			QX_FLAG_ENUM | QX_FLAG_SEMI_STATIC,				voltronic_axpert_crc,		voltronic_axpert_checkcrc,		voltronic_axpert_capability },
	{ "ups.beeper.status",			0,				NULL,					"QFLAG\r",			"",	4,	'(',	"",	1,	0,	"%s",			QX_FLAG_SEMI_STATIC,						voltronic_axpert_crc,		voltronic_axpert_checkcrc,		voltronic_axpert_capability },
#if 0
	/* Not available in NUT */
	{ "bypass_alarm",				0,				NULL,					"QFLAG\r",			"",	4,	'(',	"",	1,	0,	"%s",			QX_FLAG_SEMI_STATIC | QX_FLAG_NONUT,				voltronic_axpert_crc,		voltronic_axpert_checkcrc,		voltronic_axpert_capability },
	{ "battery_alarm",				0,				NULL,					"QFLAG\r",			"",	4,	'(',	"",	1,	0,	"%s",			QX_FLAG_SEMI_STATIC | QX_FLAG_NONUT,				voltronic_axpert_crc,		voltronic_axpert_checkcrc,		voltronic_axpert_capability },
#endif

#if 0
	/*   Enable	or	  Disable	or	Reset to safe default values	capability options
	 * > [PEX\r]		> [PDX\r]		> [PF\r]
	 * < [(ACK\r]		< [(ACK\r]		< [(ACK\r]
	 *    01234		   01234		   01234
	 *    0			   0			   0
	 */

	{ "battery.energysave",				0,				voltronic_axpert_e_cap,			"P%sJ\r",			"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_ENUM | QX_FLAG_SKIP,			voltronic_axpert_crc,		voltronic_axpert_checkcrc,		voltronic_axpert_capability_set },
	/* Not available in NUT */
	{ "reset_to_default",				0,				NULL,					"PF\r",				"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_NONUT,					voltronic_axpert_crc,		voltronic_axpert_checkcrc,		voltronic_axpert_capability_reset },
	{ "bypass_alarm",				0,				voltronic_axpert_e_cap_nonut,		"P%sP\r",			"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_ENUM | QX_FLAG_NONUT | QX_FLAG_SKIP,	voltronic_axpert_crc,		voltronic_axpert_checkcrc,		voltronic_axpert_capability_set_nonut },
	{ "battery_alarm",				0,				voltronic_axpert_e_cap_nonut,		"P%sB\r",			"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_ENUM | QX_FLAG_NONUT | QX_FLAG_SKIP,	voltronic_axpert_crc,		voltronic_axpert_checkcrc,		voltronic_axpert_capability_set_nonut },
#endif

#if 0
	/* Query device for operational options flag (P16 only)
	 * > [QENF\r]
	 * < [(A1B1C1D1E1F0G1\r]	<- required options (length: 16)
	 * < [(A1B0C1D0E1F0G0H0I_J_\r]	<- known available options
	 *    0123456789012345678901
	 *    0         1         2
	 */

	{ "charge_battery",				ST_FLAG_RW,			voltronic_axpert_e_cap_nonut,		"QENF\r",			"",	16,	'(',	"",	2,	2,	"%s",			QX_FLAG_ENUM | QX_FLAG_NONUT | QX_FLAG_SEMI_STATIC,		NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_01 },	/* A */
	{ "charge_battery_from_ac",			ST_FLAG_RW,			voltronic_axpert_e_cap_nonut,		"QENF\r",			"",	16,	'(',	"",	4,	4,	"%s",			QX_FLAG_ENUM | QX_FLAG_NONUT | QX_FLAG_SEMI_STATIC,		NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_01 },	/* B */
	{ "feed_grid",					ST_FLAG_RW,			voltronic_axpert_e_cap_nonut,		"QENF\r",			"",	16,	'(',	"",	6,	6,	"%s",			QX_FLAG_ENUM | QX_FLAG_NONUT | QX_FLAG_SEMI_STATIC,		NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_01 },	/* C */
	{ "discharge_battery_when_pv_on",		ST_FLAG_RW,			voltronic_axpert_e_cap_nonut,		"QENF\r",			"",	16,	'(',	"",	8,	8,	"%s",			QX_FLAG_ENUM | QX_FLAG_NONUT | QX_FLAG_SEMI_STATIC,		NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_01 },	/* D */
	{ "discharge_battery_when_pv_off",		ST_FLAG_RW,			voltronic_axpert_e_cap_nonut,		"QENF\r",			"",	16,	'(',	"",	10,	10,	"%s",			QX_FLAG_ENUM | QX_FLAG_NONUT | QX_FLAG_SEMI_STATIC,		NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_01 },	/* E */
	{ "feed_grid_from_battery_when_pv_on",		ST_FLAG_RW,			voltronic_axpert_e_cap_nonut,		"QENF\r",			"",	16,	'(',	"",	12,	12,	"%s",			QX_FLAG_ENUM | QX_FLAG_NONUT | QX_FLAG_SEMI_STATIC,		NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_01 },	/* F */
	{ "feed_grid_from_battery_when_pv_off",		ST_FLAG_RW,			voltronic_axpert_e_cap_nonut,		"QENF\r",			"",	16,	'(',	"",	14,	14,	"%s",			QX_FLAG_ENUM | QX_FLAG_NONUT | QX_FLAG_SEMI_STATIC,		NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_01 },	/* G */
/*	{ "unknown.?",					ST_FLAG_RW,			voltronic_axpert_e_cap_nonut,		"QENF\r",			"",	18,	'(',	"",	16,	16,	"%s",			QX_FLAG_ENUM | QX_FLAG_NONUT | QX_FLAG_SEMI_STATIC,		NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_01 },	*//* H */
/*	{ "unknown.?",					ST_FLAG_RW,			voltronic_axpert_e_cap_nonut,		"QENF\r",			"",	20,	'(',	"",	18,	18,	"%s",			QX_FLAG_ENUM | QX_FLAG_NONUT | QX_FLAG_SEMI_STATIC,		NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_01 },	*//* I */
/*	{ "unknown.?",					ST_FLAG_RW,			voltronic_axpert_e_cap_nonut,		"QENF\r",			"",	22,	'(',	"",	20,	20,	"%s",			QX_FLAG_ENUM | QX_FLAG_NONUT | QX_FLAG_SEMI_STATIC,		NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_01 },	*//* J */

	/*   Enable (<action>: 1) or disable (<action>: 0) operational option <option> (P16 only)
	 * > [ENF<option><action>\r]
	 * < [(ACK\r]
	 *    01234
	 *    0
	 */

	{ "charge_battery",				0,				voltronic_axpert_e_cap_nonut,		"ENFA%d\r",			"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_NONUT | QX_FLAG_ENUM | QX_FLAG_SKIP,	NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_01_set },	/* A */
	{ "charge_battery_from_ac",			0,				voltronic_axpert_e_cap_nonut,		"ENFB%d\r",			"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_NONUT | QX_FLAG_ENUM | QX_FLAG_SKIP,	NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_01_set },	/* B */
	{ "feed_grid",					0,				voltronic_axpert_e_cap_nonut,		"ENFC%d\r",			"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_NONUT | QX_FLAG_ENUM | QX_FLAG_SKIP,	NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_01_set },	/* C */
	{ "discharge_battery_when_pv_on",		0,				voltronic_axpert_e_cap_nonut,		"ENFD%d\r",			"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_NONUT | QX_FLAG_ENUM | QX_FLAG_SKIP,	NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_01_set },	/* D */
	{ "discharge_battery_when_pv_off",		0,				voltronic_axpert_e_cap_nonut,		"ENFE%d\r",			"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_NONUT | QX_FLAG_ENUM | QX_FLAG_SKIP,	NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_01_set },	/* E */
	{ "feed_grid_from_battery_when_pv_on",		0,				voltronic_axpert_e_cap_nonut,		"ENFF%d\r",			"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_NONUT | QX_FLAG_ENUM | QX_FLAG_SKIP,	NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_01_set },	/* F */
	{ "feed_grid_from_battery_when_pv_off",		0,				voltronic_axpert_e_cap_nonut,		"ENFG%d\r",			"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_NONUT | QX_FLAG_ENUM | QX_FLAG_SKIP,	NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_01_set },	/* G */
/*	{ "unknown.?",					0,				voltronic_axpert_e_cap_nonut,		"ENFH%d\r",			"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_NONUT | QX_FLAG_ENUM | QX_FLAG_SKIP,	NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_01_set },	*//* H */
/*	{ "unknown.?",					0,				voltronic_axpert_e_cap_nonut,		"ENFI%d\r",			"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_NONUT | QX_FLAG_ENUM | QX_FLAG_SKIP,	NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_01_set },	*//* I */
/*	{ "unknown.?",					0,				voltronic_axpert_e_cap_nonut,		"ENFJ%d\r",			"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_NONUT | QX_FLAG_ENUM | QX_FLAG_SKIP,	NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_01_set },	*//* J */
#endif

#if 0
	/* Query device for power factor curve capability (P15 only)
	 * > [QPDG\r]
	 * < [(0\r]
	 *    012
	 *    0
	 */

	{ "output.powerfactor.curve.capability",	ST_FLAG_RW,			voltronic_axpert_e_cap_nonut,		"QPDG\r",			"",	3,	'(',	"",	1,	1,	"%s",			QX_FLAG_ENUM | QX_FLAG_SEMI_STATIC,				NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_01 },

	/* Enable or disable power factor curve capability (P15 only)
	 * > [PDG<n>\r]		<n>: 1 -> enable; 0 -> disable
	 * < [(ACK\r]
	 *    01234
	 *    0
	 */

	{ "output.powerfactor.curve.capability",	0,				voltronic_axpert_e_cap_nonut,		"PDG%d\r",			"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_ENUM | QX_FLAG_SKIP,			NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_01_set },
#endif

#if 0
	/* Query device for generator as AC source option status (P16 only)
	 * > [GNTMQ\r]
	 * < [(00\r]
	 *    0123
	 *    0
	 */

/*	{ "unknown.?",					0,				NULL,					"GNTMQ\r",			"",	4,	'(',	"",	1,	1,	"%s",			0,								NULL,				voltronic_axpert_checkcrc,		NULL },	*/
	{ "generator_as_ac_source",			ST_FLAG_RW,			voltronic_axpert_e_cap_nonut,		"GNTMQ\r",			"",	4,	'(',	"",	2,	2,	"%s",			QX_FLAG_ENUM | QX_FLAG_NONUT | QX_FLAG_SEMI_STATIC,		NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_01 },

	/* Enable (<action>: 1) or disable (<action>: 0) generator as AC source option (P16 only)
	 * > [GNTM<action>\r]
	 * < [(ACK\r]
	 *    01234
	 *    0
	 */

	{ "generator_as_ac_source",			0,				voltronic_axpert_e_cap_nonut,		"PDG%d\r",			"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_NONUT | QX_FLAG_ENUM | QX_FLAG_SKIP,	NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_01_set },
#endif

#if 0
	/* Query device for PV energy supply priority (P16 only)
	 * > [QPRIO\r]
	 * < [(02\r]
	 *    0123
	 *    0
	 */


	{ "pv.energy.supplypriority",			ST_FLAG_RW,			voltronic_sunny_e_pv_priority,		"QPRIO\r",			"",	4,	'(',	"",	1,	2,	"%s",			QX_FLAG_ENUM | QX_FLAG_SEMI_STATIC,				NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_pv_priority },

	/* Set PV energy supply priority (P16 only)
	 * > [PRIO<n>\r]
	 * < [(ACK\r]
	 *    01234
	 *    0
	 */

	{ "pv.energy.supplypriority",			0,				voltronic_sunny_e_pv_priority,		"PRIO%02d\r",			"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_ENUM | QX_FLAG_SKIP,			NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_pv_priority_set },

	/*#######################################################################################################################################################################################################################################################################################################################################################################################################################################*/
#endif

	/* Query device for ratings #1
	 * > [QPIRI\r]
	 * < [(230.0 21.7 230.0 50.0 21.7 5000 5000 48.0 46.0 42.0 56.4 54.0 0 30 060 0 0 2 9 01 0 1 54.0 0 1 000\r]
	 *    012345678901234567890123456789012345678901234567
	 *    0         1         2         3         4
	 */

	{ "input.voltage.nominal",			ST_FLAG_RW,			voltronic_axpert_e_volt_nom,		"QPIRI\r",			"",	99,	'(',	"",	1,	5,	"%.1f",			QX_FLAG_ENUM | QX_FLAG_STATIC,					voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },
	{ "input.current.nominal",			0,				NULL,					"QPIRI\r",			"",	99,	'(',	"",	7,	10,	"%.1f",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },
	{ "output.voltage.nominal",			0,				NULL,					"QPIRI\r",			"",	99,	'(',	"",	12,	16,	"%.1f",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },
	{ "output.frequency.nominal",			0,				NULL,					"QPIRI\r",			"",	99,	'(',	"",	18,	21,	"%.1f",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },
	{ "output.current.nominal",			0,				NULL,					"QPIRI\r",			"",	99,	'(',	"",	23,	26,	"%.1f",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },
	{ "output.power.nominal",			0,				NULL,					"QPIRI\r",			"",	99,	'(',	"",	28,	31,	"%.1f",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },
	{ "output.realpower.nominal",			0,				NULL,					"QPIRI\r",			"",	99,	'(',	"",	33,	36,	"%.1f",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },
	{ "battery.voltage.nominal",			0,				NULL,					"QPIRI\r",			"",	99,	'(',	"",	38,	41,	"%.1f",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },
	{ "battery.voltage.recharge",			0,				NULL,					"QPIRI\r",			"",	99,	'(',	"",	43,	46,	"%.1f",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },
	{ "battery.voltage.under",			0,				NULL,					"QPIRI\r",			"",	99,	'(',	"",	48,	51,	"%.1f",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },
	{ "battery.voltage.bulk",			0,				NULL,					"QPIRI\r",			"",	99,	'(',	"",	53,	56,	"%.1f",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },
	{ "battery.voltage.float",			0,				NULL,					"QPIRI\r",			"",	99,	'(',	"",	58,	61,	"%.1f",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },
	{ "battery.type",				0,				NULL,					"QPIRI\r",			"",	99,	'(',	"",	63,	63,	"%s",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		voltronic_axpert_qpiri_battery_type },
	{ "battery.charging.current.ac.high",		0,				NULL,					"QPIRI\r",			"",	99,	'(',	"",	65,	66,	"%.1f",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },
	{ "battery.charging.current.high",		0,				NULL,					"QPIRI\r",			"",	99,	'(',	"",	68,	70,	"%.1f",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },
/*	{ "input_voltage_range",			0,				NULL,					"QPIRI\r",			"",	99,	'(',	"",	72,	72,	"%s",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		voltronic_axpert_qpiri_inputvoltage_range },*/
/*	{ "charger_source_priority",			0,				NULL,					"QPIRI\r",			"",	99,	'(',	"",	76,	76,	"%s",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		voltronic_axpert_qpiri_charger_source_priority },*/
/*	{ "parallel_max_num",				0,				NULL,					"QPIRI\r",			"",	99,	'(',	"",	76,	76,	"%s",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		voltronic_axpert_qpiri_parallel_max_num },*/
	{ "device.model.type",				0,				NULL,					"QPIRI\r",			"",	99,	'(',	"",	80,	81,	"%s",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		voltronic_axpert_qpiri_model_type },
	{ "device.description",				0,				NULL,					"QPIRI\r",			"",	99,	'(',	"",	83,	83,	"%s",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		voltronic_axpert_transformer },
/*	{ "output_mode",				0,				NULL,					"QPIRI\r",			"",	99,	'(',	"",	85,	85,	"%s",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		voltronic_axpert_qpiri_output_mode },*/
/*	{ "battery_redischarge_voltage",		0,				NULL,					"QPIRI\r",			"",	99,	'(',	"",	87,	90,	"%.1f",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },*/
/*	{ "pv_ok_condition_for_parallel",		0,				NULL,					"QPIRI\r",			"",	99,	'(',	"",	92,	92,	"%s",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		voltronic_axpert_qpiri_pv_ok_condition_for_parallel },*/
/*	{ "pv_power_balance",				0,				NULL,					"QPIRI\r",			"",	99,	'(',	"",	94,	94,	"%s",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		voltronic_axpert_qpiri_pv_power_balance },*/




#if 0
	/* Set nominal voltage (P16 only)
	 * > [V<n>\r]
	 * < [(ACK\r]
	 *    01234
	 *    0
	 */

	{ "grid.voltage.nominal",			0,				voltronic_axpert_e_volt_nom,		"V%03d\r",			"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_ENUM | QX_FLAG_SKIP,			NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_volt_nom_set },

	/* Set nominal frequency (P16 only)
	 * > [F<n>\r]
	 * < [(ACK\r]
	 *    01234
	 *    0
	 */

	{ "grid.frequency.nominal",			0,				voltronic_sunny_e_freq_nom,		"F%02.0f\r",			"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_ENUM | QX_FLAG_SKIP,			NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_process_setvar },

	/* Set number of MPP trackers in use
	 * > [PVN<n>\r]
	 * < [(ACK\r]
	 *    01234
	 *    0
	 */

	{ "mpp.number",					0,				voltronic_sunny_r_mpp_number,		"PVN%02.0f\r",			"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_RANGE | QX_FLAG_SKIP,			NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_process_setvar },
#endif

	/* Query device for ratings #2
	 * > [QMD\r]
	 * < [(#####INVERTEX5K ###5000 99 1/1 230 230 04 12.0\r]
	 *    012345678901234567890123456789012345678901234567
	 *    0         1         2         3         4
	 */

	{ "device.model",				0,				NULL,					"QMD\r",			"",	48,	'(',	"",	1,	15,	"%s",			QX_FLAG_STATIC | QX_FLAG_TRIM,					voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },
/*	{ "output.realpower.nominal",			0,				NULL,					"QMD\r",			"",	48,	'(',	"",	17,	23,	"%.0f",			QX_FLAG_STATIC | QX_FLAG_TRIM,					voltronic_axpert_crc,		voltronic_axpert_checkcrc,		voltronic_sunny_basic_preprocess_and_update_related_vars_limits },*/
	{ "output.powerfactor.nominal",			0,				NULL,					"QMD\r",			"",	48,	'(',	"",	25,	26,	"%.0f",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },	/* FIXME: value should be divided by 100 */
	{ "input.phases",				0,				NULL,					"QMD\r",			"",	48,	'(',	"",	28,	28,	"%.0f",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },
	{ "output.phases",				0,				NULL,					"QMD\r",			"",	48,	'(',	"",	30,	30,	"%.0f",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },
/*	{ "input.voltage.nominal",			0,				NULL,					"QMD\r",			"",	48,	'(',	"",	32,	34,	"%.1f",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },*/
/*	{ "output.voltage.nominal",			0,				NULL,					"QMD\r",			"",	48,	'(',	"",	36,	38,	"%.1f",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },*/
	{ "battery.number",				0,				NULL,					"QMD\r",			"",	48,	'(',	"",	40,	41,	"%.0f",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },
/*	{ "battery.voltage.nominal",			0,				NULL,					"QMD\r",			"",	48,	'(',	"",	43,	46,	"%.1f",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },*/

#if 0
	/* Query device for ratings #3
	 * > [I\r]
	 * < [(DSP:14-03-03,14:30 MCU:14-01-15,17:20\r]
	 *    012345678901234567890123456789012345678
	 *    0         1         2         3
	 */

	{ "dsp.mfr.date",				0,				NULL,					"I\r",				"",	39,	'(',	"",	5,	12,	"%04d/%02d/%02d",	QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		voltronic_sunny_yymmdd },
	{ "dsp.mfr.time",				0,				NULL,					"I\r",				"",	39,	'(',	"",	14,	18,	"%02d:%02d",		QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		voltronic_sunny_hh_mm },
	{ "mcu.mfr.date",				0,				NULL,					"I\r",				"",	39,	'(',	"",	24,	31,	"%04d/%02d/%02d",	QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		voltronic_sunny_yymmdd },
	{ "mcu.mfr.time",				0,				NULL,					"I\r",				"",	39,	'(',	"",	33,	37,	"%02d:%02d",		QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		voltronic_sunny_hh_mm },
#endif


	/*#######################################################################################################################################################################################################################################################################################################################################################################################################################################*/

	/* Query device for default values
	 * > [QDI\r]
	 * < [(230.0 50.0 0030 42.0 54.0 56.4 46.0 60 0 0 2 0 0 0 0 0 1 1 0 0 1 0 54.0 0 1 000\r]
	 *    012345678901234567890123456789012345678901234567890123456789012345678901234567890
	 *    0         1         2         3         4         5         6         7         8
	 */

	/* Note: alignments in output don't match spec */

	{ "output.voltage.default",			0,				NULL,					"QDI\r",			"",	80,	'(',	"",	1,	5,	"%.1f",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },
	{ "output.frequency.default",			0,				NULL,					"QDI\r",			"",	80,	'(',	"",	7,	10,	"%.1f",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },
	{ "battery.charging.current.ac.high.default",	0,				NULL,					"QDI\r",			"",	80,	'(',	"",	12,	15,	"%.1f",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },
	{ "battery.voltage.under.default",		0,				NULL,					"QDI\r",			"",	80,	'(',	"",	17,	20,	"%.1f",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },
	{ "battery.voltage.float.default",		0,				NULL,					"QDI\r",			"",	80,	'(',	"",	22,	25,	"%.1f",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },
	{ "battery.voltage.bulk.default",		0,				NULL,					"QDI\r",			"",	80,	'(',	"",	27,	30,	"%.1f",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },
	{ "battery.voltage.recharge.default",		0,				NULL,					"QDI\r",			"",	80,	'(',	"",	32,	35,	"%.1f",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },
	/* should be battery.charging.current.ac.high.default on MKS-*-* Plus Duo models */
	{ "battery.charging.current.high.default",		0,				NULL,					"QDI\r",			"",	80,	'(',	"",	37,	38,	"%.1f",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },
/*	{ "input_voltage_range_default",			0,				NULL,					"QDI\r",			"",	80,	'(',	"",	40,	40,	"%s",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		voltronic_axpert_qpiri_inputvoltage_range },*/
/*	{ "output_source_priority_default",			0,				NULL,					"QDI\r",			"",	80,	'(',	"",	42,	42,	"%s",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		voltronic_axpert_qpiri_output_source_priority },*/
/*	{ "charger_source_priority_default",			0,				NULL,					"QDI\r",			"",	80,	'(',	"",	44,	44,	"%s",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		voltronic_axpert_qpiri_charger_source_priority },*/
	{ "battery.type.default",				0,				NULL,					"QDI\r",			"",	80,	'(',	"",	46,	46,	"%s",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		voltronic_axpert_qpiri_battery_type },

	/* FIXME: flags go here */

/*	{ "output_mode_default",				0,				NULL,					"QDI\r",			"",	80,	'(',	"",	66,	66,	"%s",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		voltronic_axpert_qpiri_output_mode },*/
/*	{ "battery_redischarge_voltage_default",		0,				NULL,					"QDI\r",			"",	80,	'(',	"",	68,	71,	"%.1f",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },*/
/*	{ "pv_ok_condition_for_parallel_default",		0,				NULL,					"QDI\r",			"",	80,	'(',	"",	73,	73,	"%s",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		voltronic_axpert_qpiri_pv_ok_condition_for_parallel },*/
/*	{ "pv_power_balance_default",				0,				NULL,					"QDI\r",			"",	80,	'(',	"",	75,	75,	"%s",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		voltronic_axpert_qpiri_pv_power_balance },*/



	/*#######################################################################################################################################################################################################################################################################################################################################################################################################################################*/

#if 0
	/* Query device for acceptable limits -these don't need to be published (as are already used in ranges), so mark them as NONUT
	 * > [QVFTR\r]
	 * < [(276.0 235.0 225.0 180.0 55.0 50.1 49.9 45.0 070 005 58.0 48.0 25.0 00.5 500 450 200 090 450 400 200 110 03000 00000 58.0 50.0 --\r]
	 *    0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789
	 *    0         1         2         3         4         5         6         7         8         9        10        11        12
	 */

	{ "grid.output.voltage.high.max",		0,				NULL,					"QVFTR\r",			"",	130,	'(',	"",	1,	5,	"%.1f",			QX_FLAG_STATIC | QX_FLAG_NONUT,					NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_set_limits },
	{ "grid.input.voltage.high.max",		0,				NULL,					"QVFTR\r",			"",	130,	'(',	"",	1,	5,	"%.1f",			QX_FLAG_STATIC | QX_FLAG_NONUT,					NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_set_limits },	/* P16 only (same token as previous one) */
	{ "grid.output.voltage.high.min",		0,				NULL,					"QVFTR\r",			"",	130,	'(',	"",	7,	11,	"%.1f",			QX_FLAG_STATIC | QX_FLAG_NONUT,					NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_set_limits },
	{ "grid.input.voltage.high.min",		0,				NULL,					"QVFTR\r",			"",	130,	'(',	"",	7,	11,	"%.1f",			QX_FLAG_STATIC | QX_FLAG_NONUT,					NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_set_limits },	/* P16 only (same token as previous one) */
	{ "grid.output.voltage.low.max",		0,				NULL,					"QVFTR\r",			"",	130,	'(',	"",	13,	17,	"%.1f",			QX_FLAG_STATIC | QX_FLAG_NONUT,					NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_set_limits },
	{ "grid.input.voltage.low.max",			0,				NULL,					"QVFTR\r",			"",	130,	'(',	"",	13,	17,	"%.1f",			QX_FLAG_STATIC | QX_FLAG_NONUT,					NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_set_limits },	/* P16 only (same token as previous one) */
	{ "grid.output.voltage.low.min",		0,				NULL,					"QVFTR\r",			"",	130,	'(',	"",	19,	23,	"%.1f",			QX_FLAG_STATIC | QX_FLAG_NONUT,					NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_set_limits },
	{ "grid.input.voltage.low.min",			0,				NULL,					"QVFTR\r",			"",	130,	'(',	"",	19,	23,	"%.1f",			QX_FLAG_STATIC | QX_FLAG_NONUT,					NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_set_limits },	/* P16 only (same token as previous one) */
	{ "grid.output.frequency.high.max",		0,				NULL,					"QVFTR\r",			"",	130,	'(',	"",	25,	28,	"%.1f",			QX_FLAG_STATIC | QX_FLAG_NONUT,					NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_set_limits },
	{ "grid.input.frequency.high.max",		0,				NULL,					"QVFTR\r",			"",	130,	'(',	"",	25,	28,	"%.1f",			QX_FLAG_STATIC | QX_FLAG_NONUT,					NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_set_limits },	/* P16 only (same token as previous one) */
	{ "grid.output.frequency.high.min",		0,				NULL,					"QVFTR\r",			"",	130,	'(',	"",	30,	33,	"%.1f",			QX_FLAG_STATIC | QX_FLAG_NONUT,					NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_set_limits },
	{ "grid.input.frequency.high.min",		0,				NULL,					"QVFTR\r",			"",	130,	'(',	"",	30,	33,	"%.1f",			QX_FLAG_STATIC | QX_FLAG_NONUT,					NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_set_limits },	/* P16 only (same token as previous one) */
	{ "grid.output.frequency.low.max",		0,				NULL,					"QVFTR\r",			"",	130,	'(',	"",	35,	38,	"%.1f",			QX_FLAG_STATIC | QX_FLAG_NONUT,					NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_set_limits },
	{ "grid.input.frequency.low.max",		0,				NULL,					"QVFTR\r",			"",	130,	'(',	"",	35,	38,	"%.1f",			QX_FLAG_STATIC | QX_FLAG_NONUT,					NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_set_limits },	/* P16 only (same token as previous one) */
	{ "grid.output.frequency.low.min",		0,				NULL,					"QVFTR\r",			"",	130,	'(',	"",	40,	43,	"%.1f",			QX_FLAG_STATIC | QX_FLAG_NONUT,					NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_set_limits },
	{ "grid.input.frequency.low.min",		0,				NULL,					"QVFTR\r",			"",	130,	'(',	"",	40,	43,	"%.1f",			QX_FLAG_STATIC | QX_FLAG_NONUT,					NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_set_limits },	/* P16 only (same token as previous one) */
	{ "grid.waitingtime.max",			0,				NULL,					"QVFTR\r",			"",	130,	'(',	"",	45,	47,	"%.0f",			QX_FLAG_STATIC | QX_FLAG_NONUT,					NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_set_limits },
	{ "grid.waitingtime.min",			0,				NULL,					"QVFTR\r",			"",	130,	'(',	"",	49,	51,	"%.0f",			QX_FLAG_STATIC | QX_FLAG_NONUT,					NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_set_limits },
	{ "battery.charging.voltage.floating.max",	0,				NULL,					"QVFTR\r",			"",	130,	'(',	"",	53,	56,	"%.1f",			QX_FLAG_STATIC | QX_FLAG_NONUT,					NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_set_limits },	/* P16 only */
	{ "battery.charging.voltage.floating.min",	0,				NULL,					"QVFTR\r",			"",	130,	'(',	"",	58,	61,	"%.1f",			QX_FLAG_STATIC | QX_FLAG_NONUT,					NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_set_limits },	/* P16 only */
	{ "battery.charging.current.high.max",		0,				NULL,					"QVFTR\r",			"",	130,	'(',	"",	63,	66,	"%.1f",			QX_FLAG_STATIC | QX_FLAG_NONUT,					NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_set_limits },	/* P16 only */
	{ "battery.charging.current.high.min",		0,				NULL,					"QVFTR\r",			"",	130,	'(',	"",	68,	71,	"%.1f",			QX_FLAG_STATIC | QX_FLAG_NONUT,					NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_set_limits },	/* P16 only */
	{ "pv.input.voltage.high.max",			0,				NULL,					"QVFTR\r",			"",	130,	'(',	"",	73,	75,	"%.1f",			QX_FLAG_STATIC | QX_FLAG_NONUT,					NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_set_limits },
	{ "pv.input.voltage.high.min",			0,				NULL,					"QVFTR\r",			"",	130,	'(',	"",	77,	79,	"%.1f",			QX_FLAG_STATIC | QX_FLAG_NONUT,					NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_set_limits },
	{ "pv.input.voltage.low.max",			0,				NULL,					"QVFTR\r",			"",	130,	'(',	"",	81,	83,	"%.1f",			QX_FLAG_STATIC | QX_FLAG_NONUT,					NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_set_limits },
	{ "pv.input.voltage.low.min",			0,				NULL,					"QVFTR\r",			"",	130,	'(',	"",	85,	87,	"%.1f",			QX_FLAG_STATIC | QX_FLAG_NONUT,					NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_set_limits },
	{ "mpp.voltage.high.max",			0,				NULL,					"QVFTR\r",			"",	130,	'(',	"",	89,	91,	"%.1f",			QX_FLAG_STATIC | QX_FLAG_NONUT,					NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_set_limits },
	{ "mpp.voltage.high.min",			0,				NULL,					"QVFTR\r",			"",	130,	'(',	"",	93,	95,	"%.1f",			QX_FLAG_STATIC | QX_FLAG_NONUT,					NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_set_limits },
	{ "mpp.voltage.low.max",			0,				NULL,					"QVFTR\r",			"",	130,	'(',	"",	97,	99,	"%.1f",			QX_FLAG_STATIC | QX_FLAG_NONUT,					NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_set_limits },
	{ "mpp.voltage.low.min",			0,				NULL,					"QVFTR\r",			"",	130,	'(',	"",	101,	103,	"%.1f",			QX_FLAG_STATIC | QX_FLAG_NONUT,					NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_set_limits },
	{ "output.realpower.max.max",			0,				NULL,					"QVFTR\r",			"",	130,	'(',	"",	105,	109,	"%.0f",			QX_FLAG_STATIC | QX_FLAG_NONUT,					NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_set_limits },
	{ "output.realpower.max.min",			0,				NULL,					"QVFTR\r",			"",	130,	'(',	"",	111,	115,	"%.0f",			QX_FLAG_STATIC | QX_FLAG_NONUT,					NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_set_limits },
	{ "battery.charging.voltage.bulk.max",		0,				NULL,					"QVFTR\r",			"",	130,	'(',	"",	117,	120,	"%.1f",			QX_FLAG_STATIC | QX_FLAG_NONUT,					NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_set_limits },	/* P16 only */
	{ "battery.charging.voltage.bulk.min",		0,				NULL,					"QVFTR\r",			"",	130,	'(',	"",	122,	125,	"%.1f",			QX_FLAG_STATIC | QX_FLAG_NONUT,					NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_set_limits },	/* P16 only */
/*	{ "unknown.?",					0,				NULL,					"QVFTR\r",			"",	130,	'(',	"",	127,	128,	"%.1f",			QX_FLAG_STATIC | QX_FLAG_NONUT,					NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_set_limits },	*/
#endif

#if 0
	/* Query device for grid output voltage limits
	 * > [QGOV\r]
	 * < [(264.5 184.0\r]
	 *    0123456789012
	 *    0         1
	 */

	{ "grid.output.voltage.high",			ST_FLAG_RW,			voltronic_sunny_r_grid_output_volt_max,	"QGOV\r",			"",	13,	'(',	"",	1,	5,	"%.1f",			QX_FLAG_RANGE | QX_FLAG_SEMI_STATIC,				NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_unskip_setvar },
	{ "grid.output.voltage.low",			ST_FLAG_RW,			voltronic_sunny_r_grid_output_volt_min,	"QGOV\r",			"",	13,	'(',	"",	7,	11,	"%.1f",			QX_FLAG_RANGE | QX_FLAG_SEMI_STATIC,				NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_unskip_setvar },

	/* Set grid output voltage limits
	 * > [GOHV<n>\r]	> [GOLV<n>\r]
	 * < [(ACK\r]		< [(ACK\r]
	 *    01234		   01234
	 *    0			   0
	 */

	{ "grid.output.voltage.high",			0,				voltronic_sunny_r_grid_output_volt_max,	"GOHV%05.1f\r",			"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_RANGE | QX_FLAG_SKIP,			NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_process_setvar },
	{ "grid.output.voltage.low",			0,				voltronic_sunny_r_grid_output_volt_min,	"GOLV%05.1f\r",			"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_RANGE | QX_FLAG_SKIP,			NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_process_setvar },

	/* Query device for grid input voltage limits (P16 only)
	 * > [QBYV\r]
	 * < [(100.0 050.0\r]
	 *    0123456789012
	 *    0         1
	 */

	{ "grid.input.voltage.high",			ST_FLAG_RW,			voltronic_sunny_r_grid_input_volt_max,	"QBYV\r",			"",	13,	'(',	"",	1,	5,	"%.1f",			QX_FLAG_RANGE | QX_FLAG_SEMI_STATIC,				NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_unskip_setvar },
	{ "grid.input.voltage.low",			ST_FLAG_RW,			voltronic_sunny_r_grid_input_volt_min,	"QBYV\r",			"",	13,	'(',	"",	7,	11,	"%.1f",			QX_FLAG_RANGE | QX_FLAG_SEMI_STATIC,				NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_unskip_setvar },

	/* Set grid input voltage limits (P16 only)
	 * > [PHV<n>\r]		> [PLV<n>\r]
	 * < [(ACK\r]		< [(ACK\r]
	 *    01234		   01234
	 *    0			   0
	 */

	{ "grid.input.voltage.high",			0,				voltronic_sunny_r_grid_input_volt_max,	"PHV%05.1f\r",			"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_RANGE | QX_FLAG_SKIP,			NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_process_setvar },
	{ "grid.input.voltage.low",			0,				voltronic_sunny_r_grid_input_volt_min,	"PLV%05.1f\r",			"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_RANGE | QX_FLAG_SKIP,			NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_process_setvar },

	/* Query device for grid output frequency limits
	 * > [QGOF\r]
	 * < [(51.5 47.5\r]
	 *    01234567890
	 *    0         1
	 */

	{ "grid.output.frequency.high",			ST_FLAG_RW,			voltronic_sunny_r_grid_inout_freq_max,	"QGOF\r",			"",	11,	'(',	"",	1,	4,	"%.1f",			QX_FLAG_RANGE | QX_FLAG_SEMI_STATIC,				NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_unskip_setvar },
	{ "grid.output.frequency.low",			ST_FLAG_RW,			voltronic_sunny_r_grid_inout_freq_min,	"QGOF\r",			"",	11,	'(',	"",	6,	9,	"%.1f",			QX_FLAG_RANGE | QX_FLAG_SEMI_STATIC,				NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_unskip_setvar },

	/* Set grid output frequency limits
	 * > [GOHF<n>\r]	> [GOLF<n>\r]
	 * < [(ACK\r]		< [(ACK\r]
	 *    01234		   01234
	 *    0			   0
	 */

	{ "grid.output.frequency.high",			0,				voltronic_sunny_r_grid_inout_freq_max,	"GOHF%04.1f\r",			"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_RANGE | QX_FLAG_SKIP,			NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_process_setvar },
	{ "grid.output.frequency.low",			0,				voltronic_sunny_r_grid_inout_freq_min,	"GOLF%04.1f\r",			"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_RANGE | QX_FLAG_SKIP,			NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_process_setvar },

	/* Query device for grid input frequency limits (P16 only)
	 * > [QBYF\r]
	 * < [(10.0 05.0\r]
	 *    01234567890
	 *    0         1
	 */

	{ "grid.input.frequency.high",			ST_FLAG_RW,			voltronic_sunny_r_grid_inout_freq_max,	"QBYF\r",			"",	11,	'(',	"",	1,	4,	"%.1f",			QX_FLAG_RANGE | QX_FLAG_SEMI_STATIC,				NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_unskip_setvar },
	{ "grid.input.frequency.low",			ST_FLAG_RW,			voltronic_sunny_r_grid_inout_freq_min,	"QBYF\r",			"",	11,	'(',	"",	6,	9,	"%.1f",			QX_FLAG_RANGE | QX_FLAG_SEMI_STATIC,				NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_unskip_setvar },

	/* Set grid input frequency limits (P16 only)
	 * > [PGF<n>\r]		> [PSF<n>\r]
	 * < [(ACK\r]		< [(ACK\r]
	 *    01234		   01234
	 *    0			   0
	 */

	{ "grid.input.frequency.high",			0,				voltronic_sunny_r_grid_inout_freq_max,	"PGF%04.1f\r",			"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_RANGE | QX_FLAG_SKIP,			NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_process_setvar },
	{ "grid.input.frequency.low",			0,				voltronic_sunny_r_grid_inout_freq_min,	"PSF%04.1f\r",			"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_RANGE | QX_FLAG_SKIP,			NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_process_setvar },
#endif

#if 0
	/* Query device for waiting time before grid connection
	 * > [QFT\r]
	 * < [(060\r]
	 *    01234
	 *    0
	 */

	{ "grid.waitingtime",				ST_FLAG_RW,			voltronic_sunny_r_grid_waiting_time,	"QFT\r",			"",	5,	'(',	"",	1,	3,	"%.0f",			QX_FLAG_RANGE | QX_FLAG_SEMI_STATIC,				NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_unskip_setvar },

	/* Set waiting time before grid connection
	 * > [FT<n>\r]
	 * < [(ACK\r]
	 *    01234
	 *    0
	 */

	{ "grid.waitingtime",				0,				voltronic_sunny_r_grid_waiting_time,	"FT%03.0f\r",			"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_RANGE | QX_FLAG_SKIP,			NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_process_setvar },
#endif

#if 0
	/* Query device for battery-charging data (P16 only)
	 * > [QCHGS\r]
	 * < [(00.3 54.0 25.0 55.4\r]
	 *    012345678901234567890
	 *    0         1         2
	 */

	{ "battery.charging.current",			0,				NULL,					"QCHGS\r",			"",	21,	'(',	"",	1,	4,	"%.1f",			0,								voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },
	{ "battery.charging.voltage.floating",		ST_FLAG_RW,			voltronic_sunny_r_bc_v_floating,	"QCHGS\r",			"",	21,	'(',	"",	6,	9,	"%.1f",			QX_FLAG_RANGE | QX_FLAG_SEMI_STATIC,				voltronic_axpert_crc,		voltronic_axpert_checkcrc,		voltronic_sunny_unskip_setvar_and_update_related_vars_limits },
	{ "battery.charging.current.high",		ST_FLAG_RW,			voltronic_sunny_r_bc_c_max,		"QCHGS\r",			"",	21,	'(',	"",	11,	14,	"%.1f",			QX_FLAG_RANGE | QX_FLAG_SEMI_STATIC,				voltronic_axpert_crc,		voltronic_axpert_checkcrc,		voltronic_sunny_unskip_setvar_and_update_related_vars_limits },
	{ "battery.charging.voltage.bulk",		ST_FLAG_RW,			voltronic_sunny_r_bc_v_bulk,		"QCHGS\r",			"",	21,	'(',	"",	16,	19,	"%.1f",			QX_FLAG_RANGE | QX_FLAG_SEMI_STATIC,				voltronic_axpert_crc,		voltronic_axpert_checkcrc,		voltronic_sunny_unskip_setvar_and_update_related_vars_limits },
#endif

	/* Set battery-charging limits (P16 only)
	 * > [MCHGV<n>\r]	> [MCHGC<n>\r]		> [BCHGV<n>\r]
	 * < [(ACK\r]		< [(ACK\r]		< [(ACK\r]
	 *    01234		   01234		   01234
	 *    0			   0			   0
	 */
#if 0
	{ "battery.charging.voltage.floating",		0,				voltronic_sunny_r_bc_v_floating,	"MCHGV%04.1f\r",		"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_RANGE | QX_FLAG_SKIP,			NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_process_setvar },
	{ "battery.charging.current.high",		0,				voltronic_sunny_r_bc_c_max,		"MCHGC%04.1f\r",		"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_RANGE | QX_FLAG_SKIP,			NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_process_setvar },
	{ "battery.charging.voltage.bulk",		0,				voltronic_sunny_r_bc_v_bulk,		"BCHGV%04.1f\r",		"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_RANGE | QX_FLAG_SKIP,			NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_process_setvar },
#endif

#if 0
	/* Query device for battery charger limits (P16 only)
	 * > [QOFFC\r]
	 * < [(00.0 53.0 060\r]
	 *    012345678901234
	 *    0         1
	 */

	{ "battery.charging.current.floating.low",	ST_FLAG_RW,			voltronic_sunny_r_bc_c_floating_low,	"QOFFC\r",			"",	15,	'(',	"",	1,	4,	"%.1f",			QX_FLAG_RANGE | QX_FLAG_SEMI_STATIC,				NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_unskip_setvar_and_update_related_vars_limits },
	{ "battery.charging.restartvoltage",		ST_FLAG_RW,			voltronic_sunny_r_bc_v_restart,		"QOFFC\r",			"",	15,	'(',	"",	6,	9,	"%.1f",			QX_FLAG_RANGE | QX_FLAG_SEMI_STATIC,				NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_unskip_setvar_and_update_related_vars_limits },
	{ "battery.charging.timethreshold",		ST_FLAG_RW,			voltronic_sunny_r_bc_time_threshold,	"QOFFC\r",			"",	15,	'(',	"",	11,	13,	"%.0f",			QX_FLAG_RANGE | QX_FLAG_SEMI_STATIC,				NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_unskip_setvar },

	/* Set battery charger limits (P16 only)
	 * > [OFFC<a> <b> <c>\r]
	 * < [(ACK\r]
	 *    01234
	 *    0
	 */

	{ "battery.charging.current.floating.low",	0,				voltronic_sunny_r_bc_c_floating_low,	"OFFC%04.1f %04.1f %03.0f\r",	"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_RANGE | QX_FLAG_SKIP,			NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_charger_limits_set },
	{ "battery.charging.restartvoltage",		0,				voltronic_sunny_r_bc_v_restart,		"OFFC%04.1f %04.1f %03.0f\r",	"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_RANGE | QX_FLAG_SKIP,			NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_charger_limits_set },
	{ "battery.charging.timethreshold",		0,				voltronic_sunny_r_bc_time_threshold,	"OFFC%04.1f %04.1f %03.0f\r",	"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_RANGE | QX_FLAG_SKIP,			NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_charger_limits_set },

	/* Query device for MaxAcChargingCurrent (P16 only)	TODO: update with real device data	FIXME
	 * > [QACCHC\r]
	 * < [(??\r]
	 *    0
	 *    0
	 */

/*	{ "battery.charging.current.ac.high",		0,				NULL,					"QACCHC\r",			"",	2,	'(',	"",	1,	0,	"%s",			QX_FLAG_STATIC,							NULL,				NULL,					NULL },	*/

	/* Query device for battery-discharging limits (P16 only)
	 * > [QBSDV\r]
	 * < [(48.0 48.0 48.0 49.4\r]
	 *    012345678901234567890
	 *    0         1         2
	 */

	{ "battery.discharging.cutoffvoltage.gridoff",	ST_FLAG_RW,			voltronic_sunny_r_bd_v_cutoff_gridoff,	"QBSDV\r",			"",	21,	'(',	"",	1,	4,	"%.1f",			QX_FLAG_RANGE | QX_FLAG_SEMI_STATIC,				NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_unskip_setvar_and_update_related_vars_limits },
	{ "battery.discharging.cutoffvoltage.gridon",	ST_FLAG_RW,			voltronic_sunny_r_bd_v_cutoff_gridon,	"QBSDV\r",			"",	21,	'(',	"",	6,	9,	"%.1f",			QX_FLAG_RANGE | QX_FLAG_SEMI_STATIC,				NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_unskip_setvar_and_update_related_vars_limits },
	{ "battery.discharging.restartvoltage.gridoff",	ST_FLAG_RW,			voltronic_sunny_r_bd_v_restart_gridoff,	"QBSDV\r",			"",	21,	'(',	"",	11,	14,	"%.1f",			QX_FLAG_RANGE | QX_FLAG_SEMI_STATIC,				NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_unskip_setvar_and_update_related_vars_limits },
	{ "battery.discharging.restartvoltage.gridon",	ST_FLAG_RW,			voltronic_sunny_r_bd_v_restart_gridon,	"QBSDV\r",			"",	21,	'(',	"",	16,	19,	"%.1f",			QX_FLAG_RANGE | QX_FLAG_SEMI_STATIC,				NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_unskip_setvar_and_update_related_vars_limits },

	/* Set battery-discharging limits (P16 only)
	 * > [BSDV<a> <b>\r]	> [DSUBV<a> <b>\r]
	 * < [(ACK\r]		< [(ACK\r]
	 *    01234		   01234
	 *    0			   0
	 */

	{ "battery.discharging.cutoffvoltage.gridoff",	0,				voltronic_sunny_r_bd_v_cutoff_gridoff,	"BSDV%04.1f %04.1f\r",		"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_RANGE | QX_FLAG_SKIP,			NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_discharging_limits_set },
	{ "battery.discharging.cutoffvoltage.gridon",	0,				voltronic_sunny_r_bd_v_cutoff_gridon,	"BSDV%04.1f %04.1f\r",		"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_RANGE | QX_FLAG_SKIP,			NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_discharging_limits_set },
	{ "battery.discharging.restartvoltage.gridoff",	0,				voltronic_sunny_r_bd_v_restart_gridoff,	"DSUBV%04.1f %04.1f\r",		"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_RANGE | QX_FLAG_SKIP,			NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_discharging_limits_set },
	{ "battery.discharging.restartvoltage.gridon",	0,				voltronic_sunny_r_bd_v_restart_gridon,	"DSUBV%04.1f %04.1f\r",		"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_RANGE | QX_FLAG_SKIP,			NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_discharging_limits_set },

	/* Query device for PV input voltage limits
	 * > [QPVIPV\r]
	 * < [(500 090\r]
	 *    012345678
	 *    0
	 */

	{ "pv.input.voltage.high",			ST_FLAG_RW,			voltronic_sunny_r_pv_input_volt_max,	"QPVIPV\r",			"",	9,	'(',	"",	1,	3,	"%.1f",			QX_FLAG_RANGE | QX_FLAG_SEMI_STATIC,				NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_unskip_setvar },
	{ "pv.input.voltage.low",			ST_FLAG_RW,			voltronic_sunny_r_pv_input_volt_min,	"QPVIPV\r",			"",	9,	'(',	"",	5,	7,	"%.1f",			QX_FLAG_RANGE | QX_FLAG_SEMI_STATIC,				NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_unskip_setvar },

	/* Set PV input voltage limits
	 * > [PVIPHV<n>\r]	> [PVIPLV<n>\r]
	 * < [(ACK\r]		< [(ACK\r]
	 *    01234		   01234
	 *    0			   0
	 */

	{ "pv.input.voltage.high",			0,				voltronic_sunny_r_pv_input_volt_max,	"PVIPHV%03.0f\r",		"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_RANGE | QX_FLAG_SKIP,			NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_process_setvar },
	{ "pv.input.voltage.low",			0,				voltronic_sunny_r_pv_input_volt_min,	"PVIPLV%03.0f\r",		"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_RANGE | QX_FLAG_SKIP,			NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_process_setvar },

	/* Query device for max/min MPP voltages
	 * > [QMPPTV\r]
	 * < [(450 120\r]
	 *    012345678
	 *    0
	 */

	{ "mpp.voltage.high",				ST_FLAG_RW,			voltronic_sunny_r_mpp_input_volt_max,	"QMPPTV\r",			"",	9,	'(',	"",	1,	3,	"%.1f",			QX_FLAG_RANGE | QX_FLAG_SEMI_STATIC,				NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_unskip_setvar },
	{ "mpp.voltage.low",				ST_FLAG_RW,			voltronic_sunny_r_mpp_input_volt_min,	"QMPPTV\r",			"",	9,	'(',	"",	5,	7,	"%.1f",			QX_FLAG_RANGE | QX_FLAG_SEMI_STATIC,				NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_unskip_setvar },

	/* Set max/min MPP voltages
	 * > [MPPTHV<n>\r]	> [MPPTLV<n>\r]
	 * < [(ACK\r]		< [(ACK\r]
	 *    01234		   01234
	 *    0			   0
	 */

	{ "mpp.voltage.high",				0,				voltronic_sunny_r_mpp_input_volt_max,	"MPPTHV%03.0f\r",		"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_RANGE | QX_FLAG_SKIP,			NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_process_setvar },
	{ "mpp.voltage.low",				0,				voltronic_sunny_r_mpp_input_volt_min,	"MPPTLV%03.0f\r",		"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_RANGE | QX_FLAG_SKIP,			NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_process_setvar },
#endif

	/*#######################################################################################################################################################################################################################################################################################################################################################################################################################################*/

#if 0
	/* Query device for maximum output power
	 * > [QOPMP\r]
	 * < [(03000\r]
	 *    0123456
	 *    0
	 */

	{ "output.realpower.max",			ST_FLAG_RW,			voltronic_sunny_r_output_realpower_max,	"QOPMP\r",			"",	7,	'(',	"",	1,	5,	"%.0f",			QX_FLAG_RANGE | QX_FLAG_SEMI_STATIC,				NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_unskip_setvar_and_update_related_vars_limits },

	/* Set maximum output power
	 * > [OPMP<n>\r]
	 * < [(ACK\r]
	 *    01234
	 *    0
	 */

	{ "output.realpower.max",			0,				voltronic_sunny_r_output_realpower_max,	"OPMP%05.0f\r",			"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_RANGE | QX_FLAG_SKIP,			NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_process_setvar },

	/* Query device for maximum power feeding grid
	 * > [QGPMP\r]
	 * < [(03000\r]
	 *    0123456
	 *    0
	 */

	{ "grid.realpower.max",				ST_FLAG_RW,			voltronic_sunny_r_grid_realpower_max,	"QGPMP\r",			"",	7,	'(',	"",	1,	5,	"%.0f",			QX_FLAG_RANGE | QX_FLAG_SEMI_STATIC,				NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_unskip_setvar_and_update_related_vars_limits },

	/* Set maximum power feeding grid
	 * > [GPMP<n>\r]
	 * < [(ACK\r]
	 *    01234
	 *    0
	 */

	{ "grid.realpower.max",				0,				voltronic_sunny_r_grid_realpower_max,	"GPMP%05.0f\r",			"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_RANGE | QX_FLAG_SKIP,			NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_process_setvar },

	/* Query device for LCD sleep time (time after which LCD screen-saver starts)
	 * > [QLST\r]
	 * < [(10\r]
	 *    0123
	 *    0
	 */

	{ "lcd.sleeptime",				ST_FLAG_RW,			voltronic_sunny_r_lcd_sleep_time,	"QLST\r",			"",	4,	'(',	"",	1,	2,	"%d",			QX_FLAG_RANGE | QX_FLAG_SEMI_STATIC,				NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_lst },

	/* Set LCD sleep time (time after which LCD screen-saver starts)
	 * > [LST<n>\r]
	 * < [(ACK\r]
	 *    01234
	 *    0
	 */

	{ "lcd.sleeptime",				0,				voltronic_sunny_r_lcd_sleep_time,	"LST%02.0f\r",			"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_RANGE | QX_FLAG_SKIP,			NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_process_setvar },

	/* Query device for allowed charging time settings (P16 only)
	 * > [QCHT\r]
	 * < [(HHMM HHMM\r]
	 *    01234567890
	 *    0         1
	 */

	{ "charging_time.start",			ST_FLAG_RW | ST_FLAG_STRING,	voltronic_sunny_l_hhmm,			"QCHT\r",			"",	11,	'(',	"",	1,	4,	"%02d:%02d",		QX_FLAG_SEMI_STATIC,						NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_hhmm },
	{ "charging_time.end",				ST_FLAG_RW | ST_FLAG_STRING,	voltronic_sunny_l_hhmm,			"QCHT\r",			"",	11,	'(',	"",	6,	9,	"%02d:%02d",		QX_FLAG_SEMI_STATIC,						NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_hhmm },

	/* Set allowed charging time settings (P16 only)
	 * > [CHTH<time>\r]	> [CHTL<time>\r]	<time>: hhmm
	 * < [(ACK\r]		< [(ACK\r]
	 *    01234		   01234
	 *    0			   0
	 */

	{ "charging_time.start",			ST_FLAG_STRING,			NULL,					"CHTH%02d%02d\r",		"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_SKIP,					NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_hhmm_set },
	{ "charging_time.end",				ST_FLAG_STRING,			NULL,					"CHTL%02d%02d\r",		"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_SKIP,					NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_hhmm_set },

	/* Query device for allowed charging-from-AC time settings (P16 only)
	 * > [QPKT\r]
	 * < [(HHMM HHMM\r]
	 *    01234567890
	 *    0         1
	 */

	{ "charging_from_ac_time.start",		ST_FLAG_RW | ST_FLAG_STRING,	voltronic_sunny_l_hhmm,			"QPKT\r",			"",	11,	'(',	"",	1,	4,	"%02d:%02d",		QX_FLAG_SEMI_STATIC,						NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_hhmm },
	{ "charging_from_ac_time.end",			ST_FLAG_RW | ST_FLAG_STRING,	voltronic_sunny_l_hhmm,			"QPKT\r",			"",	11,	'(',	"",	6,	9,	"%02d:%02d",		QX_FLAG_SEMI_STATIC,						NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_hhmm },

	/* Set allowed charging-from-AC time settings (P16 only)
	 * > [PKT<start> <end>\r]	<start>, <end>: hhmm
	 * < [(ACK\r]
	 *    01234
	 *    0
	 */

	{ "charging_from_ac_time.start",		ST_FLAG_STRING,			NULL,					"PKT%02d%02d %02d%02d\r",	"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_SKIP,					NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_hhmm_x2_set },
	{ "charging_from_ac_time.end",			ST_FLAG_STRING,			NULL,					"PKT%02d%02d %02d%02d\r",	"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_SKIP,					NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_hhmm_x2_set },

	/* Query device for allowed AC-output time settings (P16 only)
	 * > [QLDT\r]
	 * < [(HHMM HHMM\r]
	 *    01234567890
	 *    0         1
	 */

	{ "ac_output_time.start",			ST_FLAG_RW | ST_FLAG_STRING,	voltronic_sunny_l_hhmm,			"QLDT\r",			"",	11,	'(',	"",	1,	4,	"%02d:%02d",		QX_FLAG_SEMI_STATIC,						NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_hhmm },
	{ "ac_output_time.end",				ST_FLAG_RW | ST_FLAG_STRING,	voltronic_sunny_l_hhmm,			"QLDT\r",			"",	11,	'(',	"",	6,	9,	"%02d:%02d",		QX_FLAG_SEMI_STATIC,						NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_hhmm },

	/* Set allowed AC-output time settings (P16 only)
	 * > [LDT<start> <end>\r]	<start>, <end>: hhmm
	 * < [(ACK\r]
	 *    01234
	 *    0
	 */

	{ "ac_output_time.start",			ST_FLAG_STRING,			NULL,					"LDT%02d%02d %02d%02d\r",	"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_SKIP,					NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_hhmm_x2_set },
	{ "ac_output_time.end",				ST_FLAG_STRING,			NULL,					"LDT%02d%02d %02d%02d\r",	"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_SKIP,					NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_hhmm_x2_set },

	/* Query device for grid input average voltages limits
	 * > [QGLTV\r]
	 * < [(253 ---\r]
	 *    012345678
	 *    0
	 */

	{ "grid.input.voltage.avg.max",			ST_FLAG_RW,			voltronic_sunny_r_grid_in_avg_volt_max,	"QGLTV\r",			"",	9,	'(',	"",	1,	3,	"%.0f",			QX_FLAG_RANGE | QX_FLAG_SEMI_STATIC,				NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_unskip_setvar },
	{ "grid.input.voltage.avg.min",			0,				NULL,					"QGLTV\r",			"",	9,	'(',	"",	5,	7,	"%.0f",			QX_FLAG_STATIC,							NULL,				voltronic_axpert_checkcrc,		NULL },

	/* Set maximum grid input average voltage
	 * > [GLTHV<n>\r]
	 * < [(ACK\r]
	 *    01234
	 *    0
	 */

	{ "grid.input.voltage.avg.max",			0,				voltronic_sunny_r_grid_in_avg_volt_max,	"GLTHV%03.0f\r",		"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_RANGE | QX_FLAG_SKIP,			NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_process_setvar },

	/* Query device for liFe flag (P16 only)	FIXME
	 * liFeSign = (splitArray[1] > 0) ? true : false
	 * > [QEBGP\r]
	 * < [(+000 00\r]|
	 *    012345678
	 *    0
	 */

	{ "grid.power.deviation", 			ST_FLAG_RW,			voltronic_sunny_r_grid_power_deviation,	"QEBGP\r",			"",	9,	'(',	"",	2,	4,	"%.0f",			QX_FLAG_RANGE | QX_FLAG_SEMI_STATIC,				NULL, 				voltronic_axpert_checkcrc,		voltronic_sunny_unskip_setvar },
	{ "battery.isLiFe", 				ST_FLAG_RW,			voltronic_axpert_e_cap,			"QEBGP\r",			"",	9,	'(',	"",	7,	7,	"%s",			QX_FLAG_ENUM | QX_FLAG_SEMI_STATIC,				NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_01 },

	/* Set grid power deviation (P16 only)	FIXME
	 * > [ABGP<n>\r]
	 * < [(ACK\r]
	 *    01234
	 *    0
	 */

	{ "grid.power.deviation",			0,				voltronic_sunny_r_grid_power_deviation,	"ABGP%+04.0f\r",		"",	5,	'(',	"",	1,	3,	"%d",			QX_FLAG_SETVAR | QX_FLAG_RANGE | QX_FLAG_SKIP,			NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_process_setvar },

	/* Set whether a LiFePo battery is connected or not (P16 only)	FIXME
	 * > [LBF<n>\r]
	 * < [(ACK\r]
	 *    01234
	 *    0
	 */

	{ "battery.isLiFe",				0,				voltronic_axpert_e_cap,			"LBF%d\r",			"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_ENUM | QX_FLAG_SKIP,			NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_01_set },
#endif

	/*#######################################################################################################################################################################################################################################################################################################################################################################################################################################*/

#if 0
	/* Query device for power factor (P15 only)	FIXME: value should be divided by 100
	 * > [QOPF\r]
	 * < [(090\r]
	 *    01234
	 *    0
	 */

	{ "output.powerfactor",				ST_FLAG_RW,			voltronic_sunny_r_output_powerfactor,	"QOPF\r",			"",	5,	'(',	"",	1,	3,	"%d",			QX_FLAG_RANGE | QX_FLAG_SEMI_STATIC,				NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_pf },

	/* Set power factor (P15 only)	FIXME: value should be divided by 100
	 * > [SOPF<n>\r]
	 * < [(ACK\r]
	 *    01234
	 *    0
	 */

	{ "output.powerfactor",				0,				voltronic_sunny_r_output_powerfactor,	"SOPF%+04.0f\r",		"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_RANGE | QX_FLAG_SKIP,			NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_process_setvar },

	/* Query device for power percent settings (P15 only)
	 * > [QPPS\r]
	 * < [(090\r]
	 *    01234
	 *    0
	 */

	{ "power.percentsetting",			ST_FLAG_RW,			voltronic_sunny_r_powerpercent_setting,	"QPPS\r",			"",	5,	'(',	"",	1,	3,	"%.0f",			QX_FLAG_RANGE | QX_FLAG_SEMI_STATIC,				NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_unskip_setvar },

	/* Set power percent setting (P15 only)
	 * > [PPS<n>\r]
	 * < [(ACK\r]
	 *    01234
	 *    0
	 */

	{ "power.percentsetting",			0,				voltronic_sunny_r_powerpercent_setting,	"PPS%03.0f\r",			"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_RANGE | QX_FLAG_SKIP,			NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_process_setvar },

	/* Query device for power factor percent (P15 only)
	 * > [QPPD\r]
	 * < [(090\r]
	 *    01234
	 *    0
	 */

	{ "output.powerfactor.percent",			ST_FLAG_RW,			voltronic_sunny_r_powerfactor_percent,	"QPPD\r",			"",	5,	'(',	"",	1,	3,	"%.0f",			QX_FLAG_RANGE | QX_FLAG_SEMI_STATIC,				NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_unskip_setvar },

	/* Set power factor percent (P15 only)
	 * > [PPD<n>\r]
	 * < [(ACK\r]
	 *    01234
	 *    0
	 */

	{ "output.powerfactor.percent",			0,				voltronic_sunny_r_powerfactor_percent,	"PPD%03.0f\r",			"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_RANGE | QX_FLAG_SKIP,			NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_process_setvar },

	/* Query device for power factor curve (P15 only)
	 * > [QPFL\r]
	 * < [(190\r]
	 *    01234
	 *    0
	 */

	{ "output.powerfactor.curve",			ST_FLAG_RW,			voltronic_sunny_r_powerfactor_curve,	"QPFL\r",			"",	5,	'(',	"",	1,	3,	"%d",			QX_FLAG_RANGE | QX_FLAG_SEMI_STATIC,				NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_pf },

	/* Set power factor curve (P15 only)
	 * > [PFL<n>\r]
	 * < [(ACK\r]
	 *    01234
	 *    0
	 */

	{ "output.powerfactor.curve",			0,				voltronic_sunny_r_powerfactor_curve,	"PFL%03d\r",			"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_RANGE | QX_FLAG_SKIP,			NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_pfc_set },
#endif

	/*#######################################################################################################################################################################################################################################################################################################################################################################################################################################*/

#if 0
	/* Query device for date/time
	 * > [QT\r]
	 * < [(YYYYMMDDhhmmss\r]
	 *    0123456789012345
	 *    0         1
	 */

	{ "device.date",				ST_FLAG_RW | ST_FLAG_STRING,	voltronic_sunny_l_date,			"QT\r",				"",	16,	'(',	"",	1,	8,	"%04d/%02d/%02d",	0,								NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_date },
	{ "device.time",				ST_FLAG_RW | ST_FLAG_STRING,	voltronic_sunny_l_time,			"QT\r",				"",	16,	'(',	"",	9,	14,	"%02d:%02d:%02d",	0,								NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_time },

	/* Set device date/time
	 * > [DAT<time>\r]	<time>: YYMMDDhhmmss
	 * < [(ACK\r]
	 *    01234
	 *    0
	 */

	{ "device.date",				ST_FLAG_STRING,			NULL,					"DAT%02d%02d%02d%s\r",		"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_SKIP,					NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_date_set },
	{ "device.time",				ST_FLAG_STRING,			NULL,					"DAT%s%02d%02d%02d\r",		"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_SETVAR | QX_FLAG_SKIP,					NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_time_set },

#endif

	/*#######################################################################################################################################################################################################################################################################################################################################################################################################################################*/

	/* Query device for status
	 * > [QPIGS\r]
	 * < [(235.2 50.1 235.2 50.1 0446 0382 008 440 54.00 000 100 0034 0000 000.0 00.00 00000 00010101 00 00 00000 110\r]
	 *    012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567
	 *    0         1         2         3         4         5         6         7         8         9        10
	 */

	{ "input.voltage",				0,				NULL,					"QPIGS\r",			"",	108,	'(',	"",	1,	5,	"%.1f",			0,								voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },
	{ "input.frequency",				0,				NULL,					"QPIGS\r",			"",	108,	'(',	"",	7,	10,	"%.1f",			0,								voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },
	{ "output.voltage",				0,				NULL,					"QPIGS\r",			"",	108,	'(',	"",	12,	16,	"%.1f",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },
	{ "output.frequency",				0,				NULL,					"QPIGS\r",			"",	108,	'(',	"",	18,	21,	"%.1f",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },
	{ "output.power",				0,				NULL,					"QPIGS\r",			"",	108,	'(',	"",	23,	26,	"%.1f",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },
	{ "output.realpower",				0,				NULL,					"QPIGS\r",			"",	108,	'(',	"",	28,	31,	"%.1f",			QX_FLAG_STATIC,							voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },
	{ "ups.load",					0,				NULL,					"QPIGS\r",			"",	108,	'(',	"",	33,	35,	"%.0f",			0,								voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },
/*	{ "bus_voltage",				0,				NULL,					"QPIGS\r",			"",	108,	'(',	"",	37,	39,	"%.1f",			0,								voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },	*/
	{ "battery.voltage",				0,				NULL,					"QPIGS\r",			"",	108,	'(',	"",	41,	45,	"%.1f",			0,								voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },
/*	{ "battery_charging_current",			0,				NULL,					"QPIGS\r",			"",	108,	'(',	"",	47,	49,	"%.1f",			0,								voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },	*/
	{ "battery.charge",				0,				NULL,					"QPIGS\r",			"",	108,	'(',	"",	51,	53,	"%.1f",			0,								voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },
	{ "ups.temperature",				0,				NULL,					"QPIGS\r",			"",	108,	'(',	"",	55,	58,	"%.1f",			0,								voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },
	{ "input.pv1.current",				0,				NULL,					"QPIGS\r",			"",	108,	'(',	"",	60,	63,	"%.0f",			0,								voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },
	{ "input.pv1.voltage",				0,				NULL,					"QPIGS\r",			"",	108,	'(',	"",	65,	69,	"%.0f",			0,								voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },
	{ "battery.voltage.scc1",			0,				NULL,					"QPIGS\r",			"",	108,	'(',	"",	71,	75,	"%.1f",			0,								voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },
	{ "battery.current",				0,				NULL,					"QPIGS\r",			"",	108,	'(',	"",	77,	81,	"%.1f",			0,								voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },
	{ "ups.status",					0,				NULL,					"QPIGS\r",			"",	108,	'(',	"",	86,	86,	"%s",			QX_FLAG_QUICK_POLL,						voltronic_axpert_crc,		voltronic_axpert_checkcrc,		voltronic_axpert_status },
	{ "ups.status",					0,				NULL,					"QPIGS\r",			"",	108,	'(',	"",	88,	88,	"%s",			QX_FLAG_QUICK_POLL,						voltronic_axpert_crc,		voltronic_axpert_checkcrc,		voltronic_axpert_status },
/*	{ "battery_voltage_offset_for_fans_on",		0,				NULL,					"QPIGS\r",			"",	108,	'(',	"",	92,	93,	"%.1f",			0,								voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },	*/
/*	{ "eeprom_version",				0,				NULL,					"QPIGS\r",			"",	108,	'(',	"",	95,	96,	"%.1f",			0,								voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },	*/
/*	{ "pv_charging_power_1",			0,				NULL,					"QPIGS\r",			"",	108,	'(',	"",	98,	102,	"%.1f",			0,								voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },	*/

	/* Query device for status
	 * > [QPIGS2\r]
	 * < [(235.2 50.1 235.2 50.1 0446 0382 008 440 54.00 000 100 0034 0000 000.0 00.00 00000 00010101 00 00 00000 110\r]
	 *    012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567
	 *    0         1         2         3         4         5         6         7         8         9        10
	 */

	{ "input.pv2.current",				0,				NULL,					"QPIGS2\r",			"",	69,	'(',	"",	1,	4,	"%.0f",			0,								voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },
	{ "input.pv2.voltage",				0,				NULL,					"QPIGS2\r",			"",	69,	'(',	"",	6,	10,	"%.0f",			0,								voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },
	{ "battery.voltage.scc2",			0,				NULL,					"QPIGS2\r",			"",	69,	'(',	"",	12,	16,	"%.1f",			0,								voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },
/*	{ "pv_charging_power_2",			0,				NULL,					"QPIGS2\r",			"",	69,	'(',	"",	18,	22,	"%.1f",			0,								voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },	*/
/*	{ "ups.status",					0,				NULL,					"QPIGS2\r",			"",	69,	'(',	"",	24,	24,	"%s",			QX_FLAG_QUICK_POLL,						voltronic_axpert_crc,		voltronic_axpert_checkcrc,		voltronic_axpert_status },*/
/*	{ "ac_charging_current",			0,				NULL,					"QPIGS2\r",			"",	69,	'(',	"",	33,	36,	"%.1f",			0,								voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },	*/
/*	{ "ac_charging_power",				0,				NULL,					"QPIGS2\r",			"",	69,	'(',	"",	38,	41,	"%.1f",			0,								voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },	*/
	{ "input.pv3.current",				0,				NULL,					"QPIGS2\r",			"",	69,	'(',	"",	42,	45,	"%.0f",			0,								voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },
	{ "input.pv3.voltage",				0,				NULL,					"QPIGS2\r",			"",	69,	'(',	"",	47,	51,	"%.0f",			0,								voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },
	{ "battery.voltage.scc3",			0,				NULL,					"QPIGS2\r",			"",	69,	'(',	"",	53,	57,	"%.1f",			0,								voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },
/*	{ "pv_charging_power_3",			0,				NULL,					"QPIGS2\r",			"",	69,	'(',	"",	58,	61,	"%.1f",			0,								voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },	*/
/*	{ "pv_total_charging_power",			0,				NULL,					"QPIGS2\r",			"",	69,	'(',	"",	63,	67,	"%.1f",			0,								voltronic_axpert_crc,		voltronic_axpert_checkcrc,		NULL },	*/

	/* Query device for warnings and their type
	 * > [QPIWS\r]
	 * < [(--0000000000--00000---00000-----------------------------------------------------------------------------------------------------\r]
	 * < [(0000000100000000000000000\r]														<- known warnings (-> minimum length: 27)
	 *    0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789
	 *    0         1         2         3         4         5         6         7         8         9        10        11        12
	 */

	{ "ups.alarm",					0,				NULL,					"QPIWS\r",			"",	27,	'(',	"",	1,	0,	"%s",			QX_FLAG_QUICK_POLL,						voltronic_axpert_crc,		voltronic_axpert_checkcrc,		voltronic_sunny_warning },	/* FIXME: should be "device.alarm" */

	/* Query device for actual working mode
	 * > [QMOD\r]
	 * < [(S\r]
	 *    012
	 *    0
	 */

	{ "ups.alarm",					0,				NULL,					"QMOD\r",			"",	3,	'(',	"",	1,	1,	"%s",			0,								voltronic_axpert_crc,		voltronic_axpert_checkcrc,		voltronic_axpert_mode },	/* FIXME: should be "device.alarm" */
	{ "ups.status",					0,				NULL,					"QMOD\r",			"",	3,	'(',	"",	1,	1,	"%s",			QX_FLAG_QUICK_POLL,						voltronic_axpert_crc,		voltronic_axpert_checkcrc,		voltronic_axpert_mode },	/* FIXME: should be "device.status" */

#if 0
	/* Query device for actual infos about battery (P16 only)	TODO: update with real device data
	 * > [QPIBI\r]
	 * < [(000 001 002 003 004\r]
	 *    012345678901234567890
	 *    0         1         2
	 */

/*	{ "unknown.?",					0,				NULL,					"QPIBI\r",			"",	21,	'(',	"",	1,	3,	"%s",			0,								NULL,				voltronic_axpert_checkcrc,		NULL },	*/
	{ "battery.number",				0,				NULL,					"QPIBI\r",			"",	21,	'(',	"",	5,	7,	"%.0f",			QX_FLAG_STATIC,							NULL,				voltronic_axpert_checkcrc,		NULL },
	{ "battery.capacity",				0,				NULL,					"QPIBI\r",			"",	21,	'(',	"",	9,	11,	"%.0f",			0,								NULL,				voltronic_axpert_checkcrc,		NULL },
/*	{ "unknown.?",					0,				NULL,					"QPIBI\r",			"",	21,	'(',	"",	13,	15,	"%s",			0,								NULL,				voltronic_axpert_checkcrc,		NULL },	*/
	{ "battery.runtime",				0,				NULL,					"QPIBI\r",			"",	21,	'(',	"",	17,	19,	"%.0f",			0,								NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_batt_runtime },

#endif

	/*#######################################################################################################################################################################################################################################################################################################################################################################################################################################*/

#if 0

	/* Query device for faults and their type	TODO: update with real device data
	 * IFF FW version (QSVFW2) < (in P15: 0.9; in P16: 0.3) && (QPIWS #1 == 1)
	 * > [QPIFS\r]
	 * < [(OK\r] <- No fault
	 *    0123
	 *    0
	 * < [(14 YYYYMMDDhhmmss 000 001 002 003 004 005 006 007 008 009 010 011 012 013 014 015 016 017\r] <- Fault type + Short status
	 *    0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890
	 *    0         1         2         3         4         5         6         7         8         9
	 */

	{ "ups.alarm",					0,				NULL,					"QPIFS\r",			"",	91,	'(',	"",	1,	2,	"%s",			QX_FLAG_SKIP,							NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_fault },	/* FIXME: should be "device.alarm" */
	{ "device.date.fault",				0,				NULL,					"QPIFS\r",			"",	91,	'(',	"",	4,	11,	"%04d/%02d/%02d",	QX_FLAG_SKIP,							NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_date_skip_me },
	{ "device.time.fault",				0,				NULL,					"QPIFS\r",			"",	91,	'(',	"",	12,	17,	"%02d:%02d:%02d",	QX_FLAG_SKIP,							NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_time_skip_me },
	{ "pv1.input.voltage.fault",			0,				NULL,					"QPIFS\r",			"",	91,	'(',	"",	19,	21,	"%.1f",			QX_FLAG_SKIP,							NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_skip_me },
	{ "pv1.input.current.fault",			0,				NULL,					"QPIFS\r",			"",	91,	'(',	"",	23,	25,	"%.1f",			QX_FLAG_SKIP,							NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_skip_me },
	{ "pv2.input.voltage.fault",			0,				NULL,					"QPIFS\r",			"",	91,	'(',	"",	27,	29,	"%.1f",			QX_FLAG_SKIP,							NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_skip_me },
	{ "pv2.input.current.fault",			0,				NULL,					"QPIFS\r",			"",	91,	'(',	"",	31,	33,	"%.1f",			QX_FLAG_SKIP,							NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_skip_me },
	{ "pv3.input.voltage.fault",			0,				NULL,					"QPIFS\r",			"",	91,	'(',	"",	35,	37,	"%.1f",			QX_FLAG_SKIP,							NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_skip_me },
	{ "pv3.input.current.fault",			0,				NULL,					"QPIFS\r",			"",	91,	'(',	"",	39,	41,	"%.1f",			QX_FLAG_SKIP,							NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_skip_me },
	{ "inverter.voltage.fault",			0,				NULL,					"QPIFS\r",			"",	91,	'(',	"",	43,	45,	"%.1f",			QX_FLAG_SKIP,							NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_skip_me },
	{ "inverter.current.fault",			0,				NULL,					"QPIFS\r",			"",	91,	'(',	"",	47,	49,	"%.1f",			QX_FLAG_SKIP,							NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_skip_me },
	{ "grid.voltage.fault",				0,				NULL,					"QPIFS\r",			"",	91,	'(',	"",	51,	53,	"%.1f",			QX_FLAG_SKIP,							NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_skip_me },
	{ "grid.frequency.fault",			0,				NULL,					"QPIFS\r",			"",	91,	'(',	"",	55,	57,	"%.1f",			QX_FLAG_SKIP,							NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_skip_me },
	{ "grid.current.fault",				0,				NULL,					"QPIFS\r",			"",	91,	'(',	"",	59,	61,	"%.1f",			QX_FLAG_SKIP,							NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_skip_me },
	{ "output.load.percent.fault",			0,				NULL,					"QPIFS\r",			"",	91,	'(',	"",	63,	65,	"%.1f",			QX_FLAG_SKIP,							NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_skip_me },
	{ "output.load.current.fault",			0,				NULL,					"QPIFS\r",			"",	91,	'(',	"",	67,	69,	"%.1f",			QX_FLAG_SKIP,							NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_skip_me },
	{ "output.load.voltage.fault",			0,				NULL,					"QPIFS\r",			"",	91,	'(',	"",	71,	73,	"%.1f",			QX_FLAG_SKIP,							NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_skip_me },
	{ "output.load.frequency.fault",		0,				NULL,					"QPIFS\r",			"",	91,	'(',	"",	75,	77,	"%.1f",			QX_FLAG_SKIP,							NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_skip_me },
	{ "battery.voltage.fault",			0,				NULL,					"QPIFS\r",			"",	91,	'(',	"",	79,	81,	"%.1f",			QX_FLAG_SKIP,							NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_skip_me },
	{ "device.temperature.fault",			0,				NULL,					"QPIFS\r",			"",	91,	'(',	"",	83,	85,	"%.1f",			QX_FLAG_SKIP,							NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_skip_me },
	{ "run.status.fault",				0,				NULL,					"QPIFS\r",			"",	91,	'(',	"",	87,	89,	"%s",			QX_FLAG_SKIP,							NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_skip_me },

	/* Query device for faults and their type.
	 * IFF FW version (QSVFW2) >= (in P15: 0.9; in P16: 0.3)
	 * > [QPICF\r]
	 * < [(00 01\r]	#1 = fault status: if != "00" new fault; if == "00" old fault/no fault
	 *    0123456
	 *    0
	 */

	{ "ups.alarm",					0,				NULL,					"QPICF\r",			"",	7,	'(',	"",	1,	2,	"%s",			0,								NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_fault_status },	/* FIXME: should be "device.alarm" */
	{ "fault_id",					0,				NULL,					"QPICF\r",			"",	7,	'(',	"",	4,	5,	"%02d",			QX_FLAG_NONUT | QX_FLAG_SKIP,					NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_fault_id },

	/* Query device for fault <fault_id> and its type.	TODO: update with real device data
	 * IFF FW version (QSVFW2) >= (in P15: 0.9; in P16: 0.3)
	 * > [QPIHF<fault_id>\r]	<fault_id>: QPICF #2 [00..08]
	 * < [(00\r] <- No fault
	 *    0123
	 *    0
	 * < [(14 YYYYMMDDhhmmss 000 001 002 003 004 005 006 007 008 009 010 011 012 013 014 015 016 017\r] <- Fault type + Short status
	 *    0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890
	 *    0         1         2         3         4         5         6         7         8         9
	 */

	{ "ups.alarm",					0,				NULL,					"QPIHF%02d\r",			"",	91,	'(',	"",	1,	2,	"%s",			QX_FLAG_SKIP,							voltronic_sunny_fault_query,	voltronic_axpert_checkcrc,		voltronic_sunny_fault },	/* FIXME: should be "device.alarm" */
	{ "device.date.fault",				0,				NULL,					"QPIHF%02d\r",			"",	91,	'(',	"",	4,	11,	"%04d/%02d/%02d",	QX_FLAG_SKIP,							voltronic_sunny_fault_query,	voltronic_axpert_checkcrc,		voltronic_sunny_date_skip_me },
	{ "device.time.fault",				0,				NULL,					"QPIHF%02d\r",			"",	91,	'(',	"",	12,	17,	"%02d:%02d:%02d",	QX_FLAG_SKIP,							voltronic_sunny_fault_query,	voltronic_axpert_checkcrc,		voltronic_sunny_time_skip_me },
	{ "pv1.input.voltage.fault",			0,				NULL,					"QPIHF%02d\r",			"",	91,	'(',	"",	19,	21,	"%.1f",			QX_FLAG_SKIP,							voltronic_sunny_fault_query,	voltronic_axpert_checkcrc,		voltronic_sunny_skip_me },
	{ "pv1.input.current.fault",			0,				NULL,					"QPIHF%02d\r",			"",	91,	'(',	"",	23,	25,	"%.1f",			QX_FLAG_SKIP,							voltronic_sunny_fault_query,	voltronic_axpert_checkcrc,		voltronic_sunny_skip_me },
	{ "pv2.input.voltage.fault",			0,				NULL,					"QPIHF%02d\r",			"",	91,	'(',	"",	27,	29,	"%.1f",			QX_FLAG_SKIP,							voltronic_sunny_fault_query,	voltronic_axpert_checkcrc,		voltronic_sunny_skip_me },
	{ "pv2.input.current.fault",			0,				NULL,					"QPIHF%02d\r",			"",	91,	'(',	"",	31,	33,	"%.1f",			QX_FLAG_SKIP,							voltronic_sunny_fault_query,	voltronic_axpert_checkcrc,		voltronic_sunny_skip_me },
	{ "pv3.input.voltage.fault",			0,				NULL,					"QPIHF%02d\r",			"",	91,	'(',	"",	35,	37,	"%.1f",			QX_FLAG_SKIP,							voltronic_sunny_fault_query,	voltronic_axpert_checkcrc,		voltronic_sunny_skip_me },
	{ "pv3.input.current.fault",			0,				NULL,					"QPIHF%02d\r",			"",	91,	'(',	"",	39,	41,	"%.1f",			QX_FLAG_SKIP,							voltronic_sunny_fault_query,	voltronic_axpert_checkcrc,		voltronic_sunny_skip_me },
	{ "inverter.voltage.fault",			0,				NULL,					"QPIHF%02d\r",			"",	91,	'(',	"",	43,	45,	"%.1f",			QX_FLAG_SKIP,							voltronic_sunny_fault_query,	voltronic_axpert_checkcrc,		voltronic_sunny_skip_me },
	{ "inverter.current.fault",			0,				NULL,					"QPIHF%02d\r",			"",	91,	'(',	"",	47,	49,	"%.1f",			QX_FLAG_SKIP,							voltronic_sunny_fault_query,	voltronic_axpert_checkcrc,		voltronic_sunny_skip_me },
	{ "grid.voltage.fault",				0,				NULL,					"QPIHF%02d\r",			"",	91,	'(',	"",	51,	53,	"%.1f",			QX_FLAG_SKIP,							voltronic_sunny_fault_query,	voltronic_axpert_checkcrc,		voltronic_sunny_skip_me },
	{ "grid.frequency.fault",			0,				NULL,					"QPIHF%02d\r",			"",	91,	'(',	"",	55,	57,	"%.1f",			QX_FLAG_SKIP,							voltronic_sunny_fault_query,	voltronic_axpert_checkcrc,		voltronic_sunny_skip_me },
	{ "grid.current.fault",				0,				NULL,					"QPIHF%02d\r",			"",	91,	'(',	"",	59,	61,	"%.1f",			QX_FLAG_SKIP,							voltronic_sunny_fault_query,	voltronic_axpert_checkcrc,		voltronic_sunny_skip_me },
	{ "output.load.percent.fault",			0,				NULL,					"QPIHF%02d\r",			"",	91,	'(',	"",	63,	65,	"%.1f",			QX_FLAG_SKIP,							voltronic_sunny_fault_query,	voltronic_axpert_checkcrc,		voltronic_sunny_skip_me },
	{ "output.load.current.fault",			0,				NULL,					"QPIHF%02d\r",			"",	91,	'(',	"",	67,	69,	"%.1f",			QX_FLAG_SKIP,							voltronic_sunny_fault_query,	voltronic_axpert_checkcrc,		voltronic_sunny_skip_me },
	{ "output.load.voltage.fault",			0,				NULL,					"QPIHF%02d\r",			"",	91,	'(',	"",	71,	73,	"%.1f",			QX_FLAG_SKIP,							voltronic_sunny_fault_query,	voltronic_axpert_checkcrc,		voltronic_sunny_skip_me },
	{ "output.load.frequency.fault",		0,				NULL,					"QPIHF%02d\r",			"",	91,	'(',	"",	75,	77,	"%.1f",			QX_FLAG_SKIP,							voltronic_sunny_fault_query,	voltronic_axpert_checkcrc,		voltronic_sunny_skip_me },
	{ "battery.voltage.fault",			0,				NULL,					"QPIHF%02d\r",			"",	91,	'(',	"",	79,	81,	"%.1f",			QX_FLAG_SKIP,							voltronic_sunny_fault_query,	voltronic_axpert_checkcrc,		voltronic_sunny_skip_me },
	{ "device.temperature.fault",			0,				NULL,					"QPIHF%02d\r",			"",	91,	'(',	"",	83,	85,	"%.1f",			QX_FLAG_SKIP,							voltronic_sunny_fault_query,	voltronic_axpert_checkcrc,		voltronic_sunny_skip_me },
	{ "run.status.fault",				0,				NULL,					"QPIHF%02d\r",			"",	91,	'(',	"",	87,	89,	"%s",			QX_FLAG_SKIP,							voltronic_sunny_fault_query,	voltronic_axpert_checkcrc,		voltronic_sunny_skip_me },

#endif

	/*#######################################################################################################################################################################################################################################################################################################################################################################################################################################*/

#if 0
	/* Query device for self test results	TODO: update with real device data
	 * > [QSTS\r]
	 * < [(01 001 002 003 004 005 006 007 008\r]
	 *    012345678901234567890123456789012345
	 *    0         1         2         3
	 */

	{ "device.test.result",				0,				NULL,					"QSTS\r",			"",	36,	'(',	"",	1,	2,	"%s",			0,								NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_self_test_result },
	{ "voltage.high.test",				0,				NULL,					"QSTS\r",			"",	36,	'(',	"",	4,	6,	"%.1f",			QX_FLAG_SKIP,							NULL,				voltronic_axpert_checkcrc,		NULL },
	{ "voltage.low.test",				0,				NULL,					"QSTS\r",			"",	36,	'(',	"",	8,	10,	"%.1f",			QX_FLAG_SKIP,							NULL,				voltronic_axpert_checkcrc,		NULL },
	{ "frequency.high.test",			0,				NULL,					"QSTS\r",			"",	36,	'(',	"",	12,	14,	"%.1f",			QX_FLAG_SKIP,							NULL,				voltronic_axpert_checkcrc,		NULL },
	{ "frequency.low.test",				0,				NULL,					"QSTS\r",			"",	36,	'(',	"",	16,	18,	"%.1f",			QX_FLAG_SKIP,							NULL,				voltronic_axpert_checkcrc,		NULL },
	{ "voltage.high.triptime.test",			0,				NULL,					"QSTS\r",			"",	36,	'(',	"",	20,	22,	"%.1f",			QX_FLAG_SKIP,							NULL,				voltronic_axpert_checkcrc,		NULL },
	{ "voltage.low.triptime.test",			0,				NULL,					"QSTS\r",			"",	36,	'(',	"",	24,	26,	"%.1f",			QX_FLAG_SKIP,							NULL,				voltronic_axpert_checkcrc,		NULL },
	{ "frequency.high.triptime.test",		0,				NULL,					"QSTS\r",			"",	36,	'(',	"",	28,	30,	"%.1f",			QX_FLAG_SKIP,							NULL,				voltronic_axpert_checkcrc,		NULL },
	{ "frequency.low.triptime.test",		0,				NULL,					"QSTS\r",			"",	36,	'(',	"",	32,	34,	"%.1f",			QX_FLAG_SKIP,							NULL,				voltronic_axpert_checkcrc,		NULL },

#endif

	/*#######################################################################################################################################################################################################################################################################################################################################################################################################################################*/

#if 0
	/* Query device for energy start date
	 * > [QFET\r]
	 * < [(YYYYMMDDhh\r]
	 *    012345678901
	 *    0         1
	 */

	{ "energy.startdate",				0,				NULL,					"QFET\r",			"",	12,	'(',	"",	1,	8,	"%04d/%02d/%02d",	QX_FLAG_STATIC,							NULL,				voltronic_axpert_checkcrc,		voltronic_sunny_date },

	/* Just expose the following "energy per *"-items as "energy produced during current year/month/day/hour" */

	/* Query device for energy produced in a specific hour
	 * > [QEH<date><hour><checksum>\r]	<date>: YYYYMMDD, <hour>: hh, <checksum>: 3 digit checksum
	 * < [(00004\r]
	 *    0123456
	 *    0
	 */

	{ "energy.hour",				0,				NULL,					"QEH%04d%02d%02d%02d%s\r",	"",	7,	'(',	"",	1,	5,	"%.0f",			0,								voltronic_sunny_energy_hour,	voltronic_axpert_checkcrc,		NULL },

	/* Query device for energy produced in a specific day
	 * > [QED<date><checksum>\r]		<date>: YYYYMMDD, <checksum>: 3 digit checksum
	 * < [(019297\r]
	 *    01234567
	 *    0
	 */

	{ "energy.day",					0,				NULL,					"QED%04d%02d%02d%s\r",		"",	8,	'(',	"",	1,	6,	"%.0f",			0,								voltronic_sunny_energy_day,	voltronic_axpert_checkcrc,		NULL },

	/* Query device for energy produced in a specific month
	 * > [QEM<year><month><checksum>\r]	<year>: YYYY, <month>: MM, <checksum>: 3 digit checksum
	 * < [(0000000\r]
	 *    012345678
	 *    0
	 */

	{ "energy.month",				0,				NULL,					"QEM%04d%02d%s\r",		"",	9,	'(',	"",	1,	7,	"%.0f",			0,								voltronic_sunny_energy_month,	voltronic_axpert_checkcrc,		NULL },

	/* Query device for energy produced in a specific year
	 * > [QEY<year><checksum>\r]		<year>: YYYY, <checksum>: 3 digit checksum
	 * < [(00000000\r]
	 *    0123456789
	 *    0
	 */

	{ "energy.year",				0,				NULL,					"QEY%04d%s\r",			"",	10,	'(',	"",	1,	8,	"%.0f",			0,								voltronic_sunny_energy_year,	voltronic_axpert_checkcrc,		NULL },

#endif
	/*#######################################################################################################################################################################################################################################################################################################################################################################################################################################*/

#if 0
	/* Instant commands */
	{ "load.off",					0,				NULL,					"SOFF\r",			"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_CMD,							NULL,				voltronic_axpert_checkcrc,		NULL },
	{ "load.on",					0,				NULL,					"SON\r",			"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_CMD,							NULL,				voltronic_axpert_checkcrc,		NULL },

	{ "grid.disconnect",				0,				NULL,					"FGD\r",			"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_CMD,							NULL,				voltronic_axpert_checkcrc,		NULL },
	{ "grid.connect",				0,				NULL,					"FGE\r",			"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_CMD,							NULL,				voltronic_axpert_checkcrc,		NULL },

	{ "standby.on",					0,				NULL,					"GTS1\r",			"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_CMD,							NULL,				voltronic_axpert_checkcrc,		NULL },
	{ "standby.off",				0,				NULL,					"GTS0\r",			"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_CMD,							NULL,				voltronic_axpert_checkcrc,		NULL },

	{ "test.grid",					0,				NULL,					"ST\r",				"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_CMD,							NULL,				voltronic_axpert_checkcrc,		NULL },

	/* Enable/disable beeper: unskipped if the device can control alarm (capability) */
	{ "beeper.enable",				0,				NULL,					"PEA\r",			"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_CMD | QX_FLAG_SKIP,					NULL,				voltronic_axpert_checkcrc,		NULL },
	{ "beeper.disable",				0,				NULL,					"PDA\r",			"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_CMD | QX_FLAG_SKIP,					NULL,				voltronic_axpert_checkcrc,		NULL },

	/* For internal use only */
	{ "OEEPB",					0,				NULL,					"OEEPB\r",			"",	5,	'(',	"",	1,	3,	NULL,			QX_FLAG_CMD | QX_FLAG_SKIP,					NULL,				voltronic_axpert_checkcrc,		NULL },

#endif

	/*#######################################################################################################################################################################################################################################################################################################################################################################################################################################*/

	/* End of structure. */
	{ NULL,						0,				NULL,					NULL,				"",	0,	0,	"",	0,	0,	NULL,			0,								NULL,				NULL,					NULL }

	/*#######################################################################################################################################################################################################################################################################################################################################################################################################################################*
	 *# info_type					|info_flags			|info_rw				|command			|answer	|answer	|leading|value	|from	|to	|dfl			|qxflags							|preprocess_command		|preprocess_answer			|preprocess							#*
	 *#						|				|					|				|_len	|	|	|	|	|	|			|								|				|					|								#*
	 *#######################################################################################################################################################################################################################################################################################################################################################################################################################################*/

};


/* == Testing table == */	/* TODO: update all replies with real device data/combos that make sense */
#ifdef TESTING
static testing_t	voltronic_sunny_testing[] = {
	/*###############################################################################################################################################################################################*
	 *# cmd				|answer																		|answer_len	#*
	 *###############################################################################################################################################################################################*/
	{ "QPI\r",			"(PI30\r",																	-1 },
	{ "QVFW\r",			"(VERFW:00074.50\r",																-1 },
	{ "QVFW2\r",			"(VERFW2:00000.00\r",																-1 },
	{ "QDM\r",			"(058\r",																	-1 },
	{ "DMODEL151\r",		"(ACK\r",																	-1 },
	{ "QID\r",			"(12345679012345\r",																-1 },
	/*###############################################################################################################################################################################################*/
	{ "QFLAG\r",			"(EbgpDa\r",																	-1 },
	{ "PDG\r",			"(ACK\r",																	-1 },
	{ "PEG\r",			"(ACK\r",																	-1 },
	{ "PF\r",			"(ACK\r",																	-1 },
	{ "PDP\r",			"(ACK\r",																	-1 },
	{ "PEP\r",			"(ACK\r",																	-1 },
	{ "PDB\r",			"(ACK\r",																	-1 },
	{ "PEB\r",			"(ACK\r",																	-1 },
	{ "QENF\r",			"(A1B0C1D0E1F0G0H0I_J_\r",															-1 },
	{ "ENFA1\r",			"(ACK\r",																	-1 },
	{ "ENFA0\r",			"(ACK\r",																	-1 },
	{ "ENFB1\r",			"(ACK\r",																	-1 },
	{ "ENFB0\r",			"(ACK\r",																	-1 },
	{ "ENFC1\r",			"(ACK\r",																	-1 },
	{ "ENFC0\r",			"(ACK\r",																	-1 },
	{ "ENFD1\r",			"(ACK\r",																	-1 },
	{ "ENFD0\r",			"(ACK\r",																	-1 },
	{ "ENFE1\r",			"(ACK\r",																	-1 },
	{ "ENFE0\r",			"(ACK\r",																	-1 },
	{ "ENFF1\r",			"(ACK\r",																	-1 },
	{ "ENFF0\r",			"(ACK\r",																	-1 },
	{ "ENFG1\r",			"(ACK\r",																	-1 },
	{ "ENFG0\r",			"(ACK\r",																	-1 },
	{ "QPDG\r",			"(0\r",																		-1 },
	{ "PDG1\r",			"(ACK\r",																	-1 },
	{ "PDG0\r",			"(ACK\r",																	-1 },
	{ "GNTMQ\r",			"(00\r",																	-1 },
	{ "GNTM1\r",			"(ACK\r",																	-1 },
	{ "GNTM0\r",			"(ACK\r",																	-1 },
	{ "QPRIO\r",			"(02\r",																	-1 },
	{ "PRIO02\r",			"(ACK\r",																	-1 },
	/*###############################################################################################################################################################################################*/
	{ "QPIRI\r",			"(230.0 21.7 230.0 50.0 21.7 5000 5000 48.0 46.0 42.0 56.4 54.0 0 30 060 0 0 2 9 01 0 1 54.0 0 1 000\r",					-1 },
	{ "V230\r",			"(ACK\r",																	-1 },
	{ "F50\r",			"(ACK\r",																	-1 },
	{ "PVN02\r",			"(ACK\r",																	-1 },
	{ "QMD\r",			"(#####INVERTEX5K ###5000 99 1/1 230 230 04 12.0\r",												-1 },
	{ "I\r",			"(DSP:14-03-03,14:30 MCU:14-01-15,17:20\r",													-1 },
	/*###############################################################################################################################################################################################*/
	{ "QDI\r",			"((230.0 50.0 0030 42.0 54.0 56.4 46.0 60 0 0 2 0 0 0 0 0 1 1 0 0 1 0 54.0 0 1 000\r",								-1 },
	/*###############################################################################################################################################################################################*/
	{ "QVFTR\r",			"(276.0 235.0 225.0 180.0 55.0 50.1 49.9 45.0 070 005 58.0 48.0 25.0 00.5 500 450 200 090 450 400 200 110 03000 00000 58.0 50.0 --\r",		-1 },
	{ "QGOV\r",			"(264.5 184.0\r",																-1 },
	{ "GOHV050.1\r",		"(ACK\r",																	-1 },
	{ "GOLV040.1\r",		"(ACK\r",																	-1 },
	{ "QBYV\r",			"(100.0 050.0\r",																-1 },
	{ "PHV055.1\r",			"(ACK\r",																	-1 },
	{ "PLV045.1\r",			"(ACK\r",																	-1 },
	{ "QGOF\r",			"(51.5 47.5\r",																	-1 },
	{ "GOHF10.1\r",			"(ACK\r",																	-1 },
	{ "GOLF05.1\r",			"(ACK\r",																	-1 },
	{ "QBYF\r",			"(10.0 05.0\r",																	-1 },
	{ "PGF14.1\r",			"(ACK\r",																	-1 },
	{ "PSF04.1\r",			"(ACK\r",																	-1 },
	{ "QFT\r",			"(060\r",																	-1 },
	{ "FT090\r",			"(ACK\r",																	-1 },
	{ "QCHGS\r",			"(00.3 54.0 25.0 55.4\r",															-1 },
	{ "MCHGV12.1\r",		"(ACK\r",																	-1 },
	{ "MCHGC05.1\r",		"(ACK\r",																	-1 },
	{ "BCHGV15.1\r",		"(ACK\r",																	-1 },
	{ "QOFFC\r",			"(00.0 53.0 060\r",																-1 },
	{ "OFFC10.1 10.2 103\r",	"(ACK\r",																	-1 },
	{ "QACCHC\r",			"(??\r",																	-1 },	/* TODO: update with real device data */
	{ "QBSDV\r",			"(48.0 48.0 48.0 49.4\r",															-1 },
	{ "BSDV47.9 47.9\r",		"(ACK\r",																	-1 },
	{ "DSUBV10.3 10.4\r",		"(ACK\r",																	-1 },
	{ "QPVIPV\r",			"(500 090\r",																	-1 },
	{ "PVIPHV090\r",		"(ACK\r",																	-1 },
	{ "PVIPLV040\r",		"(ACK\r",																	-1 },
	{ "QMPPTV\r",			"(450 120\r",																	-1 },
	{ "MPPTHV102\r",		"(ACK\r",																	-1 },
	{ "MPPTLV052\r",		"(ACK\r",																	-1 },
	/*###############################################################################################################################################################################################*/
	{ "QOPMP\r",			"(03000\r",																	-1 },
	{ "OPMP05000\r",		"(ACK\r",																	-1 },
	{ "QGPMP\r",			"(03000\r",																	-1 },
	{ "GPMP04500\r",		"(ACK\r",																	-1 },
	{ "QLST\r",			"(10\r",																	-1 },
	{ "LST34\r",			"(ACK\r",																	-1 },
	{ "QCHT\r",			"(0215 1835\r",																	-1 },
	{ "CHTH0915\r",			"(ACK\r",																	-1 },
	{ "CHTL2130\r",			"(ACK\r",																	-1 },
	{ "QPKT\r",			"(0304 0304\r",																	-1 },
	{ "PKT0100 0400\r",		"(ACK\r",																	-1 },
	{ "QLDT\r",			"(0000 0000\r",																	-1 },
	{ "LDT0730 1945\r",		"(ACK\r",																	-1 },
	{ "QGLTV\r",			"(253 ---\r",																	-1 },
	{ "GLTHV044\r",			"(ACK\r",																	-1 },
	{ "QEBGP\r",			"(+000 00\r",																	-1 },
	{ "ABGP+010\r",			"(ACK\r",																	-1 },	/* TODO: update with real device data */
	{ "LBF1\r",			"(ACK\r",																	-1 },	/* TODO: update with real device data */
	/*###############################################################################################################################################################################################*/
	{ "QOPF\r",			"(100\r",																	-1 },
	{ "SOPF+090\r",			"(ACK\r",																	-1 },
	{ "QPPS\r",			"(025\r",																	-1 },
	{ "PPS090\r",			"(ACK\r",																	-1 },
	{ "QPPD\r",			"(060\r",																	-1 },
	{ "PPD070\r",			"(ACK\r",																	-1 },
	{ "QPFL\r",			"(190\r",																	-1 },
	{ "PFL099\r",			"(ACK\r",																	-1 },
	/*###############################################################################################################################################################################################*/
	{ "QT\r",			"(20150620200634\r",																-1 },
	{ "DAT20140124221315\r",	"(ACK\r",																	-1 },
	/*###############################################################################################################################################################################################*/
	{ "QPIGS\r",			"(233.5 50.1 233.5 50.1 0467 0396 009 440 54.00 000 100 0033 0000 000.0 00.00 00000 00010101 00 00 00000 110\r",				-1 },
	{ "QPIWS\r",			"(00000000000000000000000000000000\r",														-1 },
	{ "QMOD\r",			"(G\r",																		-1 },
	{ "QPIBI\r",			"(000 001 002 003 004\r",															-1 },	/* TODO: update with real device data */
	/*###############################################################################################################################################################################################*/
	{ "QPIFS\r",			"(14 20140120223015 000 001 002 003 004 005 006 007 008 009 010 011 012 013 014 015 016 017\r",							-1 },	/* TODO: update with real device data */
	{ "QPIFS\r",			"(OK\r",																	-1 },	/* TODO: update with real device data */
	{ "QPICF\r",			"(00 00\r",																	-1 },
	{ "QPIHF01\r",			"(14 20140120223015 000 001 002 003 004 005 006 007 008 009 010 011 012 013 014 015 016 017\r",							-1 },	/* TODO: update with real device data */
	{ "QPIHF02\r",			"(00\r",																	-1 },	/* TODO: update with real device data */
	/*###############################################################################################################################################################################################*/
	{ "QSTS\r",			"(01 001 002 003 004 005 006 007 008\r",													-1 },	/* TODO: update with real device data */
	/*###############################################################################################################################################################################################*/
	{ "QFET\r",			"(2015012117\r",																-1 },
	{ "QEH2015062020208\r",		"(00004\r",																	-1 },
	{ "QED20150620106\r",		"(019297\r",																	-1 },
	{ "QEM201506017\r",		"(0000000\r",																	-1 },
	{ "QEY2015183\r",		"(00000000\r",																	-1 },
	/*###############################################################################################################################################################################################*/
	{ "SOFF\r",			"(ACK\r",																	-1 },
	{ "SON\r",			"(ACK\r",																	-1 },
	{ "FGD\r",			"(ACK\r",																	-1 },
	{ "FGE\r",			"(ACK\r",																	-1 },
	{ "GTS1\r",			"(ACK\r",																	-1 },
	{ "GTS0\r",			"(NAK\r",																	-1 },
	{ "ST\r",			"(NAK\r",																	-1 },
	{ "PEA\r",			"(ACK\r",																	-1 },
	{ "PDA\r",			"(ACK\r",																	-1 },
	{ "OEEPB\r",			"(ACK\r",																	-1 },
	/*###############################################################################################################################################################################################*
	 *# cmd				|answer																		|answer_len	#*
	 *###############################################################################################################################################################################################*/
	{ NULL }
};
#endif	/* TESTING */


/* == Support functions == */

/* This function allows the subdriver to "claim" a device: return 1 if the device is supported by this subdriver, else 0. */
static int	voltronic_sunny_claim(void)
{
	/* QPI - Device protocol */
	item_t	*item = find_nut_info("device.firmware.aux", 0, 0);

	/* Don't know what happened */
	if (!item)
		return 0;

	/* No reply/Unable to get value */
	if (qx_process(item, NULL))
		return 0;

	/* Unable to process value/Protocol out of range */
	if (ups_infoval_set(item) != 1)
		return 0;

	return 1;
}

/* Subdriver-specific initinfo */
static void	voltronic_axpert_initinfo(void)
{
	/* Overwrite device.type (standard: "ups") and set it to "Solar Controller Device" */
	dstate_setinfo("device.type", "scd");
}

/* Subdriver-specific flags/vars */
static void	voltronic_sunny_makevartable(void)
{
	/* Capability vars */
	addvar(VAR_FLAG, "reset_to_default", "Reset capability options and their limits to safe default values");
	addvar(VAR_VALUE, "bypass_alarm", "Alarm (BEEP!) at Bypass Mode [enabled/disabled]");
	addvar(VAR_VALUE, "battery_alarm", "Alarm (BEEP!) at Battery Mode [enabled/disabled]");
}

/* Remove *clearflag* flags from the qxflags of an item_t with info_type *varname* and with:	FIXME: worth main driver?
 *  - *flag*: flags that have to be set in the item;
 *  - *noflag*: flags that have to be absent in the item.
 * Return:
 * - -1, on failure (i.e.: cannot find *varname* with flags *flag* and without flags *noflag* in qx2nut array);
 * - 0, on success (i.e.: *varname* found and *clearflag* flags cleared, if present). */
static int	voltronic_axpert_clear_flags(const char *varname, const unsigned long flag, const unsigned long noflag, const unsigned long clearflag)
{
	item_t	*item = find_nut_info(varname, flag, noflag);

	if (!item) {
		upsdebugx(2, "%s: cannot find %s item with required flags", __func__, varname);
		return -1;
	}

	item->qxflags &= ~clearflag;

	return 0;
}

/* Add *addflag* flags to the qxflags of an item_t with info_type *varname* and with:	FIXME: worth main driver?
 *  - *flag*: flags that have to be set in the item;
 *  - *noflag*: flags that have to be absent in the item.
 * Return:
 * - -1, on failure (i.e.: cannot find *varname* with flags *flag* and without flags *noflag* in qx2nut array);
 * - 0, on success (i.e.: *varname* found and *newflag* flags added). */
static int	voltronic_axpert_add_flags(const char *varname, const unsigned long flag, const unsigned long noflag, const unsigned long addflag)
{
	item_t	*item = find_nut_info(varname, flag, noflag);

	if (!item) {
		upsdebugx(2, "%s: cannot find %s item with required flags", __func__, varname);
		return -1;
	}

	item->qxflags |= addflag;

	return 0;
}

/* Calculate (and return) checksum (as used in the 'energy produced in *' queries) of all chars in *string* preceding a percent sign (%) */
static int	voltronic_sunny_checksum(const char *string)
{
	unsigned long	sum = 0;
	int		len = (int)strcspn(string, "%");

	while (len > 0) {
		len--;
		sum += string[len];
	}

	sum &= 0xff;

	return sum;
}

/* Update *item*-related vars limits in accordance with *value* */
static void	voltronic_sunny_update_related_vars_limits(item_t *item, const char *value)
{
	int	i;
	double	val;
	const struct {
		const char	*var;		/* Name of the currently processed var */
		const char	*related_var;	/* Name of the var related to the currently processed one */
		const int	index;		/* Index of the related_var->info_rw array to set:
						 * - 0: var sets the MIN settable value of related_var;
						 * - 1: var sets the MAX settable value of related_var. */
	} vars[] = {
		{ "battery.discharging.cutoffvoltage.gridoff",	"battery.discharging.restartvoltage.gridoff",	0 },	/*	QBSDV #1 sets the MIN settable value of QBSDV #3 */
		{ "battery.discharging.cutoffvoltage.gridon",	"battery.discharging.restartvoltage.gridon",	0 },	/*	QBSDV #2 sets the MIN settable value of QBSDV #4 */
		{ "battery.discharging.restartvoltage.gridoff",	"battery.discharging.cutoffvoltage.gridoff",	1 },	/*	QBSDV #3 sets the MAX settable value of QBSDV #1 */
		{ "battery.discharging.restartvoltage.gridoff",	"battery.charging.voltage.floating",		0 },	/* [1]	QBSDV #3 sets the MIN settable value of QCHGS #2 */
		{ "battery.discharging.restartvoltage.gridon",	"battery.discharging.cutoffvoltage.gridon",	1 },	/*	QBSDV #4 sets the MAX settable value of QBSDV #2 */
		{ "battery.discharging.restartvoltage.gridon",	"battery.charging.voltage.floating",		0 },	/* [1]	QBSDV #4 sets the MIN settable value of QCHGS #2 */
		{ "battery.charging.voltage.floating",		"battery.discharging.restartvoltage.gridoff",	1 },	/*	QCHGS #2 sets the MAX settable value of QBSDV #3 */
		{ "battery.charging.voltage.floating",		"battery.discharging.restartvoltage.gridon",	1 },	/*	QCHGS #2 sets the MAX settable value of QBSDV #4 */
		{ "battery.charging.voltage.floating",		"battery.charging.voltage.bulk",		0 },	/*	QCHGS #2 sets the MIN settable value of QCHGS #4 */
		{ "battery.charging.voltage.floating",		"battery.charging.restartvoltage",		1 },	/*	QCHGS #2 sets the MAX settable value of QOFFC #2 */
		{ "battery.charging.current.high",		"battery.charging.current.floating.low",	1 },	/*	QCHGS #3 sets the MAX settable value of QOFFC #1 */
		{ "battery.charging.voltage.bulk",		"battery.charging.voltage.floating",		1 },	/*	QCHGS #4 sets the MAX settable value of QCHGS #2 */
		{ "battery.charging.current.floating.low",	"battery.charging.current.high",		0 },	/*	QOFFC #1 sets the MIN settable value of QCHGS #3 */
		{ "battery.charging.restartvoltage",		"battery.charging.voltage.floating",		0 },	/* [1]	QOFFC #2 sets the MIN settable value of QCHGS #2 */
		{ "output.realpower.max",			"grid.realpower.max",				1 },	/*	QOPMP #1 sets the MAX settable value of QGPMP #1 */
		{ "grid.realpower.max",				"output.realpower.max",				0 },	/*	QGPMP #1 sets the MIN settable value of QOPMP #1 */
		{ "output.realpower.nominal",			"output.realpower.max",				1 },	/*	QMD   #2 sets the MAX settable value of QOPMP #1 */
		{ NULL, NULL, 0 }											/* [1]:	The min settable value of QCHGS #2 is the greatest one among these 3 values	*/
	};

	val = strtod(value, NULL);

	/* QMD #2 -> QOPMP #1: address protocol-specific issues */
	if (
		!strcasecmp(item->info_type, "output.realpower.nominal") &&
		protocol == 15 &&
		val == 4600
	)
		val = 5000;

	/* Update related vars limits */
	for (i = 0; vars[i].var; i++) {

		item_t	*related_var;
		char	old_min[SMALLBUF], old_max[SMALLBUF];

		/* This one is not related to the currently processed var */
		if (strcasecmp(item->info_type, vars[i].var))
			continue;

		related_var = find_nut_info(vars[i].related_var, 0, 0);

		/* Don't know what happened */
		if (!related_var) {
			upsdebugx(2, "%s: cannot find %s var (related to %s)", __func__, vars[i].related_var, item->info_type);
			continue;
		}

		/* This should not happen, too */
		if (!related_var->info_rw) {
			upsdebugx(2, "%s: %s (related to %s) doesn't have a info_rw", __func__, related_var->info_type, item->info_type);
			continue;
		}

		/* Setting QCHGS #2 MIN settable value (see [1] above) */
		if (
			!strcasecmp(related_var->info_type, "battery.charging.voltage.floating") &&
			vars[i].index == 0
		) {
			int	j;

			for (j = 0; vars[j].var; j++) {

				const char	*complement_s;
				int		complement;

				if (
					!strcasecmp(item->info_type, vars[j].var) ||				/* Currently processed var */
					strcasecmp(vars[j].related_var, "battery.charging.voltage.floating") ||	/* Not related to QCHGS #2 */
					vars[j].index != 0							/* Not MIN */
				)
					continue;

				complement_s = dstate_getinfo(vars[j].var);

				/* Not yet processed */
				if (!complement_s)
					continue;

				complement = strtod(complement_s, NULL);

				/* Set MIN as the greatest value among the complementary ones */
				val = val >= complement ? val : complement;

			}
		}

		/* Store old values */
		snprintf(old_min, sizeof(old_min), "%s", related_var->info_rw[0].value);
		snprintf(old_max, sizeof(old_max), "%s", related_var->info_rw[1].value);

		/* Set new value */
		snprintf(related_var->info_rw[vars[i].index].value, sizeof(related_var->info_rw[vars[i].index].value), related_var->dfl, val);

		/* Related var not yet processed -> let main driver add the range */
		if (!dstate_getinfo(related_var->info_type))
			continue;

		/* Update NUT range of related var */

		/* Still don't have both limits */
		if (
			!strlen(related_var->info_rw[0].value) ||
			!strlen(related_var->info_rw[1].value)
		)
			continue;

		/* Remove old range, if appropriate	FIXME: ranges only support ints */
		if (
			strlen(old_min) &&
			strlen(old_max)
		)
			dstate_delrange(related_var->info_type, strtol(old_min, NULL, 10), strtol(old_max, NULL, 10));

		/* Add new range	FIXME: ranges only support ints */
		dstate_addrange(related_var->info_type, strtol(related_var->info_rw[0].value, NULL, 10), strtol(related_var->info_rw[1].value, NULL, 10));

	}
}

/* Unskip, run and re-skip 'OEEPB' command.
 * Return:
 * - -1, on failure;
 * - 0, on success. */
static int	voltronic_sunny_OEEPB(void)
{
	int	ret;

	/* Unskip it */
	if (voltronic_axpert_clear_flags("OEEPB", QX_FLAG_CMD, 0, QX_FLAG_SKIP))
		return -1;
	/* Excecute it */
	ret = instcmd("OEEPB", NULL);
	/* Re-skip it */
	if (voltronic_axpert_add_flags("OEEPB", QX_FLAG_CMD, 0, QX_FLAG_SKIP))
		return -1;
	/* Command failed */
	if (ret != STAT_INSTCMD_HANDLED)
		return -1;

	return 0;
}


/* == Answer preprocess functions == */

/* If appropriate, check CRC in the answer we got back from the device */
static int	voltronic_axpert_checkcrc(item_t *item, const int len)
{
	int	ret;

	/* Device doesn't use CRC */
	if (!crc)
		return len;

	/* Error/empty answer */
	if (len <= 0)
		return len;

	ret = common_voltronic_crc_check_and_remove_m(item->answer, sizeof(item->answer));

	if (ret == -1)
		upsdebugx(2, "%s: failed to CRC-validate answer [%s]", __func__, item->info_type);

	return ret;
}


/* == Command preprocess functions == */

/* Preprocess CRC */
static int	voltronic_axpert_crc(item_t *item, char *command, const size_t commandlen)
{
	NUT_UNUSED_VARIABLE(item);
	return common_voltronic_crc_calc_and_add_m(command, commandlen);
}


/* Preprocess fault query */
static int	voltronic_sunny_fault_query(item_t *item, char *command, const size_t commandlen)
{
	snprintf(command, commandlen, item->command, fault_id);

	return common_voltronic_crc_calc_and_add_m(command, commandlen);
}

/* Energy produced during current hour */
static int	voltronic_sunny_energy_hour(item_t *item, char *command, const size_t commandlen)
{
	const char	*date = dstate_getinfo("device.date"),
			*time = dstate_getinfo("device.time");
	int		yyyy, mm, dd, hh, sum;
	char		buf[commandlen];

	if (
		!date ||
		!time ||
		sscanf(date, "%4d/%2d/%2d", &yyyy, &mm, &dd) != 3 ||
		sscanf(time, "%2d", &hh) != 1
	) {
		upsdebugx(2, "%s: cannot get current device date/time [%s]", __func__, item->info_type);
		return -1;
	}

	snprintf(buf, commandlen, item->command, yyyy, mm, dd, hh, "%03d");
	sum = voltronic_sunny_checksum(buf);
	snprintf(command, commandlen, buf, sum);

	return common_voltronic_crc_calc_and_add_m(command, commandlen);
}

/* Energy produced during current day */
static int	voltronic_sunny_energy_day(item_t *item, char *command, const size_t commandlen)
{
	const char	*date = dstate_getinfo("device.date");
	int		yyyy, mm, dd, sum;
	char		buf[commandlen];

	if (
		!date ||
		sscanf(date, "%4d/%2d/%2d", &yyyy, &mm, &dd) != 3
	) {
		upsdebugx(2, "%s: cannot get current device date [%s]", __func__, item->info_type);
		return -1;
	}

	snprintf(buf, commandlen, item->command, yyyy, mm, dd, "%03d");
	sum = voltronic_sunny_checksum(buf);
	snprintf(command, commandlen, buf, sum);

	return common_voltronic_crc_calc_and_add_m(command, commandlen);
}

/* Energy produced during current month */
static int	voltronic_sunny_energy_month(item_t *item, char *command, const size_t commandlen)
{
	const char	*date = dstate_getinfo("device.date");
	int		yyyy, mm, sum;
	char		buf[commandlen];

	if (
		!date ||
		sscanf(date, "%4d/%2d", &yyyy, &mm) != 2
	) {
		upsdebugx(2, "%s: cannot get current device date [%s]", __func__, item->info_type);
		return -1;
	}

	snprintf(buf, commandlen, item->command, yyyy, mm, "%03d");
	sum = voltronic_sunny_checksum(buf);
	snprintf(command, commandlen, buf, sum);

	return common_voltronic_crc_calc_and_add_m(command, commandlen);
}

/* Energy produced during current year */
static int	voltronic_sunny_energy_year(item_t *item, char *command, const size_t commandlen)
{
	const char	*date = dstate_getinfo("device.date");
	int		yyyy, sum;
	char		buf[commandlen];

	if (
		!date ||
		sscanf(date, "%4d", &yyyy) != 1
	) {
		upsdebugx(2, "%s: cannot get current device date [%s]", __func__, item->info_type);
		return -1;
	}

	snprintf(buf, commandlen, item->command, yyyy, "%03d");
	sum = voltronic_sunny_checksum(buf);
	snprintf(command, commandlen, buf, sum);

	return common_voltronic_crc_calc_and_add_m(command, commandlen);
}


/* == Preprocess functions == */

/* Do floating point hex preprocessing to value */
static int      voltronic_axpert_hex_preprocess(item_t *item, char *value, const size_t valuelen)
{
        snprintf(value, valuelen, "0X%s", item->value);

        if (strcasecmp(item->dfl, "0X%s")) {

                if (strspn(value, "0123456789ABCDEF .-") != strlen(value)) {
                        upsdebugx(2, "%s: non hex value [%s: %s]", __func__, item->info_type, value);
                        return -1;
                }

                snprintf(value, valuelen, item->dfl, strtod(value, NULL));

        }

        return 0;
}

/* Do only basic preprocessing to value */
static int	voltronic_sunny_basic_preprocess(item_t *item, char *value, const size_t valuelen)
{
	snprintf(value, valuelen, "%s", item->value);

	if (item->qxflags & QX_FLAG_TRIM)
		str_rtrim_m(str_ltrim_m(value, "# "), "# ");

	if (strcasecmp(item->dfl, "%s")) {

		if (strspn(value, "0123456789 .-") != strlen(value)) {
			upsdebugx(2, "%s: non numerical value [%s: %s]", __func__, item->info_type, value);
			return -1;
		}

		snprintf(value, valuelen, item->dfl, strtod(value, NULL));

	}

	return 0;
}

/* Protocol used by the device */
static int	voltronic_axpert_protocol(item_t *item, char *value, const size_t valuelen)
{
	if (strncasecmp(item->value, "PI", 2)) {
		upsdebugx(2, "%s: invalid start characters [%.2s]", __func__, item->value);
		return -1;
	}

	/* Here we exclude non numerical value and other non accepted protocols (hence the restricted comparison target) */
	if (strspn(item->value + 2, "03") != strlen(item->value + 2)) {
		upslogx(LOG_ERR, "Protocol [%s] is not supported by this driver", item->value);
		return -1;
	}

	protocol = strtol(item->value + 2, NULL, 10);

	switch (protocol)
	{
	case 30:
		break;
	default:
		upslogx(LOG_ERR, "Protocol [PI%02d] is not supported by this driver", protocol);
		return -1;
	}

	snprintf(value, valuelen, "P%02d", protocol);

	return 0;
}

/* Firmware version */
static int	voltronic_axpert_fw(item_t *item, char *value, const size_t valuelen)
{
	/* Hex preprocess */
	if (voltronic_axpert_hex_preprocess(item, value, valuelen))
		return -1;

	/* Set global var */
	fw = strtod(value, NULL);

	return 0;
}

/* Device serial number */
static int	voltronic_axpert_serial_numb(item_t *item, char *value, const size_t valuelen)
{
	/* If the device reports a 00..0 serial we'll log it but we won't store it in device.serial */
	if (strspn(item->value, "0") == strlen(item->value)) {
		upslogx(LOG_INFO, "%s: device reported a non-significant serial [%s]", item->info_type, item->value);
		return -1;
	}

	snprintf(value, valuelen, item->dfl, item->value);
	return 0;
}

        /* A Enable/disable silence buzzer or open buzzer
         *          * B Enable/Disable overload bypass function
         *                   * J Enable/Disable power saving
         *                            * K Enable/Disable LCD display escape to default page after 1min timeout
         *                                     * U Enable/Disable overload restart
         *                                              * V Enable/Disable over temperature restart
         *                                                       * X Enable/Disable backlight on
         *                                                                * Y Enable/Disable alarm on when primary source interrupt
         *                                                                         * Z Enable/Disable fault code record
         *                                                                                  * L Enable/Disable data log pop-up
         *
         */

/* Device capabilities */
static int	voltronic_axpert_capability(item_t *item, char *value, const size_t valuelen)
{
	char		rawval[SMALLBUF], *enabled, *disabled, *val = NULL, *saveptr = NULL;

	snprintf(rawval, sizeof(rawval), "%s", item->value);

	enabled = strtok_r(rawval + 1, "D", &saveptr);
	disabled = strtok_r(NULL, "\0", &saveptr);

	if (!enabled && !disabled) {
		upsdebugx(2, "%s: parsing failed", __func__);
		return -1;
	}

	enabled = enabled ? enabled : "";
	disabled = disabled ? disabled : "";

#if 0
	/* NONUT items */
	if (!strcasecmp(item->info_type, "bypass_alarm")) {

		if (strchr(enabled, 'p'))
			val = bypass_alarm = "enabled";
		else if (strchr(disabled, 'p'))
			val = bypass_alarm = "disabled";

	} else if (!strcasecmp(item->info_type, "battery_alarm")) {

		if (strchr(enabled, 'b'))
			val = battery_alarm = "enabled";
		else if (strchr(disabled, 'b'))
			val = battery_alarm = "disabled";
	} else
#endif

	/* Items with a NUT variable */
	if (!strcasecmp(item->info_type, "ups.beeper.status")) {

		if (strchr(item->value, 'a')) {

			if (strchr(enabled, 'a'))
				val = "enabled";
			else if (strchr(disabled, 'a'))
				val = "disabled";

			/* Unskip beeper.{enable,disable} instcmds */
			if (voltronic_axpert_clear_flags("beeper.enable", QX_FLAG_CMD, 0, QX_FLAG_SKIP))
				return -1;
			if (voltronic_axpert_clear_flags("beeper.disable", QX_FLAG_CMD, 0, QX_FLAG_SKIP))
				return -1;

		}

	} else if (!strcasecmp(item->info_type, "battery.energysave")) {

		if (strchr(enabled, 'j'))
			val = "yes";
		else if (strchr(disabled, 'j'))
			val = "no";

	}

	/* Device doesn't have that capability */
	if (!val)
		return -1;

	snprintf(value, valuelen, item->dfl, val);

	/* This item doesn't have a NUT var and we were not asked by the user to change its value */
	if ((item->qxflags & QX_FLAG_NONUT) && !getval(item->info_type))
		return 0;

	/* This (not NONUT) item is not RW and hence it doesn't have a corresponding SETVAR */
	if (!(item->qxflags & QX_FLAG_NONUT) && !(item->info_flags & ST_FLAG_RW))
		return 0;

	/* Unskip setvar */
	if (voltronic_axpert_clear_flags(item->info_type, QX_FLAG_SETVAR, 0, QX_FLAG_SKIP))
		return -1;

	return 0;
}

/* *SETVAR* Set device capability options */
static int	voltronic_axpert_capability_set(item_t *item, char *value, const size_t valuelen)
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

/* *SETVAR/NONUT* Reset capability options and their limits to safe default values */
static int	voltronic_axpert_capability_reset(item_t *item, char *value, const size_t valuelen)
{
	/* Nothing to do */
	if (!testvar("reset_to_default"))
		return -1;

	/* Device capability options can be reset only when the device is in 'Standby Mode' (=OFF) (from QMOD) */
	if (!(qx_status() & STATUS(OFF))) {
		upslogx(LOG_ERR, "%s: device capability options can be reset only when the device is in Standby Mode (i.e. device.status = 'OFF').", item->info_type);
		return -1;
	}

	snprintf(value, valuelen, "%s", item->command);

	return 0;
}

/* *SETVAR/NONUT* Change device capability according to user configuration in ups.conf */
static int	voltronic_axpert_capability_set_nonut(item_t *item, char *value, const size_t valuelen)
{
	const char	*match = NULL;
	int		i;
	const struct {
		const char	*type;	/* Name of the option */
		const char	*match;	/* Value reported by the device */
	} capability[] = {
		{ "bypass_alarm",	bypass_alarm },
		{ "battery_alarm",	battery_alarm },
		{ NULL, NULL }
	};

	for (i = 0; capability[i].type; i++) {

		if (strcasecmp(item->info_type, capability[i].type))
			continue;

		match = capability[i].match;

		break;

	}

	/* Device doesn't have that capability */
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

/* 0 -> disabled; 1 -> enabled */
static int	voltronic_sunny_01(item_t *item, char *value, const size_t valuelen)
{
	int	val;

	if (item->value[0] != '0' && item->value[0] != '1') {
		upsdebugx(2, "%s: invalid value [%s: %s]", __func__, item->info_type, item->value);
		return -1;
	}

	val = strtol(item->value, NULL, 10);

	snprintf(value, valuelen, item->dfl, item->info_rw[val].value);

	/* Unskip setvar */
	if (voltronic_axpert_clear_flags(item->info_type, QX_FLAG_SETVAR, 0, QX_FLAG_SKIP))
		return -1;

	return 0;
}

/* *SETVAR* 0 -> disabled; 1 -> enabled */
static int	voltronic_sunny_01_set(item_t *item, char *value, const size_t valuelen)
{
	int	i;

	for (i = 0; strlen(item->info_rw[i].value) > 0; i++) {
		if (!strcasecmp(item->info_rw[i].value, value))
			break;
	}

	snprintf(value, valuelen, item->command, i);

	return 0;
}

/* PV energy supply priority */
static int	voltronic_sunny_pv_priority(item_t *item, char *value, const size_t valuelen)
{
	int		priority_id;
	const char	*priority = NULL;
	info_rw_t	*envalue;

	if (strspn(item->value, "0123456789") != strlen(item->value)) {
		upsdebugx(2, "%s: non numerical value [%s: %s]", __func__, item->info_type, item->value);
		return -1;
	}

	priority_id = strtol(item->value, NULL, 10);

	switch (model_type)
	{
	case 1:		/* Type: Off-grid */
		if (priority_id == 1)
			priority = "Battery-Load";
		else if (priority_id == 2 && model_id == 150)
			priority = "Load-Battery";
		else if (priority_id == 2 && model_id == 151)
			priority = "Load-Battery (grid relay disconnected)";
		break;
	case 10:	/* Type: Grid-tie with backup */
		if (priority_id == 1)
			priority = "Battery-Load-Grid";
		else if (priority_id == 2)
			priority = "Load-Battery-Grid";
		else if (priority_id == 3)
			priority = "Load-Grid-Battery";
		break;
	case 0:		/* Type: Grid-tie */
	case 11:	/* Type: Self-use */
		upsdebugx(2, "%s: priority not supported when actual model type [%d] is chosen", __func__, model_type);
		return -1;
	default:
		upsdebugx(2, "%s: unknown model type [%d]", __func__, model_type);
		return -1;
	}

	if (!priority) {
		upsdebugx(2, "%s: unexpected value [%s: %s]", __func__, item->info_type, item->value);
		return -1;
	}

	snprintf(value, valuelen, item->dfl, priority);

	/* Unskip setvar */
	if (voltronic_axpert_clear_flags(item->info_type, QX_FLAG_SETVAR, 0, QX_FLAG_SKIP))
		return -1;

	/* Update enums, if appropriate */

	/* Not yet processed -> let main driver add enums */
	if (!dstate_getinfo(item->info_type))
		return 0;

	/* Loop on all existing values */
	for (envalue = item->info_rw; envalue != NULL && strlen(envalue->value) > 0; envalue++) {
		/* Remove all enums */
		dstate_delenum(item->info_type, envalue->value);
		/* Skip this one */
		if (envalue->preprocess && envalue->preprocess(envalue->value, sizeof(envalue->value)))
			continue;
		/* Add new enum */
		dstate_addenum(item->info_type, "%s", envalue->value);
	}

	return 0;
}

/* *SETVAR* PV energy supply priority */
static int	voltronic_sunny_pv_priority_set(item_t *item, char *value, const size_t valuelen)
{
	int		priority = -1;
	const char	*model = NULL;

	switch (model_type)
	{
	case 1:		/* Type: Off-grid */
		if (!strcasecmp(value, "Battery-Load")) {
			priority = 1;
		} else if (!strcasecmp(value, "Load-Battery")) {				/* Model: 150 */
			priority = 2;
			if (model_id != 150)
				model = "Off-grid";
		} else if (!strcasecmp(value, "Load-Battery (grid relay disconnected)")) {	/* Model: 151 */
			priority = 2;
			if (model_id != 151)
				model = "Off-grid (Vextex)";
		}
		break;
	case 10:	/* Type: Grid-tie with backup */
		if (!strcasecmp(value, "Battery-Load-Grid"))
			priority = 1;
		else if (!strcasecmp(value, "Load-Battery-Grid"))
			priority = 2;
		else if (!strcasecmp(value, "Load-Grid-Battery"))
			priority = 3;
		break;
	case 0:		/* Type: Grid-tie */
	case 11:	/* Type: Self-use */
		upslogx(LOG_ERR, "%s: priority not supported when actual model type [%d] is chosen", item->info_type, model_type);
		return -1;
	default:
		upslogx(LOG_ERR, "%s: unknown model type [%d]", item->info_type, model_type);
		return -1;
	}

	if (priority == -1) {
		upslogx(LOG_ERR, "%s: unexpected value [%s]", item->info_type, value);
		return -1;
	}

	/* Set model type, if appropriate */
	if (model) {
		if (
			!find_nut_info("device.model.type", QX_FLAG_SETVAR, QX_FLAG_SKIP) ||
			(!strcasecmp(model, "Off-grid (Vextex)") && !dstate_addenum("device.model.type", "%s", "Off-grid (Vextex)")) ||
			setvar("device.model.type", model) != STAT_SET_HANDLED
		) {
			if (!strcasecmp(model, "Off-grid (Vextex)"))
				dstate_delenum("device.model.type", "Off-grid (Vextex)");
			upslogx(LOG_ERR, "%s: failed to set model type to '%s'", item->info_type, model);
			return -1;
		}
		if (!strcasecmp(model, "Off-grid (Vextex)"))
			dstate_delenum("device.model.type", "Off-grid (Vextex)");
	}

	snprintf(value, valuelen, item->command, priority);

	return 0;
}

/* Do only basic preprocessing to value and unskip corresponding SETVAR item */
static int	voltronic_sunny_unskip_setvar(item_t *item, char *value, const size_t valuelen)
{
	/* Basic preprocess */
	if (voltronic_sunny_basic_preprocess(item, value, valuelen))
		return -1;

	/* Unskip setvar */
	if (voltronic_axpert_clear_flags(item->info_type, QX_FLAG_SETVAR, 0, QX_FLAG_SKIP))
		return -1;

	return 0;
}

/* Battery type (from QPIRI) */
static int	voltronic_axpert_qpiri_battery_type(item_t *item, char *value, const size_t valuelen)
{
	const char	*val;
	int		type;

	if (strspn(item->value, "01") != strlen(item->value)) {
		upsdebugx(2, "%s: invalid value [%s: %s]", __func__, item->info_type, item->value);
		return -1;
	}

	type = strtol(item->value, NULL, 10);

	switch (type)
	{
	case 0:
		val = voltronic_axpert_e_battery_type[0].value; /*"AGM"*/
		break;
	case 1:
		val = voltronic_axpert_e_battery_type[1].value; /*"Flooded"*/
		break;
	case 2:
		val = voltronic_axpert_e_battery_type[2].value; /*"User"*/
		break;
	default:
		upsdebugx(2, "%s: invalid value [%s: %s]", __func__, item->info_type, item->value);
		return -1;
	}

	snprintf(value, valuelen, item->dfl, val);

	return 0;
}

/* Model type (from QPIRI) */
static int	voltronic_axpert_qpiri_model_type(item_t *item, char *value, const size_t valuelen)
{
	const char	*val;
	int		model;

	if (strspn(item->value, "01") != strlen(item->value)) {
		upsdebugx(2, "%s: invalid value [%s: %s]", __func__, item->info_type, item->value);
		return -1;
	}

	model = strtol(item->value, NULL, 10);

	switch (model)
	{
	case 0:
		val = voltronic_axpert_e_model_type[0].value; /*"Grid-tie"*/
		break;
	case 1:
		val = voltronic_axpert_e_model_type[1].value; /*"Off-grid"*/
		break;
	case 10:
		val = voltronic_axpert_e_model_type[2].value; /*"Hybrid"*/
		break;
	case 11:
		val = voltronic_axpert_e_model_type[3].value; /*"Off Grid with 2 Trackers";*/
		break;
	case 20:
		val = voltronic_axpert_e_model_type[4].value; /*"Off Grid with 3 Trackers";*/
		break;
	default:
		upsdebugx(2, "%s: invalid value [%s: %s]", __func__, item->info_type, item->value);
		return -1;
	}

	snprintf(value, valuelen, item->dfl, val);

	/* Set global var */
	model_type = model;

	return 0;
}

/* Device transformer */
static int	voltronic_axpert_transformer(item_t *item, char *value, const size_t valuelen)
{
	const char	*val;

	switch (item->value[0])
	{
	case '0':
		val = "Transformerless";
		break;
	case '1':
		val = "With transformer";
		break;
	default:
		upsdebugx(2, "%s: invalid value [%s: %s]", __func__, item->info_type, item->value);
		return -1;
	}

	snprintf(value, valuelen, item->dfl, val);

	return 0;
}

/* *SETVAR* Nominal voltage */
static int	voltronic_sunny_volt_nom_set(item_t *item, char *value, const size_t valuelen)
{
	const int	nomvolt = strtol(value, NULL, 10);

	/* Nominal voltage can be changed only when the device is in 'Standby Mode' ('S' from QMOD) */
	if (!(qx_status() & STATUS(OFF))) {
		upslogx(LOG_ERR, "%s: value can be changed only when the device is in Standby Mode.", item->info_type);
		return -1;
	}

	if (
		nomvolt < 180 &&
		find_nut_info("grid.standard", QX_FLAG_SETVAR, QX_FLAG_SKIP) &&
		setvar("grid.standard", "VDE4105") != STAT_SET_HANDLED
	) {
		upslogx(LOG_ERR, "%s: cannot set grid standard to 'VDE4105'", item->info_type);
		return -1;
	}

	snprintf(value, valuelen, item->command, nomvolt);

	/* OEEPB must be executed before any V<n> command */
	return voltronic_sunny_OEEPB();
}

/* *SETVAR(/NONUT)* Preprocess setvars */
static int	voltronic_sunny_process_setvar(item_t *item, char *value, const size_t valuelen)
{
	double	val;

	if (!strlen(value)) {
		upsdebugx(2, "%s: value not given for %s", __func__, item->info_type);
		return -1;
	}

	val = strtod(value, NULL);

	if (!strcasecmp(item->info_type, "lcd.sleeptime")) {
		/* Divide by 30 (seconds) and discard remainder */
		val = (int)val / 30;
	} else if (
		!strcasecmp(item->info_type, "output.realpower.max") ||
		!strcasecmp(item->info_type, "grid.realpower.max")
	) {
		/* Round to tens */
		val -= (int)val % 10;
	} else if (!strcasecmp(item->info_type, "grid.frequency.nominal")) {
		/* Nominal frequency can be changed only when the device is in 'Standby Mode' ('S' from QMOD) */
		if (!(qx_status() & STATUS(OFF))) {
			upslogx(LOG_ERR, "%s: value can be changed only when the device is in Standby Mode.", item->info_type);
			return -1;
		}
		/* OEEPB must be executed before any F<n> command */
		if (voltronic_sunny_OEEPB())
			return -1;
	}

	snprintf(value, valuelen, item->command, val);

	return 0;
}

/* Do only basic preprocessing and update related vars limits */
static int	voltronic_sunny_basic_preprocess_and_update_related_vars_limits(item_t *item, char *value, const size_t valuelen)
{
	/* Preprocess and unskip SETVAR */
	if (voltronic_sunny_basic_preprocess(item, value, valuelen))
		return -1;

	/* Update related vars limits */
	voltronic_sunny_update_related_vars_limits(item, value);

	return 0;
}

/* YY-MM-DD date */
static int	voltronic_sunny_yymmdd(item_t *item, char *value, const size_t valuelen)
{
	int	yy, mm, dd;

	/* Check format */
	if (
		sscanf(item->value, "%2d-%2d-%2d", &yy, &mm, &dd) != 3 ||
		snprintf(value, valuelen, "%02d-%02d-%02d", yy, mm, dd) != 8 ||
		strcasecmp(item->value, value)
	) {
		upsdebugx(2, "%s: invalid format [%s: %s]; expected 'YY-MM-DD'", __func__, item->info_type, item->value);
		return -1;
	}

	yy += 2000;

	snprintf(item->value, sizeof(item->value), "%04d%02d%02d", yy, mm, dd);

	return voltronic_sunny_date(item, value, valuelen);
}

/* hh:mm time */
static int	voltronic_sunny_hh_mm(item_t *item, char *value, const size_t valuelen)
{
	int	hh, mm;

	/* Check format, fill value */
	if (
		sscanf(item->value, "%2d:%2d", &hh, &mm) != 2 ||
		snprintf(value, valuelen, item->dfl, hh, mm) != 5 ||
		strcasecmp(item->value, value)
	) {
		upsdebugx(2, "%s: invalid format [%s: %s]; expected 'hh:mm'", __func__, item->info_type, item->value);
		return -1;
	}

	/* Check time */
	if (hh < 0 || hh > 23 || mm < 0 || mm > 59) {
		upsdebugx(2, "%s: invalid time [%s: %s]", __func__, item->info_type, item->value);
		return -1;
	}

	return 0;
}

/* LCD sleep time */
static int	voltronic_sunny_lst(item_t *item, char *value, const size_t valuelen)
{
	if (strspn(item->value, "0123456789") != strlen(item->value)) {
		upsdebugx(2, "%s: non numerical value [%s: %s]", __func__, item->info_type, item->value);
		return -1;
	}

	snprintf(value, valuelen, item->dfl, strtol(item->value, NULL, 10) * 30);

	/* .default item */
	if (strstr(item->info_type, "default"))
		return 0;

	/* Unskip setvar */
	if (voltronic_axpert_clear_flags(item->info_type, QX_FLAG_SETVAR, 0, QX_FLAG_SKIP))
		return -1;

	return 0;
}

/* Overwrite limits of items with same name of the currenly processed one (minus the max/min specifier) */
static int	voltronic_sunny_set_limits(item_t *item, char *value, const size_t valuelen)
{
	item_t		*bareitem;
	info_rw_t	*rwvalue;
	char		name[SMALLBUF];
	int		type, index;

	if (strspn(item->value, "0123456789.") != strlen(item->value)) {
		upsdebugx(2, "%s: non numerical value [%s: %s]", __func__, item->info_type, item->value);
		return -1;
	}

	snprintf(value, valuelen, item->dfl, strtod(item->value, NULL));

	/* .max/.min */
	index = (int)strlen(item->info_type) - 4;
	if (index <= 0)
		return -1;

	if (!strcasecmp(&(item->info_type[index]), ".max"))
		type = 1;
	else if (!strcasecmp(&(item->info_type[index]), ".min"))
		type = 0;
	else
		return -1;

	snprintf(name, sizeof(name), "%.*s", index, item->info_type);

	/* Get bare item */
	bareitem = find_nut_info(name, 0, 0);

	/* Don't know what happened */
	if (!bareitem)
		return -1;

	/* This should not happen */
	if (!bareitem->info_rw)
		return -1;

	/* Loop on all existing values */
	for (rwvalue = bareitem->info_rw, index = 0; rwvalue != NULL && strlen(rwvalue->value) > 0; rwvalue++, index++) {

		/* Clear preprocess function */
		if (rwvalue->preprocess)
			rwvalue->preprocess = 0;

		/* Overwrite data */
		if (index == type)
			snprintf(rwvalue->value, sizeof(rwvalue->value), "%s", value);

		if (index <= 1)
			continue;

		/* Clear data */
		memset(rwvalue->value, 0, sizeof(rwvalue->value));

	}

	return 0;
}

/* Do only basic preprocessing to value and unskip corresponding SETVAR item; plus update related vars limits */
static int	voltronic_sunny_unskip_setvar_and_update_related_vars_limits(item_t *item, char *value, const size_t valuelen)
{
	/* Preprocess and unskip SETVAR */
	if (voltronic_sunny_unskip_setvar(item, value, valuelen))
		return -1;

	/* Update related vars limits */
	voltronic_sunny_update_related_vars_limits(item, value);

	return 0;
}

/* *SETVAR* Battery charger limits */
static int	voltronic_sunny_charger_limits_set(item_t *item, char *value, const size_t valuelen)
{
	const char	*min_float_current_s = dstate_getinfo("battery.charging.current.floating.low"),
			*restart_voltage_s = dstate_getinfo("battery.charging.restartvoltage"),
			*time_threshold_s = dstate_getinfo("battery.charging.timethreshold");
	double		min_float_current,
			restart_voltage,
			time_threshold;

	if (
		!min_float_current_s ||
		!restart_voltage_s ||
		!time_threshold_s
	) {
		upslogx(LOG_ERR, "%s: cannot get complementary vars", item->info_type);
		return -1;
	}

	min_float_current = strtod(min_float_current_s, NULL);
	restart_voltage = strtod(restart_voltage_s, NULL);
	time_threshold = strtod(time_threshold_s, NULL);

	if (!strcasecmp(item->info_type, "battery.charging.current.floating.low")) {
		min_float_current = strtod(value, NULL);
	} else if (!strcasecmp(item->info_type, "battery.charging.restartvoltage")) {
		restart_voltage = strtod(value, NULL);
	} else if (!strcasecmp(item->info_type, "battery.charging.timethreshold")) {
		time_threshold = strtod(value, NULL);
	} else {
		upslogx(LOG_ERR, "%s: unexpected var", item->info_type);
		return -1;
	}

	snprintf(value, valuelen, item->command, min_float_current, restart_voltage, time_threshold);

	return 0;
}

/* *SETVAR* Battery discharging limits */
static int	voltronic_sunny_discharging_limits_set(item_t *item, char *value, const size_t valuelen)
{
	const char	*a_s, *b_s;
	double		a, b;

	if (strstr(item->info_type, "battery.discharging.cutoffvoltage.grido") == item->info_type) {
		a_s = dstate_getinfo("battery.discharging.cutoffvoltage.gridoff");
		b_s = dstate_getinfo("battery.discharging.cutoffvoltage.gridon");
	} else if (strstr(item->info_type, "battery.discharging.restartvoltage.grido") == item->info_type) {
		a_s = dstate_getinfo("battery.discharging.restartvoltage.gridoff");
		b_s = dstate_getinfo("battery.discharging.restartvoltage.gridon");
	} else {
		upslogx(LOG_ERR, "%s: unexpected var", item->info_type);
		return -1;
	}

	if (
		!a_s ||
		!b_s
	) {
		upslogx(LOG_ERR, "%s: cannot get complementary vars", item->info_type);
		return -1;
	}

	a = strtod(a_s, NULL);
	b = strtod(b_s, NULL);

	if (strstr(item->info_type, ".gridoff")) {
		a = strtod(value, NULL);
	} else if (strstr(item->info_type, ".gridon")) {
		b = strtod(value, NULL);
	} else {
		upslogx(LOG_ERR, "%s: unexpected var", item->info_type);
		return -1;
	}

	snprintf(value, valuelen, item->command, a, b);

	return 0;
}

/* hhmm to hh:mm format */
static int	voltronic_sunny_hhmm(item_t *item, char *value, const size_t valuelen)
{
	int	hh, mm;

	/* Check format */
	if (
		strspn(item->value, "0123456789") != strlen(item->value) ||
		strlen(item->value) != 4 ||
		sscanf(item->value, "%2d%2d", &hh, &mm) != 2
	) {
		upsdebugx(2, "%s: invalid format [%s: %s]; expected 'hhmm'", __func__, item->info_type, item->value);
		return -1;
	}

	/* Check time */
	if (hh < 0 || hh > 23 || mm < 0 || mm > 59) {
		upsdebugx(2, "%s: invalid time [%s: %s]", __func__, item->info_type, item->value);
		return -1;
	}

	/* Publish as "hh:mm" */
	snprintf(value, valuelen, item->dfl, hh, mm);

	/* Unskip setvar */
	if (voltronic_axpert_clear_flags(item->info_type, QX_FLAG_SETVAR, 0, QX_FLAG_SKIP))
		return -1;

	return 0;
}

/* *SETVAR* hh:mm to hhmm format */
static int	voltronic_sunny_hhmm_set(item_t *item, char *value, const size_t valuelen)
{
	int	hh, mm;
	char	buf[SMALLBUF];

	/* Check format */
	if (
		sscanf(value, "%2d:%2d", &hh, &mm) != 2 ||
		snprintf(buf, sizeof(buf), "%02d:%02d", hh, mm) != 5 ||
		strcasecmp(value, buf)
	) {
		upslogx(LOG_ERR, "%s: invalid format [%s]; expected 'hh:mm'", item->info_type, value);
		return -1;
	}

	/* Check time */
	if (hh < 0 || hh > 23 || mm < 0 || mm > 59) {
		upslogx(LOG_ERR, "%s: invalid time [%s]", item->info_type, value);
		return -1;
	}

	/* <hh><mm> */
	snprintf(value, valuelen, item->command, hh, mm);

	return 0;
}

/* *SETVAR* hh:mm to "hhmm hhmm" format */
static int	voltronic_sunny_hhmm_x2_set(item_t *item, char *value, const size_t valuelen)
{
	int		hh, mm, hh2, mm2, len;
	char		buf[SMALLBUF];
	const char	*type, *complement;

	/* Check format */
	if (
		sscanf(value, "%2d:%2d", &hh, &mm) != 2 ||
		snprintf(buf, sizeof(buf), "%02d:%02d", hh, mm) != 5 ||
		strcasecmp(value, buf)
	) {
		upslogx(LOG_ERR, "%s: invalid format [%s]; expected 'hh:mm'", item->info_type, value);
		return -1;
	}

	/* Check time */
	if (hh < 0 || hh > 23 || mm < 0 || mm > 59) {
		upslogx(LOG_ERR, "%s: invalid time [%s]", item->info_type, value);
		return -1;
	}

	/* Get complementary time var */
	type = strstr(item->info_type, ".start");
	if (type) {
		len = type - item->info_type;
		type = ".end";
	} else {
		type = strstr(item->info_type, ".end");
		if (!type) {
			upslogx(LOG_ERR, "%s: unexpected var; expected either '.start' or '.end' item", item->info_type);
			return -1;
		}
		len = type - item->info_type;
		type = ".start";
	}

	snprintf(buf, sizeof(buf), "%.*s%s", len, item->info_type, type);

	/* Get complementary time */
	complement = dstate_getinfo(buf);
	if (
		!complement ||
		sscanf(complement, "%2d:%2d", &hh2, &mm2) != 2
	) {
		upslogx(LOG_ERR, "%s: cannot get complementary time [%s]", item->info_type, buf);
		return -1;
	}

	/* Complementary time is 'start time' -> <hh2><mm2> <hh><mm> */
	if (!strcasecmp(type, ".start"))
		snprintf(value, valuelen, item->command, hh2, mm2, hh, mm);
	/* Complementary time is 'end time' -> <hh><mm> <hh2><mm2> */
	else
		snprintf(value, valuelen, item->command, hh, mm, hh2, mm2);

	return 0;
}

/* Power factor */
static int	voltronic_sunny_pf(item_t *item, char *value, const size_t valuelen)
{
	int	pf;

	if (strspn(item->value, "0123456789") != strlen(item->value)) {
		upsdebugx(2, "%s: non numerical value [%s: %s]", __func__, item->info_type, item->value);
		return -1;
	}

	pf = strtol(item->value, NULL, 10);

	if (pf > 100)
		pf = 100 - pf;

	snprintf(value, valuelen, item->dfl, pf);

	/* Unskip setvar */
	if (voltronic_axpert_clear_flags(item->info_type, QX_FLAG_SETVAR, 0, QX_FLAG_SKIP))
		return -1;

	return 0;
}

/* *SETVAR* Power factor curve */
static int	voltronic_sunny_pfc_set(item_t *item, char *value, const size_t valuelen)
{
	int	pfc = strtol(value, NULL, 10);

	if (pfc < 0)
		pfc *= -1;

	snprintf(value, valuelen, item->command, pfc);

	return 0;
}

/* YYYYMMDD date */
static int	voltronic_sunny_date(item_t *item, char *value, const size_t valuelen)
{
	int		yyyy, mm, dd;
	char		buf[SMALLBUF];
	struct tm	date;

	/* Check format */
	if (
		strspn(item->value, "0123456789") != strlen(item->value) ||
		strlen(item->value) != 8 ||
		sscanf(item->value, "%4d%2d%2d", &yyyy, &mm, &dd) != 3
	) {
		upsdebugx(2, "%s: invalid format [%s: %s]; expected 'YYYYMMDD'", __func__, item->info_type, item->value);
		return -1;
	}

	/* Check date */
	date.tm_year = yyyy - 1900;
	date.tm_mon = mm - 1;
	date.tm_mday = dd;
	date.tm_hour = 1;
	date.tm_min = 0;
	date.tm_sec = 0;
	if (
		mktime(&date) == -1 ||
		strftime(buf, sizeof(buf), "%Y%m%d", &date) != 8 ||
		strcasecmp(item->value, buf)
	) {
		upsdebugx(2, "%s: invalid date [%s: %s]", __func__, item->info_type, item->value);
		return -1;
	}

	/* Publish as "YYYY/MM/DD" */
	snprintf(value, valuelen, item->dfl, yyyy, mm, dd);

	if (!(item->info_flags & ST_FLAG_RW))
		return 0;

	/* Unskip setvar */
	if (voltronic_axpert_clear_flags(item->info_type, QX_FLAG_SETVAR, 0, QX_FLAG_SKIP))
		return -1;

	return 0;
}

/* *SETVAR* Device date */
static int	voltronic_sunny_date_set(item_t *item, char *value, const size_t valuelen)
{
	int		mc, yy, mm, dd;
	char		buf[SMALLBUF];
	item_t		*time;
	struct tm	date;

	/* Check format */
	if (
		sscanf(value, "%2d%2d/%2d/%2d", &mc, &yy, &mm, &dd) != 4 ||
		snprintf(buf, sizeof(buf), "%02d%02d/%02d/%02d", mc, yy, mm, dd) != 10 ||
		strcasecmp(value, buf)
	) {
		upslogx(LOG_ERR, "%s: invalid format [%s]; expected 'YYYY/MM/DD'", item->info_type, value);
		return -1;
	}

	/* Check date */
	date.tm_year = mc * 100 + yy - 1900;
	date.tm_mon = mm - 1;
	date.tm_mday = dd;
	date.tm_hour = 1;
	date.tm_min = 0;
	date.tm_sec = 0;
	if (
		mktime(&date) == -1 ||
		strftime(buf, sizeof(buf), "%Y/%m/%d", &date) != 10 ||
		strcasecmp(value, buf)
	) {
		upslogx(LOG_ERR, "%s: invalid date [%s]", item->info_type, value);
		return -1;
	}

	/* Get current device time */
	time = find_nut_info("device.time", 0, QX_FLAG_SETVAR);
	if (
		!time ||
		qx_process(time, NULL) ||
		ups_infoval_set(time) == -1
	) {
		upslogx(LOG_ERR, "%s: cannot get current device time", item->info_type);
		return -1;
	}

	/* DAT<date>, <date>: YYMMDDhhmmss */
	snprintf(value, valuelen, item->command, yy, mm, dd, time->value);

	return 0;
}

/* hhmmss time */
static int	voltronic_sunny_time(item_t *item, char *value, const size_t valuelen)
{
	int	hh, mm, ss;

	/* Check format */
	if (
		strspn(item->value, "0123456789") != strlen(item->value) ||
		strlen(item->value) != 6 ||
		sscanf(item->value, "%2d%2d%2d", &hh, &mm, &ss) != 3
	) {
		upsdebugx(2, "%s: invalid format [%s: %s]; expected 'hhmmss'", __func__, item->info_type, item->value);
		return -1;
	}

	/* Check time */
	if (hh < 0 || hh > 23 || mm < 0 || mm > 59 || ss < 0 || ss > 59) {
		upsdebugx(2, "%s: invalid time [%s: %s]", __func__, item->info_type, item->value);
		return -1;
	}

	/* Publish as "hh:mm:ss" */
	snprintf(value, valuelen, item->dfl, hh, mm, ss);

	if (!(item->info_flags & ST_FLAG_RW))
		return 0;

	/* Unskip setvar */
	if (voltronic_axpert_clear_flags(item->info_type, QX_FLAG_SETVAR, 0, QX_FLAG_SKIP))
		return -1;

	return 0;
}

/* *SETVAR* Device time */
static int	voltronic_sunny_time_set(item_t *item, char *value, const size_t valuelen)
{
	int	hh, mm, ss;
	char	buf[SMALLBUF];
	item_t	*date;

	/* Check format */
	if (
		sscanf(value, "%2d:%2d:%2d", &hh, &mm, &ss) != 3 ||
		snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hh, mm, ss) != 8 ||
		strcasecmp(value, buf)
	) {
		upslogx(LOG_ERR, "%s: invalid format [%s]; expected 'hh:mm:ss'", item->info_type, value);
		return -1;
	}

	/* Check time */
	if (hh < 0 || hh > 23 || mm < 0 || mm > 59 || ss < 0 || ss > 59) {
		upslogx(LOG_ERR, "%s: invalid time [%s]", item->info_type, value);
		return -1;
	}

	/* Get current device date */
	date = find_nut_info("device.date", 0, QX_FLAG_SETVAR);
	if (
		!date ||
		qx_process(date, NULL) ||
		ups_infoval_set(date) == -1
	) {
		upslogx(LOG_ERR, "%s: cannot get current device date", item->info_type);
		return -1;
	}

	/* DAT<date>, <date>: YYMMDDhhmmss */
	snprintf(value, valuelen, item->command, date->value + 2, hh, mm, ss);

	return 0;
}

/* Process sign: if char @ 0 == 0 -> positive; otherwise -> negative */
static int	voltronic_sunny_process_sign(item_t *item, char *value, const size_t valuelen)
{
	if (item->value[0] != '0')
		item->value[0] = '-';

	return voltronic_sunny_basic_preprocess(item, value, valuelen);
}

/* Process status bits */
static int	voltronic_axpert_status(item_t *item, char *value, const size_t valuelen)
{
	char	*val = "";

	/* Option not supported -> don't let QX_FLAG_QUICK_POLL items (which only have to do with status and therefore already handle an empty 'value') fail */
	if (
		strchr(item->value, '-') &&
		item->qxflags & QX_FLAG_QUICK_POLL
	) {
		upsdebugx(3, "%s: option not supported %s@%d->%s", __func__, item->info_type, item->from, item->value);
		return 0;
	}

	if (strspn(item->value, "01") != strlen(item->value)) {
		upsdebugx(3, "%s: unexpected value %s@%d->%s", __func__, item->info_type, item->from, item->value);
		return -1;
	}

	switch (item->from)
	{
	case 24:	/* Charging status( SCC2 charging on/off) */
		break;
	case 25:	/* Charging status( SCC3 charging on/off) */
		break;
	case 83:	/* add SBU priority version, 1: yes, 0: no */
		break;
	case 84:	/* configuration status: 1: Change 0: unchanged */
		break;
	case 85:	/* SCC firmware version 1: Updated 0: unchanged */
		break;
	case 86:	/* Load status */
		if (item->value[0] == '1')	/* 1 -> Device has load */
			val = "!OFF";
		else				/* 0 -> Device doesn't have load */
			val = "OFF";
		break;
	case 87:	/* battery voltage to steady while charging */
		break;
	case 88:	/* Charging status( Charging on/off) */
		if (item->value[0] == '1')	/* 1 -> Charging */
			val = "CHRG";
		else				/* 0 -> Not charging */
			val = "!CHRG";
		break;
	case 89:	/* Charging status( SCC1 charging on/off) */
		break;
	case 90:	/* Charging status(AC charging on/off) */
		break;
	case 104:	/* flag for charging to floating mode */
		break;
	case 105:	/* Switch On */
		break;
	case 106:	/* Reserved */
		break;

	default:
		/* Don't know what happened */
		return -1;
	}

	snprintf(value, valuelen, "%s", val);

	return 0;
}

/* Warnings reported by the device */
static int	voltronic_sunny_warning(item_t *item, char *value, const size_t valuelen)
{
	char	warn[SMALLBUF] = "", unk[SMALLBUF] = "", bitwarns[SMALLBUF] = "", warns[4096] = "";
	int	i;

	if (strspn(item->value, "01-") != strlen(item->value)) {
		upsdebugx(2, "%s: invalid reply from the device [%s]", __func__, item->value);
		return -1;
	}

	/* Clear global flags */
	line_loss = 0;
	pv_loss = 0;

	/* No warnings */
	if (strspn(item->value, "0-") == strlen(item->value))
		return 0;

	snprintf(value, valuelen, "Device warnings:");

	for (i = 0; i < (int)strlen(item->value); i++) {

		int	u = 0;

		if (item->value[i] != '1')
			continue;

		switch (i)
		{
		case 0:	/* Unskip fault status item, if appropriate */
			if (
				(protocol == 15 && fw < 0.9) ||
				(protocol == 16 && fw < 0.3)
			) {
				item_t	*unskip;

				for (unskip = voltronic_axpert_qx2nut; unskip->info_type != NULL; unskip++) {
					if (!unskip->command)
						continue;
					if (strcasecmp(unskip->command, "QPIFS\r"))
						continue;
					if (strcasecmp(unskip->info_type, "ups.alarm"))	/* FIXME: should be "device.alarm" */
						continue;
					unskip->qxflags &= ~QX_FLAG_SKIP;
					break;
				}
			}

			strcpy(warn, "Device has fault.");
			break;
		case 1:
			strcpy(warn, "CPU is performing the auto-correction of AD signals.");
			break;
		case 2:
			strcpy(warn, "An external Flash device failed.");
			break;
		case 3:
			strcpy(warn, "Input PV is found lost.");
			pv_loss = 1;
			break;
		case 4:
			strcpy(warn, "PV input voltage reads low.");
			break;
		case 5:
			strcpy(warn, "Power island.");
			break;
		case 6:
			strcpy(warn, "An Error occurred in the CPU initialization.");
			break;
		case 7:
			strcpy(warn, "Power grid voltage exceeds the upper threshold.");
			line_loss = 1;
			break;
		case 8:
			strcpy(warn, "Power grid voltage falls below the lower threshold.");
			line_loss = 1;
			break;
		case 9:
			strcpy(warn, "Power grid frequency exceeds the upper threshold.");
			line_loss = 1;
			break;
		case 10:
			strcpy(warn, "Power grid frequency falls below the lower threshold.");
			line_loss = 1;
			break;
		case 11:
			strcpy(warn, "Power grid-connected average voltage exceeds the maximum threshold.");
			line_loss = 1;
			break;
		case 12:
			strcpy(warn, "Require power from the power grid.");
			break;
		case 13:
			strcpy(warn, "Emergent grid disconnection.");
			break;
		case 14:
			strcpy(warn, "Battery voltage is too low.");
			break;
		case 15:
			strcpy(warn, "Low battery.");
			update_status("LB");
			break;
		case 16:
			strcpy(warn, "Battery disconnected.");
			break;
		case 17:
			strcpy(warn, "End of battery discharge.");
			break;
		case 18:
			strcpy(warn, "Overload.");
			update_status("OVER");
			break;
		case 19:
			strcpy(warn, "EPO active.");
			break;
		case 22:
			strcpy(warn, "Over temperature alarm.");
			break;
		case 23:
			strcpy(warn, "No electrical ground.");
			break;
		case 24:
			strcpy(warn, "Fan fault.");
			break;
		default:
			snprintf(warn, sizeof(warn), "Unknown warning from device [bit: #%02d]", i + 1);
			u++;
			break;
		}

		upslogx(LOG_INFO, "Warning from device: %s", warn);

		/* Unknown warnings */
		if (u) {
			snprintfcat(unk, sizeof(unk), ", #%02d", i + 1);
			continue;
		}

		/* Known warnings */

		/* Already initialized */
		if (strlen(warns) > 0) {
			/* For too long warnings (total) */
			snprintfcat(bitwarns, sizeof(bitwarns), ", #%02d", i + 1);
			/* For warnings (total) not too long */
			snprintfcat(warns, sizeof(warns), " %s", warn);
			continue;
		}

		/* Yet to initialize */
		snprintf(bitwarns, sizeof(bitwarns), "Known (see log or manual) [bit: #%02d", i + 1);
		snprintf(warns, sizeof(warns), "%s", warn);

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
	if ((ST_MAX_VALUE_LEN - 32) > strlen(warns))
		/* ..then be explicit.. */
		snprintfcat(value, valuelen, " %s", warns);
	/* ..otherwise.. */
	else
		/* ..be cryptic */
		snprintfcat(value, valuelen, " %s", bitwarns);

	return 0;
}

/* Working mode reported by the device	TODO: improve status mapping in NUT (add 'scd.status'?) */
static int	voltronic_axpert_mode(item_t *item, char *value, const size_t valuelen)
{
	char	*status = NULL, *alarm = NULL;

	switch (item->value[0])
	{
		case 'P':	/* Power on mode */
			alarm = "Device is going ON.";
			break;
		case 'S':	/* Standby mode */
			status = "OFF";
			break;
		case 'L':	/* Line Mode */
			status = "OL";
			break;
		case 'B':	/* Battery mode */
			status = "!OL";
			break;
		case 'F':	/* Fault mode */
			alarm = "Fault reported by device.";
			break;
		case 'H':	/* Power saving Mode */
			status = "ECO";
			break;
		default:
			break;
	}

	if (alarm && !strcasecmp(item->info_type, "ups.alarm"))	/* FIXME: should be "device.alarm" */
		snprintf(value, valuelen, item->dfl, alarm);
	else if (status && !strcasecmp(item->info_type, "ups.status"))	/* FIXME: should be "device.status" */
		snprintf(value, valuelen, item->dfl, status);

	return 0;
}

/* Battery runtime */
static int	voltronic_sunny_batt_runtime(item_t *item, char *value, const size_t valuelen)
{
	double	runtime;

	if (strspn(item->value, "0123456789 .") != strlen(item->value)) {
		upsdebugx(2, "%s: non numerical value [%s: %s]", __func__, item->info_type, item->value);
		return -1;
	}

	/* Battery runtime is reported by the device in minutes, NUT expects seconds */
	runtime = strtod(item->value, NULL) * 60;

	snprintf(value, valuelen, item->dfl, runtime);

	return 0;
}

/* Fault reported by the device */
static int	voltronic_sunny_fault(item_t *item, char *value, const size_t valuelen)
{
	char		alarm[SMALLBUF];
	item_t		*unskip;

	upslogx(LOG_INFO, "Checking for faults..");

	if (
		(!strcasecmp(item->command, "QPIHF%02d\r") && !strcasecmp(item->value, "00")) ||
		(!strcasecmp(item->command, "QPIFS\r") && !strcasecmp(item->value, "OK"))
	) {
		snprintf(value, valuelen, item->dfl, "No fault found");
		upslogx(LOG_INFO, "%s", value);
		item->qxflags |= QX_FLAG_SKIP;
		return 0;
	}

	if ((strspn(item->value, "0123456789") != 2))
		snprintf(alarm, sizeof(alarm), "Unknown fault [%s]", item->value);
	else
		switch (strtol(item->value, NULL, 10))
		{
		case 1:
			strcpy(alarm, "DC bus voltage exceeds the upper threshold.");
			break;
		case 2:
			strcpy(alarm, "DC bus voltage falls below the lower threshold.");
			break;
		case 3:
			strcpy(alarm, "DC bus voltage soft-start is time-out.");
			break;
		case 4:
			strcpy(alarm, "Inverter soft-start is time-out.");
			break;
		case 5:
			strcpy(alarm, "An Inverter overcurrent event is detected.");
			break;
		case 6:
			strcpy(alarm, "Over temperature fault.");
			break;
		case 7:
			strcpy(alarm, "An relay failure event is detected.");
			break;
		case 8:
			strcpy(alarm, "DC component in the output current exceeds the upper threshold.");
			break;
		case 9:
			strcpy(alarm, "PV input voltage exceeds the upper threshold.");
			break;
		case 10:
			strcpy(alarm, "Auxiliary power failed.");
			break;
		case 11:
			strcpy(alarm, "An PV input overcurrent event is detected.");
			break;
		case 12:
			strcpy(alarm, "Leakage current exceeds the allowable range.");
			break;
		case 13:
			strcpy(alarm, "PV insulation resistance is too low.");
			break;
		case 14:
			strcpy(alarm, "Inverter DC component exceeds the allowable range.");
			break;
		case 15:
			strcpy(alarm, "A difference occurred in the readings from the main and secondary controllers.");
			break;
		case 16:
			strcpy(alarm, "Leakage current CT failed.");
			break;
		case 17:
			strcpy(alarm, "Communication with the main and secondary controllers is interrupted.");
			break;
		case 18:
			strcpy(alarm, "A communicating error occurred in the handshake between MCU and DSP.");
			break;
		case 19:
			strcpy(alarm, "No electrical ground.");
			break;
		case 20:
			strcpy(alarm, "Discharge circuit fault.");
			break;
		case 21:
			strcpy(alarm, "Soft start in battery discharge fails.");
			break;
		case 22:
			strcpy(alarm, "Charging voltage is too high.");
			break;
		case 23:
			strcpy(alarm, "Overload fault.");
			update_status("OVER");
			break;
		case 24:
			strcpy(alarm, "Battery disconnected.");
			break;
		case 25:
			strcpy(alarm, "Inverter current is too high for a long time.");
			break;
		case 26:
			strcpy(alarm, "Short circuited on inverter output.");
			break;
		case 27:
			strcpy(alarm, "Fan fault.");
			break;
		case 28:
			strcpy(alarm, "OP Current Sensor fault.");
			break;
		case 29:
			strcpy(alarm, "Charger failure.");
			break;
		case 30:
			strcpy(alarm, "Version mismatch between controller board and power board.");
			break;
		case 31:
			strcpy(alarm, "Reverse connection of input and output wires.");
			break;
		default:
			snprintf(alarm, sizeof(alarm), "Unknown fault [%s]", item->value);
			break;
		}

	snprintf(value, valuelen, item->dfl, alarm);
	upslogx(LOG_INFO, "Fault found: %s", alarm);

	/* Unskip fault data items */
	for (unskip = voltronic_axpert_qx2nut; unskip->info_type != NULL; unskip++) {
		if (!unskip->command)
			continue;
		if (strcasecmp(unskip->command, item->command))
			continue;
		unskip->qxflags &= ~QX_FLAG_SKIP;
	}

	item->qxflags |= QX_FLAG_SKIP;

	return 0;
}

/* Process date and add QX_FLAG_SKIP flag to item so that the driver will skip it the next time */
static int	voltronic_sunny_date_skip_me(item_t *item, char *value, const size_t valuelen)
{
	if (voltronic_sunny_date(item, value, valuelen))
		return -1;

	item->qxflags |= QX_FLAG_SKIP;

	return 0;
}

/* Process date and add QX_FLAG_SKIP flag to item so that the driver will skip it the next time */
static int	voltronic_sunny_time_skip_me(item_t *item, char *value, const size_t valuelen)
{
	if (voltronic_sunny_time(item, value, valuelen))
		return -1;

	item->qxflags |= QX_FLAG_SKIP;

	return 0;
}

/* Do only basic preprocessing to value and add QX_FLAG_SKIP flag to item so that the driver will skip it the next time */
static int	voltronic_sunny_skip_me(item_t *item, char *value, const size_t valuelen)
{
	if (voltronic_sunny_basic_preprocess(item, value, valuelen))
		return -1;

	item->qxflags |= QX_FLAG_SKIP;

	return 0;
}

/* New fault reported by the device */
static int	voltronic_sunny_fault_status(item_t *item, char *value, const size_t valuelen)
{
	/* No (new) fault */
	if (!strcasecmp(item->value, "00"))
		return 0;

	snprintf(value, valuelen, item->dfl, "New fault found.");

	/* Unskip fault ID */
	if (voltronic_axpert_clear_flags("fault_id", 0, 0, QX_FLAG_SKIP))
		return -1;

	return 0;
}

/* ID of fault reported by the device */
static int	voltronic_sunny_fault_id(item_t *item, char *value, const size_t valuelen)
{
	item_t	*unskip;

	if (strspn(item->value, "0123456789") != strlen(item->value)) {
		upsdebugx(2, "%s: non numerical value [%s: %s]", __func__, item->info_type, item->value);
		return -1;
	}

	fault_id = strtol(item->value, NULL, 10);
	snprintf(value, valuelen, item->dfl, fault_id);

	/* Unskip status-of-ID */
	for (unskip = voltronic_axpert_qx2nut; unskip->info_type != NULL; unskip++) {
		if (!unskip->command)
			continue;
		if (strcasecmp(unskip->command, "QPIHF%02d\r"))
			continue;
		if (strcasecmp(unskip->info_type, "ups.alarm"))	/* FIXME: should be "device.alarm" */
			continue;
		unskip->qxflags &= ~QX_FLAG_SKIP;
		break;
	}

	item->qxflags |= QX_FLAG_SKIP;

	return 0;
}

/* Self test result */
static int	voltronic_sunny_self_test_result(item_t *item, char *value, const size_t valuelen)
{
	const char	*result;
	item_t		*test;
	int		res;

	if (strspn(item->value, "0123456789") != strlen(item->value)) {
		upsdebugx(2, "%s: non numerical value [%s: %s]", __func__, item->info_type, item->value);
		return -1;
	}

	res = strtol(item->value, NULL, 10);

	switch (res)
	{
	case 0:		/* Test still running */
		result = "running";
		break;
	case 1:		/* Test passed */
		result = "passed";
		break;
	default:	/* Test failed */
		result = "failed";
		break;
	}

	snprintf(value, valuelen, item->dfl, result);

	/* Clear-skip/Unskip test data items */
	for (test = voltronic_axpert_qx2nut; test->info_type != NULL; test++) {
		if (!test->command)
			continue;
		if (strcasecmp(test->command, item->command))
			continue;
		if (!strcasecmp(test->info_type, item->info_type))
			continue;
		/* Passed/Failed -> unskip */
		if (res) {
			test->qxflags &= ~QX_FLAG_SKIP;
			continue;
		}
		/* Still running -> clear and skip */
		dstate_delinfo(test->info_type);
		test->qxflags |= QX_FLAG_SKIP;
	}

	return 0;
}


/* == Subdriver interface == */
subdriver_t	voltronic_axpert_subdriver = {
	VOLTRONIC_AXPERT_VERSION,
	voltronic_sunny_claim,
	voltronic_axpert_qx2nut,
	NULL,
	voltronic_axpert_initinfo,
	voltronic_sunny_makevartable,
	"ACK",
	"(NAK\r",
#ifdef TESTING
	voltronic_sunny_testing,
#endif	/* TESTING */
};
