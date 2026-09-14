(* Tests below pass on some systems and/or augeas versions and fail on others.
 * Sometimes it involves same formal release with different packaging.
 * For peace of mind, these are not currently added to common "test_nut"
 * which is expected to pass everywhere.
 *)

module Test_nut_flaky =

(* desc is a single token made of two words below
 * encountering the "default..." line seems troublesome
 * for some versions of augtools regardless of line position
 *)
let ups_conf1 = "
[testups]
	driver = dummy-ups
	port = auto
	desc = \"Dummy UPS Driver\"
	default.battery.voltage.high = 28.8
"

test NutUpsConf.ups_lns get ups_conf1 =
	{ }
	{ "testups"
		{ "driver" = "dummy-ups"   }
		{ "port"   = "auto" }
		{ "desc"   = "Dummy UPS Driver"    }
		{ "default.battery.voltage.high" = "28.8" } }
