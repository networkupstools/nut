::This script can be used to Create and Build NUT installer using WiX.

@echo off

SET BATDIR=%~dp0
cd /d %BATDIR%

SET NUT-XML-FILE=NUT-Installer.xml
SET wixobjName=NUT-Installer.wixobj
SET msiPackageName=NUT-Installer.msi

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