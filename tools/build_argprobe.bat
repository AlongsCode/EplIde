@echo off
rem Build the console probe that prints a command's argument names from the support lib.
rem ASCII only. Writes its GBK copy + obj into gbk\probe\ so the main build's "*.cpp" glob
rem in gbk\ never picks it up (doing that once caused LNK2005: two _main).
setlocal
set ROOT=D:\eclone
set VC98=C:\Program Files (x86)\Microsoft Visual Studio\VC98
set MSDEV=C:\Program Files (x86)\Microsoft Visual Studio\Common\MSDev98
set PATH=%VC98%\Bin;%MSDEV%\Bin;%PATH%
set INCLUDE=%VC98%\Include;%VC98%\MFC\Include;%VC98%\ATL\Include
set LIB=%VC98%\Lib;%VC98%\MFC\Lib

if not exist "%ROOT%\gbk\probe" mkdir "%ROOT%\gbk\probe"
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\tools\utf8togbk.ps1" -SrcDir "%ROOT%\src" -DstDir "%ROOT%\gbk\probe" >nul
if errorlevel 1 goto :fail
powershell -NoProfile -ExecutionPolicy Bypass -Command "$e=[Text.Encoding]::GetEncoding(936);$t=[IO.File]::ReadAllText('%ROOT%\tools\argprobe.cpp',(New-Object Text.UTF8Encoding($false)));[IO.File]::WriteAllText('%ROOT%\gbk\probe\argprobe_gbk.cpp',$t,$e)"
if errorlevel 1 goto :fail

cd /d "%ROOT%\gbk\probe"
cl /nologo /c /MT /GX /O2 /W3 ec_lib.cpp
if errorlevel 1 goto :fail
cl /nologo /c /MT /GX /O2 /W3 argprobe_gbk.cpp
if errorlevel 1 goto :fail

link /nologo /subsystem:console /out:"%ROOT%\tools\argprobe.exe" argprobe_gbk.obj ec_lib.obj ^
  nafxcw.lib libcmt.lib oldnames.lib ^
  kernel32.lib user32.lib gdi32.lib winmm.lib msimg32.lib winspool.lib ^
  comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib comctl32.lib
if errorlevel 1 goto :fail
echo ArgProbe BUILD OK
goto :eof
:fail
echo ArgProbe BUILD FAILED
exit /b 1
