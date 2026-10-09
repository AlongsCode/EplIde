# Decisive typing test: steal the foreground properly, then press ONE real character key.
#
# keybd_event with a character key makes the OS generate WM_KEYDOWN + WM_CHAR + WM_KEYUP
# and deliver them to the *focused* window of the *foreground* thread -- that is exactly
# what a human keyboard does. If the model changes, typing works in real use.
#
# MUST STAY PURE ASCII.
#
# usage: powershell -NoProfile -ExecutionPolicy Bypass -File tools\typing_real.ps1
param(
    [string]$Exe = "",
    [string]$OutDir = "",
    [string]$Dump = "",
    [string]$Sample = ""
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
if ($Exe -eq "")    { $Exe = Join-Path $root "bin\EClone.exe" }
if ($OutDir -eq "") { $OutDir = Join-Path $root "shots" }
if ($Dump -eq "")   { $Dump = Join-Path $OutDir "model_typing_real.txt" }
if ($Sample -eq "") { $Sample = Join-Path $root "shots\e_samples\flow_ref.e" }

Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public class R {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint a, uint b, bool f);
  [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
  [DllImport("user32.dll")] public static extern IntPtr SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr SetFocus(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int c);
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, IntPtr extra);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, IntPtr e);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
  public static string Cls(IntPtr h){ var sb=new StringBuilder(256); GetClassNameW(h,sb,256); return sb.ToString(); }
  // real foreground: attach our input queue to the target thread, then SetForegroundWindow
  public static void StealForeground(IntPtr frame) {
    uint me = GetCurrentThreadId();
    uint pid = 0;
    uint them = GetWindowThreadProcessId(frame, out pid);
    AttachThreadInput(me, them, true);
    ShowWindow(frame, 9);            // SW_RESTORE
    BringWindowToTop(frame);
    SetForegroundWindow(frame);
    System.Threading.Thread.Sleep(250);
    AttachThreadInput(me, them, false);
  }
}
"@

$fail = 0
function Check([string]$n, [bool]$ok) { if ($ok) { Write-Host "[PASS] $n" } else { Write-Host "[FAIL] $n"; $script:fail = 1 } }
function SrcBlock() {
    if (-not (Test-Path $Dump)) { return "" }
    $t = [System.Text.Encoding]::GetEncoding(936).GetString([System.IO.File]::ReadAllBytes($Dump))
    $i = $t.IndexOf("`nSRC "); if ($i -lt 0) { return "" }
    return $t.Substring($i)
}
$script:ed = [IntPtr]::Zero
function Redump() { if ($script:ed -ne [IntPtr]::Zero) { [void][R]::PostMessage($script:ed, 0x466, [IntPtr]::Zero, [IntPtr]::Zero); Start-Sleep -Milliseconds 700 } }

$env:ECLONE_DUMP_MODEL = $Dump
$p = Start-Process -FilePath $Exe -WorkingDirectory (Split-Path $Exe) -ArgumentList "`"$Sample`"" -PassThru
Start-Sleep -Seconds 6

$script:frame = [IntPtr]::Zero
for ($t = 0; $t -lt 15 -and $script:frame -eq [IntPtr]::Zero; $t++) {
    Start-Sleep -Seconds 1
    [R]::EnumWindows([R+EnumProc]{ param($h,$q)
        $wpid = 0; [void][R]::GetWindowThreadProcessId($h, [ref]$wpid)
        if ($wpid -eq $p.Id -and [R]::Cls($h) -eq 'ENewFrame') { $script:frame = $h }
        return $true }, [IntPtr]::Zero) | Out-Null
}
[R]::EnumChildWindows($script:frame, [R+EnumProc]{ param($h,$q)
    if ([R]::Cls($h) -like 'Afx:400000:8:10005*') { $script:ed = $h }
    return $true }, [IntPtr]::Zero) | Out-Null
if ($script:ed -eq [IntPtr]::Zero) { Write-Host "[FAIL] no editor"; $p.Kill(); exit 1 }
Write-Host ("frame=0x{0:X} editor=0x{1:X}" -f [int64]$script:frame, [int64]$script:ed)

[R]::StealForeground($script:frame)
Write-Host ("foreground now = 0x{0:X}" -f [int64][R]::GetForegroundWindow())

# click into a code row with the real mouse so the editor takes focus + caret
$model = ""
if (Test-Path $Dump) { $model = [System.Text.Encoding]::GetEncoding(936).GetString([System.IO.File]::ReadAllBytes($Dump)) }
$cy = 150
foreach ($l in ($model -split "`n")) {
    $m = [regex]::Match($l, 'row\s+(\d+) kind=(\d+).*?y=\s*(\d+) h=\s*(\d+)')
    if ($m.Success -and [int]$m.Groups[2].Value -eq 1) { $cy = [int]$m.Groups[3].Value + [int]([int]$m.Groups[4].Value / 2); break }
}
$pt = New-Object R+POINT
$pt.X = 150; $pt.Y = $cy
[void][R]::ClientToScreen($script:ed, [ref]$pt)
Write-Host ("real click at screen ({0},{1})" -f $pt.X, $pt.Y)
[R]::SetCursorPos($pt.X, $pt.Y); Start-Sleep -Milliseconds 250
[R]::mouse_event(2,0,0,0,[IntPtr]::Zero); Start-Sleep -Milliseconds 90
[R]::mouse_event(4,0,0,0,[IntPtr]::Zero); Start-Sleep -Milliseconds 450
Redump
$before = SrcBlock
Write-Host ("after real click: srcbytes=" + $before.Length)

# ONE real character key: 'A' -> the OS makes WM_KEYDOWN/WM_CHAR/WM_KEYUP itself
[R]::keybd_event(0x41, 0, 0, [IntPtr]::Zero); Start-Sleep -Milliseconds 90
[R]::keybd_event(0x41, 0, 2, [IntPtr]::Zero); Start-Sleep -Milliseconds 600
Redump
$after = SrcBlock
Check "real keybd_event('A') inserts a character" ($after -ne $before)
if ($after -ne $before) {
    $bl = $before -split "`n"; $al = $after -split "`n"
    for ($i = 0; $i -lt [Math]::Max($bl.Count,$al.Count); $i++) {
        $x = if ($i -lt $bl.Count) { $bl[$i] } else { "<none>" }
        $y = if ($i -lt $al.Count) { $al[$i] } else { "<none>" }
        if ($x -ne $y) { Write-Host ("   line " + $i); Write-Host ("     - [" + $x + "]"); Write-Host ("     + [" + $y + "]"); break }
    }
}

Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
if ($fail -ne 0) { Write-Host "[FAIL] real typing"; exit 1 }
Write-Host "[PASS] real typing works"
