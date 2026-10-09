@echo off
rem One-off relink with a MAP file so a crash offset can be mapped to a function.
rem ASCII only. Run AFTER build.bat has produced the .obj files in D:\eclone\gbk.
setlocal
set ROOT=D:\eclone
set VC98=C:\Program Files (x86)\Microsoft Visual Studio\VC98
set MSDEV=C:\Program Files (x86)\Microsoft Visual Studio\Common\MSDev98
set PATH=%VC98%\Bin;%MSDEV%\Bin;%PATH%
set INCLUDE=%VC98%\Include;%VC98%\MFC\Include;%VC98%\ATL\Include
set LIB=%VC98%\Lib;%VC98%\MFC\Lib
cd /d "%ROOT%\gbk"
set RES=
if exist EClone.res set RES=EClone.res
link /nologo /subsystem:windows /map:"%ROOT%\shots\EClone.map" /out:"%ROOT%\bin\EClone.exe" *.obj %RES% ^
  nafxcw.lib libcmt.lib oldnames.lib ^
  kernel32.lib user32.lib gdi32.lib winmm.lib msimg32.lib winspool.lib ^
  comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib comctl32.lib
if errorlevel 1 goto :fail
echo MAPLINK OK
goto :eof
:fail
echo MAPLINK FAILED
exit /b 1
