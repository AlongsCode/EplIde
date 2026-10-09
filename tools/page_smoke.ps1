# Smoke-test the program tree's "table pages": click the nodes for global variables /
# user data types / constants and check that the editor switched to that page (the model
# dump contains the page header row) and that the MDI child title is the page name.
# ASCII-only on purpose (this machine reads .ps1 as ANSI): Chinese node names and expected
# strings are built from code points.
#
# The tree node is clicked with treeinfo.exe's `click` mode (real click coordinates computed
# from the VISIBLE chain), not by hand-guessed row numbers.
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
public class PH {
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
}
"@

function Shot([string]$path, [IntPtr]$hwnd) {
    $r = New-Object PH+RECT
    [void][PH]::GetWindowRect($hwnd, [ref]$r)
    $w = $r.Right - $r.Left; $h = $r.Bottom - $r.Top
    if ($w -le 0 -or $h -le 0) { return }
    $bmp = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $hdc = $g.GetHdc()
    [void][PH]::PrintWindow($hwnd, $hdc, 0)
    $g.ReleaseHdc($hdc)
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    Write-Host ("shot -> " + $path + " " + $w + "x" + $h)
}

# ---- code-point built strings (keeps this file pure ASCII) ----
$N_ROOT  = [string]([char]0x7A0B + [char]0x5E8F + [char]0x6570 + [char]0x636E)      # program data root
$N_GVAR  = [string]([char]0x5168 + [char]0x5C40 + [char]0x53D8 + [char]0x91CF)      # global variables
$N_TYPE  = [string]([char]0x81EA + [char]0x5B9A + [char]0x4E49 + [char]0x6570 + [char]0x636E + [char]0x7C7B + [char]0x578B)
$N_CONST = [string]([char]0x5E38 + [char]0x91CF + [char]0x8868) + '...'             # constants...
$H_GVAR  = [string]([char]0x5168 + [char]0x5C40 + [char]0x53D8 + [char]0x91CF + [char]0x540D)   # header cell
$H_TYPE  = [string]([char]0x6570 + [char]0x636E + [char]0x7C7B + [char]0x578B + [char]0x540D)
$H_CONST = [string]([char]0x5E38 + [char]0x91CF + [char]0x540D + [char]0x79F0)
$T_GVAR  = [string]([char]0x5168 + [char]0x5C40 + [char]0x53D8 + [char]0x91CF + [char]0x8868)
$T_TYPE  = [string]([char]0x81EA + [char]0x5B9A + [char]0x4E49 + [char]0x6570 + [char]0x636E + [char]0x7C7B + [char]0x578B + [char]0x8868)
$T_CONST = [string]([char]0x5E38 + [char]0x91CF + [char]0x6570 + [char]0x636E + [char]0x8868)

$cases = @(
    @{ node = $N_GVAR;  head = $H_GVAR;  title = $T_GVAR;  shot = 'page_gvar.png' },
    @{ node = $N_TYPE;  head = $H_TYPE;  title = $T_TYPE;  shot = 'page_type.png' },
    @{ node = $N_CONST; head = $H_CONST; title = $T_CONST; shot = 'page_const.png' }
)

$model = Join-Path $OutDir "model_page.txt"
Remove-Item $model -Force -ErrorAction SilentlyContinue
$env:ECLONE_DUMP_MODEL = $model
Start-Process -FilePath taskkill -ArgumentList '/f','/im','EClone.exe' -Wait -WindowStyle Hidden
Start-Sleep -Milliseconds 400

$p = Start-Process -FilePath $Exe -PassThru
Start-Sleep -Seconds $WaitSec
if ($p.HasExited) { Write-Host "FAIL: exited early"; exit 1 }
$script:procId = $p.Id; $script:frame = [IntPtr]::Zero
[void][PH]::EnumWindows({
    param($h,$q)
    $wpid = 0; [void][PH]::GetWindowThreadProcessId($h, [ref]$wpid)
    if ($wpid -eq $script:procId -and [PH]::IsWindowVisible($h) -and [PH]::Cls($h) -eq 'ENewFrame') { $script:frame = $h }
    return $true
}, [IntPtr]::Zero)
if ($script:frame -eq [IntPtr]::Zero) { Write-Host "FAIL: no frame"; Stop-Process -Id $p.Id -Force; exit 1 }
Write-Host ("frame=" + $script:frame + " pid=" + $p.Id)

# work pane = the AfxControlBar42s whose caption starts with the first char of the pane name
$script:work = [IntPtr]::Zero
[void][PH]::EnumChildWindows($script:frame, {
    param($h,$q)
    if ([PH]::Cls($h) -eq 'AfxControlBar42s') {
        $t = [PH]::Txt($h)
        if ($t.Length -ge 1 -and ([int][char]$t[0]) -eq 0x5DE5) { $script:work = $h; return $false }
    }
    return $true
}, [IntPtr]::Zero)
if ($script:work -eq [IntPtr]::Zero) { Write-Host "FAIL: no work pane"; Stop-Process -Id $p.Id -Force; exit 1 }

# switch the work pane to the "program" page (2nd tab) so the program tree is really shown
$tabs = New-Object System.Collections.Generic.List[IntPtr]
[PH]::Collect($script:work, 'SysTabControl32', $tabs)
if ($tabs.Count -lt 1) { Write-Host "FAIL: no work tab"; Stop-Process -Id $p.Id -Force; exit 1 }
$tab = $tabs[0]
$cr = New-Object PH+RECT
[void][PH]::GetClientRect($tab, [ref]$cr)
$cy2 = $cr.Bottom - 10
$lp2 = [IntPtr]((100 -band 0xFFFF) -bor ($cy2 -shl 16))
[void][PH]::PostMessage($tab, 0x0201, [IntPtr]1, $lp2)
Start-Sleep -Milliseconds 120
[void][PH]::PostMessage($tab, 0x0202, [IntPtr]0, $lp2)
Start-Sleep -Milliseconds 600
if ([int][PH]::SendMessage($tab, 0x130B, [IntPtr]::Zero, [IntPtr]::Zero) -ne 1) {
    Write-Host "FAIL: could not switch the work pane to the program page"
    Stop-Process -Id $p.Id -Force; exit 1
}

# the editor view (for the 1:1 shots)
$script:ed = [IntPtr]::Zero
[void][PH]::EnumChildWindows($script:frame, {
    param($h,$q)
    if ([PH]::Cls($h).StartsWith('Afx:400000:8:10005')) { $script:ed = $h; return $false }
    return $true
}, [IntPtr]::Zero)

$dump = Join-Path $OutDir "trees_mine.txt"
$fail = 0

foreach ($c in $cases) {
    cmd /c "`"D:\eclone\tools\treeinfo.exe`" $($p.Id) > `"$dump`""
    $idx = & python "D:\eclone\tools\tree_index.py" $dump $N_ROOT $c.node
    if ($LASTEXITCODE -ne 0) { Write-Host ("[FAIL] node not found: " + $c.node); $fail++; continue }
    $parts = $idx -split '\s+'
    cmd /c "`"D:\eclone\tools\treeinfo.exe`" $($p.Id) click $($parts[0]) $($parts[1]) > `"$(Join-Path $OutDir 'node_mine.txt')`""
    Start-Sleep -Milliseconds 800
    # re-dump the display model (the editor writes it again on EC_WM_REDUMP = WM_USER+102)
    if ($script:ed -ne [IntPtr]::Zero) {
        [void][PH]::PostMessage($script:ed, 0x0400 + 102, [IntPtr]::Zero, [IntPtr]::Zero)
        Start-Sleep -Milliseconds 600
    }
    $txt = Get-Content $model -Encoding Default -Raw
    $ok = ($txt -ne $null) -and ($txt.Contains($c.head))
    $title = [PH]::Txt($script:frame)
    $okTitle = $title.Contains($c.title)
    if ($ok -and $okTitle) {
        Write-Host ("[PASS] " + $c.node + " -> page opened (header + title)")
    } else {
        Write-Host ("[FAIL] " + $c.node + " header=" + $ok + " title=" + $okTitle + " titleNow=" + $title)
        $fail++
    }
    if ($script:ed -ne [IntPtr]::Zero) { Shot (Join-Path $OutDir $c.shot) $script:ed }
}

Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
Remove-Item Env:\ECLONE_DUMP_MODEL
if ($fail -ne 0) { Write-Host ("page_smoke: " + $fail + " FAILED"); exit 1 }
Write-Host "[PASS] page_smoke: all table pages opened"

