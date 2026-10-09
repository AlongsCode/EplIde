# Smoke test for the "parameter ownership lines" -- the -/+ drawn on a statement that
# calls a support-library command. Clicking it shows/hides the "param-name: value" rows
# under EVERY such statement (the original toggles the whole document).
#
# ASCII only: this machine reads .ps1 as ANSI, and one non-ASCII byte in a comment can
# swallow the following line (see HANDOFF 3.2.9).
#
# The target statement is taken from the MODEL, not from a hardcoded pixel band, because
# the sample source changes over time (HANDOFF 3.1.8). ec_editor.cpp dumps:
#   par=N   on every code row = how many parameter rows that statement has (0 = none)
#   paramshow=<0|1>          = whether they are currently drawn
param(
    [string]$Exe = "D:\eclone\bin\EClone.exe",
    [string]$OutDir = "D:\eclone\shots",
    [int]$WaitSec = 7
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public class PH {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public static string Cls(IntPtr h){ var sb=new StringBuilder(128); GetClassNameW(h,sb,128); return sb.ToString(); }
}
"@

function Shot([string]$tag, [IntPtr]$hwnd) {
    $r = New-Object PH+RECT
    [void][PH]::GetWindowRect($hwnd, [ref]$r)
    $w = $r.Right-$r.Left; $h = $r.Bottom-$r.Top
    $bmp = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $hdc = $g.GetHdc()
    [void][PH]::PrintWindow($hwnd, $hdc, 0)
    $g.ReleaseHdc($hdc)
    $path = Join-Path $OutDir $tag
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    return $path
}

function ModelRows([string]$modelPath) {
    return @(Get-Content $modelPath -Encoding Default | Where-Object { $_ -match '^row' })
}

# First code row that really has parameter rows, plus its document-space y/h.
function FindParamRow([string[]]$rows) {
    foreach ($line in $rows) {
        if ($line -notmatch '^row\s+\d+ kind=1 ') { continue }
        if ($line -notmatch 'par=[1-9]') { continue }
        $my = [regex]::Match($line, 'y=\s*(\d+)')
        $mh = [regex]::Match($line, 'h=\s*(\d+)')
        $mp = [regex]::Match($line, 'par=(\d+)')
        if ($my.Success -and $mh.Success -and $mp.Success) {
            return @{ y = [int]$my.Groups[1].Value; h = [int]$mh.Groups[1].Value;
                      par = [int]$mp.Groups[1].Value }
        }
    }
    return $null
}

# Ink in ONE parameter-row band (13px tall) restricted to x >= 86, i.e. the parameter
# column: the gutter (x<20) and the statement's own text cannot contribute there.
function ParamInk([string]$path, [int]$y0) {
    $im = [System.Drawing.Image]::FromFile($path)
    $bm = New-Object System.Drawing.Bitmap $im
    $n = 0
    $yA = [Math]::Max(0, $y0)
    $yB = [Math]::Min($y0 + 13, $bm.Height)
    for ($y = $yA; $y -lt $yB; $y++) {
        for ($x = 86; $x -lt 240; $x++) {
            $c = $bm.GetPixel($x, $y)
            if ($c.R -ne 255 -or $c.G -ne 255 -or $c.B -ne 255) { $n = $n + 1 }
        }
    }
    $bm.Dispose(); $im.Dispose()
    return $n
}

$model = Join-Path $OutDir "model_param.txt"
Remove-Item $model -Force -ErrorAction SilentlyContinue
$env:ECLONE_DUMP_MODEL = $model

$p = Start-Process -FilePath $Exe -PassThru
Start-Sleep -Seconds $WaitSec
if ($p.HasExited) { Write-Host "FAIL: exited early"; exit 1 }
$script:pid_ = $p.Id; $script:frame = [IntPtr]::Zero; $script:ed = [IntPtr]::Zero
[void][PH]::EnumWindows({
    param($h,$q)
    $wpid = 0; [void][PH]::GetWindowThreadProcessId($h, [ref]$wpid)
    if ($wpid -eq $script:pid_ -and [PH]::IsWindowVisible($h) -and [PH]::Cls($h) -eq 'ENewFrame') { $script:frame = $h }
    return $true
}, [IntPtr]::Zero)
[void][PH]::EnumChildWindows($script:frame, {
    param($h,$q)
    if ([PH]::Cls($h).StartsWith('Afx:400000:8:10005')) { $script:ed = $h; return $false }
    return $true
}, [IntPtr]::Zero)
if ($script:ed -eq [IntPtr]::Zero) { Write-Host "[FAIL] no editor window"; Stop-Process -Id $p.Id -Force; exit 1 }

$row = FindParamRow (ModelRows $model)
if ($row -eq $null) { Write-Host "[FAIL] no code row with par>0 in the model"; Stop-Process -Id $p.Id -Force; exit 1 }

# The 1:1 editor shot is the visible viewport only (~394px), so make sure the statement
# AND the 3 parameter rows below it are visible. Scroll the least amount needed: only if
# the statement is below the viewport, push it up to a comfortable margin.
$viewH = 394
$paramBottom = $row.y + 16 + 16 * 3      # first param row top + 3 rows
$scrollPos = 0
if (($paramBottom + 6) -gt $viewH) { $scrollPos = $paramBottom + 6 - $viewH }   # too low: push up
if (($row.y - $scrollPos - 24) -lt 0) { $scrollPos = $row.y - 24 }              # too high: pull down
if ($scrollPos -lt 0) { $scrollPos = 0 }
if ($scrollPos -gt 0) {
    $wp = [IntPtr](4 -bor ($scrollPos -shl 16))
    [void][PH]::PostMessage($script:ed, 0x0115, $wp, [IntPtr]::Zero)   # WM_VSCROLL
    Start-Sleep -Milliseconds 600
}
[void][PH]::PostMessage($script:ed, 0x0400 + 102, [IntPtr]::Zero, [IntPtr]::Zero)   # EC_WM_REDUMP
Start-Sleep -Milliseconds 400

$cy = $row.y + [int]($row.h / 2) - $scrollPos
$bandY = $cy + 30          # first parameter row top = statement top + 46
Write-Host ("row y=" + $row.y + " h=" + $row.h + " par=" + $row.par +
            " scroll=" + $scrollPos + " -> click client 26," + $cy)

function ClickMark() {
    $lp = [IntPtr](26 -bor ($script:cy_ -shl 16))
    [void][PH]::PostMessage($script:ed, 0x0201, [IntPtr]1, $lp)
    Start-Sleep -Milliseconds 120
    [void][PH]::PostMessage($script:ed, 0x0202, [IntPtr]0, $lp)
    Start-Sleep -Milliseconds 600
    [void][PH]::PostMessage($script:ed, 0x0400 + 102, [IntPtr]::Zero, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 400
    [void][PH]::PostMessage($script:ed, 0x0008, [IntPtr]::Zero, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 400
}
$script:cy_ = $cy

$before = Shot "param_before.png" $script:ed
$ink0 = ParamInk $before $bandY
$show0 = (Select-String -Path $model -Pattern '^paramshow=' -Encoding Default).Line -replace 'paramshow=',''

ClickMark
$after = Shot "param_after.png" $script:ed
$ink1 = ParamInk $after $bandY
$show1 = (Select-String -Path $model -Pattern '^paramshow=' -Encoding Default).Line -replace 'paramshow=',''

ClickMark
$back = Shot "param_back.png" $script:ed
$ink2 = ParamInk $back $bandY
$show2 = (Select-String -Path $model -Pattern '^paramshow=' -Encoding Default).Line -replace 'paramshow=',''

Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
Remove-Item Env:\ECLONE_DUMP_MODEL -ErrorAction SilentlyContinue

Write-Host ("paramshow: " + $show0 + " -> " + $show1 + " -> " + $show2)
Write-Host ("param band ink: " + $ink0 + " -> " + $ink1 + " -> " + $ink2)

if ($show0 -eq '0' -and $show1 -eq '1' -and $show2 -eq '0' -and
    $ink1 -gt $ink0 + 20 -and $ink2 -lt $ink1 - 20) {
    Write-Host "[PASS] parameter lines expand and collapse"
} else {
    Write-Host "[FAIL] parameter lines did not toggle"
    exit 1
}
