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

module NutUpsschedConf =
  autoload upssched_xfm


(************************************************************************
 * Group:                 UPSSCHED.CONF
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

(* Variable: quoted_word *)
let word_space  = /"[^"\n]+"/
let quoted_word = /"[^" \t\n]+"/

(* Variable: word_all *)
let word_all = word_space | word | quoted_word

let upssched_re = "CMDSCRIPT"
                  | "PIPEFN"
                  | "LOCKFN"

let upssched_opt    = [ key upssched_re . sep_spc . store word_all . eol ]

let upssched_start_timer = [ key "START-TIMER" . sep_spc 
                         . [ label "timername" . store word ] . sep_spc
                         . [ label "interval"  . store num ] ]


let upssched_cancel_timer = [ key "CANCEL-TIMER" . sep_spc
                         . [ label "timername" . store word ]
			 . ( sep_spc . [ label "cmd" . store word_all ])* ]

let upssched_execute_timer = [ key "EXECUTE" . sep_spc
                         . [ label "command"  . store word_all ] ]


let upssched_command = (upssched_start_timer|upssched_cancel_timer|upssched_execute_timer)

let upssched_at   = [ key "AT" . sep_spc 
			. [ label "notifytype" . store word ] . sep_spc 
			. [ label "upsname" . store word ] . sep_spc 
			. upssched_command . eol ]

let upssched_lns    = (upssched_at|upssched_opt|comment|empty)*

let upssched_filter = ( incl "/etc/nut/upssched.conf" )
			. Util.stdexcl

let upssched_xfm    = transform upssched_lns upssched_filter
