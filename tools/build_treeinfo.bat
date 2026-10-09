@echo off
rem Build the 32-bit tree probe (read the original IDE's SysTreeView32 items).
rem ASCII only.
setlocal
set VC98=C:\Program Files (x86)\Microsoft Visual Studio\VC98
set MSDEV=C:\Program Files (x86)\Microsoft Visual Studio\Common\MSDev98
set PATH=%VC98%\Bin;%MSDEV%\Bin;%PATH%
set INCLUDE=%VC98%\Include;%VC98%\MFC\Include;%VC98%\ATL\Include
set LIB=%VC98%\Lib;%VC98%\MFC\Lib
rem convert to GBK first: VC6 reads sources as ANSI and treeinfo.c has Chinese comments
if not exist "D:\eclone\gbk" mkdir "D:\eclone\gbk"
powershell -NoProfile -ExecutionPolicy Bypass -Command "$e=[Text.Encoding]::GetEncoding(936);$t=[IO.File]::ReadAllText('D:\eclone\tools\treeinfo.c',(New-Object Text.UTF8Encoding($false)));[IO.File]::WriteAllText('D:\eclone\gbk\treeinfo_gbk.c',$t,$e)"
if errorlevel 1 goto :fail
cd /d D:\eclone\tools
cl /nologo /MT /O2 /W3 /FeD:\eclone\tools\treeinfo.exe D:\eclone\gbk\treeinfo_gbk.c user32.lib kernel32.lib
if errorlevel 1 goto :fail
echo TreeInfo BUILD OK
goto :eof
:fail
echo TreeInfo BUILD FAILED
exit /b 1
