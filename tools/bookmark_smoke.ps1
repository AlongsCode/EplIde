# Smoke-test the editor bookmarks (edit menu #32964  Ctrl+F6 / #32967  Shift+F6 /
# #32968  F6). Assertions come from the editor's own fold dump
# ("anchor=L,C caret=L,C"). ASCII-only: no Chinese anywhere in this file.
#
# usage: powershell -NoProfile -ExecutionPolicy Bypass -File tools\bookmark_smoke.ps1
param(
    [string]$Exe = "D:\eclone\bin\EClone.exe",
    [string]$OutDir = "D:\eclone\shots",
    [int]$WaitSec = 6
)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
Add-Type @"
using System; using System.Text; using System.Runtime.InteropServices;
public class BM {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, IntPtr extra);
  public static string Cls(IntPtr h){ var sb=new StringBuilder(128); GetClassNameW(h,sb,128); return sb.ToString(); }
}
"@
$model = Join-Path $OutDir "bm_model.txt"
$fold  = $model -replace '\.txt$', '.fold.txt'
Remove-Item $model, $fold -Force -ErrorAction SilentlyContinue
$env:ECLONE_DUMP_MODEL = $model
Start-Process -FilePath taskkill -ArgumentList '/f','/im','EClone.exe' -Wait -WindowStyle Hidden
Start-Sleep -Milliseconds 400
$p = Start-Process -FilePath $Exe -PassThru
Start-Sleep -Seconds $WaitSec
if ($p.HasExited) { Write-Host "FAIL: exited early"; exit 1 }
$script:procId = $p.Id; $script:frame = [IntPtr]::Zero; $script:ed = [IntPtr]::Zero
[void][BM]::EnumWindows({ param($h,$q) $w=0; [void][BM]::GetWindowThreadProcessId($h,[ref]$w)
    if ($w -eq $script:procId -and [BM]::IsWindowVisible($h) -and [BM]::Cls($h) -eq 'ENewFrame') { $script:frame = $h }
    return $true }, [IntPtr]::Zero)
$script:child = [IntPtr]::Zero
[void][BM]::EnumChildWindows($script:frame, { param($h,$q)
    if ([BM]::Cls($h) -eq 'AfxMDIFrame42s') { $script:child = $h; return $false }; return $true }, [IntPtr]::Zero)
[void][BM]::EnumChildWindows($script:frame, { param($h,$q)
    if ([BM]::Cls($h).StartsWith('Afx:400000:8:10005')) { $script:ed = $h; return $false }; return $true }, [IntPtr]::Zero)
if ($script:ed -eq [IntPtr]::Zero -or $script:child -eq [IntPtr]::Zero) { Write-Host "FAIL: no editor/child"; Stop-Process -Id $p.Id -Force; exit 1 }

function Redump { [void][BM]::PostMessage($script:ed, 0x0400 + 102, [IntPtr]::Zero, [IntPtr]::Zero); Start-Sleep -Milliseconds 450 }
function CaretLine {
    $t = Get-Content $fold -Encoding Default -Raw
    if ($t -match 'caret=(-?\d+),(-?\d+)') { return [int]$Matches[1] }
    return -999
}
function Cmd([int]$id) { [void][BM]::SendMessage($script:child, 0x0111, [IntPtr]$id, [IntPtr]::Zero); Start-Sleep -Milliseconds 400 }
function Key([int]$vk, [bool]$shift) {
    if ($shift) { [void][BM]::keybd_event(0x10, 0, 0, [IntPtr]::Zero); Start-Sleep -Milliseconds 100 }
    [void][BM]::SendMessage($script:ed, 0x0100, [IntPtr]$vk, [IntPtr]::Zero)
    if ($shift) { [void][BM]::keybd_event(0x10, 0, 2, [IntPtr]::Zero) }
    Start-Sleep -Milliseconds 400
}
$fails = 0
function Check([string]$name, [bool]$ok, [string]$extra) {
    if ($ok) { Write-Host ("[PASS] " + $name + "  " + $extra) } else { Write-Host ("[FAIL] " + $name + "  " + $extra); $script:fails++ }
}

# --- click into a code line to place the caret, then bookmark it ---
Redump
$txt = Get-Content $model -Encoding Default -Raw
$row = ($txt -split "`n" | Where-Object { $_ -match '^row ' -and $_ -match 'kind=1' } | Select-Object -Skip 6 | Select-Object -First 1)
if (-not $row -or $row -notmatch 'y=\s*(\d+)') { Write-Host "FAIL: no code row"; Stop-Process -Id $p.Id -Force; exit 1 }
$y = [int]$Matches[1] + 10
$lp = [IntPtr]((120 -band 0xFFFF) -bor ($y -shl 16))
[void][BM]::PostMessage($script:ed, 0x0201, [IntPtr]1, $lp)
Start-Sleep -Milliseconds 150
[void][BM]::PostMessage($script:ed, 0x0202, [IntPtr]0, $lp)
Start-Sleep -Milliseconds 400
Redump
$L1 = CaretLine
Write-Host ("caret on source line " + $L1)

Cmd 32964                                   # 
Key 0x75 $false                             # F6 =  -> 
Redump
Check "Ctrl+F6 bookmarked the line, F6 stays on it" ((CaretLine) -eq $L1) ("caret=" + (CaretLine) + " bookmarked=" + $L1)

# --- move the caret away, then come back with Shift+F6 ---
[void][BM]::SendMessage($script:ed, 0x0100, [IntPtr]0x28, [IntPtr]::Zero)   # VK_DOWN
Start-Sleep -Milliseconds 300
Redump
$L2 = CaretLine
Check "the caret moved off the bookmark" ($L2 -ne $L1) ("caret=" + $L2)
Key 0x75 $true                              # Shift+F6 = 
Redump
Check "Shift+F6 jumped back to the bookmark" ((CaretLine) -eq $L1) ("caret=" + (CaretLine))

# --- toggle the bookmark off, then F6 must not move the caret ---
Cmd 32964
Redump
$L3 = CaretLine
Key 0x75 $false
Redump
Check "after toggling it off, F6 does nothing" ((CaretLine) -eq $L3) ("caret=" + (CaretLine))

Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
Remove-Item Env:\ECLONE_DUMP_MODEL
if ($fails -ne 0) { Write-Host ("bookmark_smoke: " + $fails + " FAILED"); exit 1 }
Write-Host "[PASS] bookmark_smoke: bookmarks toggle and navigate"
