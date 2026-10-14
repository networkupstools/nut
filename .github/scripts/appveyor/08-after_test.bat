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
REM Oh the joys of shell scripting with strings passed through CMD:
REM Note: currently Python installation path with MSYS is buggy [#1584]
REM Time limit: see CI_DEADLINE_EPOCH in appveyor.yml
C:\msys64\usr\bin\bash -lc 'date -u; R=$(( ${CI_DEADLINE_EPOCH:-EPOCHSECONDS+3600} - EPOCHSECONDS )) ; echo "CI deadline: $R seconds left" ; if [ "$R" -le 0 ] ; then exit 124 ; fi ; set -e ; if ! rm -rf ".inst" ; then echo "WARNING: Failed to clean away .inst" ; fi ; PATH="/mingw64/lib/ccache/bin:/mingw64/bin:$PATH" timeout -k 30 "$R" make -s -j 8 install-win-bundle DESTDIR="`pwd`/.inst/NUT-for-Windows-x86_64-SNAPSHOT-%APPVEYOR_BUILD_VERSION%"'
if errorlevel 1 exit /b %ERRORLEVEL%
C:\msys64\usr\bin\bash -lc 'date -u; set -e ; rm -rf ./.inst/NUT-for-Windows-x86_64-SNAPSHOT || true ; ln -fs "NUT-for-Windows-x86_64-SNAPSHOT-%APPVEYOR_BUILD_VERSION%" ./.inst/NUT-for-Windows-x86_64-SNAPSHOT ; ( cd .inst/NUT-for-Windows-x86_64-SNAPSHOT ; find . -ls ; )'
if errorlevel 1 exit /b %ERRORLEVEL%
C:\msys64\usr\bin\bash -lc 'date -u'
pushd .inst
if errorlevel 1 exit /b %ERRORLEVEL%
REM 7z -snh: hoping for a version that can save hardlinks as such, and in 7z format to boot:
7z a -snh ../NUT-for-Windows-x86_64-SNAPSHOT-%APPVEYOR_BUILD_VERSION%.7z NUT*
if errorlevel 1 exit /b %ERRORLEVEL%
popd

REM Report ccache stats (here rather than in a separate yml step after the
REM artifact message in 08-after_test.ps1, the order does not matter):
C:\msys64\usr\bin\bash -lc 'date -u; PATH="/mingw64/bin:$PATH" ; export PATH ; ccache -sv || ccache -s || echo "SKIP: Could not query ccache stats"'
if errorlevel 1 exit /b %ERRORLEVEL%
C:\msys64\usr\bin\bash -lc 'date -u; PATH="/mingw64/bin:$PATH" ; export PATH ; ccache -x || echo "SKIP: Could not query ccache compression stats"'
if errorlevel 1 exit /b %ERRORLEVEL%
