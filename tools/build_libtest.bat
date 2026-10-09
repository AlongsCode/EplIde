@echo off
rem Build the console harness for ec_lib (load .fne -> build tree -> build every hint).
rem ASCII only.
setlocal
set ROOT=D:\eclone
set VC98=C:\Program Files (x86)\Microsoft Visual Studio\VC98
set MSDEV=C:\Program Files (x86)\Microsoft Visual Studio\Common\MSDev98
set PATH=%VC98%\Bin;%MSDEV%\Bin;%PATH%
set INCLUDE=%VC98%\Include;%VC98%\MFC\Include;%VC98%\ATL\Include
set LIB=%VC98%\Lib;%VC98%\MFC\Lib

if not exist "%ROOT%\gbk" mkdir "%ROOT%\gbk"
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\tools\utf8togbk.ps1" -SrcDir "%ROOT%\src" -DstDir "%ROOT%\gbk" >nul
if errorlevel 1 goto :fail
powershell -NoProfile -ExecutionPolicy Bypass -Command "$e=[Text.Encoding]::GetEncoding(936);$t=[IO.File]::ReadAllText('%ROOT%\tools\libtest.cpp',(New-Object Text.UTF8Encoding($false)));[IO.File]::WriteAllText('%ROOT%\gbk\libtest_gbk.cpp',$t,$e)"
if errorlevel 1 goto :fail

cd /d "%ROOT%\gbk"
cl /nologo /c /MT /GX /O2 /W3 /Foec_lib_test.obj ec_lib.cpp
if errorlevel 1 goto :fail
cl /nologo /c /MT /GX /O2 /W3 libtest_gbk.cpp
if errorlevel 1 goto :fail

link /nologo /subsystem:console /out:"%ROOT%\tools\libtest.exe" libtest_gbk.obj ec_lib_test.obj ^
  nafxcw.lib libcmt.lib oldnames.lib ^
  kernel32.lib user32.lib gdi32.lib winmm.lib msimg32.lib winspool.lib ^
  comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib comctl32.lib
if errorlevel 1 goto :fail
echo LibTest BUILD OK
goto :eof
:fail
echo LibTest BUILD FAILED
exit /b 1
