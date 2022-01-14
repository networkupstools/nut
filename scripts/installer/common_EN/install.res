You must be root to install IPP - Unix.
Unable to determine your system.
Please specify your system like, "\"./install.sh <SYSTEM>\""
Valid systems are SOLINT SOLARI HPUX AIX6
No serial device detected. Press Enter to continue.
This is not a number, please enter a valid number.
Welcome to IPP - Unix!
To install IPP - Unix, press y and then press <ENTER>
and fill in the configuration items as they are presented to you.
NOTE: If you already have IPP - Unix installed and you
do not wish to re-install it, select (N)o to stop
IPP - Unix installation.
         Continue IPP - Unix installation? (y/n) 
Version
The install path name for this software is:
License Agreement:
Do you accept all terms of the preceding license Agreement?
If you do not accept the terms, then the Setup will close.
To install IPP - Unix, you must accept this agreement.
Do you accept all terms of the preceding license Agreement? (y/n) 
Install failed!
IPP - Unix was successfully installed on your system.
WARNING: IPP - Unix was NOT successfully installed on your system.
Installing package.
Removing old package.
Installation error, please see install.log.
===============================================================================
            IPP - Unix configuration - Select installation option:
            IPP - Unix UPS Server Configuration
We will scan your network to find available IPP - Unix UPS Server. Please enter the first IP to scan (e.g. 192.168.0.0) then the last IP to scan (e.g. 192.168.0.255) or nothing if you want to scan only one address.
                Enter first IP to scan :
                Enter last IP to scan :
Scanning...
No IPP - Unix UPS Server available ! Press Enter to continue.
                Please choose the UPS protecting this machine: 
                      0. Go back
            Select UPS Manager Password
The UPS manager password is used to secure the shut-off of UPS.
Please enter the IPP - Unix UPS Server password, this cannot be empty (characters will not appear when typed):
Re-enter the IPP - Unix UPS Server password (characters will not appear):
            Please review your configuration
Managing UPS for local use only
Managing UPS and allow remote clients
Connecting to remote IPP - Unix UPS Servers
Configured UPS devices:
Minimum number of supplies needed:
Early shutdown timeout, minutes (-1 for disabled feature):
UPS Shutoff delay, seconds:
UPS Shutoff and power-cycling enforcement (empty for auto):
Is this correct ? (y/n) 
            How is your UPS connected to this host ?
                      1. Serial Connection (COM port)
                      2. Network adapter
                      0. Go back
                Enter selection 0-2:  
            Verify Detection
Do you want Setup to automatically look for a serially connected UPS?
                      1. Yes
                      2. No
                      0. Go back
                Enter selection 0-2: 
            Choose serial device
                      0. Go back
                Enter selection: 
            Manually choose serial device
Please enter the device name (in the form /dev/name) of the serial port on which the UPS is connected (Make sure "ipp" user and/or "ipp" group have read/write access to the entered /dev file).
                Enter device name (/dev/name):
No serial UPS found on this device. Press Enter to continue.
            Network Configuration
We are scanning your network to find available network UPS.
                Enter first IP to scan :
                Enter last IP to scan :
Please enter the community name of your SNMP device (note that you have to enter a read/write community name to change UPS settings and allow shut-off of the UPS.) 
Scanning...
No network UPS available ! Press Enter to continue.
Please choose the UPS protecting this machine: 
                      0. Go back
Scan for more UPS
Starting IPP - Unix service.
LanSafe installation detected. Proceeding to uninstallation.
Netwatch installation detected. Proceeding to uninstallation.
Older IPP-Unix installation detected. Proceeding to uninstallation with its method script first.
            Shut-off UPS
Do you want your UPS to be shut-off after a system shut-down due to power failure ? (y/n) 
            Enter Power Failure Settings
Shutting down the operating system and programs on this computer takes (in second): 
Shutting down the operating system and programs on this computer takes (in second) - Note that this value may be ignored if you have set-up a greater value on your device: 
Do you want to directly manage an UPS (via serial link or network) or connect to a remote IPP - Unix UPS Server ?
                      1. Direct management
                      2. Connect to remote IPP - Unix UPS Server
                Enter selection: 
            Network connectivity
Do you want this IPP for Unix instance to be reachable over the network as an UPS Server for the power device(s) it manages (i.e. allow IPP - Unix clients to connect to it) ?
                      1. No remote access
                      2. Act as a server for managed UPS
                Enter selection: 
            Currently configured UPS
                      1. Continue with this configuration
                      2. Add another UPS
                      3. Remove an UPS
                      0. Go back
                Enter selection: 
            Minimum supplies required
Give the number of power supplies that must be receiving power to keep this system running (minimum 1): 
                Enter the number of the UPS you wish to remove
No such UPS
            Network device login/password
Enter login for network device:
Enter password for network device (characters will not appear):
Re-enter password for network device (characters will not appear):
Passwords are different, please try again.
            Power Failure Settings
Note that the shutdown duration (the time left for your system to shutdown before the UPS shuts-off) is set on the IPP - Unix UPS Server side. Push Enter key to continue... 
            Automatic or manual network device setup
Do you want to scan your network for available devices or directly enter an IP address ?
                      1. Scan network
                      2. Enter IP
                      0. Go back
                Enter selection: 
            Network device IP
Please enter your network device IP:
Do you want to "enforce" or "forbid" this host to always power-cycle the UPS as part of powerfail shutdown, or should it depend on other setup and run-time situation?
Please enter "enforce" ("e"), "forbid" ("f") or "auto" (empty line); if unsure - default to "auto":
Unexpected string was entered, please try again.
Do you want to enable early shutdown timer ("-1" or empty for usual late shutdown to maximize uptime, 0 or more for minutes (recommended 5+ minutes then) spent ONBATT with less than MINSUPPLIES power sources known protected, before beginning to shut down)?
Please enter a number of minutes for early shutdown timer (either "-1", or 0 or more); if unsure - default to "-1":
