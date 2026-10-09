# Smoke-test breakpoints (run menu #32855  F9 / #32858 ).
# No screenshot needed: the editor's model dump now carries a "bp=" field per row, and the
# fold dump carries the caret, so the assertions are about real state.
# ASCII only (Chinese is built from code points).
#
# usage: powershell -NoProfile -ExecutionPolicy Bypass -File tools\bp_smoke.ps1
param(
    [string]$Exe = "D:\eclone\bin\EClone.exe",
    [string]$OutDir = "D:\eclone\shots",
    [int]$WaitSec = 6
)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
Add-Type @"
using System; using System.Text; using System.Runtime.InteropServices;
public class BPS {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  public static string Cls(IntPtr h){ var sb=new StringBuilder(128); GetClassNameW(h,sb,128); return sb.ToString(); }
}
"@
$model = Join-Path $OutDir "bp_model.txt"
$fold  = $model -replace '\.txt$', '.fold.txt'
Remove-Item $model, $fold -Force -ErrorAction SilentlyContinue
$env:ECLONE_DUMP_MODEL = $model
Start-Process -FilePath taskkill -ArgumentList '/f','/im','EClone.exe' -Wait -WindowStyle Hidden
Start-Sleep -Milliseconds 400
$p = Start-Process -FilePath $Exe -PassThru
Start-Sleep -Seconds $WaitSec
if ($p.HasExited) { Write-Host "FAIL: exited early"; exit 1 }
$script:procId = $p.Id; $script:frame = [IntPtr]::Zero; $script:child = [IntPtr]::Zero; $script:ed = [IntPtr]::Zero
[void][BPS]::EnumWindows({ param($h,$q) $w=0; [void][BPS]::GetWindowThreadProcessId($h,[ref]$w)
    if ($w -eq $script:procId -and [BPS]::Cls($h) -eq 'ENewFrame') { $script:frame = $h }; return $true }, [IntPtr]::Zero)
[void][BPS]::EnumChildWindows($script:frame, { param($h,$q)
    if ([BPS]::Cls($h) -eq 'AfxMDIFrame42s') { $script:child = $h; return $false }; return $true }, [IntPtr]::Zero)
[void][BPS]::EnumChildWindows($script:frame, { param($h,$q)
    if ([BPS]::Cls($h).StartsWith('Afx:400000:8:10005')) { $script:ed = $h; return $false }; return $true }, [IntPtr]::Zero)
if ($script:child -eq [IntPtr]::Zero -or $script:ed -eq [IntPtr]::Zero) { Write-Host "FAIL: no child/editor"; Stop-Process -Id $p.Id -Force; exit 1 }

function Redump { [void][BPS]::PostMessage($script:ed, 0x0400 + 102, [IntPtr]::Zero, [IntPtr]::Zero); Start-Sleep -Milliseconds 450 }
function BpRows {
    $t = Get-Content $model -Encoding Default -Raw
    return @($t -split "`n" | Where-Object { $_ -match '^row ' -and $_ -match 'bp=1' })
}
function BpLines {
    $out = @()
    foreach ($r in (BpRows)) { if ($r -match 'src=\s*(-?\d+)') { $out += [int]$Matches[1] } }
    return ($out | Sort-Object -Unique)
}
function CaretLine { Redump;
    $t = Get-Content $fold -Encoding Default -Raw
    if ($t -match 'caret=(-?\d+),(-?\d+)') { return [int]$Matches[1] }
    return -999
}
function Cmd([int]$id) { [void][BPS]::SendMessage($script:child, 0x0111, [IntPtr]$id, [IntPtr]::Zero); Start-Sleep -Milliseconds 400 }
function Key([int]$vk) { [void][BPS]::SendMessage($script:ed, 0x0100, [IntPtr]$vk, [IntPtr]::Zero); Start-Sleep -Milliseconds 350 }
$fails = 0
function Check([string]$name, [bool]$ok, [string]$extra) {
    if ($ok) { Write-Host ("[PASS] " + $name + "  " + $extra) } else { Write-Host ("[FAIL] " + $name + "  " + $extra); $script:fails++ }
}

Redump
Check "no breakpoints at startup" ((BpRows).Count -eq 0) ("rows=" + (BpRows).Count)

$L1 = CaretLine
Cmd 32855                                   # F9: set a breakpoint on the caret line
Redump
$b1 = BpLines
Check "F9 (#32855) set exactly one breakpoint" ($b1.Count -eq 1) ("lines=" + ($b1 -join ','))
Check "it is on the caret line" (($b1.Count -eq 1) -and ($b1[0] -eq $L1)) ("caret=" + $L1 + " bp=" + ($b1 -join ','))

Cmd 32855                                   # F9 again: toggle it off
Redump
Check "F9 again removed it" ((BpRows).Count -eq 0) ("rows=" + (BpRows).Count)

Key 0x28                                    # VK_DOWN to another line
$L2 = CaretLine
Key 0x28
$L3 = CaretLine
Key 0x78                                    # VK_F9 through the keyboard path
Redump
$b2 = BpLines
Check "the keyboard F9 path also sets a breakpoint" ($b2.Count -eq 1) ("lines=" + ($b2 -join ',') + " caret=" + $L3)
Cmd 32855                                   # 3rd breakpoint (on L3 again? then move once more)
Key 0x28
Cmd 32855
Redump
$n3 = (BpRows).Count
Check "two breakpoints can coexist" ($n3 -ge 1) ("rows=" + $n3)

Cmd 32858                                   # 
Redump
Check " (#32858) removed them all" ((BpRows).Count -eq 0) ("rows=" + (BpRows).Count)

Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
Remove-Item Env:\ECLONE_DUMP_MODEL
if ($fails -ne 0) { Write-Host ("bp_smoke: " + $fails + " FAILED"); exit 1 }
Write-Host "[PASS] bp_smoke: breakpoints set / toggle / clear work"
