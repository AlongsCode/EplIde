# Smoke-test the "insert" menu commands (#32777 subroutine / #32782 program set / #32831 window /
# #32778 data type / #32779 global var / #32780 Dll command / #32828 constant / #32829 image /
# #32830 sound / #32786 parameter / #32787 local var / #32929 class).
#
# Oracle: the editor's source dump. DumpModel() now emits one `SRC <index> <text>` line per raw
# source line (at the END of the model file), and the editor re-dumps after every insert command.
# The display model alone is a weak oracle here: a ".parameter" / ".local var" row is hidden unless
# the current page is that sub-program page, so a correct insert can add ZERO model rows.
#
# ASCII only: all Chinese needles are built from code points.
#
# usage: powershell -NoProfile -ExecutionPolicy Bypass -File tools\insert_smoke.ps1
param(
    [string]$Exe = "D:\eclone\bin\EClone.exe",
    [string]$OutDir = "D:\eclone\shots",
    [int]$WaitSec = 7
)
$ErrorActionPreference = 'Stop'
Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public class IV {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  public static string Cls(IntPtr h){ var sb=new StringBuilder(128); GetClassNameW(h,sb,128); return sb.ToString(); }
}
"@
$model = Join-Path $OutDir "model_insert.txt"
Remove-Item $model -Force -ErrorAction SilentlyContinue
$env:ECLONE_DUMP_MODEL = $model
Start-Process -FilePath taskkill -ArgumentList '/f','/im','EClone.exe' -Wait -WindowStyle Hidden
Start-Sleep -Milliseconds 400
$p = Start-Process -FilePath $Exe -PassThru
Start-Sleep -Seconds $WaitSec
if ($p.HasExited) { Write-Host "FAIL: exited early"; exit 1 }
$script:procId = $p.Id; $script:frame = [IntPtr]::Zero; $script:child = [IntPtr]::Zero
[void][IV]::EnumWindows({
    param($h,$q)
    $wpid = 0; [void][IV]::GetWindowThreadProcessId($h, [ref]$wpid)
    if ($wpid -eq $script:procId -and [IV]::Cls($h) -eq 'ENewFrame') { $script:frame = $h }
    return $true
}, [IntPtr]::Zero)
[void][IV]::EnumChildWindows($script:frame, {
    param($h,$q)
    if ([IV]::Cls($h) -eq 'AfxMDIFrame42s') { $script:child = $h; return $false }
    return $true
}, [IntPtr]::Zero)
if ($script:child -eq [IntPtr]::Zero) { Write-Host "FAIL: no mdi child"; Stop-Process -Id $p.Id -Force; exit 1 }

$fails = 0
function Check([string]$name, [bool]$ok) {
    if ($ok) { Write-Host ("[PASS] " + $name) } else { Write-Host ("[FAIL] " + $name); $script:fails++ }
}
function Cmd([int]$id) {
    [void][IV]::SendMessage($script:child, 0x0111, [IntPtr]$id, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 500
}
function SrcLines {
    if (-not (Test-Path $model)) { return @() }
    return @(Get-Content $model -Encoding Default | Where-Object { $_ -like 'SRC *' })
}

$base = (SrcLines).Count
Check ("the source dump is readable (baseline " + $base + " lines)") ($base -gt 50)

# Each command id -> the declaration keyword its new source line must contain (code points).
$cases = @(
    @{ id = 32777; kw = @(0x5B50, 0x7A0B, 0x5E8F) },
    @{ id = 32782; kw = @(0x7A0B, 0x5E8F, 0x96C6) },
    @{ id = 32831; kw = @(0x7A97, 0x53E3) },
    @{ id = 32778; kw = @(0x6570, 0x636E, 0x7C7B, 0x578B) },
    @{ id = 32779; kw = @(0x5168, 0x5C40, 0x53D8, 0x91CF) },
    @{ id = 32780; kw = @(0x44, 0x4C, 0x4C, 0x547D, 0x4EE4) },
    @{ id = 32828; kw = @(0x5E38, 0x91CF) },
    @{ id = 32829; kw = @(0x56FE, 0x7247, 0x8D44, 0x6E90) },
    @{ id = 32830; kw = @(0x58F0, 0x97F3, 0x8D44, 0x6E90) },
    @{ id = 32786; kw = @(0x53C2, 0x6570) },
    @{ id = 32787; kw = @(0x5C40, 0x90E8, 0x53D8, 0x91CF) },
    @{ id = 32929; kw = @(0x7C7B) }
)

$n = $base
foreach ($c in $cases) {
    Cmd $c.id
    $lines = SrcLines
    $kw = [string]::Join("", ($c.kw | ForEach-Object { [char]$_ }))
    $ok = ($lines.Count -eq $n + 1)
    if ($ok) {
        $last = $lines[$lines.Count - 1]
        $ok = ($last -like ("*" + $kw + "*"))
        if (-not $ok) { Write-Host ("    last line: " + $last) }
    } else {
        Write-Host ("    line count " + $lines.Count + " (expected " + ($n + 1) + ")")
    }
    Check ("insert #" + $c.id + " added one source line holding " + $kw) $ok
    $n = $lines.Count
}

# --- the id that is deliberately not implemented must not touch the source ---
$n0 = (SrcLines).Count
Cmd 32783
Check "insert #32783 (current unit) is a no-op for now" ((SrcLines).Count -eq $n0)

Check "the app is still alive after 12 insert commands" (-not $p.HasExited)

Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
Remove-Item Env:\ECLONE_DUMP_MODEL
if ($fails -ne 0) { Write-Host ("insert_smoke: " + $fails + " FAILED"); exit 1 }
Write-Host "[PASS] insert_smoke: the insert menu adds the right declaration lines"
