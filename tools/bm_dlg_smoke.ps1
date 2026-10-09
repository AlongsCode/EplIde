# Smoke-test the  dialog (edit menu #32965 / Ctrl+F7), a 1:1 copy of the original's
# RT_DIALOG id=607 (408x264 dialog units, 5 controls: SysListView32 id=1036 + buttons 3/4/1/2).
# ASCII only (Chinese expectations are built from code points).
#
# usage: powershell -NoProfile -ExecutionPolicy Bypass -File tools\bm_dlg_smoke.ps1
param(
    [string]$Exe = "D:\eclone\bin\EClone.exe",
    [int]$WaitSec = 6
)
$ErrorActionPreference = 'Stop'
Add-Type @"
using System; using System.Text; using System.Runtime.InteropServices;
public class BMD {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h, int id);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left,Top,Right,Bottom; }
  public static string Cls(IntPtr h){ var sb=new StringBuilder(128); GetClassNameW(h,sb,128); return sb.ToString(); }
  public static string Txt(IntPtr h){ var sb=new StringBuilder(512); GetWindowTextW(h,sb,512); return sb.ToString(); }
}
"@
#  / (&R) / (&L) / (&G) / (&C)
$T_TITLE = [string]([char]0x4E66 + [char]0x7B7E + [char]0x8DF3 + [char]0x8F6C)
$T_DEL   = [string]([char]0x5220 + [char]0x9664 + '(&R)')
$T_DELALL= [string]([char]0x5168 + [char]0x90E8 + [char]0x5220 + [char]0x9664 + '(&L)')
$T_GOTO  = [string]([char]0x8DF3 + [char]0x8F6C + '(&G)')
$T_CLOSE = [string]([char]0x5173 + [char]0x95ED + '(&C)')

Start-Process -FilePath taskkill -ArgumentList '/f','/im','EClone.exe' -Wait -WindowStyle Hidden
Start-Sleep -Milliseconds 400
$p = Start-Process -FilePath $Exe -PassThru
Start-Sleep -Seconds $WaitSec
if ($p.HasExited) { Write-Host "FAIL: exited early"; exit 1 }
$script:procId = $p.Id; $script:frame = [IntPtr]::Zero; $script:child = [IntPtr]::Zero
[void][BMD]::EnumWindows({ param($h,$q) $w=0; [void][BMD]::GetWindowThreadProcessId($h,[ref]$w)
    if ($w -eq $script:procId -and [BMD]::Cls($h) -eq 'ENewFrame') { $script:frame = $h }; return $true }, [IntPtr]::Zero)
[void][BMD]::EnumChildWindows($script:frame, { param($h,$q)
    if ([BMD]::Cls($h) -eq 'AfxMDIFrame42s') { $script:child = $h; return $false }; return $true }, [IntPtr]::Zero)
if ($script:child -eq [IntPtr]::Zero) { Write-Host "FAIL: no MDI child"; Stop-Process -Id $p.Id -Force; exit 1 }

$fails = 0
function Check([string]$name, [bool]$ok, [string]$extra) {
    if ($ok) { Write-Host ("[PASS] " + $name + "  " + $extra) } else { Write-Host ("[FAIL] " + $name + "  " + $extra); $script:fails++ }
}
function Cmd([int]$id) { [void][BMD]::SendMessage($script:child, 0x0111, [IntPtr]$id, [IntPtr]::Zero); Start-Sleep -Milliseconds 450 }

# the editor starts with the caret on line 4 (model dump: caret=3,0) -> give it one bookmark
Cmd 32964
Start-Sleep -Milliseconds 300
[void][BMD]::PostMessage($script:child, 0x0111, [IntPtr]32965, [IntPtr]::Zero)                                   # 
Start-Sleep -Milliseconds 1200
$script:dlg = [IntPtr]::Zero
[void][BMD]::EnumWindows({ param($h,$q) $w=0; [void][BMD]::GetWindowThreadProcessId($h,[ref]$w)
    if ($w -eq $script:procId -and [BMD]::Cls($h) -eq '#32770') { $script:dlg = $h; return $false }; return $true }, [IntPtr]::Zero)
Check "the  dialog opened" ($script:dlg -ne [IntPtr]::Zero) ("hwnd=" + $script:dlg)
if ($script:dlg -eq [IntPtr]::Zero) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue; Write-Host "[FAIL] bm_dlg_smoke"; exit 1 }

$r = New-Object BMD+RECT; [void][BMD]::GetWindowRect($script:dlg, [ref]$r)
$w = $r.Right-$r.Left; $h = $r.Bottom-$r.Top
Check "its title is the original's" ([BMD]::Txt($script:dlg) -eq $T_TITLE) ("|" + [BMD]::Txt($script:dlg) + "|")
# 408x264 dialog units -> roughly 620x420..500 px
Check "size matches the id=607 template" ((($w -ge 600) -and ($w -le 640)) -and (($h -ge 400) -and ($h -le 520))) ($w.ToString() + "x" + $h)
foreach ($id in 1036, 3, 4, 1, 2) {
    Check ("control id=" + $id + " exists") ([BMD]::GetDlgItem($script:dlg, $id) -ne [IntPtr]::Zero) ""
}
Check "the  button label" ([BMD]::Txt([BMD]::GetDlgItem($script:dlg,3)) -eq $T_DEL) ("|" + [BMD]::Txt([BMD]::GetDlgItem($script:dlg,3)) + "|")
Check "the  button label" ([BMD]::Txt([BMD]::GetDlgItem($script:dlg,4)) -eq $T_DELALL) ("|" + [BMD]::Txt([BMD]::GetDlgItem($script:dlg,4)) + "|")
Check "the  default button label" ([BMD]::Txt([BMD]::GetDlgItem($script:dlg,1)) -eq $T_GOTO) ("|" + [BMD]::Txt([BMD]::GetDlgItem($script:dlg,1)) + "|")
Check "the  button label" ([BMD]::Txt([BMD]::GetDlgItem($script:dlg,2)) -eq $T_CLOSE) ("|" + [BMD]::Txt([BMD]::GetDlgItem($script:dlg,2)) + "|")

# the list must show the bookmark we just added (LVM_GETITEMCOUNT = 0x1004)
$list = [BMD]::GetDlgItem($script:dlg, 1036)
$n = [int][BMD]::SendMessage($list, 0x1004, [IntPtr]::Zero, [IntPtr]::Zero)
Check "the list shows the bookmark we just added" ($n -eq 1) ("items=" + $n)

#  (id=4) must empty the list
[void][BMD]::SendMessage($script:dlg, 0x0111, [IntPtr]4, [IntPtr]$list)
Start-Sleep -Milliseconds 400
$n2 = [int][BMD]::SendMessage($list, 0x1004, [IntPtr]::Zero, [IntPtr]::Zero)
Check " empties the list" ($n2 -eq 0) ("items=" + $n2)

[void][BMD]::PostMessage($script:dlg, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
Start-Sleep -Milliseconds 600
Check "closing it leaves the app alive" ((-not $p.HasExited) -and [BMD]::IsWindow($script:frame)) ""
Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
if ($fails -ne 0) { Write-Host ("bm_dlg_smoke: " + $fails + " FAILED"); exit 1 }
Write-Host "[PASS] bm_dlg_smoke: the bookmark dialog matches the original template"
