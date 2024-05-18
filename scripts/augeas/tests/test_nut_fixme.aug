(* Tests for the Nut module *)
(* FIXME: More development is needed in NUT lens definitions for corner cases
 * e.g. with multiple quote characters in a line, whether escaped or not.
 * :; ./gen-nutupsconf-aug.py ; make ; augparse --trace -I ./ ./tests/test_nut_fixme.aug && echo PASSED
 *)

module Test_nut_fixme =

(* desc is a single token made of two words surrounded by quotes
 * as part of its content below (escaped by backslashes - literally
 * a "backslash-doublequote-Dummy-doublequote-backslash" spelling)
 *)
let ups_conf3 = "
[testups]
	driver = dummy-ups
	port = auto
	desc = \"A \\\"Dummy\\\" UPS\"
"

test NutUpsConf.ups_lns get ups_conf3 =
	{ }
	{ "testups"
		{ "driver" = "dummy-ups"   }
		{ "port"   = "auto" }
		{ "desc"   = "A \\\"Dummy\\\" UPS"    } }



let upsd_conf = "
MAXAGE 30
TRACKINGDELAY 600
ALLOW_NO_DEVICE 1
LISTEN 0.0.0.0 3493
MAXCONN 1024
CERTIDENT \"My Server Cert\" \"Db Pass Phr@se\"
"

test NutUpsdConf.upsd_lns get upsd_conf = 
	{ }
	{ "MAXAGE"      = "30" }
	{ "TRACKINGDELAY" = "600" }
	{ "ALLOW_NO_DEVICE" = "1" }
	{ "LISTEN"
		{ "interface" = "0.0.0.0" }
		{ "port"     = "3493"     } }
	{ "MAXCONN"      = "1024" }
	{ "CERTIDENT"
		{ "certname"   = "My Server Cert" }
		{ "dbpass"     = "Db Pass Phr@se" } }

