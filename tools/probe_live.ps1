# Launch a fresh (non-elevated) e.exe with a sample .e, dump the window tree and
# capture 1:1 PrintWindow PNGs of the frame and of every editor view.
# ASCII-only on purpose (this machine reads .ps1 as ANSI).
param(
    [string]$Exe = "C:\Program Files (x86)\e\e.exe",
    [string]$File = "",
    [string]$OutDir = "D:\eclone\shots\live",
    [int]$WaitSec = 14,
    [switch]$Keep
)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
Add-Type -AssemblyName System.Drawing

Add-Type @"
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public class LV {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern IntPtr GetParent(IntPtr h);
  [DllImport("user32.dll")] public static extern int GetWindowLong(IntPtr h, int i);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr a, int x, int y, int cx, int cy, uint f);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public static string Text(IntPtr h){
    var sb=new StringBuilder(1024); GetWindowTextW(h,sb,1024);
    if (sb.Length>0) return sb.ToString();
    IntPtr buf = Marshal.AllocHGlobal(4096);
    for (int i=0;i<2048;i++) Marshal.WriteInt16(buf, i*2, 0);
    SendMessage(h, 0x000D, (IntPtr)2048, buf);
    string s = Marshal.PtrToStringUni(buf);
    Marshal.FreeHGlobal(buf);
    return s == null ? "" : s;
  }
  public static string Cls(IntPtr h){ var sb=new StringBuilder(256); GetClassNameW(h,sb,256); return sb.ToString(); }
  public static void Dump(IntPtr top, List<string> o){
    IntPtr root = top;
    EnumChildWindows(top, delegate(IntPtr c, IntPtr p){
      int d = 0; IntPtr q = GetParent(c);
      while (q != IntPtr.Zero && q != root) { d++; q = GetParent(q); }
      RECT r; GetWindowRect(c, out r);
      o.Add(new string(' ', (d+1)*2) + string.Format("{0,-30} vis={1,-6} {2},{3} {4}x{5}  style=0x{6:X8}  {7}",
          Cls(c), IsWindowVisible(c), r.Left, r.Top, r.Right-r.Left, r.Bottom-r.Top,
          GetWindowLong(c,-16), Text(c)));
      return true;
    }, IntPtr.Zero);
  }
}
"@

$args2 = @()
if ($File -ne "") { $args2 += $File }
$p = Start-Process -FilePath $Exe -WorkingDirectory (Split-Path $Exe) -ArgumentList $args2 -PassThru
Start-Sleep -Seconds $WaitSec
if ($p.HasExited) { Write-Host "exited early"; exit 1 }

$list = New-Object System.Collections.Generic.List[object]
$cb = [LV+EnumProc]{
    param($h, $p2)
    $wpid = 0
    [void][LV]::GetWindowThreadProcessId($h, [ref]$wpid)
    if ($wpid -eq $p.Id -and [LV]::IsWindowVisible($h)) {
        $r = New-Object LV+RECT
        [void][LV]::GetWindowRect($h, [ref]$r)
        if (($r.Right-$r.Left) -gt 100) { $list.Add([pscustomobject]@{H=$h;C=[LV]::Cls($h);T=[LV]::Text($h)}) }
    }
    return $true
}
[void][LV]::EnumWindows($cb, [IntPtr]::Zero)
$frame = $list | Where-Object { $_.C -eq 'ENewFrame' } | Select-Object -First 1
if (-not $frame) { Write-Host "no frame"; if (-not $Keep) { $p.Kill() }; exit 1 }
Write-Host ("frame hwnd=" + $frame.H + " title=" + $frame.T + " pid=" + $p.Id)

$lines = New-Object System.Collections.Generic.List[string]
[LV]::Dump($frame.H, $lines)
$lines | Set-Content -Path (Join-Path $OutDir "tree.txt") -Encoding UTF8

function Shot([string]$tag, [IntPtr]$hwnd) {
    $r = New-Object LV+RECT
    [void][LV]::GetWindowRect($hwnd, [ref]$r)
    $w = $r.Right-$r.Left; $h = $r.Bottom-$r.Top
    if ($w -le 0 -or $h -le 0) { return }
    $bmp = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $hdc = $g.GetHdc()
    [void][LV]::PrintWindow($hwnd, $hdc, 0)
    $g.ReleaseHdc($hdc)
    $png = Join-Path $OutDir $tag
    $bmp.Save($png, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    Write-Host ("shot: " + $png + " " + $w + "x" + $h)
}

[void][LV]::SetForegroundWindow($frame.H)
Start-Sleep -Milliseconds 500
Shot "frame.png" $frame.H

# editor views: children of AfxMDIFrame42s
$views = New-Object System.Collections.Generic.List[IntPtr]
[LV]::EnumChildWindows($frame.H, [LV+EnumProc]{
    param($h, $p2)
    if ([LV]::Cls($h) -eq 'AfxMDIFrame42s') {
        [LV]::EnumChildWindows($h, [LV+EnumProc]{
            param($c, $q)
            $cl = [LV]::Cls($c)
            if ($cl.StartsWith('Afx:400000:b')) { $views.Add($c) }
            return $true
        }, [IntPtr]::Zero) | Out-Null
    }
    return $true
}, [IntPtr]::Zero) | Out-Null

Write-Host ("views=" + $views.Count)
$i = 0
foreach ($v in $views) {
    Shot ("view$i.png") $v
    $i++
}

# the document tab strip is a real window of class CCustomTabCtrl at the bottom of
# the MDI client area (original layout) - shoot it 1:1 so EClone can match it
$tabbars = New-Object System.Collections.Generic.List[IntPtr]
[LV]::EnumChildWindows($frame.H, [LV+EnumProc]{
    param($h, $p2)
    if ([LV]::Cls($h) -eq 'CCustomTabCtrl') { $tabbars.Add($h) }
    return $true
}, [IntPtr]::Zero) | Out-Null
Write-Host ("tabbars=" + $tabbars.Count)
$i = 0
foreach ($t in $tabbars) {
    $r = New-Object LV+RECT
    [void][LV]::GetWindowRect($t, [ref]$r)
    Write-Host ("tabbar rect=" + $r.Left + "," + $r.Top + " " + ($r.Right-$r.Left) + "x" + ($r.Bottom-$r.Top))
    Shot ("tabbar$i.png") $t
    $i++
}
Write-Host ("pid=" + $p.Id)
if (-not $Keep) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }
