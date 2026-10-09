# One-off probe: does the 窗口 menu work? The five items in the original RC are the MFC
# standard IDs (ID_WINDOW_ARRANGE=57649 / CASCADE=57650 / TILE_HORZ=57651 / SPLIT=57653),
# so CMDIFrameWnd's own message map may already handle them.
# ASCII-only on purpose.
param([string]$Exe = "D:\eclone\bin\EClone.exe", [int]$WaitSec = 6)
$ErrorActionPreference = 'Stop'
Add-Type @"
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public class WM {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool IsZoomed(IntPtr h);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public static string Cls(IntPtr h){ var sb=new StringBuilder(128); GetClassNameW(h,sb,128); return sb.ToString(); }
  public static string Rect(IntPtr h){ RECT r; GetWindowRect(h, out r); return (r.Right-r.Left)+"x"+(r.Bottom-r.Top)+" at "+r.Left+","+r.Top; }
  public static void Collect(IntPtr top, string cls, List<IntPtr> outl){
    EnumChildWindows(top, delegate(IntPtr h, IntPtr p) { if (Cls(h)==cls) outl.Add(h); return true; }, IntPtr.Zero);
  }
}
"@
Start-Process -FilePath taskkill -ArgumentList '/f','/im','EClone.exe' -Wait -WindowStyle Hidden
Start-Sleep -Milliseconds 400
$p = Start-Process -FilePath $Exe -PassThru
Start-Sleep -Seconds $WaitSec
$script:procId = $p.Id; $script:frame = [IntPtr]::Zero
[void][WM]::EnumWindows({
    param($h,$q)
    $wpid = 0; [void][WM]::GetWindowThreadProcessId($h, [ref]$wpid)
    if ($wpid -eq $script:procId -and [WM]::IsWindowVisible($h) -and [WM]::Cls($h) -eq 'ENewFrame') { $script:frame = $h }
    return $true
}, [IntPtr]::Zero)
$kids = New-Object System.Collections.Generic.List[IntPtr]
[WM]::Collect($script:frame, 'AfxMDIFrame42s', $kids)
Write-Host ("frame=" + $script:frame + " mdi children=" + $kids.Count)
if ($kids.Count -lt 1) { Write-Host "FAIL: no mdi child"; Stop-Process -Id $p.Id -Force; exit 1 }
Write-Host ("before      : zoomed=" + [WM]::IsZoomed($kids[0]) + " " + [WM]::Rect($kids[0]))

foreach ($cmd in @(57650, 57651, 57649, 57653)) {
    [void][WM]::SendMessage($script:frame, 0x0111, [IntPtr]$cmd, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 700
    $k = New-Object System.Collections.Generic.List[IntPtr]
    [WM]::Collect($script:frame, 'AfxMDIFrame42s', $k)
    if ($k.Count -ge 1) {
        Write-Host ("cmd " + $cmd + " -> zoomed=" + [WM]::IsZoomed($k[0]) + " " + [WM]::Rect($k[0]))
    } else {
        Write-Host ("cmd " + $cmd + " -> no mdi child left")
    }
}
Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
