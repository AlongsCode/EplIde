# Smoke-test the "support library -> hint" chain: selecting a node in the work pane's
# support-library tree must put the FULL hint text into the status pane's first page
# (the "hint" page, id = IDC_EDIT_OUTPUT + 0) as well as into the main frame status bar.
# ASCII-only on purpose (this machine reads .ps1 as ANSI): Chinese names/labels are built
# from code points.
param(
    [string]$Exe = "D:\eclone\bin\EClone.exe",
    [string]$OutDir = "D:\eclone\shots",
    [int]$WaitSec = 6
)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public class HK {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, StringBuilder l);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public static string Cls(IntPtr h){ var sb=new StringBuilder(128); GetClassNameW(h,sb,128); return sb.ToString(); }
  public static string Txt(IntPtr h){ var sb=new StringBuilder(512); GetWindowTextW(h,sb,512); return sb.ToString(); }
  public static void Collect(IntPtr top, string cls, List<IntPtr> outl){
    EnumChildWindows(top, delegate(IntPtr h, IntPtr p) {
      if (Cls(h) == cls) outl.Add(h);
      return true;
    }, IntPtr.Zero);
  }
  // cross-process text read: WM_GETTEXT is marshalled for us, GetWindowText is not
  public static string RemoteText(IntPtr h){
    int n = (int)SendMessage(h, 0x000E, IntPtr.Zero, IntPtr.Zero);       // WM_GETTEXTLENGTH
    if (n <= 0) return "";
    if (n > 60000) n = 60000;
    var sb = new StringBuilder(n + 2);
    SendMessage(h, 0x000D, (IntPtr)(n + 1), sb);                          // WM_GETTEXT
    return sb.ToString();
  }
}
"@

# ---- code-point built strings (keeps this file pure ASCII) ----
# node names in the support-library tree ("program tree" is not used here)
$N_CMD  = [string]([char]0x4FE1 + [char]0x606F + [char]0x6846)                    # info box (a command)
$N_LIB  = [string]([char]0x7CFB + [char]0x7EDF + [char]0x6838 + [char]0x5FC3 +
                   [char]0x652F + [char]0x6301 + [char]0x5E93)                    # system core support library
# a fragment that every command hint must contain: the "call format" lead-in
$HINT_LEAD = [string]([char]0x8C03 + [char]0x7528 + [char]0x683C + [char]0x5F0F)  # "call format"
# a fragment that every library hint must contain: "support library name and version"
$LIB_LEAD  = [string]([char]0x652F + [char]0x6301 + [char]0x5E93 + [char]0x540D + [char]0x79F0)

$fail = 0
function Check([string]$name, [bool]$ok) {
    if ($ok) { Write-Host ("[PASS] " + $name) }
    else { Write-Host ("[FAIL] " + $name); $script:fail = 1 }
}

Start-Process -FilePath taskkill -ArgumentList '/f','/im','EClone.exe' -Wait -WindowStyle Hidden
Start-Sleep -Milliseconds 400
$p = Start-Process -FilePath $Exe -PassThru
Start-Sleep -Seconds $WaitSec
if ($p.HasExited) { Write-Host "FAIL: exited early"; exit 1 }
$script:procId = $p.Id; $script:frame = [IntPtr]::Zero
[void][HK]::EnumWindows({
    param($h,$q)
    $wpid = 0; [void][HK]::GetWindowThreadProcessId($h, [ref]$wpid)
    if ($wpid -eq $script:procId -and [HK]::IsWindowVisible($h) -and [HK]::Cls($h) -eq 'ENewFrame') { $script:frame = $h }
    return $true
}, [IntPtr]::Zero)
if ($script:frame -eq [IntPtr]::Zero) { Write-Host "FAIL: no frame"; Stop-Process -Id $p.Id -Force; exit 1 }
Write-Host ("frame=" + $script:frame + " pid=" + $p.Id)

# status pane = the AfxControlBar42s whose caption starts with the first char of "status pane"
$script:status = [IntPtr]::Zero
[void][HK]::EnumChildWindows($script:frame, {
    param($h,$q)
    if ([HK]::Cls($h) -eq 'AfxControlBar42s') {
        $t = [HK]::Txt($h)
        if ($t.Length -ge 1 -and ([int][char]$t[0]) -eq 0x72B6) { $script:status = $h; return $false }
    }
    return $true
}, [IntPtr]::Zero)
if ($script:status -eq [IntPtr]::Zero) { Write-Host "FAIL: no status pane"; Stop-Process -Id $p.Id -Force; exit 1 }

# the hint page: the FIRST Edit inside the status pane's tab control
$edits = New-Object System.Collections.Generic.List[IntPtr]
[HK]::Collect($script:status, 'Edit', $edits)
if ($edits.Count -lt 1) { Write-Host "FAIL: no hint edit"; Stop-Process -Id $p.Id -Force; exit 1 }
$hint = $edits[0]
Write-Host ("hint page hwnd=" + $hint)

# the support library tree = the VISIBLE SysTreeView32 in the work pane (page 0 is selected at start)
$script:work = [IntPtr]::Zero
[void][HK]::EnumChildWindows($script:frame, {
    param($h,$q)
    if ([HK]::Cls($h) -eq 'AfxControlBar42s') {
        $t = [HK]::Txt($h)
        if ($t.Length -ge 1 -and ([int][char]$t[0]) -eq 0x5DE5) { $script:work = $h; return $false }
    }
    return $true
}, [IntPtr]::Zero)
if ($script:work -eq [IntPtr]::Zero) { Write-Host "FAIL: no work pane"; Stop-Process -Id $p.Id -Force; exit 1 }
$trees = New-Object System.Collections.Generic.List[IntPtr]
[HK]::Collect($script:work, 'SysTreeView32', $trees)
$libTree = [IntPtr]::Zero
foreach ($t in $trees) { if ([HK]::IsWindowVisible($t)) { $libTree = $t; break } }
if ($libTree -eq [IntPtr]::Zero) { Write-Host "FAIL: no visible lib tree"; Stop-Process -Id $p.Id -Force; exit 1 }
Write-Host ("lib tree hwnd=" + $libTree)

$t0 = [HK]::RemoteText($hint)
Write-Host ("hint page text before = " + $t0.Length + " chars")
Check "hint page starts empty or is not the last command's text" ($t0.Length -lt 40)

$dump = Join-Path $OutDir "trees_hint.txt"
$failMsg = ""

# 1) select a COMMAND node (info box) -> the hint page must carry the command's full description
cmd /c "`"D:\eclone\tools\treeinfo.exe`" $($p.Id) > `"$dump`""
$idx = & python "D:\eclone\tools\tree_index.py" $dump $N_LIB $N_CMD
if ($LASTEXITCODE -ne 0) { Write-Host ("[FAIL] command node not found: " + $N_CMD); $fail = 1 }
else {
    $parts = $idx -split '\s+'
    cmd /c "`"D:\eclone\tools\treeinfo.exe`" $($p.Id) click $($parts[0]) $($parts[1]) > `"$(Join-Path $OutDir 'node_hint.txt')`""
    Start-Sleep -Milliseconds 900
    $t1 = [HK]::RemoteText($hint)
    Write-Host ("hint page text after command = " + $t1.Length + " chars")
    Check "selecting a library command fills the hint page" ($t1.Length -gt 120)
    Check "the command hint contains the call-format lead-in" ($t1.Contains($HINT_LEAD))
    $shot = Join-Path $OutDir "hint_cmd.png"
    $r = New-Object HK+RECT
    [void][HK]::GetWindowRect($script:status, [ref]$r)
    $bmp = New-Object System.Drawing.Bitmap ($r.Right-$r.Left), ($r.Bottom-$r.Top)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $hdc = $g.GetHdc(); [void][HK]::PrintWindow($script:status, $hdc, 0); $g.ReleaseHdc($hdc)
    $bmp.Save($shot, [System.Drawing.Imaging.ImageFormat]::Png); $g.Dispose(); $bmp.Dispose()
    Write-Host ("shot -> " + $shot)
}

# 2) select the LIBRARY root node -> the library description goes into the same page
cmd /c "`"D:\eclone\tools\treeinfo.exe`" $($p.Id) > `"$dump`""
$idx2 = & python "D:\eclone\tools\tree_index.py" $dump $N_LIB $N_LIB
if ($LASTEXITCODE -ne 0) { Write-Host ("[FAIL] library node not found: " + $N_LIB); $fail = 1 }
else {
    $parts2 = $idx2 -split '\s+'
    cmd /c "`"D:\eclone\tools\treeinfo.exe`" $($p.Id) click $($parts2[0]) $($parts2[1]) > `"$(Join-Path $OutDir 'node_hint2.txt')`""
    Start-Sleep -Milliseconds 900
    $t2 = [HK]::RemoteText($hint)
    Write-Host ("hint page text after library = " + $t2.Length + " chars")
    Check "selecting the library root fills the hint page" ($t2.Length -gt 200)
    Check "the library hint contains the library-name lead-in" ($t2.Contains($LIB_LEAD))
}

Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
if ($fail -ne 0) { Write-Host "[FAIL] hint_smoke"; exit 1 }
Write-Host "[PASS] hint_smoke: library/command hints reach the hint page"
