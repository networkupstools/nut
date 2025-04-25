#!/bin/sh
# Script to detect and uninstall Lansafe / Netwatch

# AQU notes:
# - default install path: /usr/Powerware/LanSafe (for all systems)
# - may be expanded to PSP 

OS_NAME="`uname -s`"

detect_psp()
{
	case $OS_NAME in
		HP-UX)
			PSP_INITSCRIPT_FN="/sbin/init.d/ls.init"
			;;
		AIX)
			PSP_INITSCRIPT_FN="`grep psp /etc/inittab 2>/dev/null | cut -d':' -f4 | cut -d' ' -f1`"
			;;
		SunOS)
			PSP_INITSCRIPT_FN="/etc/init.d/ls.init"
			;;
		*)
			#echo "$OS_NAME is not managed!"
			return 1
			;;
	esac
}

detect_lansafe()
{
	case $OS_NAME in
		HP-UX)
			LS_INITSCRIPT_FN="/sbin/init.d/ls.init"
			;;
		AIX)
			LS_INITSCRIPT_FN="`grep LanSafe /etc/inittab 2>/dev/null | cut -d':' -f4 | sed 's/&//'`"
			;;
		SunOS)
			LS_INITSCRIPT_FN="/etc/init.d/ls.init"
			;;
		*)
			#echo "$OS_NAME is not managed!"
			return 1
			;;
	esac

	if [ -n "$LS_INITSCRIPT_FN" -a -f "$LS_INITSCRIPT_FN" ] ; then

			LS_INSTPATH="`grep instpath $LS_INITSCRIPT_FN | head -n1 | cut -d'=' -f2`"
			if [ -z "$LS_INSTPATH" ] ; then
				return 1
			#else
				#echo "LanSafe installation detected."
			fi
	else
		return 1
	fi

	# Lansafe is installed
	return 0
}

uninstall_lansafe()
{
	#echo "LanSafe installation detected. Proceeding to uninstallation."
	#echo "Calling $LS_INSTPATH/uninstall.sh"

	# Actual Lansafe uninstallation
	echo "y" | $LS_INSTPATH/uninstall.sh 2>&1 1> install.log
}


detect_netwatch()
{
	case $OS_NAME in
		HP-UX)
			NW_INITSCRIPT_FN="/sbin/init.d/netwatch.init"
			;;
		AIX)
			NW_INITSCRIPT_FN="`grep netwatch /etc/inittab 2>/dev/null | cut -d':' -f4 | cut -d' ' -f1`"
			;;
		SunOS)
			NW_INITSCRIPT_FN="/etc/init.d/netwatch.init"
			;;
		*)
			#echo "$OS_NAME is not managed!"
			return 1
			;;
	esac

	if [ -n "$NW_INITSCRIPT_FN" -a -f "$NW_INITSCRIPT_FN" ] ; then

		    NW_INSTPATH="`grep INSTALL_PATH $NW_INITSCRIPT_FN | head -n1 | cut -d'=' -f2 | sed 's/\"//g'`"
		    if [ -z "$NW_INSTPATH" ] ; then
				return 1
			#else
				#echo "Netwatch installation detected."
			fi
	else
		return 1
	fi

	# Netwatch is installed
	return 0
}

uninstall_netwatch()
{
	#echo "Netwatch installation detected. Proceeding to uninstallation."
	#echo "Calling $NW_INSTPATH/uninstall.sh"

	# Actual Netwatch uninstallation
	$NW_INSTPATH/uninstall.sh silent 2>&1 1> install.log

	# Workaround for buggy uninstall of Netwatch
	case $OS_NAME in
		HP-UX)
			rm -f /sbin/*.d/*netwatch*
			;;
		AIX)
			cat /etc/inittab | grep -v netwatch > /etc/inittab.upp
			mv /etc/inittab.upp /etc/inittab
			;;
		SunOS)
			rm -f /etc/*.d/*netwatch*
			;;
	esac
}

# Main entry point
# Check if Lansafe is installed
detect_lansafe
if [ $? -eq 0 ]; then
	# Proceed to uninstallation
	uninstall_lansafe
fi

# Check if Netwatch is installed
detect_netwatch
if [ $? -eq 0 ]; then
	# Proceed to uninstallation
	uninstall_netwatch
fi

#exit $RETVAL

