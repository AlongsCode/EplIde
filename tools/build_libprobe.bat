@echo off
rem Build the 32-bit support-library probe: LoadLibrary a .fne and print the LIB_INFO
rem that the original IDE would see (data types / commands / consts).
rem ASCII only.
setlocal
set ROOT=D:\eclone
set VC98=C:\Program Files (x86)\Microsoft Visual Studio\VC98
set MSDEV=C:\Program Files (x86)\Microsoft Visual Studio\Common\MSDev98
set PATH=%VC98%\Bin;%MSDEV%\Bin;%PATH%
set INCLUDE=%VC98%\Include;%VC98%\MFC\Include;%VC98%\ATL\Include
set LIB=%VC98%\Lib;%VC98%\MFC\Lib

if not exist "%ROOT%\gbk" mkdir "%ROOT%\gbk"
powershell -NoProfile -ExecutionPolicy Bypass -Command "$e=[Text.Encoding]::GetEncoding(936);$t=[IO.File]::ReadAllText('%ROOT%\tools\libprobe.c',(New-Object Text.UTF8Encoding($false)));[IO.File]::WriteAllText('%ROOT%\gbk\libprobe_gbk.c',$t,$e)"
if errorlevel 1 goto :fail

cd /d "%ROOT%\tools"
cl /nologo /MT /O2 /W3 /Fe"%ROOT%\tools\libprobe.exe" "%ROOT%\gbk\libprobe_gbk.c" user32.lib kernel32.lib
if errorlevel 1 goto :fail
echo LibProbe BUILD OK
goto :eof
:fail
echo LibProbe BUILD FAILED
exit /b 1
