dnl Check for the exact system name and type. This is only used at the moment
dnl to determine the packaging rule to be used through the OS_NAME variable.
dnl Derived from dist.m4 - OpenSS7 (Ditributed under the GNU GPL v2)
dnl  Copyright (c) 2001-2006  OpenSS7 Corporation <http://www.openss7.com/>
dnl  Copyright (c) 1997-2000  Brian F. G. Bidulock <bidulock@openss7.org>

AC_DEFUN_ONCE([NUT_OS_FUNCTIONS],
[
 os_get_name() {
    case "$[1]" in
	(*CentOS*|*CENTOS*)					echo 'centos'	;;
	(*Lineox*|*LINEOX*)					echo 'lineox'	;;
	(*White?Box*|*WHITE?BOX*)			echo 'whitebox'	;;
	(*Fedora*|*FEDORA*)					echo 'fedora'	;;
	(*Mandrake*|*Mandriva*|*MANDRAKE*|*MANDRIVA*)	echo 'mandriva'	;;
	(*Red?Hat*|*RED?HAT*)				echo 'redhat'	;;
	(*SuSE*|*SUSE*|*Novell*|*NOVELL*)	echo 'suse'	;;
	(*Debian*|*DEBIAN*)					echo 'debian'	;;
	(*Ubuntu*|*UBUNTU*)					echo 'ubuntu'	;;
	(*Gentoo*|*gentoo*)					echo 'gentoo'	;;
# FIXME: *BSD, Solaris, HPUX, Aix, ...
	(*) # fallback for other systems
		case "${host_cpu}-${host_os}" in
			*-aix*)						echo 'aix'	;;
			*-freebsd*)					echo 'freebsd'	;;
			*-darwin*)					echo 'darwin'	;;	
			*solaris*)					echo 'esyscmd(uname -sp)'	;;
			*-hpux*)					echo 'hpux'	;;
		esac
    esac
}
 # only list special cases.
 os_get_target() {
    case "$[1]" in
    # some may fall under generic-rpm
	(centos|lineox|whitebox|fedora|redhat)	echo 'redhat'	;;
	(suse)									echo 'opensuse'	;;
	(ubuntu)								echo 'debian'	;;
	(*)										echo '$[1]'	;;
# FIXME: *BSD, Solaris, HPUX, Aix, ...
    esac
}
])# _OS_FUNCTIONS

AC_DEFUN([NUT_CHECK_OS],
[
    m4_pattern_allow([^PKG_TARGET$])
    # Look for all possible source of OS name resolution
    # 1) we look for a LSB release info file
	eval "dist_search_path=\"
	    /etc/lsb-release\""
	dist_search_path=$(echo "$dist_search_path" | sed -e 's|\<NONE\>||g;s|//|/|g')
	for dist_file in $dist_search_path
	do
	    if test -f "$dist_file"
	    then
		dist_cv_build_lsb_file="$dist_file"
		break
	    fi
	done
	if test -z "$dist_cv_build_lsb_file" ; then
	    dist_cv_build_lsb_file='no'
	fi
    # 2) we look at specific release info file
	eval "dist_search_path=\"
	    /etc/gentoo-release
	    /etc/centos-release
	    /etc/lineox-release
	    /etc/whitebox-release
	    /etc/fedora-release
	    /etc/mandrake-release
	    /etc/mandriva-release
	    /etc/redhat-release
	    /etc/SuSE-release
	    /etc/debian_version\""
	dist_search_path=$(echo "$dist_search_path" | sed -e 's|\<NONE\>||g;s|//|/|g')
	for dist_file in $dist_search_path
	do
	    if test -f "$dist_file"
	    then
		dist_cv_build_rel_file="$dist_file"
		break
	    fi
	done
	if test -z "$dist_cv_build_rel_file" ; then
	    dist_cv_build_rel_file='no'
	fi
    # 3) we try the generic issue info file
	eval "dist_search_path=\"
	    /etc/issue
	    /etc/issue.net\""
	dist_search_path=$(echo "$dist_search_path" | sed -e 's|\<NONE\>||g;s|//|/|g')
	for dist_file in $dist_search_path
	do
	    if test -f "$dist_file"
	    then
		dist_cv_build_issue_file="$dist_file"
		break
	    fi
	done
	if test -z "$dist_cv_build_issue_file" ; then
	    dist_cv_build_issue_file='no'
	fi

    # Now we parse these content to search for the OS name
    AC_REQUIRE([NUT_OS_FUNCTIONS])
    AC_CACHE_CHECK([for host system name], [dist_cv_build_flavor], [dnl
	if test -z "$dist_cv_build_flavor" -a ":${dist_cv_build_rel_file:-no}" != :no ; then
	    if test `echo "$dist_cv_build_rel_file" | sed -e 's|.*/||'` != 'debian_version' ; then
		dist_cv_build_flavor=$(os_get_name "$(cat $dist_cv_build_rel_file)")
	    fi
	fi
	if test -z "$dist_cv_build_flavor" -a ":${dist_cv_build_lsb_file:-no}" != :no ; then
	    . "$dist_cv_build_lsb_file"
	    dist_cv_build_flavor=$(os_get_name "${DISTRIB_DESCRIPTION:-unknown}")
	    if test -z "$dist_cv_build_flavor" ; then
		dist_cv_build_flavor=$(echo "$DISTRIB_ID" | tr [[:upper:]] [[:lower:]] | sed -e 's|[[[:space:]]]*||g;s|linux||g')
	    fi
	fi
	if test -z "$dist_cv_build_flavor" -a ":${dist_cv_build_issue_file:-no}" != :no ; then
	    dist_cv_build_flavor=$(os_get_name "$(cat $dist_cv_build_issue_file | grep 'Linux\|Fedora\|Ubuntu' | head -1)")
	fi
	# do debian after lsb and issue for Ubuntu
	if test -z "$dist_cv_build_flavor" -a ":${dist_cv_build_rel_file:-no}" != :no ; then
	    if test `echo "$dist_cv_build_rel_file" | sed -e 's|.*/||'` = 'debian_version' ; then
		dist_cv_build_flavor='debian'
	    fi
	fi
	# FIXME
	if test -z "$dist_cv_build_flavor" ; then
	    dist_cv_build_flavor=$(os_get_name "$(${CC-cc} $CFLAGS -v 2>&1 | grep 'gcc version')")
	fi

	# save the result
	if test -n "$dist_cv_build_flavor" ; then
		OS_NAME=$dist_cv_build_flavor
		PKG_TARGET=$(os_get_target "$dist_cv_build_flavor")
	fi
    ])
])# NUT_CHECK_OS


dnl checking for OS information file {/etc/lsb-release, /etc/xxx_version, /etc/issue, ...)
dnl Checking for host system name

dnl get the base type (linux, ...) from uname,
dnl then check the exact linux type?!
dnl FIXME: consider cross pf target

dnl detect build env (pbuilder, .rpm, ...)
