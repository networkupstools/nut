(*
Module: Nut
 Parses blah blah

Author: Raphael Pinson <raphink@gmail.com>

About: Reference
 This lens tries to keep as close as possible to `blah blah` where possible.

About: License
  This file is licensed under the GPL.

About: Lens Usage
  Sample usage of this lens in augtool

    * Get the identifier of the devices with a "Clone" option:
      > match "/files/etc/X11/xorg.conf/Device[Option = 'Clone']/Identifier"

About: Configuration files
  This lens applies to blah blah. See <filter>.
*)

module NutUpsmonConf =
  autoload upsmon_xfm


(************************************************************************
 * Group:                 UPSMON.CONF
 *************************************************************************)

(* general *)
let sep_spc  = Util.del_ws_spc
let eol      = Util.eol
let ip       = /[0-9A-Za-z\.:]+/
let num      = /[0-9]+/
let word     = /[^"#; \t\n]+/
let empty    = Util.empty
let comment  = Util.comment
(* let netblock = /[0-9A-Za-z\.:\/]+/ *)
let netblock = word
let path     = word
let quoted_string = del "\"" "\"" . store /[^"\n]+/ . del "\"" "\""

(* UPS identifier
 * <upsname>[@<hostname>[:<port>]] 
 *
 * There might be a cleaner way to write this
 *  but I'm stuck with (hostname | hostname . port)?
 *)
let hostname   = [ label "hostname" . store /[^ \t\n:]+/ ]
let port       = [ label "port"     . store num ]
let identifier = [ label "upsname" . store /[^ \t\n@]+/ ]
                   . ( ( Util.del_str "@" . hostname )
                     | ( Util.del_str "@" . hostname
                         . Util.del_str ":" . port ) )?
(* Variable: quoted_word *)
let word_space  = /"[^"\n]+"/
let quoted_word = /"[^" \t\n]+"/

(* Variable: word_all *)
let word_all = word_space | word | quoted_word

let upsmon_num_re = "DEADTIME"
                  | "FINALDELAY"
                  | "HOSTSYNC"
                  | "MINSUPPLIES"
                  | "NOCOMMWARNTIME"
                  | "POLLFREQ"
                  | "POLLFREQALERT"
                  | "RBWARNTIME"

let upsmon_num    = [ key upsmon_num_re . sep_spc . store num . eol ]


(* upsmon_word includes commands, paths, users *)
let upsmon_word_re = "NOTIFYCMD"
                  | "POWERDOWNFLAG"
                  | "RUN_AS_USER"
                  | "SHUTDOWNCMD"

let upsmon_word   = [ key upsmon_word_re . sep_spc . store word_all . eol ]

(* MONITOR system powervalue username password type *)
let upsmon_monitor = [ key "MONITOR" . sep_spc
                         . [ label "system"     . identifier ] . sep_spc
                         . [ label "powervalue" . store num  ] . sep_spc
                         . [ label "username"   . store word ] . sep_spc
                         . [ label "password"   . store word ] . sep_spc
                         . [ label "type"       . store word ] . eol  ]

let upsmon_notify_type = "ONLINE"
			| "ONBATT"
			| "LOWBATT"
			| "FSD"
			| "COMMOK"
			| "COMMBAD"
			| "SHUTDOWN"
			| "REPLBATT"
			| "NOCOMM"
			| "NOPARENT"

let upsmon_notify = [ key "NOTIFYMSG" . sep_spc
                         . [ label "type" . store upsmon_notify_type . sep_spc ]
                         . [ label "message" . quoted_string ] . eol ]

 let flags = "IGNORE"
		| "SYSLOG"
		| "WALL"
		| "EXEC"

let plus =  [ del /\+*/ "" ]

(*let entries = /IGNORE|SYSLOG|WALL|EXEC+/*)

let record = [ seq "record" . plus . store flags ]

let upsmon_notify_flag = [ counter "record"
			. key "NOTIFYFLAG" . sep_spc
			. [ label "type" . store upsmon_notify_type . sep_spc ]
			. record+ . eol ]

let upsmon_record = upsmon_num|upsmon_word|upsmon_monitor|upsmon_notify|upsmon_notify_flag

let upsmon_lns    = (upsmon_record|comment|empty)*

let upsmon_filter = ( incl "/etc/nut/upsmon.conf" )
			. Util.stdexcl

let upsmon_xfm    = transform upsmon_lns upsmon_filter
