dnl Check for presence/location of Linux hiddev. On success, set 
dnl nut_linux_hiddev to the device path. On failure, set 
dnl nut_linux_hiddev="". This macro can be run multiple times, but will
dnl do the checking only once. 

AC_DEFUN([NUT_CHECK_HIDDEV], 
[
if test -z "${nut_check_hiddev_seen}"; then
   nut_check_hiddev_seen=yes

   dnl Check for Linux hiddev
   AC_MSG_CHECKING(for Linux hiddev.h)
   AC_ARG_WITH(linux-hiddev,
   AC_HELP_STRING([--with-linux-hiddev=FILE], [linux hiddev.h location (/usr/include/linux/hiddev.h)]),
   [	case "${withval}" in
	yes)
		nut_linux_hiddev="/usr/include/linux/hiddev.h"
		;;
	no)
		nut_linux_hiddev=
		;;
	*)
		nut_linux_hiddev="${withval}"
		;;
	esac],
   [
	nut_linux_hiddev="/usr/include/linux/hiddev.h"
   ]
   )

   if test -z "${nut_linux_hiddev}"; then
	AC_MSG_RESULT(no)
   elif test -f "${nut_linux_hiddev}"; then
	AC_MSG_RESULT(${nut_linux_hiddev})
   else
	AC_MSG_RESULT(${nut_linux_hiddev} not found)
	nut_linux_hiddev=
   fi

fi
])
