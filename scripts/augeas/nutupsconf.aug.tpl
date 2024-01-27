(*
Module: NutUpsConf
 Parses @CONFPATH@/ups.conf

Author: Raphael Pinson <raphink@gmail.com>
        Frederic Bohe  <fredericbohe@eaton.com>
        Arnaud Quette <arnaud.quette@gmail.com>

About: License
  This file is licensed under the GPL.

About: Lens Usage
  Sample usage of this lens in augtool

    * Print all drivers used
      > print /files/@CONFPATH@/ups.conf/*/driver

About: Configuration files
  This lens applies to @CONFPATH@/ups.conf. See <filter>.
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
                 | "maxretry"
                 | "retrydelay"
                 | "pollinterval"
                 | "synchronous"
                 | "user"

let ups_fields   = "driver"
                 | "port"
                 | "sdorder"
                 | "desc"
                 | "nolock"
                 | "ignorelb"
                 | "maxstartdelay"
                 | "synchronous"
@SPECIFIC_DRV_VARS@

let ups_entry    = IniFile.indented_entry (ups_global|ups_fields) ups_sep ups_comment

let ups_title    = IniFile.indented_title IniFile.record_re

let ups_record   = IniFile.record ups_title ups_entry

let ups_lns      = IniFile.lns ups_record ups_comment

let ups_filter   = (incl "@CONFPATH@/ups.conf")
                . Util.stdexcl

let ups_xfm      = transform ups_lns ups_filter
