/* microsol-apc.h - APC Back-UPS BR series UPS driver

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

#ifndef INCLUDED_MICROSOL_APC_H
#define INCLUDED_MICROSOL_APC_H

#define MODEL_COUNT 3

/* Supported UPS models */
static const unsigned int MODELS[MODEL_COUNT] = {
	183 /* APC Back-UPS BZ2200I-BR */ ,
	190 /* APC Back-UPS BZ1500-BR */ ,
	191			/* APC Back-UPS BZ2200BI-BR */
};

static const unsigned int NOMINAL_POWER[MODEL_COUNT] = {
	2200 /* Model 183 */ ,
	1500 /* Model 190 */ ,
	2200			/* Model 191 */
};

/* Curves for output voltage (depends on relay state and battery state)
 * Second index: On-line (0) or On-battery (1)
 * Third index: Electric relay state
 */
/* For on-line UPS */
static const float OUTPUT_VOLTAGE_MULTIPLIER_A[MODEL_COUNT][2][8] = {
	{
	 { 1.831, 1.831, 1.831, 1.831, 1.831, 1.831, 1.831, 1.831},
	 { 0.1566, 0.1566, 0.1566, 0.1566, 0.1566, 0.1566, 0.1566, 0.1566}
	  } /* Model 183 */ ,
	{
	 { 0.9266, 0.9266, 0.9266, 0.9266, 0.9266, 0.9266, 0.9266, 0.9266},
	 { 5.59, 9.47, 13.7, 0.0, 0.0, 0.0, 0.0, 0.0}
	  } /* Model 190 */ ,
	{
	 { 1.00, 1.00, 1.00, 1.00, 1.00, 1.00, 1.00, 1.00},
	 { 1.26, 1.26, 1.26, 1.26, 1.26, 1.26, 1.26, 1.26}
	  }			/* Model 191 */
};

static const float OUTPUT_VOLTAGE_MULTIPLIER_B[MODEL_COUNT][2][8] = {
	{
	 { -2.1374, -2.1374, -2.1374, -2.1374, -2.1374, -2.1374, -2.1374, -2.1374},
	 { 204.93, 204.93, 204.93, 204.93, 204.93, 204.93, 204.93, 204.93}
	  } /* Model 183 */ ,
	{
	 { 5.0694, 5.0694, 5.0694, 5.0694, 5.0694, 5.0694, 5.0694, 5.0694},
	 { 5.4, 6.5, 17.6, 0.0, 0.0, 0.0, 0.0, 0.0}
	  } /* Model 190 */ ,
	{
	 { 20.0, 20.0, 20.0, 20.0, 20.0, 20.0, 20.0, 20.0},
	 { -5.91, -5.91, -5.91, -5.91, -5.91, -5.91, -5.91, -5.91}
	  }			/* Model 191 */
};

/* Curves for output current
 * Second index: On-line (0) or On-battery (1)
 */
static const float OUTPUT_CURRENT_MULTIPLIER_A[MODEL_COUNT][2] = {
	{
	 0.0892999991774559,
	 0.09070000052452087 } /* Model 183 */ ,
	{
	 0.1264,
	 0.1303 } /* Model 190 */ ,
	{
	 0.13819999992847443,
	 0.08959999680519104 } /* Model 191 */ ,
};

static const float OUTPUT_CURRENT_MULTIPLIER_B[MODEL_COUNT][2] = {
	{
	 0.09350000321865082,
	 0.14550000429153442 } /* Model 183 */ ,
	{
	 0.522,
	 0.468 } /* Model 190 */ ,
	{
	 0.12999999523162842,
	 0.1881999969482422 }	/* Model 191 */
};

/* Curves for input voltage (depends on nominal output voltage)
 * Second index: Nominal output voltage = 220V (1) or 115V (0).
 */
static const float INPUT_VOLTAGE_MULTIPLIER_A[MODEL_COUNT][2] = {
	{ 1.7937, 1.7937 } /* Model 183 */ ,
	{ 1.8, 1.8 } /* Model 190 */ ,
	{ 1.4825, 1.4952 }	/* Model 191 */
};

static const float INPUT_VOLTAGE_MULTIPLIER_B[MODEL_COUNT][2] = {
	{ 1.854, 1.854 } /* Model 183 */ ,
	{ 2.224, 2.224 } /* Model 190 */ ,
	{ 0.0853, -2.4241 }	/* Model 191 */
};

/* Curves for battery voltage */
static const float BATTERY_VOLTAGE_MULTIPLIER_A[MODEL_COUNT] = {
	0.1551 /* Model 183 */ ,
	0.1513 /* Model 190 */ ,
	0.1543			/* Model 191 */
};

static const float BATTERY_VOLTAGE_MULTIPLIER_B[MODEL_COUNT] = {
	0.0654 /* Model 183 */ ,
	0.7153 /* Model 190 */ ,
	0.1414			/* Model 191 */
};

/* Real power estimation curves (depends on relay state) */
/*
 * Remarks:
 * - Model 190 use a direct real power determination (no need for curve selectors)
 */
static const float REAL_POWER_CURVE_SELECTOR_A1[MODEL_COUNT][8] = {
	{ 0.24, 0.26, 0.0, 0.0, 0.24, 0.26, 0.0, 0.28 } /* Model 183 */ ,
	{ 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 } /* Model 190 */ ,
	{ 0.24, 0.26, 0.0, 0.0, 0.24, 0.26, 0.0, 0.28 }	/* Model 191 */
};

static const float REAL_POWER_CURVE_SELECTOR_B1[MODEL_COUNT][8] = {
	{ 83.15, 81.23, 0.0, 0.0, 83.49, 79.05, 0.0, 85.67 } /* Model 183 */ ,
	{ 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 } /* Model 190 */ ,
	{ 83.15, 81.23, 0.0, 0.0, 83.49, 79.05, 0.0, 85.67 }	/* Model 191 */
};

static const float REAL_POWER_CURVE_SELECTOR_A2[MODEL_COUNT][8] = {
	{ 0.0763, 0.081, 0.0919, 0.0, 0.0741, 0.0828, 0.0, 0.0938 } /* Model 183 */ ,
	{ 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 } /* Model 190 */ ,
	{ 0.0763, 0.081, 0.0919, 0.0, 0.0741, 0.0828, 0.0, 0.0938 }	/* Model 191 */
};

static const float REAL_POWER_CURVE_SELECTOR_B2[MODEL_COUNT][8] = {
	{ 81.732, 94.459, 86.686, 0.0, 84.657, 84.999, 0.0, 78.097 } /* Model 183 */ ,
	{ 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 } /* Model 190 */ ,
	{ 81.732, 94.459, 86.686, 0.0, 84.657, 84.999, 0.0, 78.097 }	/* Model 191 */
};

static const float REAL_POWER_CURVE_SELECTOR_A3[MODEL_COUNT][8] = {
	{ 0.0744, 0.0808, 0.0885, 0.0, 0.0732, 0.084, 0.0, 0.0955 } /* Model 183 */ ,
	{ 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 } /* Model 190 */ ,
	{ 0.0744, 0.0808, 0.0885, 0.0, 0.0732, 0.084, 0.0, 0.0955 }	/* Model 191 */
};

static const float REAL_POWER_CURVE_SELECTOR_B3[MODEL_COUNT][8] = {
	{ 122.06, 122.9, 125.75, 0.0, 120.39, 108.52, 0.0, 92.239 } /* Model 183 */ ,
	{ 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 } /* Model 190 */ ,
	{ 122.06, 122.9, 125.75, 0.0, 120.39, 108.52, 0.0, 92.239 }	/* Model 191 */
};

static const float REAL_POWER_MULTIPLIER_A1[MODEL_COUNT][8] = {
	{ 0.08040007075206226, 0.0894, 0.0999, 0.0, 0.0813, 0.0905, 0.0, 0.1005 } /* Model 183 */ ,
	{ 0.1127, 0.1127, 0.1127, 0.1127, 0.1127, 0.1127, 0.1127, 0.1127 } /* Model 190 */ ,
	{ 0.08040007075206226, 0.0894, 0.0999, 0.0, 0.0813, 0.0905, 0.0, 0.1005 }	/* Model 191 */
};

static const float REAL_POWER_MULTIPLIER_B1[MODEL_COUNT][8] = {
	{ 45.292, 41.928, 41.727, 0.0, 40.269, 41.81, 0.0, 43.458 } /* Model 183 */ ,
	{ 50.031, 50.031, 50.031, 50.031, 50.031, 50.031, 50.031, 50.031 } /* Model 190 */ ,
	{ 45.292, 41.928, 41.727, 0.0, 40.269, 41.81, 0.0, 43.458 }	/* Model 191 */
};

static const float REAL_POWER_MULTIPLIER_A2[MODEL_COUNT][8] = {
	{ 0.08630063689870031, 0.0946, 0.1068, 0.0, 0.086, 0.0967, 0.0, 0.1088 } /* Model 183 */ ,
	{ 0.1127, 0.1127, 0.1127, 0.1127, 0.1127, 0.1127, 0.1127, 0.1127 } /* Model 190 */ ,
	{ 0.08630063689870031, 0.0946, 0.1068, 0.0, 0.086, 0.0967, 0.0, 0.1088 }	/* Model 191 */
};

static const float REAL_POWER_MULTIPLIER_B2[MODEL_COUNT][8] = {
	{ 8.3927, 9.2393, 8.2852, 0.0, 8.301, 6.7636, 0.0, 8.2842 } /* Model 183 */ ,
	{ 50.031, 50.031, 50.031, 50.031, 50.031, 50.031, 50.031, 50.031 } /* Model 190 */ ,
	{ 8.3927, 9.2393, 8.2852, 0.0, 8.301, 6.7636, 0.0, 8.2842 }	/* Model 191 */
};

static const float REAL_POWER_MULTIPLIER_A3[MODEL_COUNT][8] = {
	{ 0.0896001146881468, 0.0991, 0.1116, 0.0, 0.0967, 0.1068, 0.0, 0.1169 } /* Model 183 */ ,
	{ 0.1127, 0.1127, 0.1127, 0.1127, 0.1127, 0.1127, 0.1127, 0.1127 } /* Model 190 */ ,
	{ 0.0896001146881468, 0.0991, 0.1116, 0.0, 0.0967, 0.1068, 0.0, 0.1169 }	/* Model 191 */
};

static const float REAL_POWER_MULTIPLIER_B3[MODEL_COUNT][8] = {
	{ -31.115, -33.777, -33.826, 0.0, -59.513, -57.729, 0.0, -41.333 } /* Model 183 */ ,
	{ 50.031, 50.031, 50.031, 50.031, 50.031, 50.031, 50.031, 50.031 } /* Model 190 */ ,
	{ -31.115, -33.777, -33.826, 0.0, -59.513, -57.729, 0.0, -41.333 }	/* Model 191 */
};

/**
 * Maximum battery voltage, used to estimate battery charge
 * Second index: Recharging battery flag: charging (1) or charged/discharging (0)
 */
static const float MAX_BATTERY_VOLTAGE[MODEL_COUNT][2] = {
	{ 27.0, 29.5 } /* Model 183 */ ,
	{ 27.0, 29.5 } /* Model 190 */ ,
	{ 27.0, 29.5 }	/* Model 191 */
};

/** Minimum battery voltage, used to estimate battery charge */
static const float MIN_BATTERY_VOLTAGE[MODEL_COUNT] = {
	20 /* Model 183 */ ,
	20 /* Model 190 */ ,
	20 /* Model 191 */
};

#endif				/* INCLUDED_MICROSOL_APC_H */
