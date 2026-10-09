# Calibration: does keybd_event + AttachThreadInput actually type into a normal app?
# If notepad gets the text, the injection mechanism is fine and any failure in the EClone
# test is about EClone; if notepad stays empty, my test harness is the broken part.
# MUST STAY PURE ASCII.
$ErrorActionPreference = 'Stop'
Add-Type @"
using System;
using System.Runtime.InteropServices;
using System.Text;
public class N {
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint a, uint b, bool f);
  [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
  [DllImport("user32.dll")] public static extern IntPtr SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int c);
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, IntPtr e);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindowW(string cls, string title);
  public static void Go(IntPtr h){
    uint me = GetCurrentThreadId(); uint pid = 0;
    uint them = GetWindowThreadProcessId(h, out pid);
    AttachThreadInput(me, them, true);
    ShowWindow(h, 9); BringWindowToTop(h); SetForegroundWindow(h);
    System.Threading.Thread.Sleep(300);
    AttachThreadInput(me, them, false);
  }
  public static void Key(byte vk){
    keybd_event(vk, 0, 0, IntPtr.Zero); System.Threading.Thread.Sleep(120);
    keybd_event(vk, 0, 2, IntPtr.Zero); System.Threading.Thread.Sleep(200);
  }
  public static string Title(IntPtr h){ var sb = new StringBuilder(512); GetWindowTextW(h, sb, 512); return sb.ToString(); }
}
"@

$np = Start-Process notepad -PassThru
Start-Sleep -Seconds 3
$np.Refresh()
$h = $np.MainWindowHandle
if ($h -eq [IntPtr]::Zero) { Write-Output "notepad has no main window handle"; exit 1 }
Write-Output ("notepad hwnd = 0x{0:X}" -f [int64]$h)

# the actual edit control inside notepad is what holds focus; find it by class Edit
Add-Type @"
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
public class E {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  public static List<IntPtr> Kids(IntPtr p){
    var o = new List<IntPtr>();
    EnumChildWindows(p, delegate(IntPtr h, IntPtr l){ o.Add(h); return true; }, IntPtr.Zero);
    return o;
  }
  public static string Cls(IntPtr h){ var sb = new StringBuilder(256); GetClassNameW(h, sb, 256); return sb.ToString(); }
}
"@
$edit = [IntPtr]::Zero
foreach ($k in [E]::Kids($h)) { if ([E]::Cls($k) -eq 'Edit') { $edit = $k; break } }
Write-Output ("notepad edit = 0x{0:X} ({1})" -f [int64]$edit, [E]::Cls($edit))

[N]::Go($h)
Start-Sleep -Milliseconds 500
[N]::Key(0x41)     # 'A'
[N]::Key(0x42)     # 'B'

# read the edit control's text back
$sb = New-Object System.Text.StringBuilder 512
[void][N]::GetWindowTextW($edit, $sb, 512)
$txt = $sb.ToString()
Write-Output ("notepad EDIT text = [" + $txt + "]")
if ($txt.Length -gt 0) { Write-Output "[PASS] keybd_event injection works (control app received input)" }
else { Write-Output "[FAIL] keybd_event injection did NOT reach notepad -> the harness is at fault" }
Stop-Process -Id $np.Id -Force -ErrorAction SilentlyContinue
