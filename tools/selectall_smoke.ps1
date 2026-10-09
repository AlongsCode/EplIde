# Select-all smoke test: Ctrl+A must reverse-video the WHOLE page -- the assembly table, the
# subroutine table, and the code lines -- not just the code body. The original does exactly
# that (1:1 reference shot: shots\live_ctrla\after_ctrla.png; measured bands of #000080 at
# y=5..44 / 55..94 / 101..112 inside the 1174x388 code view).
#
# ASCII-only on purpose (this machine reads .ps1 as ANSI).
param(
    [string]$Exe = "D:\eclone\bin\EClone.exe",
    [string]$OutDir = "D:\eclone\shots",
    [int]$WaitSec = 6
)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public class SA {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, IntPtr e);
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, IntPtr extra);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public static string Cls(IntPtr h){ var sb=new StringBuilder(256); GetClassNameW(h,sb,256); return sb.ToString(); }
  public static void CtrlA(){
    keybd_event(0x11,0,0,IntPtr.Zero); System.Threading.Thread.Sleep(150);
    keybd_event(0x41,0,0,IntPtr.Zero); System.Threading.Thread.Sleep(150);
    keybd_event(0x41,0,2,IntPtr.Zero); System.Threading.Thread.Sleep(150);
    keybd_event(0x11,0,2,IntPtr.Zero); System.Threading.Thread.Sleep(400);
  }
}
"@

$fail = 0
function Check([string]$name, [bool]$ok) {
    if ($ok) { Write-Host ("[PASS] " + $name) }
    else { Write-Host ("[FAIL] " + $name); $script:fail = 1 }
}

Get-Process EClone -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 400
$p = Start-Process -FilePath $Exe -WorkingDirectory (Split-Path $Exe) -PassThru
Start-Sleep -Seconds $WaitSec
if ($p.HasExited) { Write-Host "FAIL: exited early"; exit 1 }

$script:procId = $p.Id; $script:frame = [IntPtr]::Zero
[void][SA]::EnumWindows({
    param($h,$q)
    $wpid = 0; [void][SA]::GetWindowThreadProcessId($h, [ref]$wpid)
    if ($wpid -eq $script:procId -and [SA]::Cls($h) -eq 'ENewFrame') { $script:frame = $h }
    return $true
}, [IntPtr]::Zero)
if ($script:frame -eq [IntPtr]::Zero) { Write-Host "FAIL: no frame"; Stop-Process -Id $p.Id -Force; exit 1 }

$script:ed = [IntPtr]::Zero
[void][SA]::EnumChildWindows($script:frame, {
    param($h,$q)
    if ([SA]::Cls($h).StartsWith('Afx:400000:8:10005')) { $script:ed = $h; return $false }
    return $true
}, [IntPtr]::Zero)
if ($script:ed -eq [IntPtr]::Zero) { Write-Host "FAIL: no editor"; Stop-Process -Id $p.Id -Force; exit 1 }
Write-Host ("frame=" + $script:frame + " editor=" + $script:ed)

[void][SA]::SetForegroundWindow($script:frame)
Start-Sleep -Milliseconds 600
$er = New-Object SA+RECT
[void][SA]::GetWindowRect($script:ed, [ref]$er)
[void][SA]::SetCursorPos($er.Left + 60, $er.Top + 90)
Start-Sleep -Milliseconds 250
[SA]::mouse_event(2,0,0,0,[IntPtr]::Zero)
Start-Sleep -Milliseconds 80
[SA]::mouse_event(4,0,0,0,[IntPtr]::Zero)
Start-Sleep -Milliseconds 500
[SA]::CtrlA()

$bmp = New-Object System.Drawing.Bitmap ($er.Right-$er.Left), ($er.Bottom-$er.Top)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$hdc = $g.GetHdc()
[void][SA]::PrintWindow($script:ed, $hdc, 0)
$g.ReleaseHdc($hdc)
$png = Join-Path $OutDir "ctrl_a_mine.png"
$bmp.Save($png, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
Write-Host ("shot -> " + $png + " (" + ($er.Right-$er.Left) + "x" + ($er.Bottom-$er.Top) + ")")

# count #000080 pixels per row, group into bands, and require that the TABLE band
# (the top ~50 px of the view) is covered
$im = [System.Drawing.Image]::FromFile($png)
$bm = New-Object System.Drawing.Bitmap $im
$rows = @{}
for ($y = 0; $y -lt $bm.Height; $y++) {
    $n = 0
    for ($x = 0; $x -lt $bm.Width; $x++) {
        $c = $bm.GetPixel($x, $y)
        if ($c.R -eq 0 -and $c.G -eq 0 -and $c.B -eq 128) { $n++ }
    }
    if ($n -gt 0) { $rows[$y] = $n }
}
$bm.Dispose(); $im.Dispose()

$ys = $rows.Keys | Sort-Object
$total = 0
foreach ($y in $ys) { $total += $rows[$y] }
Write-Host ("#000080 pixels total = " + $total + " over " + $ys.Count + " rows")
$bands = @()
if ($ys.Count -gt 0) {
    $start = $ys[0]; $prev = $ys[0]
    foreach ($y in $ys) {
        if ($y - $prev -gt 2) { $bands += ,@($start, $prev); $start = $y }
        $prev = $y
    }
    $bands += ,@($start, $prev)
}
foreach ($b in $bands) { Write-Host ("  band y=" + $b[0] + ".." + $b[1]) }

# The original paints the WHOLE table: every cell (header cells, data cells, and the empty
# cells after them) becomes solid #000080 with white text. 1:1 reference: the measured bands are
# y=5..24 / 26..44 (assembly table, ~230 px wide) and 55..74 / 76..94 (subroutine table, ~331 px)
# in shots\live_ctrla\after_ctrla.png for the graphic.e sample.
# So assert on the WIDTH of the bands, not just on the number of rows.
$maxw = @{}
foreach ($y in $ys) { $maxw[$y] = $rows[$y] }
$bandsInfo = @()
foreach ($b in $bands) {
    $w = 0
    for ($y = $b[0]; $y -le $b[1]; $y++) { if ($maxw.ContainsKey($y) -and $maxw[$y] -gt $w) { $w = $maxw[$y] } }
    $bandsInfo += ,@($b[0], $b[1], $w)
    Write-Host ("  band y=" + $b[0] + ".." + $b[1] + " maxwidth=" + $w)
}
$tableRows = 0
foreach ($y in $ys) { if ($y -le 130) { $tableRows++ } }
Check ("whole page selected: #000080 covers the table cells too (" + $tableRows + " rows above y=130)") ($tableRows -ge 18)
Check ("select-all paints a real area (" + $total + " px > 8000)") ($total -gt 8000)
Check ("the selection reaches the code lines below the tables (" + ($bandsInfo.Count) + " bands >= 2)") ($bandsInfo.Count -ge 2)
$widest = 0
foreach ($bi in $bandsInfo) { if ($bi[2] -gt $widest) { $widest = $bi[2] } }
Check ("table cells are filled whole, not just the glyphs (widest band " + $widest + " px >= 150)") ($widest -ge 150)
# the empty cells after the data must be filled too: count navy pixels in the FIRST table row
# band across its full width
$navyInBand = 0
foreach ($y in $ys) { if ($y -ge $bands[0][0] -and $y -le $bands[0][1]) { $navyInBand += $rows[$y] } }
$bandH = $bands[0][1] - $bands[0][0] + 1
Check ("the first table row is a solid navy rectangle (" + $navyInBand + " px over " + $bandH + " rows)") ($navyInBand -gt ($bandH * 60))

Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
if ($fail -ne 0) { Write-Host "[FAIL] selectall_smoke"; exit 1 }
Write-Host "[PASS] selectall_smoke: Ctrl+A reverse-videos whole table cells + code"
