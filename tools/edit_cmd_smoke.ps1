# Smoke-test the "edit" menu commands: delete line / undo / redo / mask / unmask.
# Everything is asserted on the ECLONE_DUMP_MODEL display model, which is re-written when
# the editor receives EC_WM_REDUMP (WM_USER+102).
# ASCII-only on purpose: all Chinese needles are built from code points.
#
# usage: powershell -NoProfile -ExecutionPolicy Bypass -File tools\edit_cmd_smoke.ps1
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
using System.Runtime.InteropServices;
public class EV {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  public static string Cls(IntPtr h){ var sb=new StringBuilder(128); GetClassNameW(h,sb,128); return sb.ToString(); }
}
"@

# needle: a string that occurs in exactly ONE row of the fixture
# ("yi yu yan zhong guo ren zhi jiao ao" = the text of the first message-box line)
$needle = [string]([char]0x6613 + [char]0x8BED + [char]0x8A00 + [char]0x4E2D + [char]0x56FD +
                   [char]0x4EBA + [char]0x4E4B + [char]0x9A84 + [char]0x50B2)

$model = Join-Path $OutDir "model_editcmd.txt"
Remove-Item $model -Force -ErrorAction SilentlyContinue
$env:ECLONE_DUMP_MODEL = $model
Start-Process -FilePath taskkill -ArgumentList '/f','/im','EClone.exe' -Wait -WindowStyle Hidden
Start-Sleep -Milliseconds 400
$p = Start-Process -FilePath $Exe -PassThru
Start-Sleep -Seconds $WaitSec
if ($p.HasExited) { Write-Host "FAIL: exited early"; exit 1 }
$script:procId = $p.Id; $script:frame = [IntPtr]::Zero; $script:ed = [IntPtr]::Zero
[void][EV]::EnumWindows({
    param($h,$q)
    $wpid = 0; [void][EV]::GetWindowThreadProcessId($h, [ref]$wpid)
    if ($wpid -eq $script:procId -and [EV]::IsWindowVisible($h) -and [EV]::Cls($h) -eq 'ENewFrame') { $script:frame = $h }
    return $true
}, [IntPtr]::Zero)
# the active MDI child frame -- that is where MFC routes the menu commands
$script:child = [IntPtr]::Zero
[void][EV]::EnumChildWindows($script:frame, {
    param($h,$q)
    if ([EV]::Cls($h) -eq 'AfxMDIFrame42s') { $script:child = $h; return $false }
    return $true
}, [IntPtr]::Zero)
[void][EV]::EnumChildWindows($script:frame, {
    param($h,$q)
    if ([EV]::Cls($h).StartsWith('Afx:400000:8:10005')) { $script:ed = $h; return $false }
    return $true
}, [IntPtr]::Zero)
if ($script:child -eq [IntPtr]::Zero -or $script:ed -eq [IntPtr]::Zero) {
    Write-Host "FAIL: no mdi child / editor"; Stop-Process -Id $p.Id -Force; exit 1
}
Write-Host ("frame=" + $script:frame + " child=" + $script:child + " editor=" + $script:ed)

function Dump {
    [void][EV]::PostMessage($script:ed, 0x0400 + 102, [IntPtr]::Zero, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 500
    return (Get-Content $model -Encoding Default -Raw)
}
function Cmd([int]$id) {
    [void][EV]::SendMessage($script:child, 0x0111, [IntPtr]$id, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 500
}
function Rows([string]$t) { return @($t -split "`n" | Where-Object { $_ -match '^row ' }) }
function Hits([string]$t) { return @(Rows $t | Where-Object { $_ -match [regex]::Escape($needle) }) }
function RowText([string]$r) { if ($r -match '\|(.*)$') { return $Matches[1] }; return "" }

$fails = 0
function Check([string]$name, [bool]$ok) {
    if ($ok) { Write-Host ("[PASS] " + $name) } else { Write-Host ("[FAIL] " + $name); $script:fails++ }
}

# --- put the caret on the needle's code row by clicking it (coordinates from the model) ---
$txt = Dump
$hit = (Hits $txt) | Select-Object -First 1
if (-not $hit) { Write-Host "FAIL: cannot find the needle row"; Stop-Process -Id $p.Id -Force; exit 1 }
if ($hit -notmatch 'y=\s*(\d+)') { Write-Host "FAIL: no y in model row"; Stop-Process -Id $p.Id -Force; exit 1 }
$y = [int]$Matches[1] + 10
$x = 120
Write-Host ("click the code row at client " + $x + "," + $y)
$lp = [IntPtr](($x -band 0xFFFF) -bor ($y -shl 16))
[void][EV]::PostMessage($script:ed, 0x0201, [IntPtr]1, $lp)
Start-Sleep -Milliseconds 120
[void][EV]::PostMessage($script:ed, 0x0202, [IntPtr]0, $lp)
Start-Sleep -Milliseconds 400

$txt = Dump
$rows0 = (Rows $txt).Count
Check ("baseline: exactly one row carries the needle (rows=" + $rows0 + ")") ((Hits $txt).Count -eq 1)

# --- delete line, F10 / #32939 ---
Cmd 32939
$txt = Dump
$rows1 = (Rows $txt).Count
Check ("delete line removed the row (rows " + $rows0 + " -> " + $rows1 + ")") `
      (((Hits $txt).Count -eq 0) -and ($rows1 -eq $rows0 - 1))

# --- undo, Ctrl+Z / #57643 ---
Cmd 57643
$txt = Dump
Check ("undo brought it back (rows=" + (Rows $txt).Count + ")") ((Hits $txt).Count -eq 1)

# --- redo, Ctrl+Y / #57644 ---
Cmd 57644
$txt = Dump
Check "redo deleted it again" ((Hits $txt).Count -eq 0)

# --- undo once more: back to the original ---
Cmd 57643
$txt = Dump
Check ("undo again restored it (rows=" + (Rows $txt).Count + ")") ((Hits $txt).Count -eq 1)

# Undo restores the caret from the snapshot, which is NOT necessarily the row we clicked, so
# click it once more before testing the line-local commands.
[void][EV]::PostMessage($script:ed, 0x0201, [IntPtr]1, $lp)
Start-Sleep -Milliseconds 120
[void][EV]::PostMessage($script:ed, 0x0202, [IntPtr]0, $lp)
Start-Sleep -Milliseconds 400

# --- mask, Ctrl+K / #32936: the source line gets a leading quote ---
Cmd 32936
$txt = Dump
$h = (Hits $txt) | Select-Object -First 1
Check ("mask put a leading quote on the line") (($h -ne $null) -and ((RowText $h).TrimStart().StartsWith("'")))

# --- unmask, Ctrl+M / #32937 ---
Cmd 32937
$txt = Dump
$h = (Hits $txt) | Select-Object -First 1
Check ("unmask removed the quote again") (($h -ne $null) -and (-not (RowText $h).TrimStart().StartsWith("'")))
Check "the needle row is back to its original text" (($h -ne $null) -and ((RowText $h) -match [regex]::Escape($needle)))

# --- wrap the needle row in a "if true" block: #32801 (add to current block) ---
# The source order must become: block-start / needle / block-end.  Two caveats that cost a
# round: (a) the *end* keyword line is in g_szHidden, so it produces NO display row at all --
# a clean wrap therefore grows the display row count by exactly ONE (the opener);
# (b) "if true" also appears inside a table row (a subroutine is named after it), so only look
# at code rows when hunting for the block start.
$kwStart = [string]([char]0x5982 + [char]0x679C + [char]0x771F)                       # if-true
$rowsBefore = (Rows $txt).Count
Cmd 32801
$txt = Dump
$rowsAfter = (Rows $txt).Count
Check ("wrap: exactly one new display row appeared (the hidden end keyword adds none) (" + $rowsBefore + " -> " + $rowsAfter + ")") `
      ($rowsAfter -eq $rowsBefore + 1)
$code = @(Rows $txt | Where-Object { $_ -match 'kind=1' })
$i0 = -1; $i2 = -1
for ($k = 0; $k -lt $code.Count; $k++) {
    if (($i0 -lt 0) -and ($code[$k] -match [regex]::Escape($kwStart))) { $i0 = $k }
    if ($code[$k] -match [regex]::Escape($needle)) { $i2 = $k }
}
Check ("wrap: a code row holds the block start and the needle is the very next code row (idx " + $i0 + "/" + $i2 + ")") `
      (($i0 -ge 0) -and ($i2 -eq $i0 + 1))

# --- convert: #32812 (counted-loop) makes the selected line itself the block START ---
# Pick a block whose END keyword is *visible* (counted-loop-tail is not in g_szHidden) so the
# insertion shows up as a new display row; if-true-end would be swallowed by the block pairing
# and add no row at all (that is correct, and it is why this test does not use #32808).
$kwTail = [string]([char]0x8BA1 + [char]0x6B21 + [char]0x5FAA + [char]0x73AF + [char]0x5C3E)  # counted-loop-tail
$code = @(Rows $txt | Where-Object { $_ -match 'kind=1' })
$openerRow = $code[$i0]
if ($openerRow -notmatch 'y=\s*(\d+)') { Write-Host "FAIL: no y on the opener row"; Stop-Process -Id $p.Id -Force; exit 1 }
$yOp = [int]$Matches[1] + 10
$lpOp = [IntPtr](($x -band 0xFFFF) -bor ($yOp -shl 16))
[void][EV]::PostMessage($script:ed, 0x0201, [IntPtr]1, $lpOp)
Start-Sleep -Milliseconds 120
[void][EV]::PostMessage($script:ed, 0x0202, [IntPtr]0, $lpOp)
Start-Sleep -Milliseconds 400
$txt = Dump
$nOp = (Rows $txt).Count
Cmd 32812
$txt = Dump
Check ("convert: a visible loop-tail line was inserted after the block start (" + $nOp + " -> " + (Rows $txt).Count + ")") `
      ((Rows $txt).Count -eq $nOp + 1)
$srows = @(Rows $txt)
$tIdx = -1
for ($k = 0; $k -lt $srows.Count; $k++) {
    if ($srows[$k] -match [regex]::Escape($kwTail)) { $tIdx = $k; break }
}
Check ("convert: the inserted line is the loop tail (idx " + $tIdx + ")") ($tIdx -ge 0)




Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
Remove-Item Env:\ECLONE_DUMP_MODEL
if ($fails -ne 0) { Write-Host ("edit_cmd_smoke: " + $fails + " FAILED"); exit 1 }
Write-Host "[PASS] edit_cmd_smoke: all edit commands work"
