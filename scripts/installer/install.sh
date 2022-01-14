#!/bin/sh
#	install.sh
#	Copyright (c) 2013, by Eaton (R) Corporation. All rights reserved.
#	A shell script which installs IPP - Unix
#	It installs the native package then runs NUT configuration tools
#	to create an initial configuration and finally run the service.
. ./version.sh

PATH=$PATH:/bin:/usr/bin; export PATH


NUT_PACKAGE_SOLARI="NUT_solaris_sparc_package2.6.5.local"
NUT_PACKAGE_SOLINT="NUT_solaris_i386_package2.6.5.local"

COMMON_DIR="common"
LOG_FILE=install.log

#configuration data
C_MODE="standalone"
C_HOSTNAME="localhost"
C_DEVICE=""
C_OPTION=""
C_NUM_DEV=0
C_PASSWORD=""
C_SHUTDOWN_DURATION="120"
C_MINSUPPLIES=1
C_COMMUNITY="public"
C_NUM_NETWORK_DEVICE=0

FIRST_IP=""
LAST_IP=""

ADMIN_FILE="/tmp/ipp_admin_file"

cd `dirname $0`

. $COMMON_DIR/string.sh

get_parameters() {
	case $1 in
	English)
		lang=EN
		;;
#	Japanese)
#		lang=JP
#		;;
	SOLARI)
		SYSTEM=$1
		;;
	SOLINT)
		SYSTEM=$1
		;;
	HPUX)
		SYSTEM=$1
		;;
	AIX)
		SYSTEM=$1
		;;
	esac
	
}

check_locale_language() {
	set `locale` >/dev/null 2>&1
	case $1 in
	LANG=en_US)
		lang=EN
		;;
	LANG=C)
		lang=EN
		;;
	esac
}

compute_SYSTEM () {
	set `uname -a`
	case $1 in
	SunOS)
		case $3 in
		5.*)
			case $5 in
			i86pc)
				SYSTEM=SOLINT
				;;
			*)
				SYSTEM=SOLARI
				;;
			esac
		;;
		*)
			SYSTEM="Unknown System"
			;;
		esac
		;;
	HP-UX)
		SYSTEM=HPUX
		;;
	AIX)
		case $4 in
		6)
			SYSTEM=AIX
			;;
		7)
			SYSTEM=AIX
			;;
		*)	
			SYSTEM="This AIX version is not supported"
		;;
		esac
		;;
	*)	
		SYSTEM="Unknown System"
		;;
	esac

	compute_system
}

compute_system () {
	case $SYSTEM in
	SOLINT)
		system=solint
		;;
	SOLARI)
		system=solari
		;;
	HPUX)
		system=hpux
		;;
	AIX)
		system=aix
		;;
	*)
		echo
		necho $ERR_UNKNOWN_SYS_STR1
		necho $ERR_UNKNOWN_SYS_STR2
#		echo Valid systems are SOLINT SOLARI HPUX AIX
		necho $ERR_UNKNOWN_SYS_STR3
		echo
		exit
		;;
	esac
}

read_def () {
	default=$2

	if [ "$2" = "" ];then
		necho "$1"
	else
		necho "$1" "[" "$2" "]"
	fi

	read answer
	if [ -z "$answer" ]; then
		answer=$default
	fi
}

read_def_silent () {
	default=$2

	necho $1
	stty  -echo
	read answer
	stty echo
	if [ -z "$answer" ]; then
		answer=$default
	fi
}
create_admin_file () {
	echo basedir=default > $ADMIN_FILE
	echo runlevel=quit >> $ADMIN_FILE
	echo conflict=nocheck >> $ADMIN_FILE
	echo setuid=nocheck >> $ADMIN_FILE
	echo action=nocheck >> $ADMIN_FILE
	echo partial=nocheck >> $ADMIN_FILE
	echo instance=overwrite >> $ADMIN_FILE
	echo idepend=quit >> $ADMIN_FILE
	echo rdepend=quit >> $ADMIN_FILE
	echo space=quit >> $ADMIN_FILE
}

install_files () {
	echo 
	rm $LOG_FILE > /dev/null 2>&1
	touch $LOG_FILE

	. ./uninstall-lsnw.sh
	# Check if Lansafe is installed
	detect_lansafe
	if [ $? -eq 0 ]; then
		necho $CS_REMOVE_LS
        	# Proceed to uninstallation
        	uninstall_lansafe
	fi

	# Check if Netwatch is installed
	detect_netwatch
	if [ $? -eq 0 ]; then
		necho $CS_REMOVE_NW
        	# Proceed to uninstallation
        	uninstall_netwatch
	fi

	# Clean-up previous installation
	remove_profile_settings

	case $system in
	solari)
		necho $REMOVE_PACKAGE | tee -a $LOG_FILE

		create_admin_file

		pkgrm -n -a $ADMIN_FILE NUT >> $LOG_FILE 2>&1
		necho $INSTALL_PACKAGE | tee -a $LOG_FILE
		pkgadd -a $ADMIN_FILE -n -d $systemdir/$NUT_PACKAGE_SOLARI NUT >> $LOG_FILE 2>&1
		res=$?
		if [ ! $res = 0 ]; then
			necho $INSTALL_ERROR
			exit 1
		fi
		rm -f $ADMIN_FILE
	;;
	solint)
		necho $REMOVE_PACKAGE | tee -a $LOG_FILE

		create_admin_file

		pkgrm -n -a $ADMIN_FILE NUT >> $LOG_FILE 2>&1
		necho $INSTALL_PACKAGE | tee -a $LOG_FILE
		pkgadd -a $ADMIN_FILE -n -d $systemdir/$NUT_PACKAGE_SOLINT NUT >> $LOG_FILE 2>&1
		res=$?
		if [ ! $res = 0 ]; then
			necho $INSTALL_ERROR
			exit 1
		fi
		rm -f $ADMIN_FILE
	;;
	hpux)
		cd $systemdir
		rm nut.depot.tar > /dev/null 2>&1
		rm -Rf nut.depot > /dev/null 2>&1
		gunzip nut.depot.tar.gz >> ../$LOG_FILE 2>&1
		res=$?
		if [ ! $res = 0 ]; then
			cd ..
			necho $INSTALL_ERROR
			exit 1
		fi
		tar xvf nut.depot.tar >> ../$LOG_FILE 2>&1
		res=$?
		if [ ! $res = 0 ]; then
			cd ..
			necho $INSTALL_ERROR
			exit 1
		fi
		cd ..
		necho $REMOVE_PACKAGE | tee -a $LOG_FILE
		swremove NUT >> $LOG_FILE 2>&1
		necho $INSTALL_PACKAGE | tee -a $LOG_FILE
		swinstall -s `pwd`/$systemdir/nut.depot/ NUT >> $LOG_FILE 2>&1
		res=$?
		if [ ! $res = 0 ]; then
			necho $INSTALL_ERROR
			exit 1
		fi
	;;
	aix)
		necho $REMOVE_PACKAGE | tee -a $LOG_FILE
		rpm -e nut >> $LOG_FILE 2>&1
		rpm -e nut-client >> $LOG_FILE 2>&1
		necho $INSTALL_PACKAGE | tee -a $LOG_FILE
		rpm -i --nodeps `ls $systemdir/nut-*.aix6.1.ppc.rpm | grep -v 'nut-client'` >> $LOG_FILE 2>&1
		res=$?
		if [ ! $res = 0 ]; then
			necho $INSTALL_ERROR
			exit 1
		fi
		rpm -i --nodeps $systemdir/nut-client-*.aix6.1.ppc.rpm >> $LOG_FILE 2>&1
		res=$?
		if [ ! $res = 0 ]; then
			necho $INSTALL_ERROR
			exit 1
		fi
	;;

	*)
		necho $INSTALL_ERROR
		exit 1
	;;
	esac

	#install additional libraries
	cp $systemdir/libs/* $instpath/lib >> $LOG_FILE 2>&1

	#shutoff scripts
	cp $COMMON_DIR/init $instpath >> $LOG_FILE 2>&1
	chmod 744 $instpath/init >> $LOG_FILE 2>&1
	cp $COMMON_DIR/shutdown $instpath >> $LOG_FILE 2>&1
	chmod 744 $instpath/shutdown >> $LOG_FILE 2>&1

	#notifer scripts
	cp $COMMON_DIR/ipp-notifier.sh $instpath/bin >> $LOG_FILE 2>&1
	chown ipp:ipp $instpath/bin/ipp-notifier.sh >> $LOG_FILE 2>&1
	chmod 774 $instpath/bin/ipp-notifier.sh >> $LOG_FILE 2>&1

	if [ ! -f $instpath/etc/ipp.conf ];then
		cp -f $COMMON_DIR/ipp.conf $instpath/etc >> $LOG_FILE 2>&1
	fi
	chown ipp:ipp $instpath/etc/ipp.conf >> $LOG_FILE 2>&1
	chmod 640 $instpath/etc/ipp.conf >> $LOG_FILE 2>&1

	#uninstall-ipp
	cp uninstall-ipp $instpath/bin >> $LOG_FILE 2>&1
	cp $COMMON_DIR/string.sh $instpath/share >> $LOG_FILE 2>&1
	cp "$COMMON_DIR"_$lang/install.res $instpath/share >> $LOG_FILE 2>&1

	#ipp-status
	cp $COMMON_DIR/ipp-status $instpath/bin >> $LOG_FILE 2>&1
}

#######################
# Configuration screens

initial_configure () {

	conf_ok=""
	while [ -z "$conf_ok" ];do
		C_NUM_DEV=0
		choose_mode
		if [ $? = 1 ];then 
			review_conf
		fi
	done

	# Make sure configuration files are not world readable
	chown ipp:ipp $instpath/etc/ups.conf >> $LOG_FILE 2>&1
	chmod 640 $instpath/etc/ups.conf >> $LOG_FILE 2>&1

	chown ipp:ipp $instpath/etc/upsd.conf >> $LOG_FILE 2>&1
	chmod 640 $instpath/etc/upsd.conf >> $LOG_FILE 2>&1

	chown ipp:ipp $instpath/etc/nut.conf >> $LOG_FILE 2>&1
	chmod 640 $instpath/etc/nut.conf >> $LOG_FILE 2>&1

	chown ipp:ipp $instpath/etc/upsmon.conf >> $LOG_FILE 2>&1
	chmod 640 $instpath/etc/upsmon.conf >> $LOG_FILE 2>&1

	chown ipp:ipp $instpath/etc/upsd.users >> $LOG_FILE 2>&1
	chmod 640 $instpath/etc/upsd.users >> $LOG_FILE 2>&1
}

choose_network() {
	necho $CS_SEPARATOR_1
	necho $CS_NETWORK_1
	necho $CS_SEPARATOR_1
	echo
	necho $CS_NETWORK_2
	echo
	necho $CS_NETWORK_3
	necho $CS_NETWORK_4
	read_def $CS_NETWORK_5 "1"
	case $answer in
	[2])
		C_MODE="netserver"
		;;
	*)
		C_MODE="standalone"
		;;
	esac
}

display_device () {
	DISP_COUNTER=1

        while [ $DISP_COUNTER -le $C_NUM_DEV ];do
		eval TMP=\$C_DEVICE$DISP_COUNTER
#		DEV_TYPE=`printf "${DISP_COUNTER}- $TMP" | awk   -F" " '{print $2}'`
		DEV_CONF=`echo $TMP | awk   -F" " '{print $3}'`
		echo " " $DISP_COUNTER. $DEV_TYPE $DEV_CONF
		DISP_COUNTER=`expr $DISP_COUNTER + 1`
        done
}

remove_ups () {
	if [ $answer -lt 1 ];then
		necho $CS_REMOVE_UPS_2
		return
	fi
	if [ $answer -gt $C_NUM_DEV ];then
		necho $CS_REMOVE_UPS_2
		return
	fi

	OLD_NUM_DEV=$C_NUM_DEV
	C_NUM_DEV=`expr $C_NUM_DEV - 1`
	if [ $C_NUM_DEV -ne 0 ];then
		eval C_DEVICE$answer=\${C_DEVICE$OLD_NUM_DEV}
		eval C_OPTION$answer=\${C_OPTION$OLD_NUM_DEV}

	fi
}

#return 0 to go back
#return 1 to continue the configuration
#retuen 2 to add another UPS
manage_ups () {
	if [ $C_NUM_DEV = 0 ];then
		return 0
	fi

	necho $CS_SEPARATOR_1
	necho $CS_ANOTHER_UPS_1
	necho $CS_SEPARATOR_1
	echo
	display_device
	echo 
	necho $CS_ANOTHER_UPS_2
	necho $CS_ANOTHER_UPS_3
	necho $CS_ANOTHER_UPS_4
	necho $CS_ANOTHER_UPS_5
	read_def $CS_ANOTHER_UPS_6 "1"

	case $answer in
	[2])
		return 2
		;;
	[3])
		read_def $CS_REMOVE_UPS_1 ""
		remove_ups
		ret=""
		manage_ups
		ret=$?
		return $ret
		;;
	[0])
		return 0
		;;
	*)
		return 1
		;;
	esac
}

choose_mode() {
	necho $CS_SEPARATOR_1
	necho $CS_WELCOME_1
	necho $CS_SEPARATOR_1
	echo
	necho $CS_MANAGE_1
	echo
	necho $CS_MANAGE_2
	necho $CS_MANAGE_3
	read_def $CS_MANAGE_4 "1"
	case $answer in
	[2])
		necho $CS_SEPARATOR_1
		necho $CS_CLIENT_SD_DURATION_1
		necho $CS_SEPARATOR_1
		echo
		necho $CS_CLIENT_SD_DURATION_2
		read TMP

		C_MODE="netclient"
		ret=2
		while [ $ret = 2 ];do
			choose_server
			manage_ups
			ret=$?
		done
		;;
	*)
		choose_shutdown_duration
		choose_network
		ret=2
		while [ $ret = 2 ];do
			choose_connectivity
			manage_ups
			ret=$?
		done

		if [ $ret = 1 ];then
			ret=""
			choose_password
			ret=$?
		fi
		;;
	esac

	if [ $ret = 1 ];then
		if [ $C_NUM_DEV -gt 1 ];then
			ret=""
			choose_minsupplies
			ret=$?
		fi
	fi

	return $ret
}

choose_minsupplies() {
	necho $CS_SEPARATOR_1
	necho $CS_MINSUP_1
	necho $CS_SEPARATOR_1

	answer=""
	while [ "$answer" = "" ];do
		read_def $CS_MINSUP_2 $C_NUM_DEV
		#Check if this is a valid number
		if echo $answer | egrep '^[0-9]+$' > /dev/null 2>&1 ; then
			#this is a number
			if [ $answer -lt 1 ];then
				answer=""
			fi
		else
			necho $CS_ERR_NO_NUM
			answer=""
		fi
	done;

	C_MINSUPPLIES=$answer
	
	if [ $answer -gt $C_NUM_DEV ];then
		C_MINSUPPLIES=$C_NUM_DEV
	fi


	return 1
}

choose_connectivity() {
	ret="0"
	while [ $ret = "0" ];do
		necho $CS_SEPARATOR_1
		necho $CS_CONNECTIVITY_1
		necho $CS_SEPARATOR_1
		echo
		necho $CS_CONNECTIVITY_2
		necho $CS_CONNECTIVITY_3
		necho $CS_CONNECTIVITY_4
		read_def $CS_CONNECTIVITY_5 "1"
		case $answer in
		[0])
			return 0
			;;
		[2])
#TODO
			choose_xml_manual_or_auto
			ret=$?
			;;
		*)
			choose_ask_scan_serial
			ret=$?
			;;
		esac
	done
	return $ret
}

choose_ask_scan_serial() {
	ret="0"
	while [ $ret = "0" ];do
		necho $CS_SEPARATOR_1
		necho $CS_ASK_SERIAL_1
		necho $CS_SEPARATOR_1
		echo
		necho $CS_ASK_SERIAL_2
		echo
		necho $CS_ASK_SERIAL_3
		necho $CS_ASK_SERIAL_4
		necho $CS_ASK_SERIAL_5
		read_def $CS_ASK_SERIAL_6 "1"
		case $answer in
		[0])
			return 0
			;;
		[2])
			choose_manual_serial
			ret=$?
			;;
		*)
			choose_serial
			ret=$?
			;;
		esac
	done
	return $ret
}

choose_serial() {
	serial_list=`$NUTCONF --scan-serial auto 2>> $LOG_FILE`
	if [ "$serial_list" = "" ];then
		echo
		read_def $CS_ERR_NO_SERIAL ""
		echo
		return 0
	fi

	necho $CS_SEPARATOR_1
	necho $CS_SERIAL_1
	necho $CS_SEPARATOR_1
	echo

	OLD1_IFS=$IFS
	IFS="
"

	i="1"
	for s in $serial_list;do
		DEV_NAME=`echo "$s" | awk   -F" " '{print $3}'`
		echo "                      $i. $DEV_NAME"
		i=`expr $i + 1`
	done

	IFS=$OLD1_IFS

	necho $CS_SERIAL_3
	read_def $CS_SERIAL_4 "1"
	case $answer in
	[0])
		return 0
		;;
	*)
		OLD1_IFS=$IFS
		IFS="
"

		C_NUM_DEV=`expr $C_NUM_DEV + 1`
		i="1"
		for s in $serial_list;do
			if [ $i = $answer ];then
				eval C_DEVICE$C_NUM_DEV=\"$s\"
				eval C_OPTION$C_NUM_DEV=\"\"
				break;
			fi
			i=`expr $i + 1`
		done
		IFS=$OLD1_IFS
		;;
	esac

	return 1
}

choose_manual_serial () {
	necho $CS_SEPARATOR_1
	necho $CS_MANUAL_SERIAL_1
	necho $CS_SEPARATOR_1
	echo
	necho $CS_MANUAL_SERIAL_2
	read_def $CS_MANUAL_SERIAL_3 ""

	DEV=`$NUTCONF --scan-serial $answer 2>> $LOG_FILE`

	if [ "$DEV" = "" ];then
		read_def $CS_MANUAL_SERIAL_ERR ""
		return 0;
	fi

	C_NUM_DEV=`expr $C_NUM_DEV + 1`
	eval C_DEVICE$C_NUM_DEV=$DEV

	return 1
}

choose_xml_manual_or_auto() {
	necho $CS_SEPARATOR_1
	necho $CS_XML_MANU_AUTO_1
	necho $CS_SEPARATOR_1
	echo
	necho $CS_XML_MANU_AUTO_2
	echo
	necho $CS_XML_MANU_AUTO_3
	necho $CS_XML_MANU_AUTO_4
	necho $CS_XML_MANU_AUTO_5

	read_def $CS_XML_MANU_AUTO_6 "1"
	case $answer in
	[2])
		choose_xml_manual
		return $?
		;;
	[0])
		return 0
		;;
	*)
		choose_xml
		return $?
	esac
}

choose_xml_manual() {
	necho $CS_SEPARATOR_1
	necho $CS_XML_MANU_1
	necho $CS_SEPARATOR_1
	echo
	read_def $CS_XML_MANU_2 ""

	if [ "$answer" = "" ];then
		return 0;
	fi

	C_NUM_DEV=`expr $C_NUM_DEV + 1`
	eval C_DEVICE$C_NUM_DEV=\"XML netxml-ups http://$answer\"

	nmc_login
	return 1;
}

choose_xml() {
	necho $CS_SEPARATOR_1
	necho $CS_SNMP_1
	necho $CS_SEPARATOR_1
	echo
	necho $CS_SNMP_2
	echo
	necho $CS_SNMP_6
	echo

	LIST_XML=`$NUTCONF --scan-xml-http 2>> $LOG_FILE`
	if [ "$LIST_XML" = "" ];then
		read_def $CS_SNMP_7 ""
		choose_snmp
		return $?
	fi

	LIST_XML=`echo "$LIST_XML" | sort`

	echo

	OLD1_IFS=$IFS
	IFS="
"

	i="1"
	for s in $LIST_XML; do
		#FIXME: If there is a coma in the description, the description
		#will be shown up to this coma only.
		IP_ADDR=`echo "$s" | awk   -F" " '{print $3}' | sed 's/http:\/\///g'`
		#DESC=`echo "$s" | awk   -F\" '{print $6}' | sed -e "s/^.*\"\(.*\)\".*$/\1/"`
		echo "                      $i. $IP_ADDR"
		i=`expr $i + 1`
	done
	IFS=$OLD1_IFS

	# Ask to scan for more UPS
	printf "                      $i. "
	necho $CS_SNMP_10
	necho $CS_SNMP_9

	read_def $CS_SNMP_8 "1"
	case $answer in
	$i)
		choose_snmp
		return $?
		;;
	[0])
		return 0
		;;
	*)
		OLD1_IFS=$IFS
		IFS="
"

		C_NUM_DEV=`expr $C_NUM_DEV + 1`
		i="1"
		for s in $LIST_XML;do
			if [ $i = $answer ];then
				eval C_DEVICE$C_NUM_DEV=\"$s\"
				nmc_login
				IFS=$OLD1_IFS
				return 1;
			fi
			i=`expr $i + 1`
		done
		IFS=$OLD1_IFS
		C_NUM_DEV=`expr $C_NUM_DEV - 1`
		;;
	esac

	return 0
}

choose_snmp() {
	necho $CS_SEPARATOR_1
	necho $CS_SNMP_1
	necho $CS_SEPARATOR_1
	echo
	necho $CS_SNMP_2
	echo
	read_def $CS_SNMP_3 $FIRST_IP
	FIRST_IP=$answer
	if [ "$answer" = "" ];then
		return 0
	fi
	read_def $CS_SNMP_4 $LAST_IP
	LAST_IP=$answer
	if [ "$answer" = "" ];then
		LAST_IP=$FIRST_IP
	fi

	echo
	read_def $CS_SNMP_5 $C_COMMUNITY
	C_COMMUNITY=$answer

	echo
	necho $CS_SNMP_6
	echo

	list=`$NUTCONF --scan-snmp $FIRST_IP $LAST_IP community=$C_COMMUNITY 2>> $LOG_FILE`

	list=`echo "$list" | sort`
	echo

	filter_snmp_list

	if [ "$LIST_SNMP" = "" ];then
		read_def $CS_SNMP_7 ""
		return 0
	fi

	OLD1_IFS=$IFS
	IFS="
"

	i="1"
	for s in $LIST_SNMP; do
		#FIXME: If there is a coma in the description, the description
		#will be shown up to this coma only.
		IP_ADDR=`echo "$s" | awk   -F" " '{print $3}'`
		#DESC=`echo "$s" | awk   -F\" '{print $6}' | sed -e "s/^.*\"\(.*\)\".*$/\1/"`
		echo "                      $i. $IP_ADDR"
		i=`expr $i + 1`
	done
	IFS=$OLD1_IFS

	necho $CS_SNMP_9

	read_def $CS_SNMP_8 "1"
	case $answer in
	[0])
		return 0
		;;
	*)
		OLD1_IFS=$IFS
		IFS="
"

		C_NUM_DEV=`expr $C_NUM_DEV + 1`
		i="1"
		for s in $LIST_SNMP;do
			if [ $i = $answer ];then
				eval C_DEVICE$C_NUM_DEV=\"$s\"
				eval C_OPTION$C_NUM_DEV=\"community=$C_COMMUNITY\"
				IFS=$OLD1_IFS
				return 1
			fi
			i=`expr $i + 1`
		done
		IFS=$OLD1_IFS
		C_NUM_DEV=`expr $C_NUM_DEV - 1`
		;;
	esac

	return 0
}

#Remove XML devices from the SNMP list
filter_snmp_list() {
	LIST_SNMP=""
	OLD1_IFS=$IFS
	IFS="
"
	for s in $list;do
		IP_SNMP=`echo "$s" | awk   -F" " '{print $3}'`
		TO_ADD="$s"
		for x in $LIST_XML;do
			IP_XML=`echo "$x" | awk   -F" " '{print $3}' | sed 's/http:\/\///g'`
			if [ "$IP_SNMP" = "$IP_XML" ];then
				TO_ADD=""
				break;
			fi
		done
		if [ ! "$TO_ADD" = "" ];then
			LIST_SNMP=`echo "$LIST_SNMP";echo "$TO_ADD"`
		fi
	done
	IFS=$OLD1_IFS
}


#only used in netclient mode
choose_server() {
	necho $CS_SEPARATOR_1
	necho $CS_SERVER_1
	necho $CS_SEPARATOR_1
	echo
	necho $CS_SERVER_2
	echo
	read_def $CS_SERVER_3 ""
	FIRST_IP=$answer
	if [ "$answer" = "" ];then
		return 0
	fi
	read_def $CS_SERVER_4 ""
	LAST_IP=$answer
	if [ "$answer" = "" ];then
		LAST_IP=$FIRST_IP
	fi

	echo
	necho $CS_SERVER_5
	echo

	list=`$NUTCONF --scan-nut $FIRST_IP $LAST_IP 3493 2>> $LOG_FILE`
#TODO parse scan results
	if [ "$list" = "" ];then
		read_def $CS_SERVER_6 ""
		return 0
	fi

	echo

	OLD1_IFS=$IFS
	IFS="
"

	i="1"
	for s in $list;do
		UPS=`echo "$s" | awk   -F" " '{print $3}'`
		echo "                      $i. $UPS"
		i=`expr $i + 1`
	done
	IFS=$OLD1_IFS

	necho $CS_SERVER_8

	read_def $CS_SERVER_7 "1"
	case $answer in
	[0])
		return 0
		;;
	*)
		echo $C_NUM_DEV
		C_NUM_DEV=`expr $C_NUM_DEV + 1`
		echo $C_NUM_DEV
		i="1"
		OLD1_IFS=$IFS
		IFS="
"
		for s in $list;do
			if [ $i = $answer ];then
				eval C_DEVICE$C_NUM_DEV=\"$s\"
				eval C_OPTION$C_NUM_DEV=\"\"
				break;
				fi
			i=`expr $i + 1`
		done
		IFS=$OLD1_IFS
		;;
	esac

	return 1
}

choose_password() {
	necho $CS_SEPARATOR_1
	necho $CS_PASSWORD_1
	necho $CS_SEPARATOR_1
	echo
	while [ "$C_PASSWORD" = "" ];do
		necho $CS_PASSWORD_2
		read_def_silent $CS_PASSWORD_3 ""
		C_PASSWORD=$answer
	done

	return 1
}

get_networked_device() {
	NDEV=0
	C_NUM_NETWORK_DEVICE=0

	while [ $NDEV -lt $C_NUM_DEV ];do
		NDEV=`expr $NDEV + 1`
		eval TMP=\$C_DEVICE$NDEV
		DRIVER=`echo $TMP | awk -F" " '{print $2}'`
		if [ $DRIVER = "netxml-ups" ];then
			C_NUM_NETWORK_DEVICE=`expr $C_NUM_NETWORK_DEVICE + 1`
		fi
	done
}

nmc_login() {
	necho $CS_SEPARATOR_1
	necho $CS_NMC_LOGIN_1
	necho $CS_SEPARATOR_1
	echo
	read_def $CS_NMC_LOGIN_2 "admin"
	NMC_LOGIN=$answer
	answer=""
	PASS1="1"
	while [ ! "$answer" = "$PASS1" ];do
		read_def_silent $CS_NMC_LOGIN_3 ""
		PASS1=$answer
		read_def_silent $CS_NMC_LOGIN_4 ""
		if [ ! "$answer" = "$PASS1" ];then
			echo
			necho $CS_NMC_LOGIN_5
			echo
		fi
	done
	eval C_OPTION$C_NUM_DEV=\"login=$NMC_LOGIN password=$answer subscribe=yes shutdown_duration=$C_SHUTDOWN_DURATION\"
}

choose_shutdown_duration() {
	necho $CS_SEPARATOR_1
	necho $CS_SHUTOFF_DELAY_1
	necho $CS_SEPARATOR_1
	echo
	answer=""
	while [ "$answer" = "" ];do
		read_def $CS_SHUTOFF_DELAY_3 "$C_SHUTDOWN_DURATION"
		#Check if this is a valid number
		if echo $answer | egrep '^[0-9]+$' > /dev/null 2>&1; then
			#this is a number, do nothing
			sleep 0
		else
			necho $CS_ERR_NO_NUM
			answer=""
		fi
	done;
	C_SHUTDOWN_DURATION=$answer

	return 1
}

set_shutdown_command() {
	SHUTDOWN_CMD="$instpath/shutdown;/usr/sbin/shutdown"

	case $system in
	solari)
		$NUTCONF --set-shutdowncmd "$SHUTDOWN_CMD -y -g 0 -i 5" 2>> $LOG_FILE
		;;
	solint)
		$NUTCONF --set-shutdowncmd "$SHUTDOWN_CMD -y -g 0 -i 5" 2>> $LOG_FILE
		;;
	hpux)
		$NUTCONF --set-shutdowncmd "cd /;$SHUTDOWN_CMD -y -h now" 2>> $LOG_FILE
		;;
	aix)
		$NUTCONF --set-shutdowncmd "$SHUTDOWN_CMD -h +0" 2>> $LOG_FILE
		;;
	*)
		;;
	esac

	$NUTCONF --set-powerdownflag $instpath/etc/killpower 2>> $LOG_FILE
}

set_notify() {
	# Events messages (mc2/scripts/lang/eng.lbl)
	# '/Event/UPS.PowerSummary.PresentStatus.ACPresent/1'
	$NUTCONF --set-notifymsg ONLINE "The system is powered by the utility" 2>> $LOG_FILE # "UPS %s on line power"
	# '/Event/UPS.PowerSummary.PresentStatus.ACPresent/0'
	$NUTCONF --set-notifymsg ONBATT "The system is powered by the UPS battery" 2>> $LOG_FILE #"UPS %s on battery"
	# '/Event/UPS.PowerSummary.PresentStatus.BelowRemainingCapacityLimit/1'
	$NUTCONF --set-notifymsg LOWBATT "Low battery alarm" 2>> $LOG_FILE # "UPS %s battery is low"
	# ?
 	$NUTCONF --set-notifymsg FSD "UPS %s: forced shutdown in progress" 2>> $LOG_FILE
	# '/Event/System.CommunicationLost/0'
	$NUTCONF --set-notifymsg COMMOK "Communication with device is restored" 2>> $LOG_FILE # "Communications with UPS %s established"
	# '/Event/System.CommunicationLost/1'
	$NUTCONF --set-notifymsg COMMBAD "Communication with device has failed" 2>> $LOG_FILE # "Communications with UPS %s lost"
	# ?
 	$NUTCONF --set-notifymsg SHUTDOWN "Auto logout and shutdown proceeding" 2>> $LOG_FILE
	# '/Event/UPS.PowerSummary.PresentStatus.NeedReplacement/1'
	$NUTCONF --set-notifymsg REPLBATT "Battery fault" 2>> $LOG_FILE # "UPS %s battery needs to be replaced"
 	$NUTCONF --set-notifymsg NOCOMM "UPS %s is unavailable" 2>> $LOG_FILE
	$NUTCONF --set-notifymsg NOPARENT "Shutdown failure" 2>> $LOG_FILE # "upsmon parent process died - shutdown impossible"
 
	# Enable mail notification
	$NUTCONF --set-notifycmd $instpath/bin/ipp-notifier.sh 2>> $LOG_FILE

	$NUTCONF --set-notifyflags ONLINE EXEC 2>> $LOG_FILE
	$NUTCONF --set-notifyflags ONBATT EXEC 2>> $LOG_FILE
	$NUTCONF --set-notifyflags LOWBATT EXEC 2>> $LOG_FILE
	$NUTCONF --set-notifyflags FSD EXEC 2>> $LOG_FILE
	$NUTCONF --set-notifyflags COMMOK EXEC 2>> $LOG_FILE
	$NUTCONF --set-notifyflags COMMBAD EXEC 2>> $LOG_FILE
	$NUTCONF --set-notifyflags SHUTDOWN EXEC 2>> $LOG_FILE
	$NUTCONF --set-notifyflags REPLBATT EXEC 2>> $LOG_FILE
	$NUTCONF --set-notifyflags NOCOMM EXEC 2>> $LOG_FILE
	$NUTCONF --set-notifyflags NOPARENT EXEC 2>> $LOG_FILE
}

set_shutdown_duration() {
	cat $instpath/etc/ipp.conf | grep -v "DELAY=" > $instpath/etc/ipp.conf
	echo DELAY=$C_SHUTDOWN_DURATION >> $instpath/etc/ipp.conf
	cat $instpath/etc/ipp.conf | grep -v "PASSWORD=" > $instpath/etc/ipp.conf
	echo PASSWORD=$C_PASSWORD >> $instpath/etc/ipp.conf
}

set_minsupplies() {
	$NUTCONF --set-minsupplies $C_MINSUPPLIES 2>> $LOG_FILE
}
review_conf() {
	display_conf

	read_def $CS_REVIEW_8 "n"
	case $answer in
	[yY])
		conf_ok="ok"

		$NUTCONF --set-mode $C_MODE 2>> $LOG_FILE

		case $C_MODE in
		netclient)
			apply_conf_client
			;;
		netserver)
			apply_conf_server
			;;
		standalone)
			apply_conf_standalone
			;;
		esac

		set_minsupplies
		set_shutdown_command
		set_notify
		set_shutdown_duration

		;;

	*)
		;;
	esac

}

display_conf() {
	necho $CS_SEPARATOR_1
	necho $CS_REVIEW_1
	necho $CS_SEPARATOR_1
	echo
	if [ $C_MODE = "standalone" ];then
		necho $CS_REVIEW_2
	fi
	if [ $C_MODE = "netserver" ];then
		necho $CS_REVIEW_3
	fi
	if [ $C_MODE = "netclient" ];then
		necho $CS_REVIEW_4
	fi
	echo
	necho $CS_REVIEW_5
	display_device
	echo
	if [ $C_NUM_DEV -gt 1 ];then
		necho $CS_REVIEW_6 " " $C_MINSUPPLIES
	fi

	necho $CS_REVIEW_7 " " $C_SHUTDOWN_DURATION
	echo

}

apply_conf_client() {
	NDEV=1
	$NUTCONF --set-user upsmon=slave password=upsmon 2>> $LOG_FILE

	eval TMP=\$C_DEVICE$NDEV
	DEV=`echo $TMP | awk -F" " '{print $3}'`
	UPS=`echo $DEV| awk -F@ '{print $1}'`
	HOST=`echo $DEV| awk -F@ '{print $2}'`
	$NUTCONF --set-monitor ${UPS} $HOST 1 upsmon upsmon slave 2>> $LOG_FILE

	while [ $NDEV -lt $C_NUM_DEV ];do
		NDEV=`expr $NDEV + 1`
		eval TMP=\$C_DEVICE$NDEV
		DEV=`echo $TMP | awk -F" " '{print $3}'`
		UPS=`echo $DEV | awk -F@ '{print $1}'`
		HOST=`echo $DEV| awk -F@ '{print $2}'`
		$NUTCONF --add-monitor ${UPS} $HOST 1 upsmon upsmon slave 2>> $LOG_FILE
	done
}

split_device () {
	eval TMP=\$C_DEVICE$NDEV
        ID=`echo $TMP | awk -F" " '{print $1}'`
        DRIVER=`echo $TMP | awk -F" " '{print $2}'`
        PORT=`echo $TMP | awk -F" " '{print $3}'`
}

setup_tty () {
	local TTY="$1" > $LOG_FILE 2>&1

	chmod 666 "$TTY" > $LOG_FILE 2>&1
	chown ipp:ipp "$TTY" > $LOG_FILE 2>&1
}

set_device_monitor_user () {
	NDEV=1

	split_device

	eval TMP=\$C_OPTION$NDEV
	$NUTCONF --set-device ${ID}$NDEV $DRIVER $PORT $TMP 2>> $LOG_FILE
	$NUTCONF --set-monitor "${ID}$NDEV" localhost 1 upsmon upsmon master 2>> $LOG_FILE

	echo "$PORT" | grep '^/dev/tty' >/dev/null && setup_tty "$PORT"

	while [ $NDEV -lt $C_NUM_DEV ];do
		NDEV=`expr $NDEV + 1`
		split_device
		eval TMP=\$C_OPTION$NDEV
		$NUTCONF --add-device ${ID}$NDEV $DRIVER $PORT $TMP 2>> $LOG_FILE
		$NUTCONF --add-monitor "${ID}$NDEV" localhost 1 upsmon upsmon master 2>> $LOG_FILE

		echo "$PORT" | grep '^/dev/tty' >/dev/null && setup_tty "$PORT"
	done


	$NUTCONF --set-user admin password=$C_PASSWORD actions=SET instcmds=all 2>> $LOG_FILE
	$NUTCONF --add-user upsmon=master password=upsmon 2>> $LOG_FILE
}

apply_conf_server() {
	$NUTCONF --set-listen 0.0.0.0 2>> $LOG_FILE

	set_device_monitor_user
}

apply_conf_standalone () {
	$NUTCONF --set-listen 127.0.0.1 2>> $LOG_FILE

	set_device_monitor_user
}

start_service () {
	necho $CS_START_SERVICE
	stop_service
	case $system in
		solari)
		/etc/init.d/nut start >> $LOG_FILE 2>&1
		;;
		solint)
		/etc/init.d/nut start >> $LOG_FILE 2>&1
		;;
		hpux)
		/sbin/init.d/nut-drvctl start >> $LOG_FILE 2>&1
		/sbin/init.d/nut-upsd start >> $LOG_FILE 2>&1
		/sbin/init.d/nut-upsmon start >> $LOG_FILE 2>&1
		;;
		aix)
		/etc/rc.d/init.d/ups start >> $LOG_FILE 2>&1
		;;
		*)
		;;
	esac
}

stop_service () {
	case $system in
		solari)
		/etc/init.d/nut stop >> $LOG_FILE 2>&1
		;;
		solint)
		/etc/init.d/nut stop >> $LOG_FILE 2>&1
		;;
		hpux)
		/sbin/init.d/nut-drvctl stop >> $LOG_FILE 2>&1
		/sbin/init.d/nut-upsd stop >> $LOG_FILE 2>&1
		/sbin/init.d/nut-upsmon stop >> $LOG_FILE 2>&1
		;;
		aix)
		/etc/rc.d/init.d/ups stop >> $LOG_FILE 2>&1
		;;
		*)
		;;
	esac
}

add_profile_settings () {
	if [ -f /etc/profile ];then
		cat >>/etc/profile <<HERE
##### IPP - Unix settings begin #####
# Do not remove nor change the line above, it's used
# for automatic removal of the section upon uninstallation
PATH=\$PATH:/usr/local/ups/bin
export PATH
LD_LIBRARY_PATH=/usr/local/ups/lib:\$LD_LIBRARY_PATH
export LD_LIBRARY_PATH
MANPATH=$MANPATH:/usr/local/ups/share/man
export MANPATH
# Again, do not remove nor change the line below
##### IPP - Unix settings end #####
HERE
	fi
}

remove_profile_settings () {
        if /usr/bin/test -e /etc/profile; then
                awk '
                BEGIN {
                        echo = 1;
                }

                /^##### IPP - Unix settings begin #####$/ {
                        echo = 0;
                }

                /^##### IPP - Unix settings end #####$/ {
                        echo = 1;
                        next;
                }

                {
                        if (echo) print $0;
                }
                ' /etc/profile > /tmp/profile.tmp.$$

                mv /tmp/profile.tmp.$$ /etc/profile
        fi
}

main () {
# Setup some globals
instpath=/usr/local/ups
#skip_install=true
#skip_license=true
#skip_service=true
#skip_config=true
ret=TRUE;

# Must check parameters
	lang=""
	SYSTEM=""

	if [ "$1" != "" ] ; then
		get_parameters $1
	fi

	if [ "$2" != "" ] ; then
		get_parameters $2
	fi

	if [ "$3" != "" ] ; then
		get_parameters $3
	fi

# Get language locale 
	if [ "$lang" = "" ]; then
		check_locale_language
		if [ -z "$lang" ]; then
			lang=EN
		fi
	fi 

#  Initialize resource strings.
	installres="$COMMON_DIR"_$lang/install.res
	licenseres="$COMMON_DIR"_$lang/license.txt
	initDefines

	compute_SYSTEM

	systemdir="$system"
	NUTCONF="$systemdir/nutconf"

	who=`whoami`
	if [ "$who" != 'root' ]; then
#work around in case no whoami is present 
		who=`id -u`
		if [ "$who" != '0' ]; then
			necho $ERR_ROOT
			exit 1;
		fi
	fi

# Say hello
#	we'll install vanilla IPP unix.
	clear
	echo
	echo 
	echo "-----------------------------------------------------------------"
	echo
	
	echo "                      EATON"
	echo
#	echo "Welcome to IPP Unix!"
	necho $WELCOME_STR1

#	echo "         `pwd`/$0 Version 5.0.0"
	lineStr=`head -$STR_VERSION $installres |tail -1`
	#echo "         `pwd`/$0 $lineStr $lineStr1"
	echo "                       $lineStr $IPP_VERSION"

	echo
#	echo "           To install IPP - Unix, select (y)"
#	echo "and fill in the configuration items as they are presented to you."
	necho $WELCOME_STR2
	necho $WELCOME_STR3
	echo
#	echo "The default install path name for this software is:  $instpath"
	lineStr=`head -$DEFAULT_PATH_STR $installres |tail -1`
	echo "   $lineStr"
	echo "   $instpath"
	echo
	echo "-----------------------------------------------------------------"
	echo
#	echo "NOTE: If you already have LanSafe for Unix installed and you"
#	echo "      do not wish to re-install it, select (N)o to stop"
#	echo "      LanSafe for Unix installation."

	necho $INSTALL_INTRO_STR1
	necho $INSTALL_INTRO_STR2
	necho $INSTALL_INTRO_STR3
	echo
	echo

#	read_def "  Continue IPP - Unix installation? (y/n) [y] " "y"
	read_def $INSTALL_INTRO_STR4 "y"
	case $answer in
	[yY]|[yY][eE][sS])
		;;
	*)
		exit 0
		;;
	esac

	if [ x"$system" = x"Unknown System" ]
	then
		echo
#		echo "  Unable to determine your system."
		lineStr=`head -$ERR_UNKNOWN_SYS_STR1 $installres |tail -1`
		echo "  $lineStr"
		exit 0
	fi	# end-of "if system = Unknown system"

	echo
	echo

# License Agreement
	if [ -z "$skip_license" ]; then
		echo " "
		more $licenseres
		echo " "
		necho $LICENSE_AGREEMENT_1
		echo
		necho $LICENSE_AGREEMENT_2
		necho $LICENSE_AGREEMENT_3
		necho $LICENSE_AGREEMENT_4
		echo

		read_def $LICENSE_AGREEMENT_5 "n"

		case $answer in
		[yY]|[yY][eE][sS])
			;;
		*)
			necho $ERR_LIC_AGREE
			exit 1
			;;
		esac
	fi

	stop_service

	# Install files
	if [ -z "$skip_install" ]; then
		install_files
	fi

	echo
	echo

	# Build initial configuration
	if [ -z "$skip_config" ]; then
		LD_LIBRARY_PATH=$instpath/lib:\$LD_LIBRARY_PATH
		export LD_LIBRARY_PATH
		initial_configure
	fi

	# Start service
	if [ -z "$skip_service" ]; then
		start_service
	fi

	# Add IPP - Unix specific env. settings
	add_profile_settings

# Say goodbye
	if [ "$conf_ok" = "ok" ]; then
		echo "---------------------------------------------------------------"
#		echo "     IPP - Unix was successfully installed on your system.     "
		lineStr=`head -$SUCCESS_INSTALL $installres |tail -1`
		echo "     $lineStr"
		echo "---------------------------------------------------------------"
	else
		echo "-----------------------------------------------------------------------------"
#		echo "     WARNING: IPP - Unix was NOT successfully installed on your system.     "
		lineStr=`head -$UNSUCCESS_INSTALL $installres |tail -1`
		echo "     $lineStr"
		echo "-----------------------------------------------------------------------------"
		echo
	fi
}

main $1 $2 $3

