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
REM Time limits: see CI_DEADLINE_EPOCH in appveyor.yml
REM Start Mingw-based integration and unit checks:
C:\msys64\usr\bin\bash -lc 'date -u; R=$(( ${CI_DEADLINE_EPOCH:-EPOCHSECONDS+3600} - EPOCHSECONDS )) ; echo "CI deadline: $R seconds left" ; if [ "$R" -le 0 ] ; then exit 124 ; fi ; NUT_STATEPATH="C:\\Users\\appveyor\\AppData\\Local\\Temp\\nut-test"; mkdir -p "${NUT_STATEPATH}"; export NUT_STATEPATH; PATH="/mingw64/lib/ccache/bin:/mingw64/bin:$PATH" timeout -k 30 "$R" make -s check || bash -lc "for F in tests/*.log tests/*.trs ; do echo \"===---=== $F :\"; cat \"$F\"; done; exit 1;" '
if errorlevel 1 exit /b %ERRORLEVEL%
REM Start a Mingw-based documentation spellcheck (note that "make check" above could cover some documents in path of artifact delivery, but maybe not all that we have):
C:\msys64\usr\bin\bash -lc 'date -u; R=$(( ${CI_DEADLINE_EPOCH:-EPOCHSECONDS+3600} - EPOCHSECONDS )) ; echo "CI deadline: $R seconds left" ; if [ "$R" -le 0 ] ; then exit 124 ; fi ; PATH="/mingw64/lib/ccache/bin:/mingw64/bin:$PATH" timeout -k 30 "$R" make -s -j 8 spellcheck'
if errorlevel 1 exit /b %ERRORLEVEL%
