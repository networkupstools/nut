dnl Resolve real path (if we can) to a library file that would be used
dnl by gcc/clang compatible toolkit. Relies on AX_REALPATH macro and
dnl may benefit from a REALPATH envvar pointing to a command-line tool
dnl (otherwise m4/shell implementation gets used), and NUT_COMPILER_FAMILY.
dnl Copyright (C) 2024 by Jim Klimov <jimklimov+nut@gmail.com>
dnl Licensed under the terms of GPLv2 or newer.

dnl Calling script is welcome to pre-detect external REALPATH implementation,
dnl otherwise shell implementation would be used (hopefully capable enough):
dnl AC_CHECK_PROGS([REALPATH], [realpath], [])

AC_DEFUN([AX_REALPATH_LIB_PREREQ],
[
if test -z "${nut_ax_realpath_lib_prereq_seen}"; then
    nut_ax_realpath_lib_prereq_seen=yes

    AC_REQUIRE([NUT_COMPILER_FAMILY])dnl
    AS_CASE(["${target_os}"],
        [*mingw*], [
            AS_IF([test x"${DLLTOOL-}" = x],
                [AC_CHECK_TOOLS([DLLTOOL], [dlltool dlltool.exe], [false])])
            AS_IF([test x"${OBJDUMP-}" = x],
                [AC_CHECK_TOOLS([OBJDUMP], [objdump objdump.exe], [false])])
            dnl # Assuming mingw (sourceware) x86_64-w64-mingw32-ld or similar
            AS_IF([test x"${LD-}" = x],
                [AC_CHECK_TOOLS([LD], [ld ld.exe], [false])])
        ]
    )
fi
])

AC_DEFUN([AX_REALPATH_LIB],
[
    dnl # resolve libname - #1 value (not quoted by caller) if we can;
    dnl # save into varname #2.
    dnl # In case of problems return #3 (optional, defaults to empty).
    AC_REQUIRE([AX_REALPATH_LIB_PREREQ])dnl

    AS_IF([test x"$1" = x], [AC_MSG_ERROR([Bad call to REALPATH_LIB macro (arg1)])])
    AS_IF([test x"$2" = x], [AC_MSG_ERROR([Bad call to REALPATH_LIB macro (arg2)])])

    AS_IF([test "x$GCC" = xyes -o "x$CLANGCC" = xyes], [
        myLIBNAME="$1"
        AS_CASE(["${myLIBNAME}"],
            [-l*], [myLIBNAME="`echo "$myLIBNAME" | sed 's/^-l/lib/'`"]
        )

        dnl # Primarily we care to know dynamically linked (shared object)
        dnl # files, so inject the extension to the presumed base name
        AS_CASE(["${myLIBNAME}"],
            [*.so*|*.a|*.o|*.lo|*.la|*.dll|*.dll.a|*.lib], [],
            [
                AS_CASE(["${target_os}"],
                    [*mingw*], [myLIBNAME="${myLIBNAME}.dll"],
                               [myLIBNAME="${myLIBNAME}.so"])
            ]
        )

        AC_MSG_CHECKING([for real path to $1 (${myLIBNAME})])
        myLIBPATH=""
        AS_CASE(["${target_os}"],
            [*mingw*], [
                AS_IF([test x"${LD}" != x -a x"${LD}" != xfalse -a x"${DLLTOOL}" != x -a x"${DLLTOOL}" != xfalse], [
                    AS_CASE(["${myLIBNAME}"],
                        [lib*], [
                            myLIBNAME_LD="`echo "$myLIBNAME" | sed -e 's/^lib/-l/' -e 's/\.\(dll\|dll\.a\|a\)$//i'`"
                        ], [myLIBNAME_LD="-l$myLIBNAME"]    dnl best-effort...
                    )

                    dnl Expected output (Linux mingw cross-build) ends like:
                    dnl   ==================================================
                    dnl   x86_64-w64-mingw32-ld: mode i386pep
                    dnl   attempt to open /usr/x86_64-w64-mingw32/lib/libnetsnmp.dll.a succeeded
                    dnl   /usr/x86_64-w64-mingw32/lib/libnetsnmp.dll.a
                    dnl ...which we then resolve with dlltool into a libnetsnmp-40.dll
                    dnl and finally find one in /usr/x86_64-w64-mingw32/bin/libnetsnmp-40.dll
                    dnl (note the version number embedded into base name).
                    myLIBPATH="`$LD --verbose -Bdynamic ${myLIBNAME_LD} | grep -wi dll | tail -1`" \
                    && test -n "${myLIBPATH}" && test -s "${myLIBPATH}" \
                    || myLIBPATH=""

                    dnl Resolve dynamic library "internal" name from its stub, if needed
                    AS_CASE(["${myLIBPATH}"],
                        [*.dll.a], [
                            myLIBPATH="`$DLLTOOL -I "${myLIBPATH}"`" || myLIBPATH=""
                            ]
                    )

                    dnl Internal name may be just that, with no path info
                    AS_CASE(["${myLIBPATH}"],
                        [*/*], [],
                        [
                            for D in \
                                "/usr/${target}/bin" \
                                "/usr/${target}/lib" \
                                "${MSYSTEM_PREFIX}/bin" \
                                "${MSYSTEM_PREFIX}/lib" \
                                "${MINGW_PREFIX}/bin" \
                                "${MINGW_PREFIX}/lib" \
                                `${CC} --print-search-dirs 2>/dev/null | grep libraries: | sed 's,^libraries: *=/,/,'` \
                            ; do
                                if test -s "$D/${myLIBPATH}" 2>/dev/null ; then
                                    myLIBPATH="$D/${myLIBPATH}"
                                    break
                                fi
                            done
                            unset D
                        ]
                    )
                ])
            ], [ dnl # POSIX builds
                myLIBPATH="`${CC} --print-file-name="$myLIBNAME"`" || myLIBPATH=""
            ])

        AS_IF([test -n "${myLIBPATH}" && test -s "${myLIBPATH}"], [
            AC_MSG_RESULT([initially '${myLIBPATH}'])

            dnl # Resolving the directory location is a nice bonus
            dnl # (usually the paths are relative to toolkit and ugly,
            dnl # though maybe arguably portable with regard to symlinks).
            dnl # The primary goal is to resolve the actual library file
            dnl # name like "libnetsnmp.so.1.2.3", so we can preferentially
            dnl # try to dlopen() it on a system with a packaged footprint
            dnl # that does not serve short (developer-friendly) links like
            dnl # "libnetsnmp.so".
            myLIBPATH_REAL="${myLIBPATH}"
            AX_REALPATH([${myLIBPATH}], [myLIBPATH_REAL])
            AC_MSG_RESULT(${myLIBPATH_REAL})
            $2="${myLIBPATH_REAL}"
            ],[
            AC_MSG_RESULT([not found])
            $2="$3"
            ])
        ],
        [AC_MSG_WARN([Compiler not detected as GCC/CLANG-compatible, skipping REALPATH_LIB($1)])
         $2="$3"
        ]
    )
])
