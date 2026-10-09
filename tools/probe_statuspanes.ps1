Add-Type @"
using System;
using System.Runtime.InteropServices;
public class W3 {
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr l);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern int GetDlgCtrlID(IntPtr h);
  [DllImport("user32.dll", CharSet=CharSet.Auto)] public static extern int GetClassName(IntPtr h, System.Text.StringBuilder s, int n);
  [DllImport("user32.dll", CharSet=CharSet.Auto)] public static extern int GetWindowTextLength(IntPtr h);
  [DllImport("user32.dll", CharSet=CharSet.Auto)] public static extern int GetWindowText(IntPtr h, System.Text.StringBuilder s, int n);
  public delegate bool EnumProc(IntPtr h, IntPtr l);
}
"@
$thepid = [int]$args[0]
$tops = New-Object System.Collections.ArrayList
$cbTop = [W3+EnumProc]{
  param($h, $l)
  $p = 0
  [void][W3]::GetWindowThreadProcessId($h, [ref]$p)
  if ($p -eq $thepid) { [void]$tops.Add($h) }
  return $true
}
[void][W3]::EnumWindows($cbTop, [IntPtr]::Zero)
$res = New-Object System.Collections.ArrayList
foreach ($t in $tops) {
  $cb = [W3+EnumProc]{
    param($h, $l)
    $id = [W3]::GetDlgCtrlID($h)
    if ($id -ge 129 -and $id -le 137) {
      $cls = New-Object System.Text.StringBuilder 64
      [void][W3]::GetClassName($h, $cls, 64)
      $n = [W3]::GetWindowTextLength($h)
      $s = New-Object System.Text.StringBuilder 8192
      [void][W3]::GetWindowText($h, $s, 8192)
      [void]$res.Add(("id={0} {1} len={2} [{3}]" -f $id, $cls.ToString(), $n, $s.ToString().Replace("`r`n", "|")))
    }
    return $true
  }
  [void][W3]::EnumChildWindows($t, $cb, [IntPtr]::Zero)
}
$res | Sort-Object -Unique
