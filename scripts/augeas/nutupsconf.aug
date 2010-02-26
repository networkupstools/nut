 (*
Module: NutUpsConf
 Parses /etc/nut/ups.conf

Author: Raphael Pinson <raphink@gmail.com>
	Frederic Bohe  <fredericbohe@eaton.com>

About: License
  This file is licensed under the GPL.

About: Lens Usage
  Sample usage of this lens in augtool

    * Print all drivers used
      > print /files/etc/nut/ups.conf/*/driver

About: Configuration files
  This lens applies to /etc/nut/ups.conf. See <filter>.
*)

module NutUpsConf =
  autoload ups_xfm

(************************************************************************
 * Group:                 UPS.CONF
 *************************************************************************)

let ups_comment  = IniFile.comment IniFile.comment_re IniFile.comment_default

let ups_sep      = IniFile.sep IniFile.sep_re IniFile.sep_default

let ups_global   = "chroot"
                 | "driverpath"
                 | "maxstartdelay"
                 | "pollinterval"
                 | "user"

let ups_fields   = "driver"
                 | "port"
                 | "sdorder"
                 | "desc"
                 | "nolock"
                 | "maxstartdelay"
|"ups.delay.shutdown"|"ups.delay.start"|"battext"|"prgshut"|"daysweek"|"daysoff"|"houron"|"houroff"|"mfr"|"model"|"serial"|"lowbatt"|"ondelay"|"offdelay"|"battvolts"|"battvoltmult"|"ignoreoff"|"sendpace"|"dtr"|"rts"|"offdelay"|"ondelay"|"mincharge"|"minruntime"|"load.off"|"load.on"|"load.status"|"testtime"|"timeout"|"subscribe"|"manufacturer"|"modelname"|"serialnumber"|"ondelay"|"offdelay"|"cablepower"|"ondelay"|"offdelay"|"runtimecal"|"chargetime"|"idleload"|"norating"|"novendor"|"upstype"|"mfr"|"model"|"serial"|"CP"|"OL"|"LB"|"SD"|"sdtime"|"offdelay"|"ondelay"|"pollfreq"|"pollonly"|"usd"|"modelname"|"offdelay"|"startdelay"|"rebootdelay"|"manufacturer"|"linevoltage"|"modelname"|"serialnumber"|"type"|"numOfBytesFromUPS"|"methodOfFlowControl"|"shutdownArguments"|"validationSequence"|"voltage"|"frequency"|"batteryPercentage"|"loadPercentage"|"offdelay"|"vendor"|"product"|"serial"|"productid"|"bus"|"shutdown_delay"|"baud_rate"|"battext"|"lowbatt"|"offdelay"|"ondelay"|"notification"|"nombattvolt"|"ID"|"prefix"|"CS"|"subdriver"|"vendorid"|"productid"|"vendor"|"product"|"serial"|"bus"|"ondelay"|"offdelay"|"runtimecal"|"chargetime"|"idleload"|"norating"|"novendor"|"shutdown_delay"|"baud_rate"|"baudrate"|"max_load"|"ondelay"|"offdelay"|"manufacturer"|"model"|"serial"|"protocol"|"mfr"|"model"|"manufacturer"|"baudrate"|"input_timeout"|"output_pace"|"full_update"|"use_crlf"|"use_pre_lf"|"lowbatt"|"mfr"|"model"|"serial"|"lowbatt"|"ondelay"|"offdelay"|"battvolts"|"battvoltmult"|"ignoreoff"|"sendpace"|"dtr"|"rts"|"vendor"|"product"|"vendorid"|"productid"|"bus"|"subdriver"|"mibs"|"snmp_version"|"pollfreq"|"notransferoids"|"offdelay"|"ondelay"|"pollfreq"|"pollonly"|"vendor"|"product"|"serial"|"vendorid"|"productid"|"bus"|"explore"|"LowBatt"|"OnDelay"|"OffDelay"|"oldmac"|"status_only"|"fake_lowbatt"|"nowarn_noimp"|"powerup"|"cable"|"sdtype"|"wait"|"wait"|"nohang"|"flash"|"silent"|"dumbterm"

let ups_entry    = IniFile.indented_entry (ups_global|ups_fields) ups_sep ups_comment

let ups_title    = IniFile.indented_title IniFile.record_re

let ups_record   = IniFile.record ups_title ups_entry

let ups_lns      = IniFile.lns ups_record ups_comment

let ups_filter   = (incl "/etc/nut/ups.conf")
                . Util.stdexcl

let ups_xfm      = transform ups_lns ups_filter

