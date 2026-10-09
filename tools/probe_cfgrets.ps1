# Open the ORIGINAL IDE's system-config dialog and dump every child's SCREEN rect + parent-relative
# offset, so template dialog units can be checked against what really renders.
# usage: powershell -NoProfile -ExecutionPolicy Bypass -File tools\probe_cfgrets.ps1
# ASCII only.
param(
    [string]$Exe = "C:\Program Files (x86)\e\e.exe",
    [string]$File = "D:\eclone\shots\e_samples\graphic.e",
    [int]$Cmd = 32876,
    [int]$WaitSec = 14,
    [string]$Out = "D:\eclone\shots\live_syscfg\rects.txt"
)
$ErrorActionPreference = 'Stop'
Add-Type @"
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public class CR {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern int GetDlgCtrlID(IntPtr h);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public static string Cls(IntPtr h){ var sb=new StringBuilder(128); GetClassNameW(h,sb,128); return sb.ToString(); }
  public static string Text(IntPtr h){ var sb=new StringBuilder(512); GetWindowTextW(h,sb,512); return sb.ToString(); }
}
"@
Get-Process e -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 2
$p = Start-Process -FilePath $Exe -WorkingDirectory (Split-Path $Exe) -ArgumentList @($File) -PassThru
Start-Sleep -Seconds $WaitSec
if ($p.HasExited) { Write-Host "exited early"; exit 1 }

$script:frame = [IntPtr]::Zero
[void][CR]::EnumWindows([CR+EnumProc]{
    param($h,$q)
    $wpid = 0; [void][CR]::GetWindowThreadProcessId($h, [ref]$wpid)
    if ($wpid -eq $p.Id -and [CR]::Cls($h) -eq 'ENewFrame') { $script:frame = $h }
    return $true
}, [IntPtr]::Zero)
[void][CR]::PostMessage($script:frame, 0x111, [IntPtr]$Cmd, [IntPtr]::Zero)
Start-Sleep -Seconds 3

$script:dlg = [IntPtr]::Zero
[void][CR]::EnumWindows([CR+EnumProc]{
    param($h,$q)
    $wpid = 0; [void][CR]::GetWindowThreadProcessId($h, [ref]$wpid)
    if ($wpid -eq $p.Id -and [CR]::IsWindowVisible($h) -and [CR]::Cls($h) -eq '#32770') {
        $script:dlg = $h
    }
    return $true
}, [IntPtr]::Zero)
if ($script:dlg -eq [IntPtr]::Zero) { Stop-Process -Id $p.Id -Force; Write-Host "no dialog"; exit 1 }
$dr = New-Object CR+RECT
[void][CR]::GetWindowRect($script:dlg, [ref]$dr)
Write-Host ("dlg " + $dr.Left + "," + $dr.Top + " " + ($dr.Right-$dr.Left) + "x" + ($dr.Bottom-$dr.Top))

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("dlg " + $dr.Left + "," + $dr.Top + " " + ($dr.Right-$dr.Left) + "x" + ($dr.Bottom-$dr.Top))

# --- find the tab control and switch pages with REAL mouse clicks, then dump VISIBLE children ---
$script:tab = [IntPtr]::Zero
[void][CR]::EnumChildWindows($script:dlg, [CR+EnumProc]{
    param($c,$q)
    if ([CR]::Cls($c) -eq 'SysTabControl32') { $script:tab = $c }
    return $true
}, [IntPtr]::Zero)
[void][CR]::EnumChildWindows($script:dlg, [CR+EnumProc]{
    param($c,$q)
    if ([CR]::Cls($c) -eq 'SysTabControl32') { $script:tab = $c }
    return $true
}, [IntPtr]::Zero)
$tr = New-Object CR+RECT
[void][CR]::GetWindowRect($script:tab, [ref]$tr)
Write-Host ("tab " + $tr.Left + "," + $tr.Top + " " + ($tr.Right-$tr.Left) + "x" + ($tr.Bottom-$tr.Top))

# tab item rectangles come from TCM_GETITEMRECT, which needs a buffer inside the target process
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class TR2 {
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("kernel32.dll")] public static extern IntPtr OpenProcess(uint a, bool b, uint pid);
  [DllImport("kernel32.dll")] public static extern IntPtr VirtualAllocEx(IntPtr p, IntPtr a, IntPtr sz, uint t, uint prot);
  [DllImport("kernel32.dll")] public static extern bool WriteProcessMemory(IntPtr p, IntPtr a, byte[] b, IntPtr n, out IntPtr w);
  [DllImport("kernel32.dll")] public static extern bool ReadProcessMemory(IntPtr p, IntPtr a, byte[] b, IntPtr n, out IntPtr r);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, IntPtr e);
  public static int[] ItemRect(IntPtr tab, int idx, uint pid) {
    IntPtr hp = OpenProcess(0x1F0FFF, false, pid);
    if (hp == IntPtr.Zero) return null;
    IntPtr mem = VirtualAllocEx(hp, IntPtr.Zero, (IntPtr)32, 0x1000, 0x04);
    if (mem == IntPtr.Zero) return null;
    IntPtr w;
    WriteProcessMemory(hp, mem, new byte[16], (IntPtr)16, out w);
    SendMessage(tab, 0x130A, (IntPtr)idx, mem);
    byte[] b = new byte[16]; IntPtr rd;
    ReadProcessMemory(hp, mem, b, (IntPtr)16, out rd);
    return new int[] { BitConverter.ToInt32(b,0), BitConverter.ToInt32(b,4),
                       BitConverter.ToInt32(b,8), BitConverter.ToInt32(b,12) };
  }
  public static void Click(int x, int y) {
    SetCursorPos(x, y);
    System.Threading.Thread.Sleep(120);
    mouse_event(0x0002, 0, 0, 0, IntPtr.Zero);
    System.Threading.Thread.Sleep(60);
    mouse_event(0x0004, 0, 0, 0, IntPtr.Zero);
  }
}
"@
for ($k = 0; $k -lt 7; $k++) {
    $ir = [TR2]::ItemRect($script:tab, $k, $p.Id)
    if ($ir -eq $null) { $lines.Add("== page $k  <no rect>"); continue }
    $x = $tr.Left + [int](($ir[0] + $ir[2]) / 2)
    $y = $tr.Top + [int](($ir[1] + $ir[3]) / 2)
    [TR2]::Click($x, $y)
    Start-Sleep -Milliseconds 500
    $lines.Add("")
    $lines.Add("== page " + $k + " ==")
    [void][CR]::EnumChildWindows($script:dlg, [CR+EnumProc]{
        param($c,$q)
        if (-not [CR]::IsWindowVisible($c)) { return $true }
        $r = New-Object CR+RECT
        [void][CR]::GetWindowRect($c, [ref]$r)
        $lines.Add(("{0,-18} id={1,-6} rel={2},{3} {4}x{5}  |{6}|" -f `
            [CR]::Cls($c), [CR]::GetDlgCtrlID($c), `
            ($r.Left-$dr.Left), ($r.Top-$dr.Top), ($r.Right-$r.Left), ($r.Bottom-$r.Top), [CR]::Text($c)))
        return $true
    }, [IntPtr]::Zero)
}

$lines | Set-Content -Path $Out -Encoding UTF8
Write-Host ("wrote " + $Out + " (" + $lines.Count + " lines)")
Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
