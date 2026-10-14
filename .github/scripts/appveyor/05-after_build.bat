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
C:\msys64\usr\bin\bash -lc 'date -u; PATH="/mingw64/bin:$PATH" ; export PATH ; ccache -sv || ccache -s || echo "SKIP: Could not query ccache stats"'
if errorlevel 1 exit /b %ERRORLEVEL%
