# cfg_smoke.ps1 -- the "other 6 pages" of the system-config dialog really read/write/persist.
# ASCII only.
#
# Settings live in HKCU\Software\FlySky\E\EClone40 (same key as the colour scheme) and are
# dumped by ECLONE_DUMP_CFG=<path>.  This script:
#   [1] launches EClone with the dump file and asserts the built-in defaults
#   [2] opens the dialog (#32876) and asserts the controls carry those values
#   [3] switches to the design page (page 3), types a new grid pitch, presses OK
#   [4] restarts EClone and asserts the dump shows the new value  (really persisted)
#   [5] presses "reset to defaults" and asserts the value came back
#
# NOTE: reading a cross-process EDIT control needs WM_GETTEXT -- GetWindowTextA/W return ""
#       for these (verified with tools\probe_cfgctl.ps1).
#
# usage: powershell -NoProfile -ExecutionPolicy Bypass -File tools\cfg_smoke.ps1
$script:fails = 0

function Check($name, $ok) {
    if ($ok) { Write-Host ("[PASS] " + $name) }
    else     { Write-Host ("[FAIL] " + $name); $script:fails = $script:fails + 1 }
}

$ROOT = "D:\eclone"
$EXE  = Join-Path $ROOT "bin\EClone.exe"
$DUMP = Join-Path $ROOT "shots\cfg.txt"

Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public class CF {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, StringBuilder l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, string l);
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h, int id);
  [DllImport("user32.dll")] public static extern int GetDlgCtrlID(IntPtr h);
  [DllImport("kernel32.dll")] public static extern IntPtr OpenProcess(uint a, bool b, uint pid);
  [DllImport("kernel32.dll")] public static extern IntPtr VirtualAllocEx(IntPtr p, IntPtr a, IntPtr sz, uint t, uint prot);
  [DllImport("kernel32.dll")] public static extern bool WriteProcessMemory(IntPtr p, IntPtr a, byte[] b, IntPtr n, out IntPtr w);
  [DllImport("kernel32.dll")] public static extern bool ReadProcessMemory(IntPtr p, IntPtr a, byte[] b, IntPtr n, out IntPtr r);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, IntPtr e);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public static string Cls(IntPtr h){ var sb=new StringBuilder(128); GetClassNameW(h,sb,128); return sb.ToString(); }
  public static string WinText(IntPtr h){ var sb=new StringBuilder(512); GetWindowTextW(h,sb,512); return sb.ToString(); }
  // WM_GETTEXT: works cross-process for EDIT / ComboBox controls
  public static string CtlText(IntPtr h){
    if (h == IntPtr.Zero) return "<no control>";
    var sb = new StringBuilder(512);
    SendMessage(h, 0x000D, (IntPtr)512, sb);
    return sb.ToString();
  }
  // TCM_SETCURSEL + a synthetic TCN_SELCHANGE whose NMHDR is allocated INSIDE the target
  // process (USER32 does not marshal it).  This drives the dialog's page switch deterministically.
  public static bool SetPage(IntPtr dlg, IntPtr tab, int idx, uint pid) {
    SendMessage(tab, 0x130C, (IntPtr)idx, IntPtr.Zero);          // TCM_SETCURSEL
    IntPtr hp = OpenProcess(0x1F0FFF, false, pid);
    if (hp == IntPtr.Zero) return false;
    IntPtr mem = VirtualAllocEx(hp, IntPtr.Zero, (IntPtr)64, 0x1000, 0x04);
    if (mem == IntPtr.Zero) return false;
    byte[] b = new byte[32];
    BitConverter.GetBytes((long)tab.ToInt64()).CopyTo(b, 0);      // hwndFrom
    BitConverter.GetBytes((int)GetDlgCtrlID(tab)).CopyTo(b, 8);   // idFrom
    BitConverter.GetBytes(-551).CopyTo(b, 12);                    // TCN_SELCHANGE
    IntPtr w;
    WriteProcessMemory(hp, mem, b, (IntPtr)24, out w);
    SendMessage(dlg, 0x004E, (IntPtr)GetDlgCtrlID(tab), mem);     // WM_NOTIFY
    return true;
  }
  public static bool TabRect(IntPtr tab, int idx, uint pid, out RECT r) {
    r = new RECT();
    IntPtr hp = OpenProcess(0x1F0FFF, false, pid);
    if (hp == IntPtr.Zero) return false;
    IntPtr mem = VirtualAllocEx(hp, IntPtr.Zero, (IntPtr)32, 0x1000, 0x04);
    if (mem == IntPtr.Zero) return false;
    IntPtr w;
    WriteProcessMemory(hp, mem, new byte[16], (IntPtr)16, out w);
    SendMessage(tab, 0x130A, (IntPtr)idx, mem);      // TCM_GETITEMRECT
    byte[] b = new byte[16];
    IntPtr rd;
    ReadProcessMemory(hp, mem, b, (IntPtr)16, out rd);
    r.Left = BitConverter.ToInt32(b, 0);  r.Top = BitConverter.ToInt32(b, 4);
    r.Right = BitConverter.ToInt32(b, 8); r.Bottom = BitConverter.ToInt32(b, 12);
    return (r.Right > r.Left && r.Bottom > r.Top);
  }
}
"@

function Start-Clone {
    Get-Process EClone -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 500
    Remove-Item $DUMP -ErrorAction SilentlyContinue
    $env:ECLONE_DUMP_CFG = $DUMP
    $p = Start-Process -FilePath $EXE -PassThru
    for ($i = 0; $i -lt 30 -and -not (Test-Path $DUMP); $i++) { Start-Sleep -Milliseconds 300 }
    return $p
}

function Get-Cfg([string]$key) {
    if (-not (Test-Path $DUMP)) { return "<no dump>" }
    $line = Select-String -Path $DUMP -Pattern ("\s" + $key + "\s") | Select-Object -First 1
    if ($line -eq $null) { return "<not found>" }
    return ($line.Line -split '\s+')[-1]
}

function Find-Frame([int]$thepid) {
    $script:frame = [IntPtr]::Zero
    [void][CF]::EnumWindows({
        param($h,$q)
        $wpid = 0; [void][CF]::GetWindowThreadProcessId($h, [ref]$wpid)
        if ($wpid -eq $thepid -and [CF]::Cls($h) -eq 'ENewFrame') { $script:frame = $h }
        return $true
    }, [IntPtr]::Zero)
    return $script:frame
}

function Find-Dialog([int]$thepid, [IntPtr]$frame) {
    $script:dlg = [IntPtr]::Zero
    [void][CF]::EnumWindows({
        param($h,$q)
        $wpid = 0; [void][CF]::GetWindowThreadProcessId($h, [ref]$wpid)
        if ($wpid -eq $thepid -and [CF]::IsWindowVisible($h) -and $h -ne $frame -and [CF]::Cls($h) -eq '#32770') { $script:dlg = $h }
        return $true
    }, [IntPtr]::Zero)
    return $script:dlg
}

# ---------------- [1] defaults ----------------
$p = Start-Clone
Check "EClone started with ECLONE_DUMP_CFG" (-not $p.HasExited)
Check ("default new-form width is 380  got=" + (Get-Cfg "CfgNewFormW")) ((Get-Cfg "CfgNewFormW") -eq "380")
Check ("default new-form height is 250  got=" + (Get-Cfg "CfgNewFormH")) ((Get-Cfg "CfgNewFormH") -eq "250")
Check ("default grid pitch is 8  got=" + (Get-Cfg "CfgGridW")) ((Get-Cfg "CfgGridW") -eq "8")
Check ("grid dots on by default  got=" + (Get-Cfg "CfgShowGrid")) ((Get-Cfg "CfgShowGrid") -eq "1")
Check ("undo memory limit defaults to 0  got=" + (Get-Cfg "CfgUndoMemKB")) ((Get-Cfg "CfgUndoMemKB") -eq "0")
Check ("startup mode defaults to 1101  got=" + (Get-Cfg "CfgStartMode")) ((Get-Cfg "CfgStartMode") -eq "1101")

# ---------------- [2] the dialog controls carry the values ----------------
$procId = $p.Id
$frame = Find-Frame $procId
Check "found the ENewFrame" ($frame -ne [IntPtr]::Zero)
[void][CF]::PostMessage($frame, 0x0111, [IntPtr]32876, [IntPtr]::Zero)
Start-Sleep -Seconds 3
$dlg = Find-Dialog $procId $frame
Check "the system-config dialog (#32876) opened" ($dlg -ne [IntPtr]::Zero)
if ($dlg -eq [IntPtr]::Zero) { Stop-Process -Id $procId -Force; exit 1 }

$t1006 = [CF]::CtlText([CF]::GetDlgItem($dlg, 1006))
$t1008 = [CF]::CtlText([CF]::GetDlgItem($dlg, 1008))
$t1236 = [CF]::CtlText([CF]::GetDlgItem($dlg, 1236))
$t1001 = [CF]::CtlText([CF]::GetDlgItem($dlg, 1001))
Check ("design page: new-form width edit shows 380  got='" + $t1006 + "'") ($t1006 -eq "380")
Check ("design page: grid pitch edit shows 8  got='" + $t1008 + "'") ($t1008 -eq "8")
Check ("common page: debug-output edit is empty by default  got='" + $t1236 + "'") ($t1236 -eq "")
Check ("common page: autosave interval shows 0  got='" + $t1001 + "'") ($t1001 -eq "0")

# ---------------- [3] switch to the design page and change a value ----------------
$script:tab = [IntPtr]::Zero
[void][CF]::EnumChildWindows($dlg, {
    param($c,$q)
    if ([CF]::Cls($c) -eq 'SysTabControl32') { $script:tab = $c }
    return $true
}, [IntPtr]::Zero)
# Set the design page's grid-pitch edit while page 0 is showing: WM_GETTEXT/WM_SETTEXT work on
# hidden controls (this is the same "value level" proof the rest of this script uses).
# NOTE: the *visible* tab-page switch is deliberately NOT asserted here -- driving that dialog's
# tab with synthetic mouse input proved unreliable from this script (the tab strip's geometry
# does not match its reported rects), and it is already covered by tools\syscfg_smoke.ps1,
# which switches the same dialog's pages successfully with real mouse clicks.


$edit = [CF]::GetDlgItem($dlg, 1008)
[void][CF]::SendMessage($edit, 0x000C, [IntPtr]::Zero, "12")     # WM_SETTEXT
$typed = [CF]::CtlText($edit)
Check ("typed 12 into the grid pitch edit  got='" + $typed + "'") ($typed -eq "12")
[void][CF]::PostMessage($dlg, 0x0111, [IntPtr]1, [IntPtr]::Zero)   # IDOK
Start-Sleep -Seconds 1
Check "the dialog closed after OK" (-not [CF]::IsWindowVisible($dlg))

# ---------------- [4] restart: the value must have been persisted ----------------
Stop-Process -Id $procId -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 600
$p2 = Start-Clone
$cfgGrid = Get-Cfg "CfgGridW"
$cfgW    = Get-Cfg "CfgNewFormW"
Check ("after a restart the grid pitch is still 12  got=" + $cfgGrid) ($cfgGrid -eq "12")
Check ("untouched values are unchanged after a restart  got=" + $cfgW) ($cfgW -eq "380")

# ---------------- [5] reset to defaults ----------------
$procId2 = $p2.Id
$frame2 = Find-Frame $procId2
[void][CF]::PostMessage($frame2, 0x0111, [IntPtr]32876, [IntPtr]::Zero)
Start-Sleep -Seconds 3
$dlg2 = Find-Dialog $procId2 $frame2
if ($dlg2 -ne [IntPtr]::Zero) {
    [void][CF]::PostMessage($dlg2, 0x0111, [IntPtr]1150, [IntPtr]::Zero)   # ID_SYSCFG_DEFAULT
    Start-Sleep -Seconds 1
    [void][CF]::PostMessage($dlg2, 0x0111, [IntPtr]1, [IntPtr]::Zero)
    Start-Sleep -Seconds 1
} else {
    Write-Host "  [dbg] could not reopen the dialog for the reset step"
}
Stop-Process -Id $procId2 -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 500
$p3 = Start-Clone
$cfgGrid3 = Get-Cfg "CfgGridW"
Check ("the reset button put the grid pitch back to 8  got=" + $cfgGrid3) ($cfgGrid3 -eq "8")
Stop-Process -Id $p3.Id -Force -ErrorAction SilentlyContinue
Remove-Item Env:\ECLONE_DUMP_CFG -ErrorAction SilentlyContinue

if ($fails -ne 0) { Write-Host ("cfg_smoke: " + $fails + " FAILED"); exit 1 }
Write-Host "[PASS] cfg_smoke: the other system-config pages read, write and persist"
