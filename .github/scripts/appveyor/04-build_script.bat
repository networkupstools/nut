REM Network UPS Tools: AppVeyor CI recipe step (see appveyor.yml), NUT for Windows
REM with MSYS2/MinGW x64. Called by AppVeyor with the build folder as current
REM directory. AppVeyor checks each line of inline yml scripts, but only the
REM exit code of a called .bat file, so we check each command here.
REM
REM Copyright (C) 2022-2026	Jim Klimov <jimklimov+nut@gmail.com>

REM Preserve the current working directory:
set CHERE_INVOKING=yes
REM Start a 64 bit Mingw environment:
set MSYSTEM=MINGW64
REM Note: currently we save job time and do not install asciidoc/a2x
REM  # --with-docs="man=auto html-single=auto html-chunked=no pdf=no"
REM The resulting configuration defaults to --prefix=/mingw{32,64}
REM as appropriate for the platform, which ends up as a subdirectory
REM under DESTDIR when we `make install` later, and in our 7z archive.
REM The builds are relocatable however, so end-users can extract to
REM e.g. "C:\Program Files\NUT" and forfeit "mingwXX" part, probably,
REM but this can complicate automatic relative directory resolution
REM to find "nearby" program or configuration files (see common.c
REM for current implementation). Hard-coded fallback strings may
REM end up getting used in those cases.
REM Time limit: see CI_DEADLINE_EPOCH in appveyor.yml
C:\msys64\usr\bin\bash -lc 'date -u; R=$(( ${CI_DEADLINE_EPOCH:-EPOCHSECONDS+3600} - EPOCHSECONDS )) ; echo "CI deadline: $R seconds left" ; if [ "$R" -le 0 ] ; then exit 124 ; fi ; PATH="/mingw64/bin:$PATH" CI_SKIP_CHECK=true CANBUILD_WITH_LIBMODBUS_USB=yes WITH_LIBNUTPRIVATE=true NUT_SSL_VARIANTS=nss timeout -k 30 "$R" ./ci_build.sh --with-docs=no'
if errorlevel 1 exit /b %ERRORLEVEL%
