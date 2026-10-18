REM Network UPS Tools: AppVeyor CI recipe step (see appveyor.yml), NUT for Windows
REM with MSYS2/MinGW x64. Called by AppVeyor with the build folder as current
REM directory. AppVeyor checks each line of inline yml scripts, but only the
REM exit code of a called .bat file, so we check each command here.
REM
REM Copyright (C) 2022-2026	Jim Klimov <jimklimov+nut@gmail.com>

REM Do not give pacman cause for complaints:
C:\msys64\usr\bin\bash -lc "date -u; mkdir -p /var/cache/pacman/pkg; ls -la /"
if errorlevel 1 exit /b %ERRORLEVEL%

REM FIXME: Since Jan 2025, normal update below fails with the message:
REM   installing mingw-w64-i686-gdb (16.1-1) breaks dependency
REM   'mingw-w64-i686-gdb=15.2' required by mingw-w64-i686-gdb-multiarch
REM Currently we sweep it away by removing the conflicting package:
REM C:\msys64\usr\bin\bash -lc "date -u; pacman --noconfirm -R mingw-w64-i686-gdb-multiarch || true"

REM FIXME: Make this update or not of the ecosystem conditional?
REM  Store a copy of C:\msys64 elsewhere as a tarball?
REM  As of Aug 2025, the first update is quick (15sec, 18Mb DL, 91MB installed),
REM  the second is costlier (6.5min, 378MB DL, 2.5GB installed)
REM Core update (in case any core packages are outdated):
REM C:\msys64\usr\bin\bash -lc "date -u; pacman --noconfirm -Syuu"
REM Normal update (same command again):
REM C:\msys64\usr\bin\bash -lc "date -u; pacman --noconfirm -Syuu"

REM As of Aug 2025, with updates commented away above still in place
REM so many packages skipped, this step did cost additional 58MB DL,
REM 392MB installed (new data), 1min of work.
REM Without the ecosystem update above, the overhead of this step remained
REM similar. It did however use a smaller population (via deps) and older
REM versions of packages.
REM Prerequisites for NUT per https://github.com/networkupstools/nut/blob/master/docs/config-prereqs.txt :
C:\msys64\usr\bin\bash -lc "date -u; pacman --noconfirm -S --needed base-devel mingw-w64-x86_64-toolchain autoconf-wrapper automake-wrapper libtool mingw-w64-x86_64-libltdl gcc ccache mingw-w64-x86_64-ccache git aspell aspell-en python mingw-w64-x86_64-python-pygments mingw-w64-x86_64-winpthreads-git mingw-w64-x86_64-libusb mingw-w64-x86_64-libusb-compat-git mingw-w64-x86_64-neon libneon-devel mingw-w64-x86_64-libgd mingw-w64-x86_64-cppunit mingw-w64-x86_64-nss mingw-w64-x86_64-openssl"
if errorlevel 1 exit /b %ERRORLEVEL%
REM SKIP mingw-w64-x86_64-libmodbus-git : we custom-build one with USB support
REM SKIP for now NUT-Monitor prereqs (runtime Python would require somilar modules; need to fix localization builds like "fr.po"): gettext mingw-w64-x86_64-python-pyqt6

REM FIXME: Make these stats conditional for just troubleshooting sessions?
REM  It seems the first "du" call costs us about 1-2 minutes (different runs):
REM Assorted stats after package processing:
REM C:\msys64\usr\bin\bash -lc "date -u; ls -la / ; du -ksx / ; date -u; du -ks /var/cache/pacman/pkg; date -u"

REM Preserve the current working directory:
set CHERE_INVOKING=yes
REM Start a 64 bit Mingw environment:
set MSYSTEM=MINGW64
C:\msys64\usr\bin\bash -lc 'PATH="/mingw64/bin:$PATH" ; export PATH ; pwd ; ccache -sv || echo "SKIP: Could not query ccache stats" ; ccache -o sloppiness=file_macro || true ; ccache -o compression=true || true ; ccache -o max_size=750M || true '
if errorlevel 1 exit /b %ERRORLEVEL%
