# Smoke test: resize a placed component by dragging one of its 8 handles in the
# window designer.  ASCII only.
#
# usage: powershell -NoProfile -ExecutionPolicy Bypass -File resize_smoke.ps1
param(
    [string]$Exe = "D:\eclone\bin\EClone.exe",
    [int]$WaitSec = 7,
    [int]$DX = 40,
    [int]$DY = 30
)
$ErrorActionPreference = 'Continue'
$root = "D:\eclone"
$fail = 0

Add-Type @"
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public class RZ {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool ScreenToClient(IntPtr h, ref POINT p);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern IntPtr GetParent(IntPtr h);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
  public static string Cls(IntPtr h){ var sb=new StringBuilder(128); GetClassNameW(h,sb,128); return sb.ToString(); }
  public static IntPtr form = IntPtr.Zero, comp = IntPtr.Zero, over = IntPtr.Zero;
  public static void Find(int pid) {
    form = comp = over = IntPtr.Zero;
    EnumWindows(delegate(IntPtr h, IntPtr p) {
      uint wpid = 0; GetWindowThreadProcessId(h, out wpid);
      if (wpid != (uint)pid) return true;
      EnumChildWindows(h, delegate(IntPtr c, IntPtr q) {
        string s = Cls(c);
        if (s == "WTWindow" && form == IntPtr.Zero) form = c;
        else if (s == "_EL_RgnButton" && comp == IntPtr.Zero) comp = c;
        else if (s == "ECDesignOverlay" && over == IntPtr.Zero) over = c;
        return true;
      }, IntPtr.Zero);
      return true;
    }, IntPtr.Zero);
  }
  // overlay-client coordinate of a window's lower-right corner + (3,3) = the SE handle.
  // NOTE: the overlay is a child of the DESIGNER VIEW and is padded EC_DESIGN_PAD (6) beyond the
  // form rect on every side, so an overlay coordinate is NOT a form-client coordinate -- convert
  // through the screen instead of through the form.
  public static int HandleLParam(IntPtr wnd, out int hx, out int hy) {
    RECT r; GetWindowRect(wnd, out r);
    POINT p; p.X = r.Right; p.Y = r.Bottom;
    ScreenToClient(over, ref p);
    hx = p.X + 3; hy = p.Y + 3;
    return 0;
  }
  // top-left corner - (3,3) = the NW handle
  public static int HandleLParamNW(IntPtr wnd, out int hx, out int hy) {
    RECT r; GetWindowRect(wnd, out r);
    POINT p; p.X = r.Left; p.Y = r.Top;
    ScreenToClient(over, ref p);
    hx = p.X - 3; hy = p.Y - 3;
    return 0;
  }
  public static void Drag(IntPtr overlay, int x0, int y0, int x1, int y1) {
    PostMessage(overlay, 0x0201, (IntPtr)1, (IntPtr)((y0 << 16) | (x0 & 0xFFFF)));   // WM_LBUTTONDOWN
    PostMessage(overlay, 0x0200, (IntPtr)1, (IntPtr)((y1 << 16) | (x1 & 0xFFFF)));   // WM_MOUSEMOVE
    PostMessage(overlay, 0x0202, IntPtr.Zero, (IntPtr)((y1 << 16) | (x1 & 0xFFFF)));// WM_LBUTTONUP
  }
  public static string Rect(IntPtr h) {
    RECT r; GetWindowRect(h, out r);
    return String.Format("{0},{1} {2}x{3}", r.Left, r.Top, r.Right-r.Left, r.Bottom-r.Top);
  }
}
"@

Get-Process EClone -ErrorAction SilentlyContinue | Stop-Process -Force
$env:ECLONE_DESIGN_DEMO = "1"
Remove-Item Env:\ECLONE_EXTRA_LIBS -ErrorAction SilentlyContinue
$p = Start-Process -FilePath $Exe -WorkingDirectory (Split-Path $Exe) -ArgumentList @("designer") -PassThru
Start-Sleep -Seconds $WaitSec
if ($p.HasExited) { Write-Host "[FAIL] EClone exited early"; exit 1 }

[RZ]::Find($p.Id)
Write-Host ("form=" + [RZ]::form + " comp=" + [RZ]::comp + " overlay=" + [RZ]::over)
if ([RZ]::form -eq [IntPtr]::Zero -or [RZ]::comp -eq [IntPtr]::Zero -or [RZ]::over -eq [IntPtr]::Zero) {
    Write-Host "[FAIL] design form / component / overlay window not found"
    Stop-Process -Id $p.Id -Force; exit 1
}

$before = [RZ]::Rect([RZ]::comp)
Write-Host "component before: $before"
$w0 = [int](($before -split ' ')[1] -split 'x')[0]
$h0 = [int](($before -split ' ')[1] -split 'x')[1]

$hx = 0; $hy = 0
[void][RZ]::HandleLParam([RZ]::comp, [ref]$hx, [ref]$hy)
Write-Host "SE handle at overlay client $hx,$hy"
[RZ]::Drag([RZ]::over, $hx, $hy, $hx + $DX, $hy + $DY)
Start-Sleep -Milliseconds 800

$after = [RZ]::Rect([RZ]::comp)
Write-Host "component after : $after"
$w1 = [int](($after -split ' ')[1] -split 'x')[0]
$h1 = [int](($after -split ' ')[1] -split 'x')[1]

if ($w1 -eq $w0 + $DX -and $h1 -eq $h0 + $DY) {
    Write-Host "[PASS] dragging the SE handle resized the component by $DX x $DY ($w0 x $h0 -> $w1 x $h1)"
} else {
    Write-Host "[FAIL] resize did not happen as expected ($w0 x $h0 -> $w1 x $h1)"; $fail++
}

# the NW handle must move the origin instead of the size
[void][RZ]::HandleLParamNW([RZ]::comp, [ref]$hx, [ref]$hy)
Write-Host "NW handle at overlay client $hx,$hy"
[RZ]::Drag([RZ]::over, $hx, $hy, $hx + 20, $hy + 10)
Start-Sleep -Milliseconds 800
$after2 = [RZ]::Rect([RZ]::comp)
Write-Host "component after NW drag: $after2"
$w2 = [int](($after2 -split ' ')[1] -split 'x')[0]
$h2 = [int](($after2 -split ' ')[1] -split 'x')[1]
if ($w2 -eq $w1 - 20 -and $h2 -eq $h1 - 10) {
    Write-Host "[PASS] dragging the NW handle moved the origin (size $w1 x $h1 -> $w2 x $h2)"
} else {
    Write-Host "[FAIL] NW handle resize: $w1 x $h1 -> $w2 x $h2 (expected $($w1-20) x $($h1-10))"; $fail++
}

$proc = Get-Process EClone -ErrorAction SilentlyContinue
if ($proc -ne $null) { Write-Host "[PASS] survived the resize drags" } else { Write-Host "[FAIL] EClone died"; $fail++ }
Get-Process EClone -ErrorAction SilentlyContinue | Stop-Process -Force
if ($fail -eq 0) { Write-Host "[PASS] resize_smoke: all checks passed" } else { Write-Host "[FAIL] resize_smoke: $fail check(s) failed" }
exit $fail
