dnl Check for python binary program names per language version
dnl to embed into scripts and Make rules

AC_DEFUN([NUT_CHECK_PYTHON_DEFAULT],
[
    dnl Check for all present variants and pick the default PYTHON
    AC_REQUIRE([NUT_CHECK_PYTHON])
    AC_REQUIRE([NUT_CHECK_PYTHON2])
    AC_REQUIRE([NUT_CHECK_PYTHON3])

    AS_IF([test x"$PYTHON2" = xno], [PYTHON2=""])
    AS_IF([test x"$PYTHON3" = xno], [PYTHON3=""])
    AS_IF([test x"$PYTHON" = xno],  [PYTHON=""])
    AS_IF([test x"$PYTHON" = x], [
        AC_MSG_CHECKING([which python version to use by default])
        dnl Last hit wins (py3)
        AS_IF([test x"$PYTHON2" != x], [PYTHON="$PYTHON2"])
        AS_IF([test x"$PYTHON3" != x], [PYTHON="$PYTHON3"])
        AS_IF([test x"$PYTHON" = x],
            [AC_MSG_RESULT([none])],
            [AC_MSG_RESULT([$PYTHON])
             AC_MSG_WARN([A python program name was not specified during configuration, will default to '$PYTHON' (derived from --with-python2 or --with-python3 setting)])
            ])
        ])

    AS_IF([test -z "${PYTHON3}" && test x"${nut_with_python3}" = xyes], [
        AC_MSG_ERROR([A python3 interpreter was required but not found or validated])
        ])

    AS_IF([test -z "${PYTHON2}" && test x"${nut_with_python2}" = xyes], [
        AC_MSG_ERROR([A python2 interpreter was required but not found or validated])
        ])

    AS_IF([test -z "${PYTHON}" && test x"${nut_with_python}" = xyes], [
        AC_MSG_ERROR([A python interpreter was required but not found or validated])
        ])
])

dnl Note: this checks for default/un-versioned python version
dnl as the --with-python=SHEBANG_PATH setting into the PYTHON
dnl variable; it may be further tweaked by NUT_CHECK_PYTHON_DEFAULT
AC_DEFUN([NUT_CHECK_PYTHON],
[
    AS_IF([test -z "${nut_with_python}"], [
        NUT_ARG_WITH([python], [Use a particular program name of the python interpeter], [auto])

        PYTHON=""
        PYTHON_SITE_PACKAGES=""
        PYTHON_VERSION_REPORT=""
        PYTHON_VERSION_INFO_REPORT=""
        PYTHON_SYSPATH_REPORT=""
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
            [*], [
                dnl Note: no "realpath" here, see comment below
                myPYTHON="`command -v "${PYTHON}" 2>/dev/null`" && test -n "${myPYTHON}" && test -x "${myPYTHON}" \
                && PYTHON="${myPYTHON}" \
                || PYTHON="/usr/bin/env ${PYTHON}"
                unset myPYTHON
                ]
        )

        dnl Note: requesting e.g. "--with-python=python3" is valid,
        dnl but would likely use a symlink that changes over time -
        dnl and if `env` gets used, can resolve according to PATH
        dnl (by default we try to bolt pathname here if resolvable,
        dnl but do not unwrap the chain of symlinks like we do for
        dnl versioned "--with-python2/3" due to their site-packages).
        dnl For some use-cases, this lack of constraints may be
        dnl deliberately desired; for others it is a "caveat emptor!"
        AS_CASE(["${PYTHON}"],
            [*2.*|*3.*|no], [],
            [AC_MSG_WARN([A python program name without a specific version number was requested (may be a symlink prone to change over time): ${PYTHON}])])

        AS_CASE(["${PYTHON}"],
            [/usr/bin/env*], [AC_MSG_WARN([A python program will be resolved from PATH at run-time (PyNUT module may be not found if installed into site-packages of a specific version): ${PYTHON}])])

        AS_IF([test -n "${PYTHON}" && test "${PYTHON}" != "no"], [
            AS_IF([test x"`$PYTHON -c 'import sys; print (sys.version_info >= (2, 6))'`" = xTrue],
                [PYTHON_VERSION_INFO_REPORT=" (`$PYTHON -c 'import sys; print (sys.version_info)'`)"],
                [AC_MSG_WARN([Version reported by ${PYTHON} was not suitable as python])
                 PYTHON=no])
            ])

        dnl Unfulfilled "yes" is re-tested in NUT_CHECK_PYTHON_DEFAULT
        AS_IF([test -z "${PYTHON}" || test "${PYTHON}" = "no"], [
            AS_CASE([${nut_with_python}],
                [auto|yes|no|""], [],
                [AC_MSG_ERROR([A python interpreter was required but not found or validated: ${nut_with_python}])])
            ])

        AC_MSG_CHECKING([python interpeter to call])
        AC_MSG_RESULT([${PYTHON}${PYTHON_VERSION_INFO_REPORT}])
        AC_SUBST([PYTHON], [${PYTHON}])
        AM_CONDITIONAL([HAVE_PYTHON], [test -n "${PYTHON}" && test "${PYTHON}" != "no"])
        AS_IF([test -n "${PYTHON}" && test "${PYTHON}" != "no"], [
            AC_MSG_CHECKING([python build sys.version])
            dnl Can have extra lines about compiler used, etc.
            PYTHON_VERSION_REPORT="`${PYTHON} -c 'import sys; print(sys.version);' | tr '\n' ' '`" \
            || PYTHON_VERSION_REPORT=""
            AC_MSG_RESULT([${PYTHON_VERSION_REPORT}])

            AC_MSG_CHECKING([python sys.path used to search for modules])
            PYTHON_SYSPATH_REPORT="`${PYTHON} -c 'import sys; print(sys.path);'`" \
            || PYTHON_SYSPATH_REPORT=""
            AC_MSG_RESULT([${PYTHON_SYSPATH_REPORT}])

            export PYTHON
            AC_CACHE_CHECK([python site-packages location], [nut_cv_PYTHON_SITE_PACKAGES], [
                dnl sysconfig introduced in python3.2
                AS_IF([test x"`${PYTHON} -c 'import sys; print (sys.version_info >= (3, 2))'`" = xTrue],
                    [nut_cv_PYTHON_SITE_PACKAGES="`${PYTHON} -c 'import sysconfig; print(sysconfig.get_path("purelib"))'`"],
                    [nut_cv_PYTHON_SITE_PACKAGES="`${PYTHON} -c 'import site; print(site.getsitepackages().pop(0))'`"])
                AS_CASE(["$nut_cv_PYTHON_SITE_PACKAGES"],
                    [*:*], [
                        dnl Note: on Windows MSYS2 this embeds "C:/msys64/mingw..." into the string [nut#1584]
                        nut_cv_PYTHON_SITE_PACKAGES="`cd "$nut_cv_PYTHON_SITE_PACKAGES" && pwd`"
                        ]
                   )
               ])
            ])
        AC_SUBST([PYTHON_SITE_PACKAGES], [${nut_cv_PYTHON_SITE_PACKAGES}])
        AM_CONDITIONAL([HAVE_PYTHON_SITE_PACKAGES], [test x"${PYTHON_SITE_PACKAGES}" != "x"])
    ])
])

AC_DEFUN([NUT_CHECK_PYTHON2],
[
    AS_IF([test -z "${nut_with_python2}"], [
        NUT_ARG_WITH([python2], [Use a particular program name of the python2 interpeter for code that needs that version and is not compatible with python3], [auto])

        PYTHON2=""
        PYTHON2_SITE_PACKAGES=""
        PYTHON2_VERSION_REPORT=""
        PYTHON2_VERSION_INFO_REPORT=""
        PYTHON2_SYSPATH_REPORT=""
        AS_CASE([${nut_with_python2}],
            [auto|yes|""], [
                dnl Cross check --with-python results:
                AS_CASE(["${PYTHON_VERSION_INFO_REPORT}"],
                    [*major=2,*], [
                        PYTHON2="`${PYTHON} -c 'import sys; print(sys.executable);' 2>/dev/null`" && test -n "${PYTHON2}" || PYTHON2="${PYTHON}"
                        PYTHON2="`realpath "${PYTHON2}" 2>/dev/null`" && test -n "${PYTHON2}" || {
                            PYTHON2="${PYTHON}"
                            PYTHON_CONFIG="`command -v "${PYTHON}-config" 2>/dev/null`" || PYTHON_CONFIG=""
                            if test -n "${PYTHON_CONFIG}" ; then
                                mySHEBANG_SCRIPT="`${PYTHON_CONFIG} --config-dir 2>/dev/null`/python-config.py" \
                                || mySHEBANG_SCRIPT="${PYTHON_CONFIG}"
                                if test -f "${mySHEBANG_SCRIPT}" ; then
                                    mySHEBANG="`head -1 "${mySHEBANG_SCRIPT}" | grep -E '^#!'`" || mySHEBANG=""
                                    if test -n "${mySHEBANG}" ; then
                                        PYTHON2="`echo "${mySHEBANG}" | sed 's,^#! *,,'`" \
                                        && test -n "${PYTHON2}" || PYTHON2="${PYTHON}"
                                    fi
                                fi
                            fi
                            unset mySHEBANG_SCRIPT
                            unset mySHEBANG
                            unset PYTHON_CONFIG
                        }

                        dnl Only accept fully qualified names that refer to expected
                        dnl python version or quietly fall back to search below:
                        AS_CASE(["${PYTHON2}"],
                            [/usr/bin/env*], [PYTHON2=""],
                            [/*py*2.*], [AC_MSG_WARN([A python2 program name was not specified during configuration, will default to '$PYTHON2' (derived from --with-python setting which has a suitable version)])],
                            [/*py*2*], [AC_MSG_WARN([A python2 program name was not specified during configuration, will default to '$PYTHON2' (derived from --with-python setting which has a suitable version, but without a specific version number - so may be a symlink prone to change over time)])],
                            [PYTHON2=""])
                    ])
                AS_IF([test x"${PYTHON2}" = x], [
                    AC_CHECK_PROGS([PYTHON2], [python2 python2.7 python-2.7 python], [_python2_runtime])
                    ])
                ],
            [no], [PYTHON2="no"],
            [PYTHON2="${nut_with_python2}"]
        )

        dnl Default to calling a basename from PATH, only use a specific full pathname
        dnl if provided by the caller:
        AS_CASE([${PYTHON2}],
            [_python2_runtime], [
                PYTHON2="/usr/bin/env python2"
                AC_MSG_WARN([A python2 program name was not detected during configuration, will default to '$PYTHON2' (scripts will fail if that is not in PATH at run time)])
                ],
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
            [*], [
                myPYTHON="`command -v "${PYTHON2}" 2>/dev/null`" && test -n "${myPYTHON}" && test -x "${myPYTHON}" \
                && PYTHON2="${myPYTHON}" \
                || PYTHON2="/usr/bin/env ${PYTHON2}"
                unset myPYTHON
                ]
        )

        AS_IF([test -n "${PYTHON2}" && test "${PYTHON2}" != "no"], [
            AS_IF([test x"`$PYTHON2 -c 'import sys; print (sys.version_info >= (2, 6) and sys.version_info < (3, 0))'`" = xTrue],
                [PYTHON2_VERSION_INFO_REPORT=" (`$PYTHON2 -c 'import sys; print (sys.version_info)'`)"],
                [AC_MSG_WARN([Version reported by ${PYTHON2} was not suitable as python2])
                 PYTHON2=no])
            ])

        dnl Unfulfilled "yes" is re-tested in NUT_CHECK_PYTHON_DEFAULT
        AS_IF([test -z "${PYTHON2}" || test "${PYTHON2}" = "no"], [
            AS_CASE([${nut_with_python2}],
                [auto|yes|no|""], [],
                [AC_MSG_ERROR([A python2 interpreter was required but not found or validated: ${nut_with_python2}])])
            ])

        AC_MSG_CHECKING([python2 interpeter to call])
        AC_MSG_RESULT([${PYTHON2}${PYTHON2_VERSION_INFO_REPORT}])
        AC_SUBST([PYTHON2], [${PYTHON2}])
        AM_CONDITIONAL([HAVE_PYTHON2], [test -n "${PYTHON2}" && test "${PYTHON2}" != "no"])
        AS_IF([test -n "${PYTHON2}" && test "${PYTHON2}" != "no"], [
            AC_MSG_CHECKING([python2 build sys.version])
            dnl Can have extra lines about compiler used, etc.
            PYTHON2_VERSION_REPORT="`${PYTHON2} -c 'import sys; print(sys.version);' | tr '\n' ' '`" \
            || PYTHON2_VERSION_REPORT=""
            AC_MSG_RESULT([${PYTHON2_VERSION_REPORT}])

            AC_MSG_CHECKING([python2 sys.path used to search for modules])
            PYTHON2_SYSPATH_REPORT="`${PYTHON2} -c 'import sys; print(sys.path);'`" \
            || PYTHON2_SYSPATH_REPORT=""
            AC_MSG_RESULT([${PYTHON2_SYSPATH_REPORT}])

            export PYTHON2
            AC_CACHE_CHECK([python2 site-packages location], [nut_cv_PYTHON2_SITE_PACKAGES], [
                nut_cv_PYTHON2_SITE_PACKAGES="`${PYTHON2} -c 'import site; print(site.getsitepackages().pop(0))'`"
                AS_CASE(["$nut_cv_PYTHON2_SITE_PACKAGES"],
                    [*:*], [
                        dnl Note: on Windows MSYS2 this embeds "C:/msys64/mingw..." into the string [nut#1584]
                        nut_cv_PYTHON2_SITE_PACKAGES="`cd "$nut_cv_PYTHON2_SITE_PACKAGES" && pwd`"
                        ]
                    )
                ])
            ])
        AC_SUBST([PYTHON2_SITE_PACKAGES], [${nut_cv_PYTHON2_SITE_PACKAGES}])
        AM_CONDITIONAL([HAVE_PYTHON2_SITE_PACKAGES], [test x"${PYTHON2_SITE_PACKAGES}" != "x"])
    ])
])

AC_DEFUN([NUT_CHECK_PYTHON3],
[
    AS_IF([test -z "${nut_with_python3}"], [
        NUT_ARG_WITH([python3], [Use a particular program name of the python3 interpeter for code that needs that version and is not compatible with python2], [auto])

        PYTHON3=""
        PYTHON3_SITE_PACKAGES=""
        PYTHON3_VERSION_REPORT=""
        PYTHON3_VERSION_INFO_REPORT=""
        PYTHON3_SYSPATH_REPORT=""
        AS_CASE([${nut_with_python3}],
            [auto|yes|""], [
                dnl Cross check --with-python results:
                AS_CASE(["${PYTHON_VERSION_INFO_REPORT}"],
                    [*major=3,*], [
                        PYTHON3="`${PYTHON} -c 'import sys; print(sys.executable);' 2>/dev/null`" && test -n "${PYTHON3}" || PYTHON3="${PYTHON}"
                        PYTHON3="`realpath "${PYTHON3}" 2>/dev/null`" && test -n "${PYTHON3}" || {
                            PYTHON3="${PYTHON}"
                            PYTHON_CONFIG="`command -v "${PYTHON}-config" 2>/dev/null`" || PYTHON_CONFIG=""
                            if test -n "${PYTHON_CONFIG}" ; then
                                mySHEBANG_SCRIPT="`${PYTHON_CONFIG} --config-dir 2>/dev/null`/python-config.py" \
                                || mySHEBANG_SCRIPT="${PYTHON_CONFIG}"
                                if test -f "${mySHEBANG_SCRIPT}" ; then
                                    mySHEBANG="`head -1 "${mySHEBANG_SCRIPT}" | grep -E '^#!'`" || mySHEBANG=""
                                    if test -n "${mySHEBANG}" ; then
                                        PYTHON3="`echo "${mySHEBANG}" | sed 's,^#! *,,'`" \
                                        && test -n "${PYTHON3}" || PYTHON3="${PYTHON}"
                                    fi
                                fi
                            fi
                            unset mySHEBANG_SCRIPT
                            unset mySHEBANG
                            unset PYTHON_CONFIG
                        }

                        dnl Only accept fully qualified names that refer to expected
                        dnl python version or quietly fall back to search below:
                        AS_CASE(["${PYTHON3}"],
                            [/usr/bin/env*], [PYTHON3=""],
                            [/*py*3.*], [AC_MSG_WARN([A python3 program name was not specified during configuration, will default to '$PYTHON3' (derived from --with-python setting which has a suitable version)])],
                            [/*py*3*], [AC_MSG_WARN([A python3 program name was not specified during configuration, will default to '$PYTHON3' (derived from --with-python setting which has a suitable version, but without a specific version number - so may be a symlink prone to change over time)])],
                            [PYTHON3=""])
                    ])
                AS_IF([test x"${PYTHON3}" = x], [
                    AC_CHECK_PROGS([PYTHON3], [python3 python3.14 python-3.14 python3.13 python-3.13 python3.12 python-3.12 python3.11 python-3.11 python3.10 python-3.10 python3.9 python-3.9 python3.7 python-3.7 python3.6 python-3.6 python3.5 python-3.5 python], [_python3_runtime])
                    ])
                ],
            [no], [PYTHON3="no"],
            [PYTHON3="${nut_with_python3}"]
        )

        dnl Default to calling a basename from PATH, only use a specific full pathname
        dnl if provided by the caller:
        AS_CASE([${PYTHON3}],
            [_python3_runtime], [
                PYTHON3="/usr/bin/env python3"
                AC_MSG_WARN([A python3 program name was not detected during configuration, will default to '$PYTHON3' (scripts will fail if that is not in PATH at run time)])
                ],
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
            [*], [
                myPYTHON="`command -v "${PYTHON3}" 2>/dev/null`" && test -n "${myPYTHON}" && test -x "${myPYTHON}" \
                && PYTHON3="${myPYTHON}" \
                || PYTHON3="/usr/bin/env ${PYTHON3}"
                unset myPYTHON
                ]
        )

        AS_IF([test -n "${PYTHON3}" && test "${PYTHON3}" != "no"], [
            AS_IF([test x"`$PYTHON3 -c 'import sys; print (sys.version_info >= (3, 0))'`" = xTrue],
                [PYTHON3_VERSION_INFO_REPORT=" (`$PYTHON3 -c 'import sys; print (sys.version_info)'`)"],
                [AC_MSG_WARN([Version reported by ${PYTHON3} was not suitable as python3])
                 PYTHON3=no])
            ])

        dnl Unfulfilled "yes" is re-tested in NUT_CHECK_PYTHON_DEFAULT
        AS_IF([test -z "${PYTHON3}" || test "${PYTHON3}" = "no"], [
            AS_CASE([${nut_with_python3}],
                [auto|yes|no|""], [],
                [AC_MSG_ERROR([A python3 interpreter was required but not found or validated: ${nut_with_python3}])])
            ])

        AC_MSG_CHECKING([python3 interpeter to call])
        AC_MSG_RESULT([${PYTHON3}${PYTHON3_VERSION_INFO_REPORT}])
        AC_SUBST([PYTHON3], [${PYTHON3}])
        AM_CONDITIONAL([HAVE_PYTHON3], [test -n "${PYTHON3}" && test "${PYTHON3}" != "no"])
        AS_IF([test -n "${PYTHON3}" && test "${PYTHON3}" != "no"], [
            AC_MSG_CHECKING([python3 build sys.version])
            dnl Can have extra lines about compiler used, etc.
            PYTHON3_VERSION_REPORT="`${PYTHON3} -c 'import sys; print(sys.version);' | tr '\n' ' '`" \
            || PYTHON3_VERSION_REPORT=""
            AC_MSG_RESULT([${PYTHON3_VERSION_REPORT}])

            AC_MSG_CHECKING([python3 sys.path used to search for modules])
            PYTHON3_SYSPATH_REPORT="`${PYTHON3} -c 'import sys; print(sys.path);'`" \
            || PYTHON3_SYSPATH_REPORT=""
            AC_MSG_RESULT([${PYTHON3_SYSPATH_REPORT}])

            export PYTHON3
            AC_CACHE_CHECK([python3 site-packages location], [nut_cv_PYTHON3_SITE_PACKAGES], [
                dnl sysconfig introduced in python3.2
                AS_IF([test x"`${PYTHON3} -c 'import sys; print (sys.version_info >= (3, 2))'`" = xTrue],
                    [nut_cv_PYTHON3_SITE_PACKAGES="`${PYTHON3} -c 'import sysconfig; print(sysconfig.get_path("purelib"))'`"],
                    [nut_cv_PYTHON3_SITE_PACKAGES="`${PYTHON3} -c 'import site; print(site.getsitepackages().pop(0))'`"])
                AS_CASE(["$nut_cv_PYTHON3_SITE_PACKAGES"],
                    [*:*], [
                        dnl Note: on Windows MSYS2 this embeds "C:/msys64/mingw..." into the string [nut#1584]
                        nut_cv_PYTHON3_SITE_PACKAGES="`cd "$nut_cv_PYTHON3_SITE_PACKAGES" && pwd`"
                        ]
                    )
                ])
            ])
        AC_SUBST([PYTHON3_SITE_PACKAGES], [${nut_cv_PYTHON3_SITE_PACKAGES}])
        AM_CONDITIONAL([HAVE_PYTHON3_SITE_PACKAGES], [test x"${PYTHON3_SITE_PACKAGES}" != "x"])
    ])
])
