= README for the ipp-unix-1.40-4_AIX_Clusterware hotfix edition

== General information

This delivery contains a customized variant of ipp-unix-1.40-4 with the
same basic binary software and enhanced scripts and configuration which
enable to configure "early shutdown" functionality for chosen AIX hosts.
This allows selected systems to power themselves off after a certain
time that less than `$MINSUPPLIES` power sources are protected by UPSes 
that are fully "ONLINE".

In order for this to happen, a new configuration variable was introduced
in `ipp.conf` file, the `SHUTDOWN_TIMER` which allows to specify the
number of minutes that the power protection is insufficient, after which
the irreversible shutdown of this host begins. Default value for this
variable is `-1` which retains the standard behavior of staying up as
long as possible, and shutting down only when the UPS sends an alert
that too little battery runtime remains (as configured by default or
customized by `shutdown_duration` option for the `netxml-ups` driver).

Another added option is `POWERDOWNFLAG_USER` which may be pre-set to
`enforce` or `forbid` to enable or disable UPS power-cycling at the
end of the shutdown procedure. Generally it does not need to be pre-set
in the `ipp.conf` file as the script relies on the `POWERDOWNFLAG` file
managed by `upsmon` (this in turn depends on whether `upsmon` triggered
the shutdown due to an alarm, such as low-battery condition, or just
the delayed early shutdown was used as scheduled by "notification").
However it is possible to configure a specific behavior on this host,
e.g. to be sure to avoid power-cycling when early shutdown hosts are
shutting down due to whatever powerfail-driven reason.

These options should be configured by an administrator of the particular
host in the `/usr/local/ups/etc/ipp.conf` file.

When UPS events are processed by `upsmon` with its configured `NOTIFYCMD`
(the packaged `ipp-notifier.sh` script, enhanced for this delivery) it
has an ability to detect how many UPSes are `ONLINE` and how many are
required by the `MINSUPPLIES` setting. UPSes whose state is currently
`unknown` are not considered, in order to avoid erroneous shutdowns
while the communications are just starting up. If the protection is
deemed insufficient, the new `ipp-os-shutdown` script is launched,
which can invoke a custom `AIX_Clusterware_shutdown` for host-specific procedures
(such as clusterware shutdown).

This script has several roles:
* it manages the delayed shutdown (which can be canceled before the timer
expires, and can not be aborted after the timer expires - so it proceeds
to the end)
* it wraps the custom logic for the AIX cluster-ware shutdown and
a long sleep for it to complete, if its `clstop` script is detected
* the same script (with an option of zero delay) is configured as the
`SHUTDOWNCMD` in `upsmon.conf` so the same logic is executed in all
the different supported shutdown scenarios.

It is recommended to configure certain `SHUTDOWN_TIMER` values for the
hosts which should shut down early and leave more battery runtime power
remaining for the more important hosts. Note that if the external power
returns after the early-shutdown hosts have powered off, they will stay
down until an administrator boots them. However, if the external power
becomes sufficient again during the shutdown procedure (checked between
cluster-ware shutdown and the OS shutdown steps) then a reboot of the
host is requested instead of a power-off.

It is recommended to configure `SHUTDOWN_TIMER=-1` (default) on those
more important hosts which should stay up as long as possible and only
shut down if all required UPSes have posted a low-battery status or
forced-shutdown command. These hosts would by default schedule delayed
UPS powercycling. To be on the safe side, sufficient `shutdown_duration`
seconds should be configured in their `netxml-ups` driver blocks in the
`/usr/local/ups/etc/ups.conf` file on the host.

If the customer elects to use the early shutdown strategy for all hosts,
the `POWERDOWNFLAG_USER=enforce` should be configured in the hosts with
the highest `SHUTDOWN_TIMER` value so they would cause UPS powercycling.
Do not forget to define sufficient `DELAY` value for the OS shutdown to
complete before the UPS turns itself off. The UPS would turn on the load
automatically after some time, when external power is back and it has
charged the battery sufficiently (configurable in the Network Management
web-interface for the UPS).

== Installation

Since the package already includes modified scripts and files to suit
AIX_Clusterware requirements, the installation is simplified.

* Install the package as usual

- Uncompress the `ipp-aix-1.40-4_AIX_Clusterware.powerpc.tar.gz` archive

- Change into the resulting `ipp-aix-1.40-4_AIX_Clusterware.powerpc` subdirectory

- Launch the `install.sh` script and follow the installer instructions

* Configure the early shutdown timer

You can later change the early shutdown timer, for the cluster nodes,
by editing `/usr/local/ups/etc/ipp.conf`, and set `SHUTDOWN_TIMER` to
a suitable value (in minutes).

* Configure clusterware shutdown command

You can configure the variable `SHUTDOWNSCRIPT_CUSTOM` in `ipp.conf` to
point at a custom complementary shutdown script with a shutdown routine
required by this particular host, which will be called by the master
powerfail shutdown script (`/usr/local/ups/sbin/ipp-os-shutdown`).
This variable is set by default to `/usr/local/ups/sbin/AIX_Clusterware_shutdown`
for this delivery.

* Configure operating system shutdown command and options

Optionally, modify the operating system shutdown command and type.
By default, operating system shutdown uses:

- the `/usr/sbin/shutdown` command on AIX systems,
- `-p` (poweroff) for standard hosts,
- `-p -F` (poweroff, fast) for cluster hosts.

To modify the shutdown command or specific option for the various types
of shutdown), edit the file `/usr/local/ups/etc/ipp-os-shutdown.conf`
and set or adapt the variables:

- `CMD_SHUTDOWN` to point at the shutdown command. This may include
the basic mandatory option options, such as the non-interactive flag
(`-y`) on some OS such as HP-UX,
- `SDFLAG_*` to point at the right option for poweroff, reboot or halt.

To modify the default shutdown option to halt or reboot, edit the file
`/usr/local/ups/etc/ipp.conf` and set `SDFLAG_POWERSTATE_DEFAULT` to
either `$SDFLAG_HALT` (halt) or `$SDFLAG_REBOOT` (reboot).

* (optionally) Configure UPS power-cycling

If the customer elects to use the early shutdown strategy for all hosts,
the `POWERDOWNFLAG_USER=enforce` should be configured in the `ipp.conf`
file on hosts with the highest `SHUTDOWN_TIMER` value so they would cause
UPS powercycling explicitly. By default, it may be enabled or forbidden
depending on the cause of shutdown.

Make sure to define a sufficient `DELAY` value (in seconds) as well, for
the OS shutdown to complete safely before the UPS(es) power is cut.


== Testing

The following points about this delivery were verified on AIX 7.1.
It was tested:

- to be installable (and runnable via init-script)

- to work as a `SHUTDOWNCMD` handler, including interaction with the
killpower flag-file maintained by `upsmon` (with the default setting
of `SHUTDOWN_TIMER=-1` in the `ipp.conf` file)

- to report progress of the powerfail shutdown onto the system consoles
(with `wall`) and into the `syslog`

- to execute early shutdown when `SHUTDOWN_TIMER=2` (after 2 minutes
ONBATT) is defined manually in the `ipp.conf` file

- to cancel shutdown if power returns back before timeout expires

- to not cancel shutdown if power returns back after timeout expires
and shutdown routine has started (and reported to be irreversible)

- to report that the shutdown is in irreversible stage, as reaction
to CTRL+C during console-run invokations like `ipp-os-shutdown -t now`

- to execute `clstop+sleep` if the clusterware scripts are found,
and to skip them if they are not available

- to power-off the host (LPAR) if power remains lost at the moment
when we are about to proceed to `/sbin/shutdown`

- to reboot the host if power is already back when we are about to
proceed to `/sbin/shutdown`

- to implement numerous fixes and improvements in the `install.sh`
script, including integration of new settings for early shutdown
and UPS powercycling strategy

=== A few important notes helpful during testing

* currently running IPP - Unix processes, UPS states and the pending
shutdown status can be queried with the following command:

----
:; ps -ef | grep -v grep | egrep 'ipp|ups|nut|shut|sleep' ; \
   ( ls -la /usr/local/ups/etc/killpower ) 2>/dev/null ; \
   /usr/local/ups/bin/ipp-status; \
   /usr/local/ups/sbin/ipp-os-shutdown -s; date
----

* a pending shutdown that is not yet irreversible can be aborted
manually with:

----
:; /usr/local/ups/sbin/ipp-os-shutdown -c
----

* the administrator can create a special file to abort the script
just before proceeding to irreversible shutdown; this is automated
in the `ipp-os-shutdown` script (undocumented option):

----
:; /usr/local/ups/sbin/ipp-os-shutdown block
----

Do not forget to remove this file when testing is completed to
allow actual shutdowns to happen:

----
:; /usr/local/ups/sbin/ipp-os-shutdown unblock
----

* also note that if the host is booted with an administrative
action while the remaining UPS battery runtime is under the
threshold set with `shutdown_duration`, an emergency powerfail
can be triggered by the `netxml-ups` driver as soon as IPP - Unix
services are initialized, even if the battery state is "CHARGING".
To avoid such shutdowns, an administrator can log in and quickly
create the special file described above (temporarily). Recommended
procedure is to wait for the hosts to boot up in due time, when
the batteries are charged enough to survive another power failure.

== Disclaimer of future backwards-compatibility

Please note that this delivery was prepared outside the normal product
development lifecycle and that this is a hotfix for a specific customer's
usecase, and that the possible future releases of IPP - Unix may be not
directly compatible with it if/when the feature is backported (e.g. some
variables and files may be named differently, even if similar concepts 
are adopted by the main line of development in the future).
