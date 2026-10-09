# Check EClone's real caret geometry via Win32 GetGUIThreadInfo.
# Reads the caret rectangle the OS actually holds for the thread that owns ECLone's
# editor window -- authoritative, no screenshot flicker guessing.
#
# NOTE: this file must stay pure ASCII. The project's own HANDOFF warns that a
# .ps1 with non-ASCII bytes gets read as GBK and can silently eat the next line.
#
# usage: powershell -NoProfile -ExecutionPolicy Bypass -File tools\caret_check.ps1
$ErrorActionPreference = 'Stop'

Add-Type @'
using System;
using System.Runtime.InteropServices;
public class CaretProbe {
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int left, top, right, bottom; }
    [StructLayout(LayoutKind.Sequential)]
    public struct GUITHREADINFO {
        public int cbSize; public int flags;
        public IntPtr hwndActive, hwndFocus, hwndCapture, hwndMenuOwner, hwndMoveSize, hwndCaret;
        public RECT rcCaret;
    }
    [DllImport("user32.dll")] public static extern bool GetGUIThreadInfo(uint idThread, ref GUITHREADINFO lpgui);
    public static GUITHREADINFO Get(uint tid) {
        GUITHREADINFO g = new GUITHREADINFO();
        g.cbSize = Marshal.SizeOf(g);
        if (!GetGUIThreadInfo(tid, ref g)) throw new System.ComponentModel.Win32Exception();
        return g;
    }
}
'@

$p = Get-Process EClone -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $p) { Write-Output "EClone is not running"; exit 2 }
Write-Output ("pid=" + $p.Id)

$found = $false
foreach ($t in $p.Threads) {
    try { $g = [CaretProbe]::Get($t.Id) } catch { continue }
    if ($g.hwndCaret -ne [IntPtr]::Zero) {
        $w = $g.rcCaret.right - $g.rcCaret.left
        $h = $g.rcCaret.bottom - $g.rcCaret.top
        Write-Output ("thread {0}: hwndCaret=0x{1:X} rc=({2},{3})-({4},{5})  =>  {6} x {7}" -f `
            $t.Id, [int64]$g.hwndCaret, $g.rcCaret.left, $g.rcCaret.top, $g.rcCaret.right, $g.rcCaret.bottom, $w, $h)
        $found = $true
    }
}
if (-not $found) {
    Write-Output "no thread holds a caret (window unfocused, or caret in hidden blink phase)"
    exit 1
}
Write-Output "reference: orig sub_4B4EA0 creates 2 x (rowheight+4); sub_4B55C0 positions it 2px higher"
