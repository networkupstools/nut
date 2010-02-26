(*
Module: NutUpsdConf
 Parses /etc/nut/upsd.conf

Author: Raphael Pinson <raphink@gmail.com>
        Frederic Bohe  <fredericbohe@eaton.com>

About: License
  This file is licensed under the GPL.

About: Lens Usage
  Sample usage of this lens in augtool

    * Print all network interface upsd will listen to
      > print /files/etc/nut/upsd.conf/LISTEN

About: Configuration files
  This lens applies to /etc/nut/upsd.conf. See <filter>.
*)

module NutUpsdConf =
  autoload upsd_xfm

(************************************************************************
 * Group:                 UPSD.CONF
 *************************************************************************)

(* general *)
let sep_spc  = Util.del_ws_spc
let opt_spc  = Util.del_opt_ws ""
let eol      = Util.eol
let ip       = /[0-9A-Za-z\.:]+/
let num      = /[0-9]+/
let word     = /[^"#; \t\n]+/
let empty    = Util.empty
let comment  = Util.comment
(* let netblock = /[0-9A-Za-z\.:\/]+/ *)
let netblock = word
let path     = word

let upsd_acl       = [ opt_spc . key "ACL" . sep_spc
                          . [ label "name" . store word ]
                          . sep_spc
                          . [ label "netblock" . store word ] . eol  ]
let upsd_aclname   = [ label "aclname" . store word ]
let upsd_accept    = [ opt_spc . key /ACCEPT|REJECT/ . sep_spc
                          . upsd_aclname
                          . ( sep_spc . upsd_aclname )* . eol ]

let upsd_access    = upsd_acl | upsd_accept

let upsd_maxage    = [ opt_spc . key "MAXAGE"    . sep_spc . store num  . eol ]
let upsd_statepath = [ opt_spc . key "STATEPATH" . sep_spc . store path . eol ]
let upsd_listen    = [ opt_spc . key "LISTEN"    . sep_spc 
                          . [ label "interface" . store ip ]
                          . [ sep_spc . label "port" . store num]? ]
let upsd_maxconn   = [ opt_spc . key "MAXCONN"    . sep_spc . store num  . eol ]
let upsd_listen_list = upsd_listen . eol 

(************************************************************************
 * MAXAGE seconds
 * STATEPATH path
 * LISTEN interface port
 *    Multiple LISTEN addresses may be specified. The default is to bind to 0.0.0.0 if no LISTEN addresses are specified.
 *    LISTEN 127.0.0.1 LISTEN 192.168.50.1 LISTEN ::1 LISTEN 2001:0db8:1234:08d3:1319:8a2e:0370:7344
 *
 *************************************************************************)
let upsd_other  =  upsd_maxage | upsd_statepath | upsd_listen_list | upsd_maxconn

let upsd_lns    = (upsd_access|upsd_other|comment|empty)*

let upsd_filter = (incl "/etc/nut/upsd.conf")
		. Util.stdexcl

let upsd_xfm    = transform upsd_lns upsd_filter
