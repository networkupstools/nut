dnl Check for python binary program names per language version
dnl to embed into scripts and Make rules

AC_DEFUN([NUT_CHECK_PYTHON],
[
    AS_IF([test -z "${nut_with_python}"], [
        NUT_ARG_WITH([python], [Use a particular program name of the python interpeter], [auto])

        PYTHON=""
        PYTHON_SITE_PACKAGES=""
        AS_CASE([${nut_with_python}],
            [auto|yes|""], [AC_CHECK_PROGS([PYTHON], [python python3 python2], [_python_runtime])],
            [no], [PYTHON="no"],
            [PYTHON="${nut_with_python}"]
        )

        dnl Default to calling a basename from PATH, only use a specific full pathname
        dnl if provided by the caller:
        AS_CASE([${PYTHON}],
            [_python_runtime], [
                PYTHON="/usr/bin/env python"
                AC_MSG_WARN([A python program name was not detected during configuration, will default to '$PYTHON' (scripts will fail if that is not in PATH at run time)])],
            [no], [],
            [/*" "*" "*], [
                AC_MSG_WARN([A python program name is not a single token (was specified with more than one argument?), so shebangs can be not reliable])
                ],
            [/*], [],
            [*" "*" "*], [
                AC_MSG_WARN([A python program name is not a single token (was specified with more than one argument?), so shebangs can be not reliable])
                PYTHON="/usr/bin/env ${PYTHON}"
                ],
            [*" "*], [
                AC_MSG_WARN([A python program name is not a single token (was specified with an argument?), so /usr/bin/env shebangs can be not reliable])
                PYTHON="/usr/bin/env ${PYTHON}"
                ],
            [*], [PYTHON="/usr/bin/env ${PYTHON}"]
        )

        PYTHON_VERSION_REPORT=""
        AS_IF([test -n "${PYTHON}"], [
            AS_IF([test x"`$PYTHON -c 'import sys; print (sys.version_info >= (2, 6))'`" = xTrue],
                [PYTHON_VERSION_REPORT=" (`$PYTHON -c 'import sys; print ("%s.%s.%s" % sys.version_info[:3])'`)"], [PYTHON=no])
            ])

        AC_MSG_CHECKING([python interpeter to call])
        AC_MSG_RESULT([${PYTHON}${PYTHON_VERSION_REPORT}])
        AC_SUBST([PYTHON], [${PYTHON}])
        AM_CONDITIONAL([HAVE_PYTHON], [test "${PYTHON}" != "no"])
        AS_IF([test -n "${PYTHON}"], [
            export PYTHON
            AC_MSG_CHECKING([python site-packages location])
            PYTHON_SITE_PACKAGES="`${PYTHON} -c 'import site; print(site.getsitepackages().pop(0))'`"
            AS_CASE(["$PYTHON_SITE_PACKAGES"],
                [*:*], [
                    dnl Note: on Windows MSYS2 this embeds "C:/msys64/mingw..." into the string [nut#1584]
                    PYTHON_SITE_PACKAGES="`cd "$PYTHON_SITE_PACKAGES" && pwd`"
                    ]
                )
            AC_MSG_RESULT([${PYTHON_SITE_PACKAGES}])
            ])
        AC_SUBST([PYTHON_SITE_PACKAGES], [${PYTHON_SITE_PACKAGES}])
        AM_CONDITIONAL([HAVE_PYTHON_SITE_PACKAGES], [test x"${PYTHON_SITE_PACKAGES}" != "x"])
    ])
])

AC_DEFUN([NUT_CHECK_PYTHON2],
[
    AS_IF([test -z "${nut_with_python2}"], [
        NUT_ARG_WITH([python2], [Use a particular program name of the python2 interpeter for code that needs that version and is not compatible with python3], [auto])

        PYTHON2=""
        PYTHON2_SITE_PACKAGES=""
        AS_CASE([${nut_with_python2}],
            [auto|yes|""], [AC_CHECK_PROGS([PYTHON2], [python2 python2.7 python-2.7 python], [_python2_runtime])],
            [no], [PYTHON2="no"],
            [PYTHON2="${nut_with_python2}"]
        )

        dnl Default to calling a basename from PATH, only use a specific full pathname
        dnl if provided by the caller:
        AS_CASE([${PYTHON2}],
            [_python2_runtime], [
                PYTHON2="/usr/bin/env python2"
                AC_MSG_WARN([A python2 program name was not detected during configuration, will default to '$PYTHON2' (scripts will fail if that is not in PATH at run time)])],
            [no], [],
            [/*" "*" "*], [
                AC_MSG_WARN([A python2 program name is not a single token (was specified with more than one argument?), so shebangs can be not reliable])
                ],
            [/*], [],
            [*" "*" "*], [
                AC_MSG_WARN([A python2 program name is not a single token (was specified with more than one argument?), so shebangs can be not reliable])
                PYTHON2="/usr/bin/env ${PYTHON2}"
                ],
            [*" "*], [
                AC_MSG_WARN([A python2 program name is not a single token (was specified with an argument?), so /usr/bin/env shebangs can be not reliable])
                PYTHON2="/usr/bin/env ${PYTHON2}"
                ],
            [*], [PYTHON2="/usr/bin/env ${PYTHON2}"]
        )

        PYTHON2_VERSION_REPORT=""
        AS_IF([test -n "${PYTHON2}"], [
            AS_IF([test x"`$PYTHON2 -c 'import sys; print (sys.version_info >= (2, 6) and sys.version_info < (3, 0))'`" = xTrue],
                [PYTHON2_VERSION_REPORT=" (`$PYTHON2 -c 'import sys; print ("%s.%s.%s" % sys.version_info[:3])'`)"], [PYTHON2=no])
            ])

        AC_MSG_CHECKING([python2 interpeter to call])
        AC_MSG_RESULT([${PYTHON2}${PYTHON2_VERSION_REPORT}])
        AC_SUBST([PYTHON2], [${PYTHON2}])
        AM_CONDITIONAL([HAVE_PYTHON2], [test "${PYTHON2}" != "no"])
        AS_IF([test -n "${PYTHON2}"], [
            export PYTHON2
            AC_MSG_CHECKING([python2 site-packages location])
            PYTHON2_SITE_PACKAGES="`${PYTHON2} -c 'import site; print(site.getsitepackages().pop(0))'`"
            AS_CASE(["$PYTHON2_SITE_PACKAGES"],
                [*:*], [
                    dnl Note: on Windows MSYS2 this embeds "C:/msys64/mingw..." into the string [nut#1584]
                    PYTHON2_SITE_PACKAGES="`cd "$PYTHON2_SITE_PACKAGES" && pwd`"
                    ]
                )
            AC_MSG_RESULT([${PYTHON2_SITE_PACKAGES}])
            ])
        AC_SUBST([PYTHON2_SITE_PACKAGES], [${PYTHON2_SITE_PACKAGES}])
        AM_CONDITIONAL([HAVE_PYTHON2_SITE_PACKAGES], [test x"${PYTHON2_SITE_PACKAGES}" != "x"])
    ])
])

AC_DEFUN([NUT_CHECK_PYTHON3],
[
    AS_IF([test -z "${nut_with_python3}"], [
        NUT_ARG_WITH([python3], [Use a particular program name of the python3 interpeter for code that needs that version and is not compatible with python2], [auto])

        PYTHON3=""
        PYTHON3_SITE_PACKAGES=""
        AS_CASE([${nut_with_python3}],
            [auto|yes|""], [AC_CHECK_PROGS([PYTHON3], [python3 python3.9 python-3.9 python3.7 python-3.7 python3.5 python-3.5 python], [_python3_runtime])],
            [no], [PYTHON3="no"],
            [PYTHON3="${nut_with_python3}"]
        )

        dnl Default to calling a basename from PATH, only use a specific full pathname
        dnl if provided by the caller:
        AS_CASE([${PYTHON3}],
            [_python3_runtime], [
                PYTHON3="/usr/bin/env python3"
                AC_MSG_WARN([A python3 program name was not detected during configuration, will default to '$PYTHON3' (scripts will fail if that is not in PATH at run time)])],
            [no], [],
            [/*" "*" "*], [
                AC_MSG_WARN([A python3 program name is not a single token (was specified with more than one argument?), so shebangs can be not reliable])
                ],
            [/*], [],
            [*" "*" "*], [
                AC_MSG_WARN([A python3 program name is not a single token (was specified with more than one argument?), so shebangs can be not reliable])
                PYTHON3="/usr/bin/env ${PYTHON3}"
                ],
            [*" "*], [
                AC_MSG_WARN([A python3 program name is not a single token (was specified with an argument?), so /usr/bin/env shebangs can be not reliable])
                PYTHON3="/usr/bin/env ${PYTHON3}"
                ],
            [*], [PYTHON3="/usr/bin/env ${PYTHON3}"]
        )

        PYTHON3_VERSION_REPORT=""
        AS_IF([test -n "${PYTHON3}"], [
            AS_IF([test x"`$PYTHON3 -c 'import sys; print (sys.version_info >= (3, 0))'`" = xTrue],
                [PYTHON3_VERSION_REPORT=" (`$PYTHON3 -c 'import sys; print ("%s.%s.%s" % sys.version_info[:3])'`)"], [PYTHON3=no])
            ])

        AC_MSG_CHECKING([python3 interpeter to call])
        AC_MSG_RESULT([${PYTHON3}${PYTHON3_VERSION_REPORT}])
        AC_SUBST([PYTHON3], [${PYTHON3}])
        AM_CONDITIONAL([HAVE_PYTHON3], [test "${PYTHON3}" != "no"])
        AS_IF([test -n "${PYTHON3}"], [
            export PYTHON3
            AC_MSG_CHECKING([python3 site-packages location])
            PYTHON3_SITE_PACKAGES="`${PYTHON3} -c 'import site; print(site.getsitepackages().pop(0))'`"
            AS_CASE(["$PYTHON3_SITE_PACKAGES"],
                [*:*], [
                    dnl Note: on Windows MSYS2 this embeds "C:/msys64/mingw..." into the string [nut#1584]
                    PYTHON3_SITE_PACKAGES="`cd "$PYTHON3_SITE_PACKAGES" && pwd`"
                    ]
                )
            AC_MSG_RESULT([${PYTHON3_SITE_PACKAGES}])
            ])
        AC_SUBST([PYTHON3_SITE_PACKAGES], [${PYTHON3_SITE_PACKAGES}])
        AM_CONDITIONAL([HAVE_PYTHON3_SITE_PACKAGES], [test x"${PYTHON3_SITE_PACKAGES}" != "x"])
    ])
])
