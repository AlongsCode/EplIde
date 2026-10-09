# Smoke-test the  dialog (edit menu #32877 / Ctrl+F), which is a 1:1 copy of the
# original's RT_DIALOG id=284 (254x76 dialog units, 5 controls, ids 1151/1041/1/2).
# Asserts: the dialog opens on WM_COMMAND 32877, its title/size match the original
# template, all four original control ids exist with the original labels, and closing it
# leaves the app alive. ASCII-only: Chinese strings are built from code points.
#
# usage: powershell -NoProfile -ExecutionPolicy Bypass -File tools\finddlg_smoke.ps1
param(
    [string]$Exe = "D:\eclone\bin\EClone.exe",
    [int]$WaitSec = 6
)
$ErrorActionPreference = 'Stop'
Add-Type @"
using System; using System.Text; using System.Runtime.InteropServices;
public class FDG {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h, int id);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left,Top,Right,Bottom; }
  public static string Cls(IntPtr h){ var sb=new StringBuilder(128); GetClassNameW(h,sb,128); return sb.ToString(); }
  public static string Txt(IntPtr h){ var sb=new StringBuilder(128); GetWindowTextW(h,sb,128); return sb.ToString(); }
}
"@
#  /  / (&F) / (&C)
$T_TITLE = [string]([char]0x5BFB + [char]0x627E + [char]0x5BF9 + [char]0x8BDD + [char]0x6846)
$T_CASE  = [string]([char]0x533A + [char]0x5206 + [char]0x5927 + [char]0x5C0F + [char]0x5199)
$T_NEXT  = [string]([char]0x641C + [char]0x5BFB + [char]0x4E0B + [char]0x4E00 + [char]0x4E2A + '(&F)')
$T_CANC  = [string]([char]0x53D6 + [char]0x6D88 + '(&C)')

Start-Process -FilePath taskkill -ArgumentList '/f','/im','EClone.exe' -Wait -WindowStyle Hidden
Start-Sleep -Milliseconds 400
$p = Start-Process -FilePath $Exe -PassThru
Start-Sleep -Seconds $WaitSec
if ($p.HasExited) { Write-Host "FAIL: exited early"; exit 1 }
$script:procId = $p.Id; $script:frame = [IntPtr]::Zero
[void][FDG]::EnumWindows({ param($h,$q) $w=0; [void][FDG]::GetWindowThreadProcessId($h,[ref]$w)
    if ($w -eq $script:procId -and [FDG]::Cls($h) -eq 'ENewFrame') { $script:frame = $h }; return $true }, [IntPtr]::Zero)
if ($script:frame -eq [IntPtr]::Zero) { Write-Host "FAIL: no frame"; Stop-Process -Id $p.Id -Force; exit 1 }

$fails = 0
function Check([string]$name, [bool]$ok, [string]$extra) {
    if ($ok) { Write-Host ("[PASS] " + $name + "  " + $extra) }
    else { Write-Host ("[FAIL] " + $name + "  " + $extra); $script:fails++ }
}

[void][FDG]::PostMessage($script:frame, 0x0111, [IntPtr]32877, [IntPtr]::Zero)   # WM_COMMAND 
Start-Sleep -Seconds 2
$script:dlg = [IntPtr]::Zero
[void][FDG]::EnumWindows({ param($h,$q) $w=0; [void][FDG]::GetWindowThreadProcessId($h,[ref]$w)
    if ($w -eq $script:procId -and [FDG]::Cls($h) -eq '#32770') { $script:dlg = $h; return $false }; return $true }, [IntPtr]::Zero)
Check "the  dialog opened" ($script:dlg -ne [IntPtr]::Zero) ("hwnd=" + $script:dlg)
if ($script:dlg -eq [IntPtr]::Zero) {
    Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue; Write-Host "[FAIL] finddlg_smoke"; exit 1
}
$r = New-Object FDG+RECT; [void][FDG]::GetWindowRect($script:dlg, [ref]$r)
$w = $r.Right - $r.Left; $h = $r.Bottom - $r.Top
Check "title is the original's caption" ([FDG]::Txt($script:dlg) -eq $T_TITLE) ("|" + [FDG]::Txt($script:dlg) + "|")
# 254x76 dialog units at the 9pt font this project uses -> 387x143 px (libcfg: 322x290 -> 489x464)
Check "size matches the id=284 template" ((($w -ge 380) -and ($w -le 392)) -and (($h -ge 136) -and ($h -le 150))) ($w.ToString() + "x" + $h)

$want = @{ 1151 = "ComboBox"; 1041 = "Button"; 1 = "Button"; 2 = "Button" }
foreach ($id in 1151, 1041, 1, 2) {
    $c = [FDG]::GetDlgItem($script:dlg, $id)
    if ($c -eq [IntPtr]::Zero) { Check ("control id=" + $id + " exists") $false "" }
    else {
        $cr = New-Object FDG+RECT; [void][FDG]::GetWindowRect($c, [ref]$cr)
        Check ("control id=" + $id) ($cr.Right -gt $cr.Left) ([FDG]::Cls($c) + " " + ($cr.Right-$cr.Left) + "x" + ($cr.Bottom-$cr.Top) + " |" + [FDG]::Txt($c) + "|")
    }
}
Check "the case checkbox text is the original's" ([FDG]::Txt([FDG]::GetDlgItem($script:dlg,1041)) -eq $T_CASE) ""
Check "the default button is "        ([FDG]::Txt([FDG]::GetDlgItem($script:dlg,1))    -eq $T_NEXT) ""
Check "the cancel button is "              ([FDG]::Txt([FDG]::GetDlgItem($script:dlg,2))    -eq $T_CANC) ""

[void][FDG]::PostMessage($script:dlg, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)   # WM_CLOSE
Start-Sleep -Milliseconds 700
Check "closing the dialog leaves the app alive" ((-not $p.HasExited) -and [FDG]::IsWindow($script:frame)) ""


# ---- the  dialog (original RT_DIALOG id=578) ----
$R_TITLE = [string]([char]0x5BFB + [char]0x627E + [char]0x66FF + [char]0x6362 + [char]0x5BF9 + [char]0x8BDD + [char]0x6846)
$R_L1    = [string]([char]0x6B32 + [char]0x641C + [char]0x5BFB + [char]0x6587 + [char]0x672C + [char]0xFF1A)
$R_L2    = [string]([char]0x66FF + [char]0x6362 + [char]0x6587 + [char]0x672C + [char]0xFF1A)
$R_FIND  = [string]([char]0x641C + [char]0x5BFB + [char]0x4E0B + [char]0x4E00 + [char]0x4E2A + '(&F)')
$R_DO    = [string]([char]0x66FF + [char]0x6362 + '(&R)')
$R_ALL   = [string]([char]0x5168 + [char]0x90E8 + [char]0x66FF + [char]0x6362 + '(&A)')
$R_CLOSE = [string]([char]0x5173 + [char]0x95ED + '(&C)')

[void][FDG]::PostMessage($script:frame, 0x0111, [IntPtr]3, [IntPtr]::Zero)   # WM_COMMAND 
Start-Sleep -Seconds 2
$script:rdlg = [IntPtr]::Zero
[void][FDG]::EnumWindows({ param($h,$q) $w=0; [void][FDG]::GetWindowThreadProcessId($h,[ref]$w)
    if ($w -eq $script:procId -and [FDG]::Cls($h) -eq '#32770') { $script:rdlg = $h; return $false }; return $true }, [IntPtr]::Zero)
Check "the  dialog opened" ($script:rdlg -ne [IntPtr]::Zero) ("hwnd=" + $script:rdlg)
if ($script:rdlg -ne [IntPtr]::Zero) {
    $rr = New-Object FDG+RECT; [void][FDG]::GetWindowRect($script:rdlg, [ref]$rr)
    $rw = $rr.Right - $rr.Left; $rh = $rr.Bottom - $rr.Top
    Check "its title is the original's" ([FDG]::Txt($script:rdlg) -eq $R_TITLE) ("|" + [FDG]::Txt($script:rdlg) + "|")
    # 270x82 dialog units at 9pt -> about 411x154 px
    Check "size matches the id=578 template" ((($rw -ge 398) -and ($rw -le 428)) -and (($rh -ge 144) -and ($rh -le 168))) ($rw.ToString() + "x" + $rh)
    foreach ($id in 1181, 1152, 1047, 1041, 1, 3, 4, 2) {
        Check ("control id=" + $id + " exists") ([FDG]::GetDlgItem($script:rdlg, $id) -ne [IntPtr]::Zero) ""
    }
    Check "the first label is " ([FDG]::Txt([FDG]::GetDlgItem($script:rdlg,2262)) -eq $R_L1) ("|" + [FDG]::Txt([FDG]::GetDlgItem($script:rdlg,2262)) + "|")
    Check "the  button is the original label" ([FDG]::Txt([FDG]::GetDlgItem($script:rdlg,3)) -eq $R_DO) ("|" + [FDG]::Txt([FDG]::GetDlgItem($script:rdlg,3)) + "|")
    Check "the  button is the original label" ([FDG]::Txt([FDG]::GetDlgItem($script:rdlg,4)) -eq $R_ALL) ("|" + [FDG]::Txt([FDG]::GetDlgItem($script:rdlg,4)) + "|")
    Check "the  button is the original label" ([FDG]::Txt([FDG]::GetDlgItem($script:rdlg,2)) -eq $R_CLOSE) ("|" + [FDG]::Txt([FDG]::GetDlgItem($script:rdlg,2)) + "|")
    Check "the default button is " ([FDG]::Txt([FDG]::GetDlgItem($script:rdlg,1)) -eq $R_FIND) ("|" + [FDG]::Txt([FDG]::GetDlgItem($script:rdlg,1)) + "|")
    [void][FDG]::PostMessage($script:rdlg, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)   # WM_CLOSE
    Start-Sleep -Milliseconds 500
}

Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
if ($fails -ne 0) { Write-Host ("finddlg_smoke: " + $fails + " FAILED"); exit 1 }
Write-Host "[PASS] finddlg_smoke: the  dialog matches the original template"
