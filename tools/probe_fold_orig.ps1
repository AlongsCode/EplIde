# Probe the ORIGINAL e.exe: find the fold marker (+ / -) in the code editor gutter,
# click it inside the real editor view and capture 1:1 before/after shots.
# ASCII only (this machine reads .ps1 as ANSI).
# usage: powershell -NoProfile -ExecutionPolicy Bypass -File probe_fold_orig.ps1 [-File x.e] [-OutDir d] [-MarkerPick 0]
param(
    [string]$Exe = "C:\Program Files (x86)\e\e.exe",
    [string]$File = "D:\eclone\shots\e_samples\graphic.e",
    [string]$OutDir = "D:\eclone\shots\live_fold",
    [int]$WaitSec = 15,
    [int]$MarkerPick = 0
)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public class FD {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern IntPtr GetParent(IntPtr h);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public static string Cls(IntPtr h){ var sb=new StringBuilder(128); GetClassNameW(h,sb,128); return sb.ToString(); }
}
"@

function Shot([string]$tag, [IntPtr]$hwnd) {
    $r = New-Object FD+RECT
    [void][FD]::GetWindowRect($hwnd, [ref]$r)
    $w = $r.Right-$r.Left; $h = $r.Bottom-$r.Top
    $bmp = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $hdc = $g.GetHdc()
    [void][FD]::PrintWindow($hwnd, $hdc, 0)
    $g.ReleaseHdc($hdc)
    $bmp.Save((Join-Path $OutDir $tag), [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    Write-Host ("shot -> " + $tag + " " + $w + "x" + $h)
    return @($w, $h)
}

$p = Start-Process -FilePath $Exe -WorkingDirectory (Split-Path $Exe) -ArgumentList @($File) -PassThru
Start-Sleep -Seconds $WaitSec
if ($p.HasExited) { Write-Host "exited early"; exit 1 }

$script:frame = [IntPtr]::Zero
[void][FD]::EnumWindows([FD+EnumProc]{
    param($h, $q)
    $wpid = 0
    [void][FD]::GetWindowThreadProcessId($h, [ref]$wpid)
    if ($wpid -eq $p.Id -and [FD]::Cls($h) -eq 'ENewFrame') { $script:frame = $h; return $false }
    return $true
}, [IntPtr]::Zero)
if ($script:frame -eq [IntPtr]::Zero) { Write-Host "no frame"; $p.Kill(); exit 1 }
Write-Host ("frame=" + $script:frame + " pid=" + $p.Id)

# editor view: the Afx:400000:b child that lives INSIDE an AfxMDIFrame42s window
$script:ed = [IntPtr]::Zero
$mdi = New-Object System.Collections.Generic.List[IntPtr]
[void][FD]::EnumChildWindows($script:frame, [FD+EnumProc]{
    param($h, $q)
    if ([FD]::Cls($h) -eq 'AfxMDIFrame42s') { $mdi.Add($h) }
    return $true
}, [IntPtr]::Zero)
foreach ($m in $mdi) {
    [void][FD]::EnumChildWindows($m, [FD+EnumProc]{
        param($h, $q)
        if ([FD]::Cls($h).StartsWith('Afx:400000:b') -and [FD]::IsWindowVisible($h)) { $script:ed = $h; return $false }
        return $true
    }, [IntPtr]::Zero)
    if ($script:ed -ne [IntPtr]::Zero) { break }
}
if ($script:ed -eq [IntPtr]::Zero) { Write-Host "no editor view"; $p.Kill(); exit 1 }
$er = New-Object FD+RECT
[void][FD]::GetWindowRect($script:ed, [ref]$er)
Write-Host ("editor=" + $script:ed + " rect=" + $er.Left + "," + $er.Top + " " + ($er.Right-$er.Left) + "x" + ($er.Bottom-$er.Top))

[void][FD]::PostMessage($script:ed, 0x0008, [IntPtr]::Zero, [IntPtr]::Zero)  # WM_KILLFOCUS hides the caret
Start-Sleep -Milliseconds 400
Shot "view_before.png" $script:ed

# find markers: dark blue (0,0,128) ink in the gutter, x = 16..40 (editor client coords)
$im = [System.Drawing.Image]::FromFile((Join-Path $OutDir "view_before.png"))
$bm = New-Object System.Drawing.Bitmap $im
$bands = New-Object System.Collections.Generic.List[object]
for ($y = 0; $y -lt $bm.Height; $y++) {
    $n = 0
    for ($x = 16; $x -lt 40; $x++) {
        $c = $bm.GetPixel($x, $y)
        if ($c.R -lt 60 -and $c.G -lt 60 -and $c.B -ge 100 -and $c.B -le 160) { $n++ }
    }
    if ($n -gt 0) { $bands.Add([pscustomobject]@{Y=$y; N=$n}) }
}
$bm.Dispose(); $im.Dispose()
# group consecutive y
$groups = New-Object System.Collections.Generic.List[object]
foreach ($b in $bands) {
    if ($groups.Count -gt 0 -and $b.Y - $groups[$groups.Count-1].Y1 -le 2) {
        $groups[$groups.Count-1].Y1 = $b.Y
        $groups[$groups.Count-1].Max = [Math]::Max($groups[$groups.Count-1].Max, $b.N)
    } else {
        $groups.Add([pscustomobject]@{Y0=$b.Y; Y1=$b.Y; Max=$b.N})
    }
}
$i = 0
foreach ($g in $groups) { Write-Host ("  marker band " + $i + ": y=" + $g.Y0 + ".." + $g.Y1 + " maxw=" + $g.Max); $i++ }
if ($groups.Count -eq 0) { Write-Host "[FAIL] no marker found"; Stop-Process -Id $p.Id -Force; exit 1 }

$g = $groups[[Math]::Min($MarkerPick, $groups.Count-1)]
$cy = [int](($g.Y0 + $g.Y1) / 2)
$cx = 26
$lp = [IntPtr](($cx -band 0xFFFF) -bor ($cy -shl 16))
Write-Host ("click marker #" + $MarkerPick + " at editor client " + $cx + "," + $cy)
[void][FD]::PostMessage($script:ed, 0x0201, [IntPtr]1, $lp)
Start-Sleep -Milliseconds 120
[void][FD]::PostMessage($script:ed, 0x0202, [IntPtr]0, $lp)
Start-Sleep -Seconds 1
[void][FD]::PostMessage($script:ed, 0x0008, [IntPtr]::Zero, [IntPtr]::Zero)
Start-Sleep -Milliseconds 400
Shot "view_after.png" $script:ed

Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
