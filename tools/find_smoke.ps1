# Smoke-test "find next / previous" (edit menu #32878 / #32879, F3 / Shift+F3).
# Assertions come from the editor's own model dump + its fold dump:
#   <model>.txt      -> rows   ("row %3d kind=%d ... src=%2d | text")
#   <model>.fold.txt -> "scroll=0 rows=N anchor=L,C caret=L,C lines=M"
# ASCII-only: Chinese needles are built from code points.
#
# usage: powershell -NoProfile -ExecutionPolicy Bypass -File tools\find_smoke.ps1
param(
    [string]$Exe = "D:\eclone\bin\EClone.exe",
    [string]$OutDir = "D:\eclone\shots",
    [int]$WaitSec = 6
)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
Add-Type @"
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public class FS {
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
# needle = the fixture's message-box command name (occurs exactly TWICE in the sample)
$needle = [string]([char]0x4FE1 + [char]0x606F + [char]0x6846)

$model = Join-Path $OutDir "find_model.txt"
$fold = $model -replace '\.txt$', '.fold.txt'
Remove-Item $model, $fold -Force -ErrorAction SilentlyContinue
$env:ECLONE_DUMP_MODEL = $model
Start-Process -FilePath taskkill -ArgumentList '/f','/im','EClone.exe' -Wait -WindowStyle Hidden
Start-Sleep -Milliseconds 400
$p = Start-Process -FilePath $Exe -PassThru
Start-Sleep -Seconds $WaitSec
if ($p.HasExited) { Write-Host "FAIL: exited early"; exit 1 }
$script:procId = $p.Id; $script:frame = [IntPtr]::Zero; $script:ed = [IntPtr]::Zero
[void][FS]::EnumWindows({
    param($h,$q)
    $wpid = 0; [void][FS]::GetWindowThreadProcessId($h, [ref]$wpid)
    if ($wpid -eq $script:procId -and [FS]::IsWindowVisible($h) -and [FS]::Cls($h) -eq 'ENewFrame') { $script:frame = $h }
    return $true
}, [IntPtr]::Zero)
[void][FS]::EnumChildWindows($script:frame, {
    param($h,$q)
    if ([FS]::Cls($h).StartsWith('Afx:400000:8:10005')) { $script:ed = $h; return $false }
    return $true
}, [IntPtr]::Zero)
if ($script:ed -eq [IntPtr]::Zero) { Write-Host "FAIL: no editor"; Stop-Process -Id $p.Id -Force; exit 1 }

function Redump { [void][FS]::PostMessage($script:ed, 0x0400 + 102, [IntPtr]::Zero, [IntPtr]::Zero); Start-Sleep -Milliseconds 500 }
function Caret {
    $t = (Get-Content $fold -Encoding Default -Raw)
    if ($t -match 'anchor=(-?\d+),(-?\d+)\s+caret=(-?\d+),(-?\d+)') {
        return @([int]$Matches[1], [int]$Matches[2], [int]$Matches[3], [int]$Matches[4])
    }
    return $null
}
function Hits([string]$t) {
    return @($t -split "`n" | Where-Object { $_ -match '^row ' -and $_ -match 'kind=1' -and $_.Contains($needle) })
}

$fails = 0
function Check([string]$name, [bool]$ok) {
    if ($ok) { Write-Host ("[PASS] " + $name) } else { Write-Host ("[FAIL] " + $name); $script:fails++ }
}

Redump
$txt = Get-Content $model -Encoding Default -Raw
$rows = Hits $txt
Check ("the fixture has exactly two rows with the needle (n=" + $rows.Count + ")") ($rows.Count -eq 2)
if ($rows.Count -lt 2) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue; Remove-Item Env:\ECLONE_DUMP_MODEL; exit 1 }
if ($rows[0] -notmatch 'src=\s*(\d+)') { Write-Host "FAIL: no src in row"; Stop-Process -Id $p.Id -Force; exit 1 }
$src1 = [int]$Matches[1]
if ($rows[1] -notmatch 'src=\s*(\d+)') { Write-Host "FAIL: no src in 2nd row"; Stop-Process -Id $p.Id -Force; exit 1 }
$src2 = [int]$Matches[1]
if ($rows[0] -notmatch 'y=\s*(\d+)') { Write-Host "FAIL: no y in row"; Stop-Process -Id $p.Id -Force; exit 1 }
$y = [int]$Matches[1] + 10
Write-Host ("first needle row: src=$src1 y=$y ; second: src=$src2")

# click INSIDE the needle: indent 1 -> text starts at x=68, the 3 CJK glyphs span ~45px
$x = 78
$lp = [IntPtr](($x -band 0xFFFF) -bor ($y -shl 16))
[void][FS]::PostMessage($script:ed, 0x0201, [IntPtr]1, $lp)
Start-Sleep -Milliseconds 150
[void][FS]::PostMessage($script:ed, 0x0202, [IntPtr]0, $lp)
Start-Sleep -Milliseconds 400
Redump
$a = Caret
Check ("caret landed on the first needle row (caret=" + ($a -join ',') + ")") (($a -ne $null) -and ($a[2] -eq $src1))

# F3 -> find next
[void][FS]::SendMessage($script:ed, 0x0100, [IntPtr]0x72, [IntPtr]::Zero)
Start-Sleep -Milliseconds 500
Redump
$b = Caret
Check ("F3 jumped to the SECOND needle row (caret=" + ($b -join ',') + ")") `
      (($b -ne $null) -and ($b[2] -eq $src2) -and (($b[2] -ne $a[2]) -or ($b[3] -ne $a[3])))

# Shift+F3 -> find previous; the shift state must come from a real keybd_event (see HANDOFF 3.3-5)
[void][FS]::keybd_event(0x10, 0, 0, [IntPtr]::Zero)          # VK_SHIFT down
Start-Sleep -Milliseconds 120
[void][FS]::SendMessage($script:ed, 0x0100, [IntPtr]0x72, [IntPtr]::Zero)
[void][FS]::keybd_event(0x10, 0, 2, [IntPtr]::Zero)          # VK_SHIFT up
Start-Sleep -Milliseconds 500
Redump
$c = Caret
Check ("Shift+F3 came back to the FIRST needle row (caret=" + ($c -join ',') + ")") `
      (($c -ne $null) -and ($c[2] -eq $src1))

Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
Remove-Item Env:\ECLONE_DUMP_MODEL
if ($fails -ne 0) { Write-Host ("find_smoke: " + $fails + " FAILED"); exit 1 }
Write-Host "[PASS] find_smoke: find next / previous work"
