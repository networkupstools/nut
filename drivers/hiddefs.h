/**
 * @file
 * @brief	HID definitions
 *		(mostly from "Device Class Definition for HID", v. 1.11).
 * @copyright	@parblock
 * Copyright (C):
 * - 2018 -- Daniele Pezzini (<hyouko@gmail.com>)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *		@endparblock
 */

#ifndef HIDDEFS_H
#define HIDDEFS_H

/** @brief Class-specific requests (`bRequest` field). */
enum hid_request {
	HID_REQUEST_GET_REPORT		= 0x01,					/**< The GET_REPORT request allows the host to receive a report via the control pipe. */
	HID_REQUEST_GET_IDLE		= 0x02,					/**< The GET_IDLE request reads the current idle rate for a particular input report
										 * (see @ref HID_REQUEST_SET_IDLE "SET_IDLE" request).*/
	HID_REQUEST_GET_PROTOCOL	= 0x03,					/**< The GET_PROTOCOL request reads which protocol is currently active
										 * (either the boot protocol or the report protocol). */
					/* 0x04-0x08 are reserved */
	HID_REQUEST_SET_REPORT		= 0x09,					/**< The SET_REPORT request allows the host to send a report to the device,
										 * possibly setting the state of input, output, or feature controls. */
	HID_REQUEST_SET_IDLE		= 0x0A,					/**< The SET_IDLE request silences a particular report on the interrupt IN pipe
										 * until a new event occurs or the specified amount of time passes. */
	HID_REQUEST_SET_PROTOCOL	= 0x0B					/**< The SET_PROTOCOL switches between the boot protocol and the report protocol
										 * (or viceversa). */
};

/** @brief Report types (high byte of the `wValue` field) for @ref HID_REQUEST_GET_REPORT "GET_REPORT"/@ref HID_REQUEST_SET_REPORT "SET_REPORT" requests. */
enum hid_report_type {
	HID_RT_INPUT			= 0x01,					/**< Input report. */
	HID_RT_OUTPUT			= 0x02,					/**< Output report. */
	HID_RT_FEATURE			= 0x03					/**< Feature report. */
					/* 0x04-0xFF are reserved */
};

#define HID_DT_HID_SIZE			9					/**< @brief Size of a HID descriptor. */
#define HID_DT_REPORT_SIZE_MAX		0x1800					/**< @brief Maximum size of a report descriptor. */

#endif	/* HIDDEFS_H */
