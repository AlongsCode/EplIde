Add-Type @"
using System;
using System.Runtime.InteropServices;
public class W5 {
  [DllImport("user32.dll")] public static extern IntPtr PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr l);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern int GetDlgCtrlID(IntPtr h);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll", CharSet=CharSet.Auto)] public static extern int GetClassName(IntPtr h, System.Text.StringBuilder s, int n);
  public delegate bool EnumProc(IntPtr h, IntPtr l);
}
"@
$thepid = [int]$args[0]
$cmd    = [int]$args[1]

$script:frame = [IntPtr]::Zero
$cbTop = [W5+EnumProc]{
  param($h, $l)
  $p = 0
  [void][W5]::GetWindowThreadProcessId($h, [ref]$p)
  if ($p -eq $thepid) {
    $cls = New-Object System.Text.StringBuilder 64
    [void][W5]::GetClassName($h, $cls, 64)
    if ($cls.ToString() -eq "ENewFrame") { $script:frame = $h }
  }
  return $true
}
[void][W5]::EnumWindows($cbTop, [IntPtr]::Zero)
Write-Host ("ENewFrame = {0}" -f $script:frame)

function Dump($tag) {
  $states = @{}
  $cb = [W5+EnumProc]{
    param($h, $l)
    $id = [W5]::GetDlgCtrlID($h)
    if ($id -eq 129 -or $id -eq 130 -or $id -eq 133 -or $id -eq 135 -or $id -eq 59392 -or $id -eq 59393 -or $id -eq 110) {
      if (-not $states.ContainsKey($id)) { $states[$id] = [W5]::IsWindowVisible($h) }
    }
    return $true
  }
  [void][W5]::EnumChildWindows($script:frame, $cb, [IntPtr]::Zero)
  foreach ($k in ($states.Keys | Sort-Object)) { Write-Host ("{0} id={1} visible={2}" -f $tag, $k, $states[$k]) }
}

Dump "before "
[void][W5]::PostMessage($script:frame, 0x111, [IntPtr]$cmd, [IntPtr]::Zero)
Start-Sleep -Milliseconds 800
Dump "after  "
