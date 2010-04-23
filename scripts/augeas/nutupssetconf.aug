(*
Module: NutUpssetConf
 Parses /etc/nut/upsset.conf

Author: Raphael Pinson <raphink@gmail.com>
	Frederic Bohe  <fredericbohe@eaton.com>

About: License
  This file is licensed under the GPL.

About: Lens Usage
  Sample usage of this lens in augtool

    * Print the string declaring secured cgi directory:
      > print /files/etc/nut/upsset.conf/auth

About: Configuration files
  This lens applies to /etc/nut/upsset.conf. See <filter>.
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

