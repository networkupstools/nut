::This script can be used to Create and Build NUT installer using WiX.

@echo off

SET BATDIR=%~dp0
cd /d %BATDIR%

SET NUT-XML-FILE=NUT-Installer.xml
SET wixobjName=NUT-Installer.wixobj
SET msiPackageName=NUT-Installer.msi

REM copy DLL files from system
copy /Y c:\mingw\msys\1.0\bin\msys-1.0.dll .\ImageFiles\Others
copy /Y c:\mingw\msys\1.0\bin\msys-crypto-1.0.0.dll .\ImageFiles\Others
copy /Y c:\mingw\msys\1.0\bin\msys-ssl-1.0.0.dll .\ImageFiles\Others
copy /Y c:\mingw\bin\libgnurx-0.dll .\ImageFiles\Others

REM use "candle.exe" to create the "object" file
candle.exe "%NUT-XML-FILE%" -out "%wixobjName%" >"log.txt"
@echo =========================================================
@echo Please wait as MSI package creation in progress...

@echo off
REM use "light.exe" to create the "MSi" package
light.exe "%wixobjName%" -out "%msiPackageName%" >>"log.txt"

@echo =========================================================
@echo MSI package "%msiPackageName%" complete
@echo =========================================================
@echo Check output file "log.txt" for status of completion...
@echo =========================================================
