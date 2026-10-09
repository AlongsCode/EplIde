# Cell editing smoke test (ASCII only).
#  1) start EClone with ECLONE_DUMP_MODEL
#  2) ask python (tools\cellpos.py) where the "public" checkbox cell of a given row is
#  3) click it, re-dump the model and assert that cell is now checked
# usage: powershell -NoProfile -ExecutionPolicy Bypass -File cell_smoke.ps1
param(
    [string]$Exe = "D:\eclone\bin\EClone.exe",
    [string]$OutDir = "D:\eclone\shots",
    [string]$ColHead = "",
    [string]$RowName = "",
    [int]$WaitSec = 7
)
$ErrorActionPreference = 'Continue'
Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public class CH {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, string l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  public static string Cls(IntPtr h){ var sb=new StringBuilder(128); GetClassNameW(h,sb,128); return sb.ToString(); }
}
"@
$colHead = $ColHead
$rowName = $RowName
if ($colHead -eq "") { $colHead = [char]0x516C + [char]0x5F00 }                       # "public" column
if ($rowName -eq "") { $rowName = "_" + [char]0x56FE + [char]0x5F62 + [char]0x6309 + [char]0x94AE + "1_" + [char]0x88AB + [char]0x5355 + [char]0x51FB }   # the sample's 2nd subroutine

$model = Join-Path $OutDir "model_cell.txt"
Remove-Item $model -Force -ErrorAction SilentlyContinue
$env:ECLONE_DUMP_MODEL = $model
$p = Start-Process -FilePath $Exe -PassThru
Start-Sleep -Seconds $WaitSec
if ($p.HasExited) { Write-Host "FAIL: exited early"; exit 1 }

$script:frame = [IntPtr]::Zero; $script:ed = [IntPtr]::Zero
[void][CH]::EnumWindows({
    param($h,$q)
    $wpid = 0; [void][CH]::GetWindowThreadProcessId($h, [ref]$wpid)
    if ($wpid -eq $p.Id -and [CH]::Cls($h) -eq 'ENewFrame') { $script:frame = $h }
    return $true
}, [IntPtr]::Zero)
[void][CH]::EnumChildWindows($script:frame, {
    param($h,$q)
    if ([CH]::Cls($h).StartsWith('Afx:400000:8:10005')) { $script:ed = $h; return $false }
    return $true
}, [IntPtr]::Zero)
if ($script:ed -eq [IntPtr]::Zero) { Write-Host "FAIL: no editor"; Stop-Process -Id $p.Id -Force; exit 1 }

[void][CH]::PostMessage($script:ed, 0x0400 + 102, [IntPtr]::Zero, [IntPtr]::Zero)   # EC_WM_REDUMP
Start-Sleep -Milliseconds 600

# ---- part 0: block-END keyword rows must NOT be rendered (the original hides them) ----
# The sample source has block-end keyword lines (GBK text), but the display
# model must not contain a row for them.
$tail = [char]0x7ED3 + [char]0x675F            # the "end" suffix
$loopTail = [char]0x8BA1 + [char]0x6B21 + [char]0x5FAA + [char]0x73AF + [char]0x5C3E   # loop-tail keyword
$txt = Get-Content $model -Encoding Default -Raw
if ($txt -match ("\[" + [regex]::Escape($tail) + "\]")) {
    Write-Host "[FAIL] hidden block-end keyword row (XX + end-suffix) is still rendered"
} elseif ($txt -notmatch ("\| " + [regex]::Escape($loopTail))) {
    Write-Host "[FAIL] loop-tail keyword row (XX + loop-tail) is NOT rendered - the original shows it"
} else {
    Write-Host "[PASS] block-end keywords: end-suffix hidden, loop-tail rendered"
}

$pos0 = & python "D:\eclone\tools\cellpos.py" $model $colHead $rowName
if ($LASTEXITCODE -ne 0) { Write-Host ("FAIL: cannot locate cell " + $colHead + " / " + $rowName); Stop-Process -Id $p.Id -Force; exit 1 }
# Scroll the target row into view FIRST: the sample fixture grew (round 21 added table pages),
# so the row can start below the visible client area. Clicking it there would make the editor
# auto-scroll (EnsureVisible) and the second click would land on a different row.
$cyContent = [int](($pos0 -split '\s+')[1])
$script:scroll = [Math]::Max(0, $cyContent - 150)
$wparam = [IntPtr](4 -bor ($script:scroll -shl 16))          # WM_VSCROLL / SB_THUMBPOSITION
[void][CH]::SendMessage($script:ed, 0x0115, $wparam, [IntPtr]::Zero)
Start-Sleep -Milliseconds 400
[void][CH]::PostMessage($script:ed, 0x0400 + 102, [IntPtr]::Zero, [IntPtr]::Zero)   # EC_WM_REDUMP
Start-Sleep -Milliseconds 600
Write-Host ("scrolled to " + $script:scroll + " (content y=" + $cyContent + ")")

$pos = & python "D:\eclone\tools\cellpos.py" $model $colHead $rowName $script:scroll
$xy = $pos -split '\s+'
$cx = [int]$xy[0]; $cy = [int]$xy[1]
Write-Host ("click cell " + $colHead + " at client " + $cx + "," + $cy)

$before = (Select-String -Path $model -Pattern ("\[" + [regex]::Escape($rowName) + "\]") -Encoding Default).Line
[void][CH]::PostMessage($script:ed, 0x0201, [IntPtr]1, [IntPtr](($cx -band 0xFFFF) -bor ($cy -shl 16)))
Start-Sleep -Milliseconds 150
[void][CH]::PostMessage($script:ed, 0x0202, [IntPtr]0, [IntPtr](($cx -band 0xFFFF) -bor ($cy -shl 16)))
Start-Sleep -Milliseconds 800
[void][CH]::PostMessage($script:ed, 0x0400 + 102, [IntPtr]::Zero, [IntPtr]::Zero)
Start-Sleep -Milliseconds 600

$after = (Select-String -Path $model -Pattern ("\[" + [regex]::Escape($rowName) + "\]") -Encoding Default).Line
Write-Host ("before: " + $before)
Write-Host ("after : " + $after)
if ($before -ne $after -and $after -match '\[\]\*') { Write-Host "[PASS] flag cell toggled" }
else { Write-Host "[FAIL] flag cell did not toggle" }

# ---- part 2: double-click a TEXT cell (the subroutine name) -> EDIT -> type -> Enter ----
# The editor creates a real EDIT child when a text cell is double-clicked (like the original).
$colName = $null
$rowName2 = "_" + [char]0x56FE + [char]0x5F62 + [char]0x6309 + [char]0x94AE + "1_" + [char]0x88AB + [char]0x5355 + [char]0x51FB
$pos2 = & python "D:\eclone\tools\cellpos.py" $model ([char]0x5B50 + [char]0x7A0B + [char]0x5E8F + [char]0x540D) $rowName2 $script:scroll
if ($LASTEXITCODE -eq 0) {
    $xy2 = $pos2 -split '\s+'
    $cx2 = [int]$xy2[0]; $cy2 = [int]$xy2[1]
    $lp2 = [IntPtr](($cx2 -band 0xFFFF) -bor ($cy2 -shl 16))
    Write-Host ("double-click name cell at " + $cx2 + "," + $cy2)
    [void][CH]::PostMessage($script:ed, 0x0201, [IntPtr]1, $lp2)
    [void][CH]::PostMessage($script:ed, 0x0202, [IntPtr]0, $lp2)
    [void][CH]::PostMessage($script:ed, 0x0203, [IntPtr]1, $lp2)     # WM_LBUTTONDBLCLK
    [void][CH]::PostMessage($script:ed, 0x0202, [IntPtr]0, $lp2)
    Start-Sleep -Milliseconds 700
    $script:edit = [IntPtr]::Zero
    [void][CH]::EnumChildWindows($script:ed, {
        param($h,$q)
        if ([CH]::Cls($h) -eq 'Edit') { $script:edit = $h; return $false }
        return $true
    }, [IntPtr]::Zero)
    if ($script:edit -ne [IntPtr]::Zero) {
        $newName = $rowName2 + "_X"
        [void][CH]::SendMessage($script:edit, 0x000C, [IntPtr]::Zero, $newName)   # WM_SETTEXT
        [void][CH]::PostMessage($script:edit, 0x0100, [IntPtr]0x0D, [IntPtr]::Zero)  # WM_KEYDOWN VK_RETURN
        Start-Sleep -Milliseconds 900
        [void][CH]::PostMessage($script:ed, 0x0400 + 102, [IntPtr]::Zero, [IntPtr]::Zero)
        Start-Sleep -Milliseconds 600
        $txt = Get-Content $model -Encoding Default -Raw
        if ($txt -match [regex]::Escape($newName)) { Write-Host "[PASS] in-place text edit committed" }
        else { Write-Host "[FAIL] in-place text edit not committed" }
    } else {
        Write-Host "[FAIL] no EDIT child created on double-click"
    }
} else {
    Write-Host "[FAIL] cannot locate the subroutine-name cell"
}
Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
Remove-Item Env:\ECLONE_DUMP_MODEL
