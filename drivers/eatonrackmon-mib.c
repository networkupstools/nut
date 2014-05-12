/* eatonrackmon-mib.c - subdriver to monitor EatonRackMon SNMP devices with NUT
 *
 *  Copyright (C)
 *  2011 - 2012	Arnaud Quette <arnaud.quette@free.fr>
 *
 *  Note: this subdriver was initially generated as a "stub" by the
 *  gen-snmp-subdriver script. It must be customized!
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "eatonrackmon-mib.h"

#define EATONRACKMON_MIB_VERSION  "0.1"

#define EATONRACKMON_SYSOID       "1.3.6.1.4.1.534.6.7.1"

/* To create a value lookup structure (as needed on the 2nd line of the example
 * below), use the following kind of declaration, outside of the present snmp_info_t[]:
 * static info_lkp_t onbatt_info[] = {
 * 	{ 1, "OB" },
 * 	{ 2, "OL" },
 * 	{ 0, "NULL" }
 * };
 */

/* EATONRACKMON Snmp2NUT lookup table */
static snmp_info_t eatonrackmon_mib[] = {

	/* Data format:
	 * { info_type, info_flags, info_len, OID, dfl, flags, oid2info, setvar },
	 *
	 *	info_type:	NUT INFO_ or CMD_ element name
	 *	info_flags:	flags to set in addinfo
	 *	info_len:	length of strings if STR
	 *				cmd value if CMD, multiplier otherwise
	 *	OID: SNMP OID or NULL
	 *	dfl: default value
	 *	flags: snmp-ups internal flags (FIXME: ...)
	 *	oid2info: lookup table between OID and NUT values
	 *	setvar: variable to set for SU_FLAG_SETINT
	 *
	 * Example:
	 * { "input.voltage", 0, 0.1, ".1.3.6.1.4.1.705.1.6.2.1.2.1", "", SU_INPUT_1, NULL },
	 * { "ups.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.705.1.7.3.0", "", SU_FLAG_OK | SU_STATUS_BATT, onbatt_info },
	 *
	 * To create a value lookup structure (as needed on the 2nd line), use the
	 * following kind of declaration, outside of the present snmp_info_t[]:
	 * static info_lkp_t onbatt_info[] = {
	 * 	{ 1, "OB" },
	 * 	{ 2, "OL" },
	 * 	{ 0, "NULL" }
	 * };
	 */

	{ "device.type", ST_FLAG_STRING, SU_INFOSIZE, NULL, "sensor",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL, NULL },

	/* insIdentManufacturer.0 = STRING: "EATON" */
	{ "ups.mfr", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.7.1.1.1.1.0", NULL, SU_FLAG_OK, NULL },
	/* insIdentModel.0 = "" */
	/* right "model" OID, but is empty, but we must map this for snmp-ups to work currently!
	 * for now, we use "Ident" instead of "model"s */
	/*{ "ups.model", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.7.1.1.1.2.0",
		"Eaton Rack Monitor", SU_FLAG_STATIC | SU_FLAG_OK, NULL, NULL },*/
	/* insIdentAgentSoftwareVersion.0 = STRING: "Rack Monitor v2.01 (SN 11110041235009)" */
	{ "unmapped.insIdentAgentSoftwareVersion", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.7.1.1.1.3.0", NULL, SU_FLAG_OK, NULL },
	/* insIdentName.0 = STRING: "Rack Monitor" */
	{ "ups.model", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.7.1.1.1.4.0",
		"Eaton Rack Monitor", SU_FLAG_OK, NULL, NULL },
	/* rmConfigMibVersion.0 = INTEGER: 100 */
	{ "unmapped.rmConfigMibVersion", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.1.0", NULL, SU_FLAG_OK, NULL },
	/* rmConfigIpAddress.0 = IpAddress: 10.130.34.198 */
	{ "unmapped.rmConfigIpAddress", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.2.1.0", NULL, SU_FLAG_OK, NULL },
	/* rmConfigGateway.0 = IpAddress: 10.130.32.1 */
	{ "unmapped.rmConfigGateway", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.2.2.0", NULL, SU_FLAG_OK, NULL },
	/* rmConfigSubnetMask.0 = IpAddress: 255.255.252.0 */
	{ "unmapped.rmConfigSubnetMask", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.2.3.0", NULL, SU_FLAG_OK, NULL },
	/* rmConfigDate.0 = STRING: "05/12/2014" */
	{ "unmapped.rmConfigDate", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.7.1.1.2.3.1.0", NULL, SU_FLAG_OK, NULL },
	/* rmConfigTime.0 = STRING: "19:53:24" */
	{ "unmapped.rmConfigTime", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.7.1.1.2.3.2.0", NULL, SU_FLAG_OK, NULL },
	/* rmConfigTimeFromNtp.0 = INTEGER: disabled(2) */
	{ "unmapped.rmConfigTimeFromNtp", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.3.3.0", NULL, SU_FLAG_OK, NULL },
	/* rmConfigNtpIpAddress.0 = IpAddress: 0.0.0.0 */
	{ "unmapped.rmConfigNtpIpAddress", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.3.4.0", NULL, SU_FLAG_OK, NULL },
	/* rmConfigNtpTimeZone.0 = INTEGER: gMT-0000(14) */
	{ "unmapped.rmConfigNtpTimeZone", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.3.5.0", NULL, SU_FLAG_OK, NULL },
	/* rmConfigDayLightSaving.0 = INTEGER: disabled(2) */
	{ "unmapped.rmConfigDayLightSaving", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.3.6.0", NULL, SU_FLAG_OK, NULL },
	/* rmConfigHistoryLogFrequency.0 = INTEGER: 60 */
	{ "unmapped.rmConfigHistoryLogFrequency", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.4.1.0", NULL, SU_FLAG_OK, NULL },
	/* rmConfigExtHistoryLogFrequency.0 = INTEGER: 60 */
	{ "unmapped.rmConfigExtHistoryLogFrequency", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.4.2.0", NULL, SU_FLAG_OK, NULL },
	/* rmConfigConfigurationLog.0 = INTEGER: enabled(1) */
	{ "unmapped.rmConfigConfigurationLog", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.4.3.0", NULL, SU_FLAG_OK, NULL },
	/* rmConfigDhcpStatue.0 = INTEGER: enabled(1) */
	{ "unmapped.rmConfigDhcpStatue", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.5.0", NULL, SU_FLAG_OK, NULL },
	/* rmConfigPingStatue.0 = INTEGER: enabled(1) */
	{ "unmapped.rmConfigPingStatue", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.6.0", NULL, SU_FLAG_OK, NULL },
	/* rmConfigTftpStatue.0 = INTEGER: enabled(1) */
	{ "unmapped.rmConfigTftpStatue", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.7.0", NULL, SU_FLAG_OK, NULL },
	/* rmConfigTelnetStatue.0 = INTEGER: enabled(1) */
	{ "unmapped.rmConfigTelnetStatue", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.8.1.0", NULL, SU_FLAG_OK, NULL },
	/* rmConfigTelnetPortNumber.0 = INTEGER: 23 */
	{ "unmapped.rmConfigTelnetPortNumber", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.8.2.0", NULL, SU_FLAG_OK, NULL },
	/* rmConfigHttpStatue.0 = INTEGER: enabled(1) */
	{ "unmapped.rmConfigHttpStatue", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.9.1.0", NULL, SU_FLAG_OK, NULL },
	/* rmConfigHttpPortNumber.0 = INTEGER: 80 */
	{ "unmapped.rmConfigHttpPortNumber", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.9.2.0", NULL, SU_FLAG_OK, NULL },
	/* rmConfigSnmpStatue.0 = INTEGER: enabled(1) */
	{ "unmapped.rmConfigSnmpStatue", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.10.1.0", NULL, SU_FLAG_OK, NULL },
	/* rmConfigSnmpPortNumber.0 = INTEGER: 161 */
	{ "unmapped.rmConfigSnmpPortNumber", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.10.2.0", NULL, SU_FLAG_OK, NULL },
	/* rmConfigResetToDefault.0 = INTEGER: nothing(2) */
	{ "unmapped.rmConfigResetToDefault", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.11.1.0", NULL, SU_FLAG_OK, NULL },
	/* rmConfigRestart.0 = INTEGER: nothing(2) */
	{ "unmapped.rmConfigRestart", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.11.2.0", NULL, SU_FLAG_OK, NULL },
	/* rmConfigTrapRetryCount.0 = INTEGER: 1 */
	{ "unmapped.rmConfigTrapRetryCount", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.12.1.0", NULL, SU_FLAG_OK, NULL },
	/* rmConfigTrapRetryTime.0 = INTEGER: 10 */
	{ "unmapped.rmConfigTrapRetryTime", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.12.2.0", NULL, SU_FLAG_OK, NULL },
	/* rmConfigTrapAckSignature.0 = INTEGER: 0 */
	{ "unmapped.rmConfigTrapAckSignature", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.12.3.0", NULL, SU_FLAG_OK, NULL },
	/* rmConfigPollRate.0 = INTEGER: 5 */
	{ "unmapped.rmConfigPollRate", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.13.0", NULL, SU_FLAG_OK, NULL },
	/* trapsIndex.1 = INTEGER: 1 */
	{ "unmapped.trapsIndex", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.14.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* trapsIndex.2 = INTEGER: 2 */
	{ "unmapped.trapsIndex", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.14.1.1.2", NULL, SU_FLAG_OK, NULL },
	/* trapsIndex.3 = INTEGER: 3 */
	{ "unmapped.trapsIndex", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.14.1.1.3", NULL, SU_FLAG_OK, NULL },
	/* trapsIndex.4 = INTEGER: 4 */
	{ "unmapped.trapsIndex", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.14.1.1.4", NULL, SU_FLAG_OK, NULL },
	/* trapsIndex.5 = INTEGER: 5 */
	{ "unmapped.trapsIndex", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.14.1.1.5", NULL, SU_FLAG_OK, NULL },
	/* trapsIndex.6 = INTEGER: 6 */
	{ "unmapped.trapsIndex", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.14.1.1.6", NULL, SU_FLAG_OK, NULL },
	/* trapsIndex.7 = INTEGER: 7 */
	{ "unmapped.trapsIndex", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.14.1.1.7", NULL, SU_FLAG_OK, NULL },
	/* trapsIndex.8 = INTEGER: 8 */
	{ "unmapped.trapsIndex", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.14.1.1.8", NULL, SU_FLAG_OK, NULL },
	/* trapsReceiverAddr.1 = IpAddress: 0.0.0.0 */
	{ "unmapped.trapsReceiverAddr", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.14.1.2.1", NULL, SU_FLAG_OK, NULL },
	/* trapsReceiverAddr.2 = IpAddress: 0.0.0.0 */
	{ "unmapped.trapsReceiverAddr", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.14.1.2.2", NULL, SU_FLAG_OK, NULL },
	/* trapsReceiverAddr.3 = IpAddress: 0.0.0.0 */
	{ "unmapped.trapsReceiverAddr", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.14.1.2.3", NULL, SU_FLAG_OK, NULL },
	/* trapsReceiverAddr.4 = IpAddress: 0.0.0.0 */
	{ "unmapped.trapsReceiverAddr", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.14.1.2.4", NULL, SU_FLAG_OK, NULL },
	/* trapsReceiverAddr.5 = IpAddress: 0.0.0.0 */
	{ "unmapped.trapsReceiverAddr", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.14.1.2.5", NULL, SU_FLAG_OK, NULL },
	/* trapsReceiverAddr.6 = IpAddress: 0.0.0.0 */
	{ "unmapped.trapsReceiverAddr", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.14.1.2.6", NULL, SU_FLAG_OK, NULL },
	/* trapsReceiverAddr.7 = IpAddress: 0.0.0.0 */
	{ "unmapped.trapsReceiverAddr", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.14.1.2.7", NULL, SU_FLAG_OK, NULL },
	/* trapsReceiverAddr.8 = IpAddress: 0.0.0.0 */
	{ "unmapped.trapsReceiverAddr", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.14.1.2.8", NULL, SU_FLAG_OK, NULL },
	/* receiverCommunityString.1 = STRING: "*" */
	{ "unmapped.receiverCommunityString", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.7.1.1.2.14.1.3.1", NULL, SU_FLAG_OK, NULL },
	/* receiverCommunityString.2 = STRING: "*" */
	{ "unmapped.receiverCommunityString", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.7.1.1.2.14.1.3.2", NULL, SU_FLAG_OK, NULL },
	/* receiverCommunityString.3 = STRING: "*" */
	{ "unmapped.receiverCommunityString", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.7.1.1.2.14.1.3.3", NULL, SU_FLAG_OK, NULL },
	/* receiverCommunityString.4 = STRING: "*" */
	{ "unmapped.receiverCommunityString", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.7.1.1.2.14.1.3.4", NULL, SU_FLAG_OK, NULL },
	/* receiverCommunityString.5 = STRING: "*" */
	{ "unmapped.receiverCommunityString", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.7.1.1.2.14.1.3.5", NULL, SU_FLAG_OK, NULL },
	/* receiverCommunityString.6 = STRING: "*" */
	{ "unmapped.receiverCommunityString", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.7.1.1.2.14.1.3.6", NULL, SU_FLAG_OK, NULL },
	/* receiverCommunityString.7 = STRING: "*" */
	{ "unmapped.receiverCommunityString", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.7.1.1.2.14.1.3.7", NULL, SU_FLAG_OK, NULL },
	/* receiverCommunityString.8 = STRING: "*" */
	{ "unmapped.receiverCommunityString", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.7.1.1.2.14.1.3.8", NULL, SU_FLAG_OK, NULL },
	/* receiverSeverityLevel.1 = INTEGER: informational(1) */
	{ "unmapped.receiverSeverityLevel", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.14.1.4.1", NULL, SU_FLAG_OK, NULL },
	/* receiverSeverityLevel.2 = INTEGER: informational(1) */
	{ "unmapped.receiverSeverityLevel", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.14.1.4.2", NULL, SU_FLAG_OK, NULL },
	/* receiverSeverityLevel.3 = INTEGER: informational(1) */
	{ "unmapped.receiverSeverityLevel", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.14.1.4.3", NULL, SU_FLAG_OK, NULL },
	/* receiverSeverityLevel.4 = INTEGER: informational(1) */
	{ "unmapped.receiverSeverityLevel", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.14.1.4.4", NULL, SU_FLAG_OK, NULL },
	/* receiverSeverityLevel.5 = INTEGER: informational(1) */
	{ "unmapped.receiverSeverityLevel", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.14.1.4.5", NULL, SU_FLAG_OK, NULL },
	/* receiverSeverityLevel.6 = INTEGER: informational(1) */
	{ "unmapped.receiverSeverityLevel", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.14.1.4.6", NULL, SU_FLAG_OK, NULL },
	/* receiverSeverityLevel.7 = INTEGER: informational(1) */
	{ "unmapped.receiverSeverityLevel", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.14.1.4.7", NULL, SU_FLAG_OK, NULL },
	/* receiverSeverityLevel.8 = INTEGER: informational(1) */
	{ "unmapped.receiverSeverityLevel", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.14.1.4.8", NULL, SU_FLAG_OK, NULL },
	/* receiverDescription.1 = "" */
	{ "unmapped.receiverDescription", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.14.1.5.1", NULL, SU_FLAG_OK, NULL },
	/* receiverDescription.2 = "" */
	{ "unmapped.receiverDescription", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.14.1.5.2", NULL, SU_FLAG_OK, NULL },
	/* receiverDescription.3 = "" */
	{ "unmapped.receiverDescription", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.14.1.5.3", NULL, SU_FLAG_OK, NULL },
	/* receiverDescription.4 = "" */
	{ "unmapped.receiverDescription", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.14.1.5.4", NULL, SU_FLAG_OK, NULL },
	/* receiverDescription.5 = "" */
	{ "unmapped.receiverDescription", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.14.1.5.5", NULL, SU_FLAG_OK, NULL },
	/* receiverDescription.6 = "" */
	{ "unmapped.receiverDescription", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.14.1.5.6", NULL, SU_FLAG_OK, NULL },
	/* receiverDescription.7 = "" */
	{ "unmapped.receiverDescription", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.14.1.5.7", NULL, SU_FLAG_OK, NULL },
	/* receiverDescription.8 = "" */
	{ "unmapped.receiverDescription", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.14.1.5.8", NULL, SU_FLAG_OK, NULL },
	/* accessIndex.1 = INTEGER: 1 */
	{ "unmapped.accessIndex", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.15.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* accessIndex.2 = INTEGER: 2 */
	{ "unmapped.accessIndex", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.15.1.1.2", NULL, SU_FLAG_OK, NULL },
	/* accessIndex.3 = INTEGER: 3 */
	{ "unmapped.accessIndex", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.15.1.1.3", NULL, SU_FLAG_OK, NULL },
	/* accessIndex.4 = INTEGER: 4 */
	{ "unmapped.accessIndex", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.15.1.1.4", NULL, SU_FLAG_OK, NULL },
	/* accessIndex.5 = INTEGER: 5 */
	{ "unmapped.accessIndex", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.15.1.1.5", NULL, SU_FLAG_OK, NULL },
	/* accessIndex.6 = INTEGER: 6 */
	{ "unmapped.accessIndex", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.15.1.1.6", NULL, SU_FLAG_OK, NULL },
	/* accessIndex.7 = INTEGER: 7 */
	{ "unmapped.accessIndex", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.15.1.1.7", NULL, SU_FLAG_OK, NULL },
	/* accessIndex.8 = INTEGER: 8 */
	{ "unmapped.accessIndex", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.15.1.1.8", NULL, SU_FLAG_OK, NULL },
	/* accessControlAddr.1 = IpAddress: 0.0.0.0 */
	{ "unmapped.accessControlAddr", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.15.1.2.1", NULL, SU_FLAG_OK, NULL },
	/* accessControlAddr.2 = IpAddress: 0.0.0.0 */
	{ "unmapped.accessControlAddr", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.15.1.2.2", NULL, SU_FLAG_OK, NULL },
	/* accessControlAddr.3 = IpAddress: 0.0.0.0 */
	{ "unmapped.accessControlAddr", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.15.1.2.3", NULL, SU_FLAG_OK, NULL },
	/* accessControlAddr.4 = IpAddress: 0.0.0.0 */
	{ "unmapped.accessControlAddr", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.15.1.2.4", NULL, SU_FLAG_OK, NULL },
	/* accessControlAddr.5 = IpAddress: 0.0.0.0 */
	{ "unmapped.accessControlAddr", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.15.1.2.5", NULL, SU_FLAG_OK, NULL },
	/* accessControlAddr.6 = IpAddress: 0.0.0.0 */
	{ "unmapped.accessControlAddr", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.15.1.2.6", NULL, SU_FLAG_OK, NULL },
	/* accessControlAddr.7 = IpAddress: 0.0.0.0 */
	{ "unmapped.accessControlAddr", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.15.1.2.7", NULL, SU_FLAG_OK, NULL },
	/* accessControlAddr.8 = IpAddress: 0.0.0.0 */
	{ "unmapped.accessControlAddr", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.15.1.2.8", NULL, SU_FLAG_OK, NULL },
	/* accessCommunityString.1 = Wrong Type (should be OCTET STRING): INTEGER: 2 */
	{ "unmapped.accessCommunityString", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.7.1.1.2.15.1.3.1", NULL, SU_FLAG_OK, NULL },
	/* accessCommunityString.2 = Wrong Type (should be OCTET STRING): INTEGER: 2 */
	{ "unmapped.accessCommunityString", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.7.1.1.2.15.1.3.2", NULL, SU_FLAG_OK, NULL },
	/* accessCommunityString.3 = Wrong Type (should be OCTET STRING): INTEGER: 2 */
	{ "unmapped.accessCommunityString", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.7.1.1.2.15.1.3.3", NULL, SU_FLAG_OK, NULL },
	/* accessCommunityString.4 = Wrong Type (should be OCTET STRING): INTEGER: 2 */
	{ "unmapped.accessCommunityString", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.7.1.1.2.15.1.3.4", NULL, SU_FLAG_OK, NULL },
	/* accessCommunityString.5 = Wrong Type (should be OCTET STRING): INTEGER: 2 */
	{ "unmapped.accessCommunityString", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.7.1.1.2.15.1.3.5", NULL, SU_FLAG_OK, NULL },
	/* accessCommunityString.6 = Wrong Type (should be OCTET STRING): INTEGER: 2 */
	{ "unmapped.accessCommunityString", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.7.1.1.2.15.1.3.6", NULL, SU_FLAG_OK, NULL },
	/* accessCommunityString.7 = Wrong Type (should be OCTET STRING): INTEGER: 2 */
	{ "unmapped.accessCommunityString", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.7.1.1.2.15.1.3.7", NULL, SU_FLAG_OK, NULL },
	/* accessCommunityString.8 = Wrong Type (should be OCTET STRING): INTEGER: 2 */
	{ "unmapped.accessCommunityString", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.7.1.1.2.15.1.3.8", NULL, SU_FLAG_OK, NULL },
	/* rmConfigTemperatureUnit.0 = INTEGER: celsius(1) */
	{ "unmapped.rmConfigTemperatureUnit", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.16.0", NULL, SU_FLAG_OK, NULL },
	/* rmConfigDateFormat.0 = INTEGER: mm-dd-yyyy(1) */
	{ "unmapped.rmConfigDateFormat", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.17.0", NULL, SU_FLAG_OK, NULL },
	/* rmConfig.18.1.1.1 = INTEGER: 1 */
	{ "unmapped.rmConfig", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.18.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* rmConfig.18.1.1.2 = INTEGER: 2 */
	{ "unmapped.rmConfig", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.18.1.1.2", NULL, SU_FLAG_OK, NULL },
	/* rmConfig.18.1.1.3 = INTEGER: 3 */
	{ "unmapped.rmConfig", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.18.1.1.3", NULL, SU_FLAG_OK, NULL },
	/* rmConfig.18.1.1.4 = INTEGER: 4 */
	{ "unmapped.rmConfig", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.18.1.1.4", NULL, SU_FLAG_OK, NULL },
	/* rmConfig.18.1.2.1 = "" */
	{ "unmapped.rmConfig", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.18.1.2.1", NULL, SU_FLAG_OK, NULL },
	/* rmConfig.18.1.2.2 = "" */
	{ "unmapped.rmConfig", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.18.1.2.2", NULL, SU_FLAG_OK, NULL },
	/* rmConfig.18.1.2.3 = "" */
	{ "unmapped.rmConfig", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.18.1.2.3", NULL, SU_FLAG_OK, NULL },
	/* rmConfig.18.1.2.4 = "" */
	{ "unmapped.rmConfig", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.18.1.2.4", NULL, SU_FLAG_OK, NULL },
	/* rmConfig.18.1.3.1 = STRING: "*" */
	{ "unmapped.rmConfig", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.7.1.1.2.18.1.3.1", NULL, SU_FLAG_OK, NULL },
	/* rmConfig.18.1.3.2 = STRING: "*" */
	{ "unmapped.rmConfig", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.7.1.1.2.18.1.3.2", NULL, SU_FLAG_OK, NULL },
	/* rmConfig.18.1.3.3 = STRING: "*" */
	{ "unmapped.rmConfig", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.7.1.1.2.18.1.3.3", NULL, SU_FLAG_OK, NULL },
	/* rmConfig.18.1.3.4 = STRING: "*" */
	{ "unmapped.rmConfig", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.7.1.1.2.18.1.3.4", NULL, SU_FLAG_OK, NULL },
	/* rmConfig.18.1.4.1 = INTEGER: 3 */
	{ "unmapped.rmConfig", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.18.1.4.1", NULL, SU_FLAG_OK, NULL },
	/* rmConfig.18.1.4.2 = INTEGER: 3 */
	{ "unmapped.rmConfig", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.18.1.4.2", NULL, SU_FLAG_OK, NULL },
	/* rmConfig.18.1.4.3 = INTEGER: 3 */
	{ "unmapped.rmConfig", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.18.1.4.3", NULL, SU_FLAG_OK, NULL },
	/* rmConfig.18.1.4.4 = INTEGER: 3 */
	{ "unmapped.rmConfig", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.2.18.1.4.4", NULL, SU_FLAG_OK, NULL },
	/* rmConfig.19.0 = STRING: "*" */
	{ "unmapped.rmConfig", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.7.1.1.2.19.0", NULL, SU_FLAG_OK, NULL },
	/* rmConfig.20.0 = STRING: "*" */
	{ "unmapped.rmConfig", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.7.1.1.2.20.0", NULL, SU_FLAG_OK, NULL },
	/* smSensorNumber.0 = INTEGER: 2 */
	{ "unmapped.smSensorNumber", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.3.1.0", NULL, SU_FLAG_OK, NULL },
	/* smDeviceIndex.1 = INTEGER: 1 */
	{ "unmapped.smDeviceIndex", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.3.2.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* smDeviceIndex.2 = INTEGER: 2 */
	{ "unmapped.smDeviceIndex", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.3.2.1.1.2", NULL, SU_FLAG_OK, NULL },
	/* smDeviceStatus.1 = INTEGER: eMD-HT(3) */
	{ "unmapped.smDeviceStatus", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.3.2.1.2.1", NULL, SU_FLAG_OK, NULL },
	/* smDeviceStatus.2 = INTEGER: disabled(2) */
	{ "unmapped.smDeviceStatus", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.3.2.1.2.2", NULL, SU_FLAG_OK, NULL },
	/* smDeviceTemperature.1 = INTEGER: 264 */
	{ "ambient.temperature", 0, 0.1, ".1.3.6.1.4.1.534.6.7.1.1.3.2.1.3.1", NULL, SU_FLAG_OK, NULL },
	/* smDeviceTemperature.2 = INTEGER: -1 */
	{ "unmapped.smDeviceTemperature", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.3.2.1.3.2", NULL, SU_FLAG_OK, NULL },
	/* smDeviceTemperatureAlarm.1 = INTEGER: normal(3) */
	{ "unmapped.smDeviceTemperatureAlarm", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.3.2.1.4.1", NULL, SU_FLAG_OK, NULL },
	/* smDeviceTemperatureAlarm.2 = INTEGER: unknow(1) */
	{ "unmapped.smDeviceTemperatureAlarm", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.3.2.1.4.2", NULL, SU_FLAG_OK, NULL },
	/* smDeviceHumidity.1 = INTEGER: 260 */
	{ "ambient.humidity", 0, 0.1, ".1.3.6.1.4.1.534.6.7.1.1.3.2.1.5.1", NULL, SU_FLAG_OK, NULL },
	/* smDeviceHumidity.2 = INTEGER: -1 */
	{ "unmapped.smDeviceHumidity", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.3.2.1.5.2", NULL, SU_FLAG_OK, NULL },
	/* smDeviceHumidityAlarm.1 = INTEGER: lowCritical(5) */
	{ "unmapped.smDeviceHumidityAlarm", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.3.2.1.6.1", NULL, SU_FLAG_OK, NULL },
	/* smDeviceHumidityAlarm.2 = INTEGER: unknow(1) */
	{ "unmapped.smDeviceHumidityAlarm", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.3.2.1.6.2", NULL, SU_FLAG_OK, NULL },
	/* smAlarm1.1 = INTEGER: disabled(2) */
	{ "unmapped.smAlarm1", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.3.2.1.7.1", NULL, SU_FLAG_OK, NULL },
	/* smAlarm1.2 = INTEGER: unknow(1) */
	{ "unmapped.smAlarm1", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.3.2.1.7.2", NULL, SU_FLAG_OK, NULL },
	/* smAlarm2.1 = INTEGER: disabled(2) */
	{ "unmapped.smAlarm2", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.3.2.1.8.1", NULL, SU_FLAG_OK, NULL },
	/* smAlarm2.2 = INTEGER: unknow(1) */
	{ "unmapped.smAlarm2", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.3.2.1.8.2", NULL, SU_FLAG_OK, NULL },
	/* scSensorNumber.0 = INTEGER: 2 */
	{ "unmapped.scSensorNumber", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.1.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor1DeviceName.0 = STRING: "SENSOR 1" */
	{ "unmapped.scSensor1DeviceName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.7.1.1.4.3.1.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor1DeviceState.0 = INTEGER: auto(2) */
	{ "unmapped.scSensor1DeviceState", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.3.2.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor1TemperatureName.0 = STRING: "Temperature-1" */
	{ "unmapped.scSensor1TemperatureName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.7.1.1.4.3.3.1.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor1TemperatureLowWarning.0 = INTEGER: 23 */
	{ "unmapped.scSensor1TemperatureLowWarning", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.3.3.2.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor1TemperatureLowCritical.0 = INTEGER: 20 */
	{ "unmapped.scSensor1TemperatureLowCritical", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.3.3.3.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor1TemperatureHighWarning.0 = INTEGER: 27 */
	{ "unmapped.scSensor1TemperatureHighWarning", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.3.3.4.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor1TemperatureHighCritical.0 = INTEGER: 30 */
	{ "unmapped.scSensor1TemperatureHighCritical", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.3.3.5.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor1TemperatureHysteresis.0 = INTEGER: 2 */
	{ "unmapped.scSensor1TemperatureHysteresis", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.3.3.6.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor1TemperatureCalibration.0 = INTEGER: temperatureIncrease0Point0(1) */
	{ "unmapped.scSensor1TemperatureCalibration", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.3.3.7.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor1TemperatureLowWarningStatus.0 = INTEGER: disabled(2) */
	{ "unmapped.scSensor1TemperatureLowWarningStatus", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.3.3.8.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor1TemperatureLowCriticalStatus.0 = INTEGER: enabled(1) */
	{ "unmapped.scSensor1TemperatureLowCriticalStatus", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.3.3.9.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor1TemperatureHighWarningStatus.0 = INTEGER: disabled(2) */
	{ "unmapped.scSensor1TemperatureHighWarningStatus", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.3.3.10.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor1TemperatureHighCriticalStatus.0 = INTEGER: enabled(1) */
	{ "unmapped.scSensor1TemperatureHighCriticalStatus", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.3.3.11.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor1HumdityName.0 = STRING: "Humidity-1" */
	{ "unmapped.scSensor1HumdityName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.7.1.1.4.3.4.1.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor1HumidityLowWarning.0 = INTEGER: 40 */
	{ "unmapped.scSensor1HumidityLowWarning", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.3.4.2.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor1HumidityLowCritical.0 = INTEGER: 35 */
	{ "unmapped.scSensor1HumidityLowCritical", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.3.4.3.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor1HumidityHighWarning.0 = INTEGER: 50 */
	{ "unmapped.scSensor1HumidityHighWarning", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.3.4.4.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor1HumidityHighCritical.0 = INTEGER: 55 */
	{ "unmapped.scSensor1HumidityHighCritical", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.3.4.5.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor1HumidityHysteresis.0 = INTEGER: 5 */
	{ "unmapped.scSensor1HumidityHysteresis", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.3.4.6.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor1HumidityCalibration.0 = INTEGER: humidityIncrease0Point0(1) */
	{ "unmapped.scSensor1HumidityCalibration", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.3.4.7.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor1HumidityLowWarningStatus.0 = INTEGER: disabled(2) */
	{ "unmapped.scSensor1HumidityLowWarningStatus", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.3.4.8.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor1HumidityLowCriticalStatus.0 = INTEGER: enabled(1) */
	{ "unmapped.scSensor1HumidityLowCriticalStatus", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.3.4.9.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor1HumidityHighWarningStatus.0 = INTEGER: disabled(2) */
	{ "unmapped.scSensor1HumidityHighWarningStatus", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.3.4.10.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor1HumidityHighCriticalStatus.0 = INTEGER: enabled(1) */
	{ "unmapped.scSensor1HumidityHighCriticalStatus", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.3.4.11.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor1Alarm1Name.0 = STRING: "Alarm-1" */
	{ "unmapped.scSensor1Alarm1Name", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.7.1.1.4.3.5.1.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor1Alarm1State.0 = INTEGER: disabled(1) */
	{ "unmapped.scSensor1Alarm1State", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.3.5.2.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor1Alarm1Hysteresis.0 = INTEGER: 0 */
	{ "unmapped.scSensor1Alarm1Hysteresis", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.3.5.3.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor1Alarm2Name.0 = STRING: "Alarm-2" */
	{ "unmapped.scSensor1Alarm2Name", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.7.1.1.4.3.6.1.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor1Alarm2State.0 = INTEGER: disabled(1) */
	{ "unmapped.scSensor1Alarm2State", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.3.6.2.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor1Alarm2Hysteresis.0 = INTEGER: 0 */
	{ "unmapped.scSensor1Alarm2Hysteresis", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.3.6.3.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor2DeviceName.0 = STRING: "SENSOR 2" */
	{ "unmapped.scSensor2DeviceName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.7.1.1.4.4.1.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor2DeviceState.0 = INTEGER: disabled(1) */
	{ "unmapped.scSensor2DeviceState", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.4.2.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor2TemperatureName.0 = STRING: "Temperature-2" */
	{ "unmapped.scSensor2TemperatureName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.7.1.1.4.4.3.1.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor2TemperatureLowWarning.0 = INTEGER: 23 */
	{ "unmapped.scSensor2TemperatureLowWarning", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.4.3.2.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor2TemperatureLowCritical.0 = INTEGER: 20 */
	{ "unmapped.scSensor2TemperatureLowCritical", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.4.3.3.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor2TemperatureHighWarning.0 = INTEGER: 27 */
	{ "unmapped.scSensor2TemperatureHighWarning", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.4.3.4.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor2TemperatureHighCritical.0 = INTEGER: 30 */
	{ "unmapped.scSensor2TemperatureHighCritical", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.4.3.5.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor2TemperatureHysteresis.0 = INTEGER: 2 */
	{ "unmapped.scSensor2TemperatureHysteresis", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.4.3.6.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor2TemperatureCalibration.0 = INTEGER: temperatureIncrease0Point0(1) */
	{ "unmapped.scSensor2TemperatureCalibration", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.4.3.7.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor2TemperatureLowWarningStatus.0 = INTEGER: disabled(2) */
	{ "unmapped.scSensor2TemperatureLowWarningStatus", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.4.3.8.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor2TemperatureLowCriticalStatus.0 = INTEGER: enabled(1) */
	{ "unmapped.scSensor2TemperatureLowCriticalStatus", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.4.3.9.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor2TemperatureHighWarningStatus.0 = INTEGER: disabled(2) */
	{ "unmapped.scSensor2TemperatureHighWarningStatus", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.4.3.10.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor2TemperatureHighCriticalStatus.0 = INTEGER: enabled(1) */
	{ "unmapped.scSensor2TemperatureHighCriticalStatus", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.4.3.11.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor2HumdityName.0 = STRING: "Humidity-2" */
	{ "unmapped.scSensor2HumdityName", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.7.1.1.4.4.4.1.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor2HumidityLowWarning.0 = INTEGER: 40 */
	{ "unmapped.scSensor2HumidityLowWarning", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.4.4.2.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor2HumidityLowCritical.0 = INTEGER: 35 */
	{ "unmapped.scSensor2HumidityLowCritical", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.4.4.3.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor2HumidityHighWarning.0 = INTEGER: 50 */
	{ "unmapped.scSensor2HumidityHighWarning", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.4.4.4.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor2HumidityHighCritical.0 = INTEGER: 55 */
	{ "unmapped.scSensor2HumidityHighCritical", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.4.4.5.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor2HumidityHysteresis.0 = INTEGER: 5 */
	{ "unmapped.scSensor2HumidityHysteresis", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.4.4.6.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor2HumidityCalibration.0 = INTEGER: humidityIncrease0Point0(1) */
	{ "unmapped.scSensor2HumidityCalibration", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.4.4.7.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor2HumidityLowWarningStatus.0 = INTEGER: disabled(2) */
	{ "unmapped.scSensor2HumidityLowWarningStatus", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.4.4.8.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor2HumidityLowCriticalStatus.0 = INTEGER: enabled(1) */
	{ "unmapped.scSensor2HumidityLowCriticalStatus", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.4.4.9.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor2HumidityHighWarningStatus.0 = INTEGER: disabled(2) */
	{ "unmapped.scSensor2HumidityHighWarningStatus", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.4.4.10.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor2HumidityHighCriticalStatus.0 = INTEGER: enabled(1) */
	{ "unmapped.scSensor2HumidityHighCriticalStatus", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.4.4.11.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor2Alarm1Name.0 = STRING: "Alarm-3" */
	{ "unmapped.scSensor2Alarm1Name", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.7.1.1.4.4.5.1.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor2Alarm1State.0 = INTEGER: disabled(1) */
	{ "unmapped.scSensor2Alarm1State", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.4.5.2.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor2Alarm1Hysteresis.0 = INTEGER: 0 */
	{ "unmapped.scSensor2Alarm1Hysteresis", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.4.5.3.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor2Alarm2Name.0 = STRING: "Alarm-4" */
	{ "unmapped.scSensor2Alarm2Name", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.7.1.1.4.4.6.1.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor2Alarm2State.0 = INTEGER: disabled(1) */
	{ "unmapped.scSensor2Alarm2State", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.4.6.2.0", NULL, SU_FLAG_OK, NULL },
	/* scSensor2Alarm2Hysteresis.0 = INTEGER: 0 */
	{ "unmapped.scSensor2Alarm2Hysteresis", 0, 1, ".1.3.6.1.4.1.534.6.7.1.1.4.4.6.3.0", NULL, SU_FLAG_OK, NULL },

	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, 0, NULL }
};

mib2nut_info_t	eatonrackmon = { "eatonrackmon", EATONRACKMON_MIB_VERSION, NULL, ".1.3.6.1.4.1.534.6.7.1.1.1.1.0", eatonrackmon_mib, EATONRACKMON_SYSOID };
