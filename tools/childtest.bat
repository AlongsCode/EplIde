@echo off
setlocal
set VC98=C:\Program Files (x86)\Microsoft Visual Studio\VC98
set MSDEV=C:\Program Files (x86)\Microsoft Visual Studio\Common\MSDev98
set PATH=%VC98%\Bin;%MSDEV%\Bin;%PATH%
set INCLUDE=%VC98%\Include
set LIB=%VC98%\Lib
cd /d "%~dp0"
echo compiling...
cl /nologo /MT /O2 /c /TP childtest.c
if errorlevel 1 goto :eof
link /nologo /out:childtest.exe childtest.obj user32.lib gdi32.lib
echo errorlevel=%errorlevel%
if not exist childtest.exe goto :eof
childtest.exe
