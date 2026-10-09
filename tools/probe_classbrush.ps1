# Render the background pattern of a window class: read GCL_HBRBACKGROUND of the
# class of every window matching -Class in the target process, select that brush into
# a memory DC, FillRect an 8x8 (and 16x16) area, and print the resulting pixel grid.
# This tells us whether the "dot grid" behind the original window designer is a
# CreatePatternBrush() class background or something painted at runtime.
# ASCII only.
param(
    [string]$Exe = "C:\Program Files (x86)\e\e.exe",
    [string]$File = "D:\eclone\shots\e_samples\graphic.e",
    [string]$Class = "WTWindow",
    [string]$ExeArg = "",
    [int]$WaitSec = 12
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Text;
using System.Drawing;
using System.Runtime.InteropServices;
public class CB {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern int GetClassLong(IntPtr h, int i);
  [DllImport("user32.dll")] public static extern IntPtr GetClassLongPtr(IntPtr h, int i);
  [DllImport("gdi32.dll")] public static extern IntPtr CreateCompatibleDC(IntPtr h);
  [DllImport("gdi32.dll")] public static extern IntPtr SelectObject(IntPtr dc, IntPtr o);
  [DllImport("gdi32.dll")] public static extern bool DeleteDC(IntPtr dc);
  [DllImport("gdi32.dll")] public static extern uint GetObjectType(IntPtr o);
  [DllImport("gdi32.dll")] public static extern bool PatBlt(IntPtr dc, int x, int y, int w, int h, uint rop);
  [DllImport("gdi32.dll")] public static extern uint GetPixel(IntPtr dc, int x, int y);
  [DllImport("gdi32.dll")] public static extern IntPtr CreateCompatibleBitmap(IntPtr dc, int w, int h);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public static string Cls(IntPtr h){ var sb=new StringBuilder(256); GetClassNameW(h,sb,256); return sb.ToString(); }
  // returns "type,bits" where bits is 16 rows of 16 chars
  public static string Probe(IntPtr hwnd) {
    int brush = GetClassLong(hwnd, -10);           // GCL_HBRBACKGROUND
    int style = GetClassLong(hwnd, -26);           // GCL_STYLE
    StringBuilder sb = new StringBuilder();
    sb.Append("hwnd=" + hwnd + " class=" + Cls(hwnd) + " brush=" + brush + " style=0x" + style.ToString("X8") + "\n");
    IntPtr dc = CreateCompatibleDC(IntPtr.Zero);
    IntPtr bmp = CreateCompatibleBitmap(dc, 16, 16);
    IntPtr old = SelectObject(dc, bmp);
    // fill white
    PatBlt(dc, 0, 0, 16, 16, 0x00FF0062 /*WHITENESS*/);
    if (brush != 0) {
      IntPtr ob = SelectObject(dc, (IntPtr)brush);
      sb.Append("brush objtype=" + GetObjectType((IntPtr)brush) + " prev=" + ob + "\n");
      PatBlt(dc, 0, 0, 16, 16, 0x00F00021 /*PATCOPY*/);
      SelectObject(dc, ob);
    } else {
      sb.Append("brush objtype=n/a\n");
    }
    for (int y = 0; y < 16; y++) {
      sb.Append("  ");
      for (int x = 0; x < 16; x++) {
        uint c = GetPixel(dc, x, y);
        int r = (int)(c & 0xFF), g = (int)((c >> 8) & 0xFF), b = (int)((c >> 16) & 0xFF);
        sb.Append((r < 128) ? '#' : (r == 255 && g == 255 && b == 255 ? '.' : '+'));
      }
      sb.Append("\n");
    }
    SelectObject(dc, old);
    DeleteDC(dc);
    return sb.ToString();
  }
}
"@

$args2 = @()
if ($File -ne "" -and $File -ne "none") { $args2 += $File }
if ($ExeArg -ne "") { $args2 += $ExeArg }
$p = Start-Process -FilePath $Exe -WorkingDirectory (Split-Path $Exe) -ArgumentList $args2 -PassThru
Start-Sleep -Seconds $WaitSec
if ($p.HasExited) { Write-Host "exited early"; exit 1 }

$targets = New-Object System.Collections.Generic.List[IntPtr]
[void][CB]::EnumWindows([CB+EnumProc]{
    param($h, $p2)
    $wpid = 0
    [void][CB]::GetWindowThreadProcessId($h, [ref]$wpid)
    if ($wpid -eq $p.Id) {
        [void][CB]::EnumChildWindows($h, [CB+EnumProc]{
            param($c, $q)
            if ([CB]::Cls($c) -eq $Class) { $targets.Add($c) }
            return $true
        }, [IntPtr]::Zero)
    }
    return $true
}, [IntPtr]::Zero)
Write-Host ("targets=" + $targets.Count)
foreach ($t in $targets) { Write-Host ([CB]::Probe($t)) }
Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
