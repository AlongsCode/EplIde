# Drag-select inside the original e.exe code editor and screenshot the result 1:1,
# so the selection colours can be measured. Read-only (never saves the file).
# ASCII only.
param(
    [string]$Exe = "C:\Program Files (x86)\e\e.exe",
    [string]$File = "",
    [string]$OutDir = "D:\eclone\shots\live_drag",
    [int]$WaitSec = 12,
    [int]$X0 = 400, [int]$Y0 = 60, [int]$X1 = 700, [int]$Y1 = 150
)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
Add-Type -AssemblyName System.Drawing

Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public class DG {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, IntPtr e);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr a, int x, int y, int cx, int cy, uint f);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public static string Cls(IntPtr h){ var sb=new StringBuilder(256); GetClassNameW(h,sb,256); return sb.ToString(); }
  public static string Txt(IntPtr h){ var sb=new StringBuilder(512); GetWindowTextW(h,sb,512); return sb.ToString(); }
  public static void Drag(int x0,int y0,int x1,int y1){
    SetCursorPos(x0,y0); System.Threading.Thread.Sleep(250);
    mouse_event(2,0,0,0,IntPtr.Zero); System.Threading.Thread.Sleep(150);
    int n=12;
    for(int i=1;i<=n;i++){
      SetCursorPos(x0+(x1-x0)*i/n, y0+(y1-y0)*i/n);
      System.Threading.Thread.Sleep(35);
    }
    System.Threading.Thread.Sleep(250);
    mouse_event(4,0,0,0,IntPtr.Zero); System.Threading.Thread.Sleep(250);
  }
}
"@

$args2 = @()
if ($File -ne "") { $args2 += $File }
$p = Start-Process -FilePath $Exe -WorkingDirectory (Split-Path $Exe) -ArgumentList $args2 -PassThru
Start-Sleep -Seconds $WaitSec
if ($p.HasExited) { Write-Host "exited early"; exit 1 }

$frame = [IntPtr]::Zero
[void][DG]::EnumWindows([DG+EnumProc]{
    param($h, $q)
    $wpid = 0
    [void][DG]::GetWindowThreadProcessId($h, [ref]$wpid)
    if ($wpid -eq $p.Id -and [DG]::Cls($h) -eq 'ENewFrame') { $script:frame = $h }
    return $true
}, [IntPtr]::Zero)
if ($frame -eq [IntPtr]::Zero) { Write-Host "no frame"; $p.Kill(); exit 1 }
Write-Host ("frame title=" + [DG]::Txt($frame))
[void][DG]::BringWindowToTop($frame)
[void][DG]::SetWindowPos($frame, [IntPtr]::Zero, 0, 0, 0, 0, 0x0001 -bor 0x0002 -bor 0x0040)
[void][DG]::SetForegroundWindow($frame)
Start-Sleep -Milliseconds 900

$views = New-Object System.Collections.Generic.List[IntPtr]
[void][DG]::EnumChildWindows($frame, [DG+EnumProc]{
    param($h, $q)
    if ([DG]::Cls($h).StartsWith('Afx:400000:b')) { $views.Add($h) }
    return $true
}, [IntPtr]::Zero)
Write-Host ("views=" + $views.Count)

function Shot([string]$tag, [IntPtr]$hwnd) {
    $r = New-Object DG+RECT
    [void][DG]::GetWindowRect($hwnd, [ref]$r)
    $w = $r.Right-$r.Left; $h = $r.Bottom-$r.Top
    if ($w -le 0 -or $h -le 0) { return }
    $bmp = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $hdc = $g.GetHdc()
    [void][DG]::PrintWindow($hwnd, $hdc, 0)
    $g.ReleaseHdc($hdc)
    $bmp.Save((Join-Path $OutDir $tag), [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    Write-Host ("shot: " + $tag + " " + $w + "x" + $h)
}

$best = [IntPtr]::Zero; $area = 0; $bestr = New-Object DG+RECT
foreach ($v in $views) {
    $r = New-Object DG+RECT
    [void][DG]::GetWindowRect($v, [ref]$r)
    $a = ($r.Right-$r.Left) * ($r.Bottom-$r.Top)
    if ($a -gt $area) { $area = $a; $best = $v; $bestr = $r }
}
Write-Host ("editor=" + $best + " at " + $bestr.Left + "," + $bestr.Top + " " + ($bestr.Right-$bestr.Left) + "x" + ($bestr.Bottom-$bestr.Top))
if ($best -eq [IntPtr]::Zero) { $p.Kill(); exit 1 }

Shot "before.png" $best
[DG]::Drag($bestr.Left + $X0, $bestr.Top + $Y0, $bestr.Left + $X1, $bestr.Top + $Y1)
Start-Sleep -Milliseconds 600
Shot "after.png" $best

Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
