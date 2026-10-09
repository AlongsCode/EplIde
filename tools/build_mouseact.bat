@echo off
rem Build the 32-bit real-input helper (click / move / key / type into another window).
rem ASCII only.
setlocal
set VC98=C:\Program Files (x86)\Microsoft Visual Studio\VC98
set MSDEV=C:\Program Files (x86)\Microsoft Visual Studio\Common\MSDev98
set PATH=%VC98%\Bin;%MSDEV%\Bin;%PATH%
set INCLUDE=%VC98%\Include;%VC98%\MFC\Include;%VC98%\ATL\Include
set LIB=%VC98%\Lib;%VC98%\MFC\Lib
cd /d D:\eclone\tools
cl /nologo /MT /O2 /W3 /FeD:\eclone\tools\mouseact.exe D:\eclone\tools\mouseact.c user32.lib kernel32.lib
if errorlevel 1 goto :fail
echo MouseAct BUILD OK
goto :eof
:fail
echo MouseAct BUILD FAILED
exit /b 1
