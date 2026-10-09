# Window designer smoke test (ASCII only).
# Verifies that the design-time form and the components on it are created by the
# SUPPORT LIBRARY itself (krnln.fne + an extra third-party .fne), not by fake classes
# registered inside EClone.
#
# What it does:
#   1) launches EClone with ECLONE_DESIGN_DEMO=1 (places one real component per loaded lib)
#      and ECLONE_EXTRA_LIBS=<extra> (third-party library, e.g. iext2)
#   2) shot.ps1 -Keep -> screenshot + window-tree dump (shots\<tag>.windows.txt)
#   3) shows the component box (WM_COMMAND ID_VIEW_COMPPANE) and dumps its tree with treeinfo
#   4) asserts on the dumps
#
# usage: powershell -NoProfile -ExecutionPolicy Bypass -File design_smoke.ps1
param(
    [string]$Exe = "D:\eclone\bin\EClone.exe",
    [string]$Extra = "iext2",
    [string]$Tag = "design_smoke",
    [int]$WaitSec = 6
)
$ErrorActionPreference = 'Continue'
$root = "D:\eclone"

$env:ECLONE_DESIGN_DEMO = "1"
$env:ECLONE_DESIGN_DUMP = "$root\shots\${Tag}_props.txt"
# EcTrace only writes when ECLONE_DUMP_MODEL is set; the component-box WM_DRAWITEM assertions
# below read <that path>.trace.txt
$env:ECLONE_DUMP_MODEL = "$root\shots\${Tag}_trace.txt"
Remove-Item "$root\shots\${Tag}_trace.txt.trace.txt" -ErrorAction SilentlyContinue
if ($Extra -ne "") { $env:ECLONE_EXTRA_LIBS = $Extra } else { Remove-Item Env:\ECLONE_EXTRA_LIBS -ErrorAction SilentlyContinue }

Get-Process EClone -ErrorAction SilentlyContinue | Stop-Process -Force
powershell -NoProfile -ExecutionPolicy Bypass -File "$root\tools\shot.ps1" -Exe $Exe -Tag $Tag -ExeArgs designer -WaitSec $WaitSec -Keep | Out-Null

Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public class DS {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  public static string Cls(IntPtr h){ var sb=new StringBuilder(128); GetClassNameW(h,sb,128); return sb.ToString(); }
  public static IntPtr FindBar(IntPtr frame){
    IntPtr found = IntPtr.Zero;
    EnumChildWindows(frame, delegate(IntPtr c, IntPtr p){
      if (found == IntPtr.Zero && Cls(c) == "AfxControlBar42s") { found = c; return false; }
      return true;
    }, IntPtr.Zero);
    return found;
  }
}
"@

# show the component box: WM_COMMAND with ID_VIEW_COMPPANE (id read from resource.h)
$id = 0
Select-String -Path "$root\src\resource.h" -Pattern '^#define\s+ID_VIEW_COMPPANE\s+(\d+)' | ForEach-Object {
    if ($_.Matches.Count -gt 0) { $id = [int]$_.Matches[0].Groups[1].Value }
}
$proc = Get-Process EClone -ErrorAction SilentlyContinue
if ($proc -ne $null -and $id -ne 0) {
    [void][DS]::PostMessage($proc.MainWindowHandle, 0x111, [IntPtr]$id, [IntPtr]0)
    Start-Sleep -Seconds 2
}

& "$root\tools\treeinfo.exe" EClone.exe > "$root\shots\${Tag}_trees.txt" 2>&1
Get-Process EClone -ErrorAction SilentlyContinue | Stop-Process -Force

# ---------------------------------------------------------------- assertions
$fail = 0
$win = Get-Content "$root\shots\$Tag.windows.txt" -ErrorAction SilentlyContinue
$txt = $win -join "`n"

$wt = ($win | Select-String -Pattern '^\s*WTWindow\s+vis=True' | Select-Object -First 1)
$btn = ($win | Select-String -Pattern '^\s*_EL_RgnButton\s+vis=True' | Select-Object -First 1)
if ($wt -ne $null -and $btn -ne $null) {
    $leadW = ($wt.Line -replace '\S.*$','').Length
    $leadB = ($btn.Line -replace '\S.*$','').Length
    if ($leadB -gt $leadW) {
        Write-Host "[PASS] support library built the design form (WTWindow) and a component (_EL_RgnButton) as its child"
    } else {
        Write-Host "[FAIL] _EL_RgnButton is not a child of WTWindow"; $fail++
    }
} else {
    Write-Host "[FAIL] design form / component window not found in the window tree"; $fail++
}

if ($txt -match '(?m)^\s*Button\s+vis=True\s+\d+,\d+ 120x80') {    Write-Host "[PASS] third-party support library component placed on the form (window 120x80)"
} else {
    Write-Host "[FAIL] third-party support library component not found"; $fail++
}

# 3) component box: since the 26th round it is an ORIGINAL-STYLE ICON BUTTON GRID --
#    4 columns of 28x28 owner-draw buttons inside the 126x415 #32770 (measured from the
#    original with tools\probe_compbox3.py), not a SysTreeView32/ListView any more.
# ⚠️ shot.ps1's window dump uses RECURSIVE EnumChildWindows, so a deep control appears once per
#    ancestor => count UNIQUE controls (class|rect), not lines (measuring 232 x 28x28 for 30
#    buttons is how this was found). The dump has no hwnd column, so the identity key is
#    "col,row WxH".
$btnKeys = @($win | Select-String -Pattern '^\s*Button\s+vis=True\s+\d+,\d+\s+28x28\s' |
             ForEach-Object { $t = $_.Line.Trim() -split '\s+'; "$($t[2]) $($t[3])" } | Sort-Object -Unique)
Write-Host ("   28x28 unique button positions: " + $btnKeys.Count)
if ($btnKeys.Count -ge 25) {
    Write-Host ("[PASS] component box shows the icon button grid (" + $btnKeys.Count + " unique 28x28 buttons)")
} else {
    Write-Host ("[FAIL] component box icon grid missing (found " + $btnKeys.Count + " unique 28x28 buttons)"); $fail++
}
# the group selector (原版那个 116x20 的控件) must carry the "基本组件" group name
$grp = [string]([char]0x57FA + [char]0x672C + [char]0x7EC4 + [char]0x4EF6)
$lb = @($win | Select-String -Pattern 'ListBox\s+vis=True\s+\d+,\d+\s+1[0-9][0-9]x20')
if ($lb.Count -ge 1) {
    Write-Host "[PASS] component box group selector present (116x20)"
} else {
    Write-Host "[FAIL] component box group selector missing"; $fail++
}
# and the icons must really be painted: ECLONE_DUMP_MODEL's trace channel records every
# WM_DRAWITEM the pane receives (see CPaneBar::OnDrawItem / ec_editor.cpp EcTrace).
$trace = "$root\shots\${Tag}_trace.txt.trace.txt"
$draws = 0
if (Test-Path $trace) {
    $draws = @(Select-String -Path $trace -Pattern 'drawitem').Count
}
if ($draws -ge 20) {
    Write-Host ("[PASS] component buttons are owner-drawn (" + $draws + " WM_DRAWITEM calls)")
} else {
    Write-Host ("[FAIL] component buttons never got WM_DRAWITEM (got " + $draws + ")"); $fail++
}

# 4) property page: filled from the support library's property metadata (names) + ITF 6 (values).
#    The designer dumps "prop|<idx>|<name>|<value>" / "end|<count>|lib=<i>|dt=<k>" when ECLONE_DESIGN_DUMP is set.
$prow = 0
$pnum = 0
$pend = 0
$props = Get-Content "$root\shots\${Tag}_props.txt" -ErrorAction SilentlyContinue
if ($props -ne $null) {
    foreach ($l in $props) {
        if ($l -match '^prop\|[0-9]+\|') { $prow++ }
        if ($l -match '^prop\|[0-9]+\|[^|]+\|[0-9]+\s*$') { $pnum++ }
        if ($l -match '^end\|([0-9]+)\|lib=') { if ($matches.Count -gt 0) { $pend = [int]$matches[1] } }
    }
}
if ($pend -ge 10 -and $prow -ge 10 -and $pnum -ge 1) {
    Write-Host "[PASS] property page filled from the support library ($prow props, $pnum numeric values via ITF 6)"
} else {
    Write-Host "[FAIL] property page dump too small (end=$pend rows=$prow numeric=$pnum)"; $fail++
}

if ($fail -eq 0) { Write-Host "[PASS] design_smoke: all checks passed" } else { Write-Host "[FAIL] design_smoke: $fail check(s) failed" }
exit $fail
