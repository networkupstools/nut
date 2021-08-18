Network UPS Tools Overview
===========================

Description
-----------

Network UPS Tools is a collection of programs which provide a common
interface for monitoring and administering UPS, PDU and SCD hardware.
It uses a layered approach to connect all of the parts.

Drivers are provided for a wide assortment of equipment.  They
understand the specific language of each device and map it back to a
compatibility layer.  This means both an expensive high end UPS, a simple
"power strip" PDU, or any other power device can be handled transparently with
a uniform management interface.

This information is cached by the network server `upsd`, which then
answers queries from the clients.  upsd contains a number of access
control features to limit the abilities of the clients.  Only authorized
hosts may monitor or control your hardware if you wish.  Since the
notion of monitoring over the network is built into the software, you
can hang many systems off one large UPS, and they will all shut down
together. You can also use NUT to power on, off or cycle your data center
nodes, individually or globally through PDU outlets.

Clients such as `upsmon` check on the status of the hardware and do things
when necessary.  The most important task is shutting down the operating
system cleanly before the UPS runs out of power.  Other programs are
also provided to log information regularly, monitor status through your
web browser, and more.


Installing
----------

If you are installing these programs for the first time, go read the
<<_installation_instructions,installation instructions>>
to find out how to do that.  This document contains more information on what all
of this stuff does.


Upgrading
---------

When upgrading from an older version, always check the
<<Upgrading_notes,upgrading notes>> to see what may have
changed.  Compatibility issues and other changes will be listed there to ease
the process.


Configuring and using
---------------------

Once NUT is installed, refer to the
<<Configuration_notes,configuration notes>> for directions.


Documentation
-------------

This is just an overview of the software.  You should read the man pages,
included example configuration files, and auxiliary documentation for the parts
that you intend to use.


Network Information
-------------------

These programs are designed to share information over the network.  In
the examples below, `localhost` is used as the hostname.  This can also
be an IP address or a fully qualified domain name.  You can specify a
port number if your upsd process runs on another port.

In the case of the program `upsc`, to view the variables on the UPS called
sparky on the `upsd` server running on the local machine, you'd do this:

	/usr/local/ups/bin/upsc sparky@localhost

The default port number is 3493.  You can change this with
"configure --with-port" at compile-time.  To make a client talk to upsd
on a specific port, add it after the hostname with a colon, like this:

	/usr/local/ups/bin/upsc sparky@localhost:1234

This is handy when you have a mixed environment and some of the systems
are on different ports.

The general form for UPS identifiers is this:

	<upsname>[@<hostname>[:<port>]]

Keep this in mind when viewing the examples below.


Manifest
--------

This package is broken down into several categories:

- *drivers*	- These programs talk directly to your UPS hardware.
- *server*	- upsd serves data from the drivers to the network.
- *clients*	- They talk to upsd and do things with the status data.
- *cgi-bin*	- Special class of clients that you can use with your web server.
- *scripts*	- Contains various scripts, like the Perl and Python binding,
integration bits and applications. 

Drivers
-------

These programs provide support for specific UPS models.  They understand
the protocols and port specifications which define status information
and convert it to a form that upsd can understand.

To configure drivers, edit ups.conf.  For this example, we'll have a UPS
called "sparky" that uses the apcsmart driver and is connected to
`/dev/ttyS1`.  That's the second serial port on most Linux-based systems.
The entry in `ups.conf` looks like this:

	[sparky]
		driver = apcsmart
		port = /dev/ttyS1

To start and stop drivers, use upsdrvctl of upsdrvsvcctl (installed on
operating systems with a service management framework supported by NUT).
By default, it will start or stop every UPS in the config file:

	/usr/local/ups/sbin/upsdrvctl start
	/usr/local/ups/sbin/upsdrvctl stop

However, you can also just start or stop one by adding its name:

	/usr/local/ups/sbin/upsdrvctl start sparky
	/usr/local/ups/sbin/upsdrvctl stop sparky

On operating systems with a supported service management framework,
you might wrap your NUT drivers into individual services instances
with:

	/usr/local/ups/sbin/upsdrvsvcctl resync

and then manage those service instances with commands like:

	/usr/local/ups/sbin/upsdrvsvcctl start sparky
	/usr/local/ups/sbin/upsdrvsvcctl stop sparky

To find the driver name for your device, refer to the section below
called "HARDWARE SUPPORT TABLE".

Extra Settings
~~~~~~~~~~~~~~

Some drivers may require additional settings to properly communicate
with your hardware.  If it doesn't detect your UPS by default, check the
driver's man page or help (-h) to see which options are available.

For example, the usbhid-ups driver allows you to use USB serial numbers to
distinguish between units via the "serial" configuration option.  To use this
feature, just add another line to your ups.conf section for that UPS:

	[sparky]
		driver = usbhid-ups
		port = auto
		serial = 1234567890

Hardware Compatibility List
~~~~~~~~~~~~~~~~~~~~~~~~~~~

The <<HCL,Hardware Compatibility List>> is available in the source directory
('nut-X.Y.Z/data/driver.list'), and is generally distributed with packages.
For example, it is available on Debian systems as:

	/usr/share/nut/driver.list

This table is also available link:http://www.networkupstools.org/stable-hcl.html[online].


If your driver has vanished, see the link:FAQ.html[FAQ] and
<<Upgrading_notes,Upgrading notes>>.

Generic Device Drivers
~~~~~~~~~~~~~~~~~~~~~~

NUT provides several generic drivers that support a variety of very similar models.

- The `genericups` driver supports many serial models that use the same basic
principle to communicate with the computer.  This is known as "contact
closure", and basically involves raising or lowering signals to indicate
power status.
+
This type of UPS tends to be cheaper, and only provides the very simplest
data about power and battery status.  Advanced features like battery
charge readings and such require a "smart" UPS and a driver which
supports it.
+
See the linkman:genericups[8] man page for more information.

- The `usbhid-ups` driver attempts to communicate with USB HID Power Device
Class (PDC) UPSes. These units generally implement the same basic protocol,
with minor variations in the exact set of supported attributes. This driver
also applies several correction factors when the UPS firmware reports values
with incorrect scale factors.
+
See the linkman:usbhid-ups[8] man page for more information.

- The `blazer_ser` and `blazer_usb` drivers supports the Megatec / Q1
protocol that is used in many brands (Blazer, Energy Sistem, Fenton
Technologies, Mustek and many others).
+
See the linkman:blazer[8] man page for more information.

- The `snmp-ups` driver handles various SNMP enabled devices, from many
different manufacturers. In SNMP terms, `snmp-ups` is a manager, that
monitors SNMP agents.
+
See the linkman:snmp-ups[8] man page for more information.

- The `powerman-pdu` is a bridge to the PowerMan daemon, thus handling all
PowerMan supported devices. The PowerMan project supports several serial
and networked PDU, along with Blade and IPMI enabled servers.
+
See the linkman:powerman-pdu[8] man page for more
information.

- The `apcupsd-ups` driver is a bridge to the Apcupsd daemon, thus handling
all Apcupsd supported devices. The Apcupsd project supports many serial,
USB and networked APC UPS.
+
See the linkman:apcupsd-ups[8] man page for more information.

UPS Shutdowns
~~~~~~~~~~~~~

upsdrvctl can also shut down (power down) all of your UPS hardware.

WARNING: if you play around with this command, expect your filesystems
to die.  Don't power off your computers unless they're ready for it:

	/usr/local/ups/sbin/upsdrvctl shutdown
	/usr/local/ups/sbin/upsdrvctl shutdown sparky

You should read the <<UPS_shutdown,Configuring automatic UPS shutdowns>>
chapter to learn more about when to use this feature.  If called at the wrong
time, you may cause data loss by turning off a system with a filesystem
mounted read-write.

Power distribution unit management
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

NUT also provides an advanced support for power distribution units.

You should read the <<outlet_management,NUT outlets management and PDU notes>>
chapter to learn more about when to use this feature. 

Network Server
--------------

`upsd` is responsible for passing data from the drivers to the client
programs via the network.  It should be run immediately after `upsdrvctl`
in your system's startup scripts.

`upsd` should be kept running whenever possible, as it is the only source
of status information for the monitoring clients like `upsmon`.


Monitoring client
-----------------

`upsmon` provides the essential feature that you expect to find in UPS
monitoring software: safe shutdowns when the power fails.

In the layered scheme of NUT software, it is a client.  It has this
separate section in the documentation since it is so important.

You configure it by telling it about UPSes that you want to monitor in
upsmon.conf.  Each UPS can be defined as one of two possible types:

Master
~~~~~~

This UPS supplies power to the system running `upsmon`, and this system is also
responsible for shutting it down when the battery is depleted.  This occurs
after any slave systems have disconnected safely.

If your UPS is plugged directly into a system's serial port, the `upsmon`
process on that system should define that UPS as a master.

For a typical home user, there's one computer connected to one UPS.
That means you run a driver, `upsd`, and `upsmon` in master mode.

Slave
~~~~~

This UPS may supply power to the system running `upsmon`, but this system can't
shut it down directly.

Use this mode when you run multiple computers on the same UPS.  Obviously, only
one can be connected to the serial port on the UPS, and that system is the
master.  Everything else is a slave.

For a typical home user, there's one computer connected to one UPS.
That means you run a driver, upsd, and upsmon in master mode.

Additional Information
~~~~~~~~~~~~~~~~~~~~~~

More information on configuring upsmon can be found in these places:

- The linkman:upsmon[8] man page
- <<BigServers,Typical setups for big servers>>
- <<UPS_shutdown,Configuring automatic UPS shutdowns>> chapter
- The stock `upsmon.conf` that comes with the package


Clients
-------

Clients talk to upsd over the network and do useful things with the data
from the drivers.  There are tools for command line access, and a few
special clients which can be run through your web server as CGI
programs.

For more details on specific programs, refer to their man pages.

upsc
~~~~

`upsc` is a simple client that will display the values of variables known
to `upsd` and your UPS drivers.  It will list every variable by default,
or just one if you specify an additional argument.  This can be useful
in shell scripts for monitoring something without writing your own
network code.

`upsc` is a quick way to find out if your driver(s) and upsd are working
together properly.  Just run `upsc <ups>` to see what's going on, i.e.:

	morbo:~$ upsc sparky@localhost
	ambient.humidity: 035.6
	ambient.humidity.alarm.maximum: NO,NO
	ambient.humidity.alarm.minimum: NO,NO
	ambient.temperature: 25.14
	...

If you are interested in writing a simple client that monitors `upsd`,
the source code for `upsc` is a good way to learn about using the
upsclient functions.

See the linkman:upsc[8] man page and
<<nut-names,NUT command and variable naming scheme>> for more information.

upslog
~~~~~~

`upslog` will write status information from `upsd` to a file at set
intervals.  You can use this to generate graphs or reports with other
programs such as `gnuplot`.

upsrw
~~~~~

`upsrw` allows you to display and change the read/write variables in your
UPS hardware.  Not all devices or drivers implement this, so this may
not have any effect on your system.

A driver that supports read/write variables will give results like this:

	$ upsrw sparky@localhost

	( many skipped )

	[ups.test.interval]
	Interval between self tests
	Type: ENUM
	Option: "1209600"
	Option: "604800" SELECTED
	Option: "0"

	( more skipped )

On the other hand, one that doesn't support them won't print anything:

	$ upsrw fenton@gearbox

	( nothing )

`upsrw` requires administrator powers to change settings in the hardware.
Refer to linkman:upsd.users[5] for information on defining
users in `upsd`.

upscmd
~~~~~~

Some UPS hardware and drivers support the notion of an instant command -
a feature such as starting a battery test, or powering off the load.
You can use upscmd to list or invoke instant commands if your
hardware/drivers support them.

Use the -l command to list them, like this:

	$ upscmd -l sparky@localhost
	Instant commands supported on UPS [sparky@localhost]:

	load.on - Turn on the load immediately
	test.panel.start - Start testing the UPS panel
	calibrate.start - Start run time calibration
	calibrate.stop - Stop run time calibration
	...

`upscmd` requires administrator powers to start instant commands.
To define users and passwords in `upsd`, see
linkman:upsd.users[5].


CGI Programs
------------

The CGI programs are clients that run through your web server.  They
allow you to see UPS status and perform certain administrative commands
from any web browser.  Javascript and cookies are not required.

These programs are not installed or compiled by default.  To compile
and install them, first run `configure --with-cgi`, then do `make` and
`make install`.  If you receive errors about "gd" during configure, go
get it and install it before continuing.

You can get the source here:

	http://www.libgd.org/

In the event that you need libpng or zlib in order to compile gd,
they can be found at these URLs:

	http://www.libpng.org/pub/png/pngcode.html

	http://www.gzip.org/zlib/


Access Restrictions
~~~~~~~~~~~~~~~~~~~

The CGI programs use hosts.conf to see if they are allowed to talk to a
host.  This keeps malicious visitors from creating queries from your web
server to random hosts on the Internet.

If you get error messages that say "Access to that host is not
authorized", you're probably missing an entry in your hosts.conf.

upsstats
~~~~~~~~

`upsstats` generates web pages from HTML templates, and plugs in status
information in the right places.  It looks like a distant relative of
APC's old Powerchute interface.  You can use it to monitor several
systems or just focus on one.

It also can generate IMG references to `upsimage`.

upsimage
~~~~~~~~

This is usually called by upsstats via IMG SRC tags to draw either the
utility or outgoing voltage, battery charge percent, or load percent.

upsset
~~~~~~

`upsset` provides several useful administration functions through a web
interface.  You can use `upsset` to kick off instant commands on your UPS
hardware like running a battery test.  You can also use it to change
variables in your UPS that accept user-specified values.

Essentially, `upsset` provides the functions of `upsrw` and `upscmd`, but
with a happy pointy-clicky interface.

`upsset` will not run until you convince it that you have secured your
system.  You *must* secure your CGI path so that random interlopers
can't run this program remotely.  See the `upsset.conf` file.  Once you
have secured the directory, you can enable this program in that
configuration file.  It is not active by default.


Version Numbering
-----------------

The version numbers work like this: if the middle number is odd, it's a
development tree, otherwise it is the stable tree.

The past stable trees were 1.0, 1.2, 1.4, 2.0, 2.2 and 2.4, with the
latest stable tree designated 2.6.  The development trees were 1.1, 1.3,
1.5, 2.1 and 2.3.  As of the 2.4 release, there is no real development
branch anymore since the code is available through a revision control
system (namely Subversion) and snapshots.

Major release jumps are mostly due to large changes to the features
list.  There have also been a number of architectural changes which
may not be noticeable to most users, but which can impact developers.


Backwards and Forwards Compatibility
------------------------------------

The old network code spans a range from about 0.41.1 when TCP support 
was introduced up to the recent 1.4 series.  It used variable names
like STATUS, UTILITY, and LOADPCT.  Many of these names go back to the
earliest prototypes of this software from 1997.  At that point there
was no way to know that so many drivers would come along and introduce 
so many new variables and commands.  The resulting mess grew out of
control over the years.

During the 1.3 development cycle, all variables and instant commands
were renamed to fit into a tree-like structure.  There are major groups,
like input, output and battery.  Members of those groups have been
arranged to make sense - input.voltage and output.voltage compliment
each other.  The old names were UTILITY and OUTVOLT.  The benefits in
this change are obvious.

The 1.4 clients can talk to either type of server, and can handle either
naming scheme.  1.4 servers have a compatibility mode where they can
answer queries for both names, even though the drivers are internally
using the new format.

When 1.4 clients talk to 1.4 or 2.0 (or more recent) servers, they will
use the new names.

Here's a table to make it easier to visualize:

[options="header"]
|=============================================
|                   4+| Server version
| *Client version*    | 1.0 | 1.2 | 1.4 | 2.0+
| 1.0                 | yes | yes | yes | no
| 1.2                 | yes | yes | yes | no
| 1.4                 | yes | yes | yes | yes
| 2.0+                | no  | no  | yes | yes
|=============================================

Version 2.0, and more recent, do not contain backwards compatibility for
the old protocol and variable/command names.  As a result, 2.0 clients can't 
talk to anything older than a 1.4 server.  If you ask a 2.0 client to 
fetch "STATUS", it will fail.  You'll have to ask for "ups.status" 
instead.

Authors of separate monitoring programs should have used the 1.4 series
to write support for the new variables and command names.  Client
software can easily support both versions as long as they like.  If upsd
returns 'ERR UNKNOWN-COMMAND' to a GET request, you need to use REQ.


Support / Help / etc.
---------------------

If you are in need of help, refer to the
<<Support_Request,Support instructions>> in the user manual.


Hacking / Development Info
--------------------------

Additional documentation can be found in:

- the linkdoc:developer-guide[Developer Guide],
- the linkdoc:packager-guide[Packager Guide].


Acknowledgements / Contributions
--------------------------------

The many people who have participated in creating and improving NUT are
listed in the user manual <<Acknowledgements,acknowledgements appendix>>.
