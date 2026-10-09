Add-Type @"
using System;
using System.Runtime.InteropServices;
public class W6 {
  [DllImport("user32.dll")] public static extern IntPtr PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr l);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern int GetDlgCtrlID(IntPtr h);
  [DllImport("user32.dll", CharSet=CharSet.Auto)] public static extern int GetClassName(IntPtr h, System.Text.StringBuilder s, int n);
  [DllImport("user32.dll", CharSet=CharSet.Auto)] public static extern int GetWindowText(IntPtr h, System.Text.StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int l, t, r, b; }
  public delegate bool EnumProc(IntPtr h, IntPtr l);
}
"@
$thepid = [int]$args[0]
$cmds = @()
if ($args.Count -gt 1) { $cmds = $args[1..($args.Count-1)] | ForEach-Object { [int]$_ } }

$script:frame = [IntPtr]::Zero
$cbTop = [W6+EnumProc]{
  param($h, $l)
  $p = 0
  [void][W6]::GetWindowThreadProcessId($h, [ref]$p)
  if ($p -eq $thepid) {
    $cls = New-Object System.Text.StringBuilder 64
    [void][W6]::GetClassName($h, $cls, 64)
    if ($cls.ToString() -eq "ENewFrame") { $script:frame = $h }
  }
  return $true
}
[void][W6]::EnumWindows($cbTop, [IntPtr]::Zero)

function DumpForms($tag) {
  $rows = New-Object System.Collections.ArrayList
  $cb = [W6+EnumProc]{
    param($h, $l)
    $cls = New-Object System.Text.StringBuilder 64
    [void][W6]::GetClassName($h, $cls, 64)
    if ($cls.ToString() -eq "WTWindow") {
      $t = New-Object System.Text.StringBuilder 128
      [void][W6]::GetWindowText($h, $t, 128)
      $wr = New-Object W6+RECT
      [void][W6]::GetWindowRect($h, [ref]$wr)
      $cr2 = New-Object W6+RECT
      [void][W6]::GetClientRect($h, [ref]$cr2)
      [void]$rows.Add(("WTWindow {0} vis={1} win={2}x{3} client={4}x{5} title='{6}'" -f `
        $h, [W6]::IsWindowVisible($h), ($wr.r-$wr.l), ($wr.b-$wr.t), ($cr2.r-$cr2.l), ($cr2.b-$cr2.t), $t.ToString()))
    }
    return $true
  }
  [void][W6]::EnumChildWindows($script:frame, $cb, [IntPtr]::Zero)
  Write-Host ("{0}: {1} WTWindow(s)" -f $tag, $rows.Count)
  foreach ($r in $rows) { Write-Host ("   " + $r) }
}

DumpForms "before"
foreach ($c in $cmds) {
  [void][W6]::PostMessage($script:frame, 0x111, [IntPtr]$c, [IntPtr]::Zero)
  Start-Sleep -Milliseconds 800
  DumpForms ("after cmd " + $c)
}
