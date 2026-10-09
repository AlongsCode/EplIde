# Dump the window tree of an already-running process by PID (read-only).
# ASCII-only on purpose (this machine reads .ps1 as ANSI).
# Usage: powershell -NoProfile -ExecutionPolicy Bypass -File probe_pid.ps1 -Pid 12108 -Out shots\live.txt
param(
    [int]$ProcId = 0,
    [string]$Out = "D:\eclone\shots\live_windows.txt"
)
$ErrorActionPreference = 'Stop'

Add-Type @"
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public class PP {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern IntPtr GetParent(IntPtr h);
  [DllImport("user32.dll")] public static extern int GetWindowLong(IntPtr h, int i);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public static string Cls(IntPtr h){ var sb=new StringBuilder(256); GetClassNameW(h,sb,256); return sb.ToString(); }
  public static void Dump(IntPtr top, List<string> o){
    IntPtr root = top;
    EnumChildWindows(top, delegate(IntPtr c, IntPtr p){
      int d = 0; IntPtr q = GetParent(c);
      while (q != IntPtr.Zero && q != root) { d++; q = GetParent(q); }
      RECT r; GetWindowRect(c, out r);
      o.Add(new string(' ', (d+1)*2) + string.Format("{0,-30} vis={1,-6} {2},{3} {4}x{5}  style=0x{6:X8} ex=0x{7:X8}",
          Cls(c), IsWindowVisible(c), r.Left, r.Top, r.Right-r.Left, r.Bottom-r.Top,
          GetWindowLong(c,-16), GetWindowLong(c,-20)));
      return true;
    }, IntPtr.Zero);
  }
}
"@

$lines = New-Object System.Collections.Generic.List[string]
$cb = [PP+EnumProc]{
    param($h, $p2)
    $wpid = 0
    [void][PP]::GetWindowThreadProcessId($h, [ref]$wpid)
    if ($wpid -eq $ProcId -and [PP]::IsWindowVisible($h)) {
        $r = New-Object PP+RECT
        [void][PP]::GetWindowRect($h, [ref]$r)
        $lines.Add(("TOP " + [PP]::Cls($h) + "  " + $r.Left + "," + $r.Top + " " + ($r.Right-$r.Left) + "x" + ($r.Bottom-$r.Top) + "  hwnd=" + $h))
        [PP]::Dump($h, $lines)
    }
    return $true
}
[void][PP]::EnumWindows($cb, [IntPtr]::Zero)
$lines | Set-Content -Path $Out -Encoding UTF8
Write-Host ("wrote " + $Out + " lines=" + $lines.Count)

