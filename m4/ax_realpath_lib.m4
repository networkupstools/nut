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
        ],
        [*darwin*], [
            dnl # Can be a non-GNU ld
            AS_IF([test x"${LD-}" = x],
                [AC_CHECK_TOOLS([LD], [ld], [false])])
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
            [*.so*|*.a|*.o|*.lo|*.la|*.dll|*.dll.a|*.lib|*.dylib|*.sl], [],
            [
                AS_CASE(["${target_os}"],
                    [*mingw*],  [myLIBNAME="${myLIBNAME}.dll"],
                    [*darwin*], [myLIBNAME="${myLIBNAME}.dylib"],
                    [*hpux*|*hp?ux*], [
                                 dnl # See detailed comments in nut_platform.h
                                 myLIBNAME="${myLIBNAME}.sl"
                                ],[myLIBNAME="${myLIBNAME}.so"])
            ]
        )

        AS_CASE(["${myLIBNAME}"],
            [lib*], [
                dnl Alas, the portable solution with sed is to avoid
                dnl parentheses and pipe chars, got too many different
                dnl ways to escape them in the wild
                myLIBNAME_LD="`echo "$myLIBNAME" | sed -e 's/^lib/-l/' -e 's/\.dll$//' -e 's/\.dll\.a$//' -e 's/\.a$//' -e 's/\.o$//' -e 's/\.la$//' -e 's/\.lo$//' -e 's/\.lib$//' -e 's/\.dylib$//' -e 's/\.so\..*$//' -e 's/\.so//' -e 's/\.sl\..*$//' -e 's/\.sl//'`"
            ], [myLIBNAME_LD="-l$myLIBNAME"]    dnl best-effort...
        )

        AC_MSG_CHECKING([for real path to $1 (re-parsed as ${myLIBNAME} likely file name / ${myLIBNAME_LD} linker arg)])
        myLIBPATH=""
        AS_CASE(["${target_os}"],
            [*mingw*], [
                AS_IF([test x"${LD}" != x -a x"${LD}" != xfalse -a x"${DLLTOOL}" != x -a x"${DLLTOOL}" != xfalse], [
                    dnl Expected output (Linux mingw cross-build) ends like:
                    dnl   ==================================================
                    dnl   x86_64-w64-mingw32-ld: mode i386pep
                    dnl   attempt to open /usr/x86_64-w64-mingw32/lib/libnetsnmp.dll.a succeeded
                    dnl   /usr/x86_64-w64-mingw32/lib/libnetsnmp.dll.a
                    dnl ...which we then resolve with dlltool into a libnetsnmp-40.dll
                    dnl and finally find one in /usr/x86_64-w64-mingw32/bin/libnetsnmp-40.dll
                    dnl (note the version number embedded into base name).
                    { myLIBPATH="`$LD --verbose -Bdynamic ${myLIBNAME_LD} | grep -wi dll | tail -1`" \
                    && test -n "${myLIBPATH}" && test -s "${myLIBPATH}" ; } \
                    || { myLIBPATH="`$LD $LDFLAGS $LIBS --verbose -Bdynamic ${myLIBNAME_LD} | grep -wi dll | tail -1`" \
                    && test -n "${myLIBPATH}" && test -s "${myLIBPATH}" ; } \
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
                                `${CC} --print-search-dirs 2>/dev/null | grep libraries: | sed 's,^@<:@^=@:>@*=,:,' | sed 's,\(@<:@:;@:>@\)\(@<:@A-Z@:>@\):/,:/\2/,g' | tr ':' '\n'` \
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
            ], [ dnl # POSIX/MacOS builds
                { myLIBPATH="`${CC} --print-file-name="$myLIBNAME"`" && test -n "$myLIBPATH" && test -s "$myLIBPATH" ; } \
                || { myLIBPATH="`${CC} $CFLAGS --print-file-name="$myLIBNAME"`" && test -n "$myLIBPATH" && test -s "$myLIBPATH" ; } \
                || { myLIBPATH="`${CC} $CFLAGS $LDFLAGS $LIBS --print-file-name="$myLIBNAME"`" && test -n "$myLIBPATH" && test -s "$myLIBPATH" ; } \
                || myLIBPATH=""
            ]
        )

        AS_IF([test -z "${myLIBPATH}" && test x"${LD}" != x -a x"${LD}" != xfalse], [
            AS_CASE(["${target_os}"],
                [*darwin*], [
                    dnl Try MacOS-style LD as fallback; expecting strings like
                    dnl   ld: warning: /usr/local/lib/libneon.dylib, ignoring unexpected dylib file
                    my_uname_m="`uname -m`"
                    { myLIBPATH="`${LD} -dynamic -r -arch "${target_cpu}" -search_dylibs_first "${myLIBNAME_LD}" 2>&1 | grep -w dylib | sed 's/^@<:@^\/@:>@*\(\/.*\.dylib\),.*$/\1/'`" && test -n "$myLIBPATH" && test -s "$myLIBPATH" ; } \
                    || { myLIBPATH="`${LD} $LDFLAGS -dynamic -r -arch "${target_cpu}" -search_dylibs_first "${myLIBNAME_LD}" 2>&1 | grep -w dylib | sed 's/^@<:@^\/@:>@*\(\/.*\.dylib\),.*$/\1/'`" && test -n "$myLIBPATH" && test -s "$myLIBPATH" ; } \
                    || { myLIBPATH="`${LD} $LDFLAGS $LIBS -dynamic -r -arch "${target_cpu}" -search_dylibs_first "${myLIBNAME_LD}" 2>&1 | grep -w dylib | sed 's/^@<:@^\/@:>@*\(\/.*\.dylib\),.*$/\1/'`" && test -n "$myLIBPATH" && test -s "$myLIBPATH" ; } \
                    || if test x"${target_cpu}" != x"${my_uname_m}" ; then
                        { myLIBPATH="`${LD} -dynamic -r -arch "${my_uname_m}" -search_dylibs_first "${myLIBNAME_LD}" 2>&1 | grep -w dylib | sed 's/^@<:@^\/@:>@*\(\/.*\.dylib\),.*$/\1/'`" && test -n "$myLIBPATH" && test -s "$myLIBPATH" ; } \
                        || { myLIBPATH="`${LD} $LDFLAGS -dynamic -r -arch "${my_uname_m}" -search_dylibs_first "${myLIBNAME_LD}" 2>&1 | grep -w dylib | sed 's/^@<:@^\/@:>@*\(\/.*\.dylib\),.*$/\1/'`" && test -n "$myLIBPATH" && test -s "$myLIBPATH" ; } \
                        || { myLIBPATH="`${LD} $LDFLAGS $LIBS -dynamic -r -arch "${my_uname_m}" -search_dylibs_first "${myLIBNAME_LD}" 2>&1 | grep -w dylib | sed 's/^@<:@^\/@:>@*\(\/.*\.dylib\),.*$/\1/'`" && test -n "$myLIBPATH" && test -s "$myLIBPATH" ; } \
                        || myLIBPATH=""
                    else
                        myLIBPATH=""
                    fi
                    rm -f a.out 2>/dev/null || true
                    unset my_uname_m
                ]
            )
        ])

        AS_IF([test -n "${myLIBPATH}" && test -s "${myLIBPATH}"], [
            AC_MSG_RESULT([initially '${myLIBPATH}'])

            AC_MSG_CHECKING([whether the file is a "GNU ld script" and not a binary])
            AS_IF([LANG=C LC_ALL=C file "${myLIBPATH}" | grep -Ei '(ascii|text)' && grep -w GROUP "${myLIBPATH}" >/dev/null], [
                # dnl e.g. # cat /usr/lib/x86_64-linux-gnu/libusb.so
                # dnl    /* GNU ld script.  */
                # dnl    GROUP ( /lib/x86_64-linux-gnu/libusb-0.1.so.4.4.4 )
                # dnl Note that spaces around parentheses vary, more keywords
                # dnl may be present in a group (e.g. AS_NEEDED), and comment
                # dnl strings are inconsistent (useless to match by).
                AC_MSG_RESULT([yes, iterate further])
                myLIBPATH_LDSCRIPT="`grep -w GROUP "${myLIBPATH}" | sed 's,^.*GROUP *( *\(/@<:@^ @:>@*\.so@<:@^ @:>@*\)@<:@^0-9a-zA-Z_.-@:>@.*$,\1,'`"
                AS_IF([test -n "${myLIBPATH_LDSCRIPT}" && test -s "${myLIBPATH_LDSCRIPT}"], [
                    AC_MSG_NOTICE([will dig into ${myLIBPATH_LDSCRIPT}])

                    dnl # See detailed comments just below
                    myLIBPATH_REAL="${myLIBPATH_LDSCRIPT}"
                    AX_REALPATH([${myLIBPATH_LDSCRIPT}], [myLIBPATH_REAL])
                ], [
                    AC_MSG_NOTICE([could not determine a further path name, will use what we have])

                    dnl # See detailed comments just below
                    myLIBPATH_REAL="${myLIBPATH}"
                    AX_REALPATH([${myLIBPATH}], [myLIBPATH_REAL])
                ])
            ],[
                AC_MSG_RESULT([no, seems like a normal binary])

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
            ])

            AC_MSG_RESULT(${myLIBPATH_REAL})
            $2="${myLIBPATH_REAL}"
        ],[
            AC_MSG_RESULT([not found])
            $2="$3"
        ])
    ],
    [AC_MSG_WARN([Compiler not detected as GCC/CLANG-compatible, skipping REALPATH_LIB($1)])
     $2="$3"
    ])
])
