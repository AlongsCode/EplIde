# Fold test, part 3: verify the collapse state itself (not the rendering) by dumping
# the model right after the click. ASCII only.
param(
    [string]$Exe = "D:\eclone\bin\EClone.exe",
    [string]$OutDir = "D:\eclone\shots",
    [int]$WaitSec = 7
)
$ErrorActionPreference = 'Continue'
Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public class EH {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public static string Cls(IntPtr h){ var sb=new StringBuilder(128); GetClassNameW(h,sb,128); return sb.ToString(); }
}
"@
$model = Join-Path $OutDir "model_fold3.txt"
Remove-Item $model -Force -ErrorAction SilentlyContinue
$env:ECLONE_DUMP_MODEL = $model

$p = Start-Process -FilePath $Exe -PassThru
Start-Sleep -Seconds $WaitSec
if ($p.HasExited) { Write-Host "FAIL: exited early"; exit 1 }
$script:pid_ = $p.Id; $script:frame = [IntPtr]::Zero; $script:ed = [IntPtr]::Zero
[void][EH]::EnumWindows({
    param($h,$q)
    $wpid = 0; [void][EH]::GetWindowThreadProcessId($h, [ref]$wpid)
    if ($wpid -eq $script:pid_ -and [EH]::IsWindowVisible($h) -and [EH]::Cls($h) -eq 'ENewFrame') { $script:frame = $h }
    return $true
}, [IntPtr]::Zero)
[void][EH]::EnumChildWindows($script:frame, {
    param($h,$q)
    if ([EH]::Cls($h).StartsWith('Afx:400000:8:10005')) { $script:ed = $h; return $false }
    return $true
}, [IntPtr]::Zero)
if ($script:ed -eq [IntPtr]::Zero) { Write-Host "FAIL: no editor"; Stop-Process -Id $p.Id -Force; exit 1 }

$before = (Get-Content $model | Where-Object { $_ -match '^row' }).Count
$er = New-Object EH+RECT; [void][EH]::GetWindowRect($script:ed, [ref]$er)

# Find the fold marker from the model instead of hardcoding a row: rows with
# kind=0 mark=2 are the 子程序 data rows that carry the -/+ (see ec_editor.cpp:
# ECM_FOLD = 2; the marker only exists for subroutines that have params/vars).
$foldLine = Get-Content $model | Where-Object { $_ -match '^row\s+\d+ kind=0 table=\s*\d+ y=\s*\d+ h=\s*\d+ indent=\d+ mark=2' } | Select-Object -First 1
if (-not $foldLine) { Write-Host "[FAIL] no fold marker in the model"; Stop-Process -Id $p.Id -Force; exit 1 }
if ($foldLine -match 'y=\s*(\d+) h=\s*(\d+)') {
    $ry = [int]$Matches[1]; $rh = [int]$Matches[2]
} else { Write-Host "[FAIL] cannot parse model row"; Stop-Process -Id $p.Id -Force; exit 1 }

# NOTE: lParam of WM_LBUTTONDOWN is CLIENT coordinates, not screen coordinates.
# The marker's hit box is CRect(17, cy-7, 37, cy+7) around the row's vertical centre.
$cx = 21
$cy = $ry + [int]($rh / 2)
$lp = [IntPtr](($cx -band 0xFFFF) -bor ($cy -shl 16))
Write-Host ("rows before = " + $before + "; fold row y=" + $ry + " h=" + $rh + "; client click at " + $cx + "," + $cy)
[void][EH]::PostMessage($script:ed, 0x0201, [IntPtr]1, $lp)
[void][EH]::PostMessage($script:ed, 0x0202, [IntPtr]0, $lp)
Start-Sleep -Seconds 1
[void][EH]::PostMessage($script:ed, 0x0400 + 102, [IntPtr]::Zero, [IntPtr]::Zero)
Start-Sleep -Milliseconds 800
$after = (Get-Content $model | Where-Object { $_ -match '^row' }).Count
Write-Host ("rows after  = " + $after)
if ($after -lt $before) { Write-Host "[PASS] collapse works (row count dropped)" } else { Write-Host "[FAIL] collapse did not happen" }
Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
Remove-Item Env:\ECLONE_DUMP_MODEL
