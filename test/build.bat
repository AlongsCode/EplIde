@echo off
call ..\env.bat
cl /nologo /c /MT /GX /O2 hello.cpp
if errorlevel 1 goto :eof
link /nologo /subsystem:windows /out:hello.exe hello.obj nafxcw.lib libcmt.lib kernel32.lib user32.lib gdi32.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib
echo EXIT=%ERRORLEVEL%
