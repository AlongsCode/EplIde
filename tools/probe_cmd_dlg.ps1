Add-Type @"
using System;
using System.Runtime.InteropServices;
public class W {
  [DllImport("user32.dll")] public static extern IntPtr PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern IntPtr FindWindowEx(IntPtr p, IntPtr c, string cls, string win);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h, int id);
  [DllImport("user32.dll", CharSet=CharSet.Auto)] public static extern int GetWindowText(IntPtr h, System.Text.StringBuilder s, int n);
  [DllImport("user32.dll", CharSet=CharSet.Auto)] public static extern int GetClassName(IntPtr h, System.Text.StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern IntPtr GetParent(IntPtr h);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  public struct RECT { public int l, t, r, b; }
}
"@
$pid_ = [int]$args[0]
$cmd  = [int]$args[1]
$want = if ($args.Count -gt 2) { $args[2] } else { "" }
$found = @()
$cb = [W+EnumProc]{
  param($h, $l)
  $p = 0
  [void][W]::GetWindowThreadProcessId($h, [ref]$p)
  if ($p -eq $pid_) {
    $cls = New-Object System.Text.StringBuilder 128
    [void][W]::GetClassName($h, $cls, 128)
    $t = New-Object System.Text.StringBuilder 256
    [void][W]::GetWindowText($h, $t, 256)
    $script:found += [pscustomobject]@{ hwnd=$h; cls=$cls.ToString(); title=$t.ToString(); vis=[W]::IsWindowVisible($h) }
  }
  return $true
}
# find the frame first
$frame = [IntPtr]::Zero
foreach ($h in @(Get-Process -Id $pid_).MainWindowHandle) { $frame = $h }
Write-Host ("frame={0}" -f $frame)
[void][W]::PostMessage($frame, 0x111, [IntPtr]$cmd, [IntPtr]::Zero)
Start-Sleep -Milliseconds 900
$script:found = @()
[void][W]::EnumWindows($cb, [IntPtr]::Zero)
foreach ($f in $script:found) {
  if ($f.cls -like "*#32770*" -or $f.title -like "*$want*") {
    $r = New-Object W+RECT
    [void][W]::GetWindowRect($f.hwnd, [ref]$r)
    Write-Host ("{0} {1} {2}x{3} '{4}' vis={5}" -f $f.hwnd, $f.cls, ($r.r-$r.l), ($r.b-$r.t), $f.title, $f.vis)
    if ($f.cls -like "*#32770*" -and ($r.r-$r.l) -gt 100) {
      foreach ($id in @(1151,1152,1041,1042,1101,1102,1047,1,2)) {
        $c = [W]::GetDlgItem($f.hwnd, $id)
        if ($c -ne [IntPtr]::Zero) {
          $cc = New-Object System.Text.StringBuilder 64
          [void][W]::GetClassName($c, $cc, 64)
          $tt = New-Object System.Text.StringBuilder 128
          [void][W]::GetWindowText($c, $tt, 128)
          $rr = New-Object W+RECT
          [void][W]::GetWindowRect($c, [ref]$rr)
          Write-Host ("    id={0} {1} {2}x{3} '{4}'" -f $id, $cc.ToString(), ($rr.r-$rr.l), ($rr.b-$rr.t), $tt.ToString())
        }
      }
    }
  }
}
