# Pane tab paging smoke test (ASCII only).
#   work pane   : page ids 1019 (library) / 1020 (program) / 1021 (properties)
#   status pane : page ids 1110..1117
# Clicks the tab strip of each pane and asserts that exactly the matching page is visible.
# This is the regression for the "ON_NOTIFY(TCN_SELCHANGE, 0, ...) never fires" bug:
# MFC's ON_NOTIFY takes a control-ID *range*, so id 0 means 0..0 and never matches.
# usage: powershell -NoProfile -ExecutionPolicy Bypass -File tabs_smoke.ps1
param(
    [string]$Exe = "D:\eclone\bin\EClone.exe",
    [int]$WaitSec = 6
)
$ErrorActionPreference = 'Continue'
Add-Type @"
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public class TH {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern int GetDlgCtrlID(IntPtr h);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public static string Cls(IntPtr h){ var sb=new StringBuilder(128); GetClassNameW(h,sb,128); return sb.ToString(); }
  public static string Txt(IntPtr h){ var sb=new StringBuilder(256); GetWindowTextW(h,sb,256); return sb.ToString(); }
  public static IntPtr FindChild(IntPtr parent, string cls){
    IntPtr found = IntPtr.Zero;
    EnumChildWindows(parent, delegate(IntPtr c, IntPtr p){
      if (found == IntPtr.Zero && Cls(c) == cls) { found = c; return false; }
      return true;
    }, IntPtr.Zero);
    return found;
  }
  public static IntPtr FindBar(IntPtr frame, int firstChar){
    IntPtr found = IntPtr.Zero;
    EnumChildWindows(frame, delegate(IntPtr c, IntPtr p){
      if (Cls(c) == "AfxControlBar42s") {
        string t = Txt(c);
        if (t.Length >= 1 && (int)t[0] == firstChar) { found = c; return false; }
      }
      return true;
    }, IntPtr.Zero);
    return found;
  }
  public static void CollectBars(IntPtr frame, List<IntPtr> o){
    EnumChildWindows(frame, delegate(IntPtr c, IntPtr p){
      if (Cls(c) == "AfxControlBar42s") o.Add(c);
      return true;
    }, IntPtr.Zero);
  }
  public static bool PageVisible(IntPtr pane, int id){
    IntPtr h = FindChild(pane, "SysTreeView32");
    /* find by control id instead: use GetDlgItem via SendMessage is not available, walk children */
    bool vis = false;
    EnumChildWindows(pane, delegate(IntPtr c, IntPtr p){
      if (GetDlgCtrlID(c) == id) { vis = IsWindowVisible(c); return false; }
      return true;
    }, IntPtr.Zero);
    return vis;
  }
}
"@

$p = Start-Process -FilePath $Exe -PassThru
Start-Sleep -Seconds $WaitSec
if ($p.HasExited) { Write-Host "FAIL: exited early"; exit 1 }

$frame = [IntPtr]::Zero
[void][TH]::EnumWindows([TH+EnumProc]{
    param($h,$q)
    $wpid = 0; [void][TH]::GetWindowThreadProcessId($h, [ref]$wpid)
    if ($wpid -eq $p.Id -and [TH]::Cls($h) -eq 'ENewFrame') { $script:frame = $h; return $false }
    return $true
}, [IntPtr]::Zero)
if ($script:frame -eq [IntPtr]::Zero) { Write-Host "FAIL: no frame"; Stop-Process -Id $p.Id -Force; exit 1 }

$bars = New-Object System.Collections.Generic.List[IntPtr]
[TH]::CollectBars($script:frame, $bars)
Write-Host ("dock bars = " + $bars.Count)
$workPane = [IntPtr]::Zero
$statPane = [IntPtr]::Zero
foreach ($b in $bars) {
    $t = [TH]::Txt($b)
    $c0 = -1
    if ($t.Length -ge 1) { $c0 = [int][char]$t[0] }
    Write-Host ("  bar " + $b + " first=U+" + ("{0:X4}" -f [int]$c0))
    if ($c0 -eq 0x5DE5) { $script:workPane = $b }     # work pane
    if ($c0 -eq 0x72B6) { $script:statPane = $b }     # status pane
}
if (([int64]$workPane) -eq 0 -or ([int64]$statPane) -eq 0) {
    Write-Host ("FAIL: panes not found (work=" + $workPane + " status=" + $statPane + ")")
    Stop-Process -Id $p.Id -Force; exit 1
}

# Walk the tab strip with the keyboard (comctl32 tab controls switch on VK_RIGHT and send
# TCN_SELCHANGE) -- this avoids guessing pixel positions of tab items.
function TestPane([string]$name, [IntPtr]$pane, [int]$tabId, [int[]]$pageIds) {
    $tab = [TH]::FindChild($pane, 'SysTabControl32')
    if ($tab -eq [IntPtr]::Zero) { Write-Host ("FAIL " + $name + ": no tab control"); return $false }
    $bad = 0
    $n = $pageIds.Length
    $want = 0
    # VK_RIGHT walks forward and does NOT wrap; VK_HOME goes back to the first tab.
    for ($i = 1; $i -le $n; $i++) {
        if ($i -lt $n) {
            [void][TH]::SendMessage($tab, 0x0100, [IntPtr]0x27, [IntPtr]::Zero)   # VK_RIGHT
            $want = $i
        } else {
            [void][TH]::SendMessage($tab, 0x0100, [IntPtr]0x24, [IntPtr]::Zero)   # VK_HOME
            $want = 0
        }
        Start-Sleep -Milliseconds 250
        $cur = [int][TH]::SendMessage($tab, 0x130B, [IntPtr]::Zero, [IntPtr]::Zero)
        $vis = -1
        for ($k = 0; $k -lt $n; $k++) {
            if ([TH]::PageVisible($pane, $pageIds[$k])) { $vis = $k }
        }
        if ($cur -ne $want -or $vis -ne $want) {
            Write-Host ("  FAIL " + $name + " step#" + $i + ": cursel=" + $cur + " visiblePageIdx=" + $vis + " want=" + $want)
            $bad++
        }
    }
    if ($bad -eq 0) { Write-Host ("  [ok] " + $name + ": " + $n + " pages switch") }
    return ($bad -eq 0)
}

Write-Host "work pane:"
$ok1 = TestPane "work" $workPane 1000 @(1019,1020,1021)
Write-Host "status pane:"
$ok2 = TestPane "status" $statPane 1100 @(1110,1111,1112,1113,1114,1115,1116,1117)

Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
if ($ok1 -and $ok2) { Write-Host "[PASS] pane tab paging works"; exit 0 }
Write-Host "[FAIL] pane tab paging broken"; exit 1
