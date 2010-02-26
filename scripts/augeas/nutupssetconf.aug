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

module NutUpssetConf =
  autoload upsset_xfm


(************************************************************************
 * Group:                 UPSSET.CONF
 *************************************************************************)

(* general *)
let sep_spc  = Util.del_opt_ws ""
let eol      = Util.eol
let comment  = Util.comment
let empty    = Util.empty


let upsset_key_word = "I_HAVE_SECURED_MY_CGI_DIRECTORY"

let upsset_key = [ label "auth" . sep_spc . store upsset_key_word . eol ]

let upsset_lns  = (upsset_key|comment|empty)*

let upsset_filter = ( incl "/etc/nut/upsset.conf" )
                        . Util.stdexcl

let upsset_xfm    = transform upsset_lns upsset_filter

