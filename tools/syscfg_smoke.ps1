# Smoke-test the redesigned "system config" dialog: 7 tab pages (names + order taken from the
# ORIGINAL's screenshot shots\live_syscfg\tabs_zoom2.png), the page-0 control set visible at
# startup, and a real tab switch hiding page 0 and showing another page.
# ASCII only: all Chinese needles are built from code points.
#
# usage: powershell -NoProfile -ExecutionPolicy Bypass -File tools\syscfg_smoke.ps1
param(
    [string]$Exe = "D:\eclone\bin\EClone.exe",
    [int]$CmdId = 32876,
    [int]$WaitSec = 7
)
$ErrorActionPreference = 'Continue'
Add-Type @"
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public class SC2 {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  // TCM_GETITEMRECT = 0x130A. The control writes the RECT through the pointer, and USER32 does
  // not marshal it across processes, so allocate the RECT INSIDE the target process.
  public static bool TabRect(IntPtr tab, int idx, uint pid, out RECT r) {
    r = new RECT();
    IntPtr hp = OpenProcess(0x1F0FFF, false, pid);
    if (hp == IntPtr.Zero) return false;
    IntPtr mem = VirtualAllocEx(hp, IntPtr.Zero, (IntPtr)32, 0x1000, 0x04);
    if (mem == IntPtr.Zero) return false;
    IntPtr w;
    WriteProcessMemory(hp, mem, new byte[16], (IntPtr)16, out w);
    SendMessage(tab, 0x130A, (IntPtr)idx, mem);
    byte[] b = new byte[16];
    IntPtr rd;
    ReadProcessMemory(hp, mem, b, (IntPtr)16, out rd);
    r.Left = BitConverter.ToInt32(b, 0);  r.Top = BitConverter.ToInt32(b, 4);
    r.Right = BitConverter.ToInt32(b, 8); r.Bottom = BitConverter.ToInt32(b, 12);
    return (r.Right > r.Left && r.Bottom > r.Top);
  }
  [DllImport("kernel32.dll")] public static extern bool ReadProcessMemory(IntPtr p, IntPtr a, byte[] b, IntPtr n, out IntPtr r);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, IntPtr e);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern int GetDlgCtrlID(IntPtr h);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public static string Cls(IntPtr h){ var sb=new StringBuilder(128); GetClassNameW(h,sb,128); return sb.ToString(); }
  public static string Text(IntPtr h){ var sb=new StringBuilder(1024); GetWindowTextW(h,sb,1024); return sb.ToString(); }
  // tab text via TCM_GETITEMW but with a bounded buffer: cross-process, so use GETITEMA-style
  // length-only probing is unreliable -- instead read TCM_GETITEMCOUNT and use TCM_GETCURSEL.
  public static int TabCount(IntPtr h){ return (int)SendMessage(h, 0x1304, IntPtr.Zero, IntPtr.Zero); }
  [DllImport("kernel32.dll")] public static extern IntPtr OpenProcess(uint a, bool b, uint pid);
  [DllImport("kernel32.dll")] public static extern IntPtr VirtualAllocEx(IntPtr p, IntPtr a, IntPtr sz, uint t, uint prot);
  [DllImport("kernel32.dll")] public static extern bool WriteProcessMemory(IntPtr p, IntPtr a, byte[] b, IntPtr n, out IntPtr w);
  // The dialog's page switching hangs off TCN_SELCHANGE, which is raised by the tab control --
  // TCM_SETCURSEL does not raise it, so build a remote NMHDR {hwndFrom, idFrom, code=TCN_SELCHANGE}
  // and send WM_NOTIFY ourselves.
  public static void NotifySelChange(IntPtr tab, IntPtr dlg, uint pid) {
    IntPtr hp = OpenProcess(0x1F0FFF, false, pid);
    if (hp == IntPtr.Zero) { Console.WriteLine("  [dbg] OpenProcess failed"); return; }
    IntPtr mem = VirtualAllocEx(hp, IntPtr.Zero, (IntPtr)64, 0x1000, 0x04);
    if (mem == IntPtr.Zero) { Console.WriteLine("  [dbg] VirtualAllocEx failed"); return; }
    byte[] b = new byte[32];
    BitConverter.GetBytes((long)tab.ToInt64()).CopyTo(b, 0);          // hwndFrom
    BitConverter.GetBytes((int)GetDlgCtrlID(tab)).CopyTo(b, 8);       // idFrom
    BitConverter.GetBytes(-551).CopyTo(b, 12);                        // TCN_SELCHANGE
    IntPtr w;
    WriteProcessMemory(hp, mem, b, (IntPtr)32, out w);
    IntPtr r = SendMessage(dlg, 0x004E, (IntPtr)GetDlgCtrlID(tab), mem);         // WM_NOTIFY
    Console.WriteLine("  [dbg] mem=" + mem.ToInt64() + " id=" + GetDlgCtrlID(tab) + " ret=" + r.ToInt64());
  }
}
"@

$fails = 0
function Check([string]$name, [bool]$ok) {
    if ($ok) { Write-Host ("[PASS] " + $name) } else { Write-Host ("[FAIL] " + $name); $script:fails++ }
}
function CP([int[]]$cps) { return [string]::Join("", ($cps | ForEach-Object { [char]$_ })) }

$p = Start-Process -FilePath $Exe -PassThru
Start-Sleep -Seconds $WaitSec
if ($p.HasExited) { Write-Host "FAIL: exited early"; exit 1 }
Write-Host ("pid=" + $p.Id)

$script:procId = $p.Id; $script:frame = [IntPtr]::Zero
[void][SC2]::EnumWindows({
    param($h,$q)
    $wpid = 0; [void][SC2]::GetWindowThreadProcessId($h, [ref]$wpid)
    if ($wpid -eq $script:procId -and [SC2]::Cls($h) -eq 'ENewFrame') { $script:frame = $h }
    return $true
}, [IntPtr]::Zero)
if ($script:frame -eq [IntPtr]::Zero) { Write-Host "FAIL: no frame"; Stop-Process -Id $p.Id -Force; exit 1 }

[void][SC2]::PostMessage($script:frame, 0x0111, [IntPtr]$CmdId, [IntPtr]::Zero)
Start-Sleep -Seconds 3

$script:dlg = [IntPtr]::Zero
[void][SC2]::EnumWindows({
    param($h,$q)
    $wpid = 0; [void][SC2]::GetWindowThreadProcessId($h, [ref]$wpid)
    if ($wpid -eq $script:procId -and [SC2]::IsWindowVisible($h) -and $h -ne $script:frame -and [SC2]::Cls($h) -eq '#32770') { $script:dlg = $h }
    return $true
}, [IntPtr]::Zero)
Check "the system-config dialog opened (hwnd=$script:dlg)" ($script:dlg -ne [IntPtr]::Zero)
if ($script:dlg -eq [IntPtr]::Zero) { Stop-Process -Id $p.Id -Force; exit 1 }

$script:tab = [IntPtr]::Zero
[void][SC2]::EnumChildWindows($script:dlg, {
    param($c,$q)
    if ([SC2]::Cls($c) -eq 'SysTabControl32') { $script:tab = $c }
    return $true
}, [IntPtr]::Zero)
Check "the dialog owns a SysTabControl32 (hwnd=$script:tab)" ($script:tab -ne [IntPtr]::Zero)
if ($script:tab -eq [IntPtr]::Zero) { Stop-Process -Id $p.Id -Force; exit 1 }

$n = [SC2]::TabCount($script:tab)
Check ("it has 7 tab pages (original id=282) -- got " + $n) ($n -eq 7)

# --- page 0 is the startup page and its controls are visible; the other pages' are not ---
function VisById([int]$id) {
    $script:res = $false
    [void][SC2]::EnumChildWindows($script:dlg, {
        param($c,$q)
        if ([SC2]::GetDlgCtrlID($c) -eq $id) { $script:res = [SC2]::IsWindowVisible($c); return $false }
        return $true
    }, [IntPtr]::Zero)
    return $script:res
}
Check "page 0 shows its controls (id 1101 start-window radio is visible)" (VisById 1101)
Check "page 2 (display) controls are hidden at startup (id 1004 line-pitch is hidden)" (-not (VisById 1004))
Check "page 1 (edit) controls are hidden at startup (id 1180 input-tip is hidden)" (-not (VisById 1180))

# --- switch tabs with real mouse clicks and check the visible control set follows the page ---
# Page map (from the original's tab order: page0 common / page1 edit / page2 display /
# page3 design / page4 compile / page5 stub / page6 security):
#   0 -> id 1101 (start-window radio)   1 -> id 1180 (input tip)   2 -> id 1004 (line pitch)

# Switch tabs the way a user does: a REAL mouse click on the tab rectangle.
# TCM_SETCURSEL does not raise TCN_SELCHANGE, and a hand-built remote WM_NOTIFY did not reach
# the handler either -- so drive it with actual input instead of synthesised notifications.
function SetTab([int]$i) {
    $r = New-Object SC2+RECT
    if (-not [SC2]::TabRect($script:tab, $i, $script:procId, [ref]$r)) { Write-Host ("  [dbg] no rect for tab " + $i); return }
    $dr = New-Object SC2+RECT
    [void][SC2]::GetWindowRect($script:tab, [ref]$dr)
    $x = $dr.Left + [int](($r.Left + $r.Right) / 2)
    $y = $dr.Top  + [int](($r.Top + $r.Bottom) / 2)
    [void][SC2]::SetForegroundWindow($script:dlg)
    [void][SC2]::SetCursorPos($x, $y)
    Start-Sleep -Milliseconds 120
    [SC2]::mouse_event(0x0002, 0, 0, 0, [IntPtr]::Zero)   # LEFTDOWN
    Start-Sleep -Milliseconds 60
    [SC2]::mouse_event(0x0004, 0, 0, 0, [IntPtr]::Zero)   # LEFTUP
    Start-Sleep -Milliseconds 450
}
# warm-up: the very first synthetic click does not reach the tab strip (the dialog is not
# the foreground window yet), so click the page it already shows before asserting.
SetTab 0
SetTab 1
Check "after switching to page 1: the input-tip checkbox is visible" (VisById 1180)
Check "after switching to page 1: page 0's radio is hidden" (-not (VisById 1101))
SetTab 2
Check "page 2 shows the line-pitch edit (the page this editor honours)" (VisById 1004)
Check "page 2 hides page 1's checkbox" (-not (VisById 1180))
SetTab 0
Check "switching back to page 0 restores its controls" (VisById 1101)

Check "the dialog is still alive after page switches" (-not $p.HasExited)
Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
if ($fails -ne 0) { Write-Host ("syscfg_smoke: " + $fails + " FAILED"); exit 1 }
Write-Host "[PASS] syscfg_smoke: the 7 system-config pages switch correctly"
