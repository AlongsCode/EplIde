@echo off
rem Build designprobe: verify that a support library's ITF interface can create real
rem component windows (32-bit, VC6). ASCII only.
setlocal
set ROOT=D:\eclone
set VC98=C:\Program Files (x86)\Microsoft Visual Studio\VC98
set MSDEV=C:\Program Files (x86)\Microsoft Visual Studio\Common\MSDev98
set PATH=%VC98%\Bin;%MSDEV%\Bin;%PATH%
set INCLUDE=%VC98%\Include;%VC98%\MFC\Include;%VC98%\ATL\Include
set LIB=%VC98%\Lib;%VC98%\MFC\Lib

if not exist "%ROOT%\gbk" mkdir "%ROOT%\gbk"
powershell -NoProfile -ExecutionPolicy Bypass -Command "$e=[Text.Encoding]::GetEncoding(936);$t=[IO.File]::ReadAllText('%ROOT%\tools\designprobe.c',(New-Object Text.UTF8Encoding($false)));[IO.File]::WriteAllText('%ROOT%\gbk\designprobe_gbk.c',$t,$e)"
if errorlevel 1 goto :fail

cd /d "%ROOT%\tools"
cl /nologo /MT /O2 /W3 /Fe"%ROOT%\tools\designprobe.exe" "%ROOT%\gbk\designprobe_gbk.c" user32.lib kernel32.lib
if errorlevel 1 goto :fail
echo DesignProbe BUILD OK
goto :eof
:fail
echo DesignProbe BUILD FAILED
exit /b 1
