# Does typing work? Uses the SAME input path as tools\sel_smoke.ps1 (proven in this project):
#   * PostMessage WM_MOUSEMOVE/LBUTTONDOWN/UP into the editor  -> click + SetFocus + caret
#   * keybd_event for the character keys, and for Ctrl held down
#   * assert on the ECLONE_DUMP_MODEL dump (raw bytes, so a char at end-of-line is visible)
#
# MUST STAY PURE ASCII (GBK .ps1 hazard).
#
# usage: powershell -NoProfile -ExecutionPolicy Bypass -File tools\typing_check.ps1
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
if ($Dump -eq "")   { $Dump = Join-Path $OutDir "model_typing.txt" }
if ($Sample -eq "") { $Sample = Join-Path $root "shots\e_samples\flow_ref.e" }

Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public class K {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, IntPtr extra);
  public static string Cls(IntPtr h){ var sb=new StringBuilder(256); GetClassNameW(h,sb,256); return sb.ToString(); }
  public static void Key(byte vk){ keybd_event(vk,0,0,IntPtr.Zero); System.Threading.Thread.Sleep(60); keybd_event(vk,0,2,IntPtr.Zero); }
}
"@

$fail = 0
function Check([string]$name, [bool]$ok) {
    if ($ok) { Write-Host ("[PASS] " + $name) }
    else { Write-Host ("[FAIL] " + $name); $script:fail = 1 }
}
function MkLp([int]$x, [int]$y) { return [IntPtr](($x -band 0xFFFF) -bor ($y -shl 16)) }
function SrcBlock() {
    if (-not (Test-Path $Dump)) { return "" }
    $b = [System.IO.File]::ReadAllBytes($Dump)
    $t = [System.Text.Encoding]::GetEncoding(936).GetString($b)
    $i = $t.IndexOf("`nSRC ")
    if ($i -lt 0) { return "" }
    return $t.Substring($i)
}
function Redump() {
    if ($ed -eq [IntPtr]::Zero) { return }
    [void][K]::PostMessage($ed, 0x466, [IntPtr]::Zero, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 700
}
function CaretOf() {
    $fold = $Dump -replace '\.txt$', '.fold.txt'
    if (-not (Test-Path $fold)) { return @(-1, -1, -1) }
    $t = [System.Text.Encoding]::GetEncoding(936).GetString([System.IO.File]::ReadAllBytes($fold))
    $m = [regex]::Match($t, 'anchor=(-?\d+),(-?\d+) caret=(-?\d+),(-?\d+)')
    if (-not $m.Success) { return @(-1, -1, -1) }
    return @([int]$m.Groups[3].Value, [int]$m.Groups[4].Value, [int]$m.Groups[1].Value)
}

$env:ECLONE_DUMP_MODEL = $Dump
$p = Start-Process -FilePath $Exe -WorkingDirectory (Split-Path $Exe) -ArgumentList "`"$Sample`"" -PassThru
Start-Sleep -Seconds 6

$frame = [IntPtr]::Zero
for ($try = 0; $try -lt 15 -and $frame -eq [IntPtr]::Zero; $try++) {
    Start-Sleep -Seconds 1
    [K]::EnumWindows([K+EnumProc]{
        param($h,$q)
        $wpid = 0; [void][K]::GetWindowThreadProcessId($h, [ref]$wpid)
        if ($wpid -eq $p.Id -and [K]::Cls($h) -eq 'ENewFrame') { $script:frame = $h }
        return $true
    }, [IntPtr]::Zero) | Out-Null
}
$ed = [IntPtr]::Zero
[K]::EnumChildWindows($frame, [K+EnumProc]{
    param($h,$q)
    if ([K]::Cls($h) -like 'Afx:400000:8:10005*') { $script:ed = $h }
    return $true
}, [IntPtr]::Zero) | Out-Null
if ($ed -eq [IntPtr]::Zero) { Write-Host "[FAIL] no editor window"; $p.Kill(); exit 1 }
Write-Host ("editor = 0x{0:X}" -f [int64]$ed)

# find a code row (kind=1) from the model
$model = ""
if (Test-Path $Dump) { $model = [System.Text.Encoding]::GetEncoding(936).GetString([System.IO.File]::ReadAllBytes($Dump)) }
$cy = 150
foreach ($l in ($model -split "`n")) {
    $r = [regex]::Match($l, 'row\s+(\d+) kind=(\d+).*?y=\s*(\d+) h=\s*(\d+)')
    if ($r.Success -and [int]$r.Groups[2].Value -eq 1) { $cy = [int]$r.Groups[3].Value + [int]([int]$r.Groups[4].Value / 2); break }
}
Write-Host ("clicking at client (150,$cy)")

$before = SrcBlock
$c0 = CaretOf
Write-Host ("before: caret=" + ($c0 -join ",") + " srcbytes=" + $before.Length)

# click (PostMessage path from sel_smoke)
[void][K]::PostMessage($ed, 0x0200, [IntPtr]0, (MkLp 150 $cy))
[void][K]::PostMessage($ed, 0x0201, [IntPtr]1, (MkLp 150 $cy))
Start-Sleep -Milliseconds 150
[void][K]::PostMessage($ed, 0x0202, [IntPtr]0, (MkLp 150 $cy))
Start-Sleep -Milliseconds 500
Redump
$c1 = CaretOf
$afterClick = SrcBlock
Write-Host ("after click: caret=" + ($c1 -join ","))
Check "click moves the caret" (($c1[0] -ne $c0[0]) -or ($c1[1] -ne $c0[1]))

# --- real keyboard: keybd_event 'j' ---
$frm = $frame
[void][K]::PostMessage($frm, 0x0006, [IntPtr]1, [IntPtr]0)   # WM_ACTIVATE active (nudge)
Start-Sleep -Milliseconds 300
[K]::Key(0x4A)   # 'j'
Start-Sleep -Milliseconds 500
Redump
$afterKey = SrcBlock
$changed = ($afterKey -ne $before)
Check "real keybd_event inserts a character" $changed

# --- synthetic WM_CHAR 'k' (control: proves the handler itself works) ---
$mid = SrcBlock
[void][K]::PostMessage($ed, 0x0102, [IntPtr]0x6B, [IntPtr]1)   # WM_CHAR 'k'
Start-Sleep -Milliseconds 500
Redump
$afterChar = SrcBlock
Check "PostMessage WM_CHAR inserts a character" ($afterChar -ne $mid)

# --- report what landed, showing the first changed line ---
if ($changed) {
    $bl = $before -split "`n"; $al = $afterKey -split "`n"
    for ($i = 0; $i -lt [Math]::Max($bl.Count, $al.Count); $i++) {
        $x = if ($i -lt $bl.Count) { $bl[$i] } else { "<none>" }
        $y = if ($i -lt $al.Count) { $al[$i] } else { "<none>" }
        if ($x -ne $y) { Write-Host ("   line " + $i + ":"); Write-Host ("     - [" + $x + "]"); Write-Host ("     + [" + $y + "]"); break }
    }
}
if ($afterChar -ne $mid) {
    $bl = $mid -split "`n"; $al = $afterChar -split "`n"
    for ($i = 0; $i -lt [Math]::Max($bl.Count, $al.Count); $i++) {
        $x = if ($i -lt $bl.Count) { $bl[$i] } else { "<none>" }
        $y = if ($i -lt $al.Count) { $al[$i] } else { "<none>" }
        if ($x -ne $y) { Write-Host ("   WM_CHAR line " + $i + ":"); Write-Host ("     - [" + $x + "]"); Write-Host ("     + [" + $y + "]"); break }
    }
}

Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
if ($fail -ne 0) { Write-Host "[FAIL] typing"; exit 1 }
Write-Host "[PASS] typing works"
