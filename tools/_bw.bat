@echo off
setlocal
set VC98=C:\Program Files (x86)\Microsoft Visual Studio\VC98
set MSDEV=C:\Program Files (x86)\Microsoft Visual Studio\Common\MSDev98
set PATH=%VC98%\Bin;%MSDEV%\Bin;%PATH%
set INCLUDE=%VC98%\Include
set LIB=%VC98%\Lib
cd /d "%~dp0"
cl /nologo /MT /O2 /c /TP wndproc32.c
if errorlevel 1 goto :eof
link /nologo /out:wndproc32.exe wndproc32.obj user32.lib
