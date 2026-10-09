# Focus the EClone editor pane and then report the caret rectangle.
#
# Locates the editor child window (class Afx:400000:..., the one with the most children
# of none / the largest after the MDI client), posts a left-button click into its client
# area so CECEditor::OnLButtonDown -> SetFocus() fires, then reads the caret the OS holds
# for that thread via GetGUIThreadInfo.
#
# MUST STAY PURE ASCII (GBK .ps1 hazard, see project HANDOFF).
#
# usage: powershell -NoProfile -ExecutionPolicy Bypass -File tools\caret_focus_check.ps1
$ErrorActionPreference = 'Stop'

Add-Type @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
public class W {
    public delegate bool EnumProc(IntPtr h, IntPtr l);
    [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr l);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
    [DllImport("user32.dll", CharSet=CharSet.Ansi)] public static extern int GetClassNameA(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool PostMessageA(IntPtr h, uint m, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern IntPtr SetFocus(IntPtr h);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll")] public static extern bool GetGUIThreadInfo(uint tid, ref GUITHREADINFO g);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int left, top, right, bottom; }
    [StructLayout(LayoutKind.Sequential)] public struct GUITHREADINFO {
        public int cbSize; public int flags;
        public IntPtr hwndActive, hwndFocus, hwndCapture, hwndMenuOwner, hwndMoveSize, hwndCaret;
        public RECT rcCaret;
    }
    public static string Cls(IntPtr h) { StringBuilder sb = new StringBuilder(128); GetClassNameA(h, sb, 128); return sb.ToString(); }
    public static List<IntPtr> All() {
        List<IntPtr> o = new List<IntPtr>();
        EnumWindows(delegate(IntPtr h, IntPtr l) { o.Add(h); return true; }, IntPtr.Zero);
        return o;
    }
    public static List<IntPtr> Kids(IntPtr p) {
        List<IntPtr> o = new List<IntPtr>();
        EnumChildWindows(p, delegate(IntPtr h, IntPtr l) { o.Add(h); return true; }, IntPtr.Zero);
        return o;
    }
    public static GUITHREADINFO Caret(uint tid) {
        GUITHREADINFO g = new GUITHREADINFO(); g.cbSize = Marshal.SizeOf(g);
        if (!GetGUIThreadInfo(tid, ref g)) throw new System.ComponentModel.Win32Exception();
        return g;
    }
}
'@

$proc = Get-Process EClone -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $proc) { Write-Output "EClone is not running"; exit 2 }

# top-level window owned by EClone
$main = [IntPtr]::Zero
foreach ($h in [W]::All()) {
    $pid2 = 0; [void][W]::GetWindowThreadProcessId($h, [ref]$pid2)
    if ($pid2 -eq $proc.Id -and [W]::IsWindowVisible($h)) { $main = $h; break }
}
if ($main -eq [IntPtr]::Zero) { Write-Output "no visible top-level window"; exit 2 }
Write-Output ("main window: 0x{0:X} class={1}" -f [int64]$main, [W]::Cls($main))
[void][W]::SetForegroundWindow($main)
Start-Sleep -Milliseconds 600

# candidate editor panes: visible descendants whose class starts with Afx:400000
$cands = @()
foreach ($h in [W]::Kids($main)) {
    $c = [W]::Cls($h)
    if (-not [W]::IsWindowVisible($h)) { continue }
    if ($c -notlike 'Afx:400000*') { continue }
    $r = New-Object W+RECT; [void][W]::GetClientRect($h, [ref]$r)
    $w = $r.right - $r.left; $ht = $r.bottom - $r.top
    $cands += [pscustomobject]@{ h = $h; cls = $c; w = $w; hh = $ht; kids = ([W]::Kids($h)).Count }
}
$cands | Sort-Object -Property w -Descending | Select-Object -First 12 |
    ForEach-Object { Write-Output ("  cand 0x{0:X} {1} {2}x{3} kids={4}" -f [int64]$_.h, $_.cls, $_.w, $_.hh, $_.kids) }

# the editor view is the Afx:400000:8:10005:6:0 pane (see src/ec_editor.h header comment:
# "the window class is still Afx:400000:8:10005:6:0"). Prefer an exact class match.
$ed = ($cands | Where-Object { $_.cls -eq 'Afx:400000:8:10005:6:0' } | Sort-Object -Property w -Descending | Select-Object -First 1)
if (-not $ed) {
    $ed = ($cands | Where-Object { $_.cls -like 'Afx:400000:8:*' -and $_.kids -eq 0 } | Sort-Object -Property w -Descending | Select-Object -First 1)
}
if (-not $ed) { Write-Output "no editor pane candidate"; exit 2 }
Write-Output ("editor pane: 0x{0:X} class={1} {2}x{3}" -f [int64]$ed.h, $ed.cls, $ed.w, $ed.hh)

# post LButtonDown/Up in the middle of its client area -> SetFocus() -> CreateCaret
$lx = [intptr]([int]($ed.w / 2) -band 0xFFFF)
$ly = [intptr]([int]($ed.hh / 2) -band 0xFFFF)
$lp = [intptr](([int]$lx) -bor ([int]$ly -shl 16))
[void][W]::PostMessageA($ed.h, 0x0201, [intptr]1, $lp)   # WM_LBUTTONDOWN
[void][W]::PostMessageA($ed.h, 0x0202, [intptr]0, $lp)   # WM_LBUTTONUP
Start-Sleep -Milliseconds 900

$tid = 0; $pid3 = 0
$tid = [W]::GetWindowThreadProcessId($ed.h, [ref]$pid3)
$g = [W]::Caret($tid)
if ($g.hwndCaret -eq [IntPtr]::Zero) {
    Write-Output "still no caret on the editor thread (click may not have landed)"
    exit 1
}
$cw = $g.rcCaret.right - $g.rcCaret.left
$ch = $g.rcCaret.bottom - $g.rcCaret.top
Write-Output ("CARET: hwnd=0x{0:X} rc=({1},{2})-({3},{4})  =>  {5} x {6}" -f `
    [int64]$g.hwndCaret, $g.rcCaret.left, $g.rcCaret.top, $g.rcCaret.right, $g.rcCaret.bottom, $cw, $ch)
Write-Output "reference (orig e.exe): width 2, height = rowheight + 4, y positioned 2px higher"
