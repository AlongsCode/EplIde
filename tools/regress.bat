@echo off
rem Full round-19 regression list from HANDOFF.md section header. ASCII only.
rem Usage: cmd /c "D:\eclone\tools\regress.bat"   (log goes to stdout, tee it yourself)
setlocal
set ROOT=D:\eclone
cd /d "%ROOT%"
set FAIL=0

echo ==== [1] build ============================================
taskkill /f /im EClone.exe >nul 2>&1
cmd /c "%ROOT%\build.bat"
if errorlevel 1 set FAIL=1

echo ==== [2] libtest ==========================================
"%ROOT%\tools\libtest.exe"
echo libtest exit=%errorlevel%

echo ==== [3] layout ===========================================
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\tools\shot.ps1" -Exe "%ROOT%\bin\EClone.exe" -Tag check
python "%ROOT%\tools\check_layout.py" "%ROOT%\shots\check.windows.txt"

echo ==== [4] edit_smoke =======================================
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\tools\edit_smoke.ps1"

echo ==== [5] fold_smoke =======================================
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\tools\fold_smoke.ps1"

echo ==== [6] tabs_smoke =======================================
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\tools\tabs_smoke.ps1"

echo ==== [7] cell_smoke =======================================
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\tools\cell_smoke.ps1"

echo ==== [8] param_smoke ======================================
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\tools\param_smoke.ps1"

echo ==== [9] sel_smoke ========================================
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\tools\sel_smoke.ps1"

echo ==== [10] design_smoke ====================================
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\tools\design_smoke.ps1"

echo ==== [11] theme ===========================================
set ECLONE_DUMP_THEME=%ROOT%\shots\theme.txt
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\tools\shot.ps1" -Exe "%ROOT%\bin\EClone.exe" -Tag theme
set ECLONE_DUMP_THEME=
python "%ROOT%\tools\check_theme.py" "%ROOT%\shots\theme.txt"

echo ==== [12] dll page ========================================
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\tools\dll_smoke.ps1"
python "%ROOT%\tools\check_dll.py" "%ROOT%\shots\dll_view.png" "%ROOT%\shots\dll_view2.png" "%ROOT%\shots\ref_dll_page.png"

echo ==== [13] dll pixel diff (ashift=4) =======================
python "%ROOT%\tools\diff_png.py" "%ROOT%\shots\ref_dll_page.png" "%ROOT%\shots\dll_view.png" 0 0 460 390 --ashift=4

echo ==== [14] code page geometry ==============================
python "%ROOT%\tools\patch_sample_graphic.py"
taskkill /f /im EClone.exe >nul 2>&1
cmd /c "%ROOT%\build.bat"
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\tools\code_shot.ps1"
python "%ROOT%\tools\diff_png.py" "%ROOT%\shots\live_tabs\view0.png" "%ROOT%\shots\code_view.png" 0 0 1174 388
python "%ROOT%\tools\restore_sample.py"
taskkill /f /im EClone.exe >nul 2>&1
cmd /c "%ROOT%\build.bat"

echo ==== [15] support library config (dynamic load/unload) =====
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\tools\libcfg_smoke.ps1"

echo ==== [16] designer component resize =======================
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\tools\resize_smoke.ps1"

echo ==== [17] table document pages ===========================
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\tools\page_smoke.ps1"

echo ==== [18] multi-line comment box =========================
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\tools\code_shot.ps1" -Tag cmt
python "%ROOT%\tools\check_comment_box.py" "%ROOT%\shots\cmt.png" 22

echo ==== [19] edit menu commands =============================
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\tools\edit_cmd_smoke.ps1"

echo ==== [20] find next / previous ===========================
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\tools\find_smoke.ps1"

echo ==== [21] find dialog (original id=284) ==================
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\tools\finddlg_smoke.ps1"

echo ==== [22] editor bookmarks ===============================
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\tools\bookmark_smoke.ps1"

echo ==== [23] bookmark jump dialog (original id=607) =========
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\tools\bm_dlg_smoke.ps1"

echo ==== [24] breakpoints (debugging step 1) ================
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\tools\bp_smoke.ps1"

echo ==== [25] system config: 7 tab pages =====================
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\tools\syscfg_smoke.ps1"

echo ==== [26] insert menu declarations =======================
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\tools\insert_smoke.ps1"

echo ==== [27] UI smoke: bars / table pages / whole find / forms
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\tools\ui_smoke.ps1"
if errorlevel 1 set FAIL=1

echo ==== [28] system config: other 6 pages read+write+persist
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\tools\cfg_smoke.ps1"
if errorlevel 1 set FAIL=1

echo ==== [29] support library hint -> status pane hint page
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\tools\hint_smoke.ps1"
if errorlevel 1 set FAIL=1

echo ==== [30] Ctrl+A selects the whole page (tables too) ======
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\tools\selectall_smoke.ps1"
if errorlevel 1 set FAIL=1

echo ==== DONE =================================================
echo regression FAIL flag = %FAIL%
endlocal
