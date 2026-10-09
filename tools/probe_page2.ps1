# Probe the ORIGINAL e.exe: select a program-tree node BY TEXT and record what page opens.
# ASCII-only on purpose (this machine reads .ps1 as ANSI).
#
# Why TVM_SELECTITEM instead of a mouse click: the tree's initial expansion state is not
# "only the root", so a coordinate click needs to know the visible-row index, which cannot
# be computed from the dump. TVM_SELECTITEM (TVGN_CARET) still makes the control send
# TVN_SELCHANGED, so the IDE reacts exactly as for a click.
#
# usage: powershell -File tools\probe_page2.ps1 -File x.e -Node <gbk text> -Root <gbk root> -OutDir d
param(
    [string]$Exe = "C:\Program Files (x86)\e\e.exe",
    [string]$File = "D:\eclone\shots\e_samples\jy.e",
    [string]$OutDir = "D:\eclone\shots\live_page2",
    [string]$Node = "",
    [string]$Root = "",
    [int]$WaitSec = 16
)
$ErrorActionPreference = 'Stop'
if ($Root -eq "") { Write-Host "-Root is required (gbk text of the program tree root)"; exit 2 }
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public class P2 {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public static string Cls(IntPtr h){ var sb=new StringBuilder(128); GetClassNameW(h,sb,128); return sb.ToString(); }
  public static string Txt(IntPtr h){ var sb=new StringBuilder(512); GetWindowTextW(h,sb,512); return sb.ToString(); }
  public static string Rect(IntPtr h){ RECT r; GetWindowRect(h, out r); return (r.Right-r.Left)+"x"+(r.Bottom-r.Top)+" at "+r.Left+","+r.Top; }
}
"@

function Shot([IntPtr]$h, [string]$path) {
    $r = New-Object P2+RECT
    [void][P2]::GetWindowRect($h, [ref]$r)
    $w = $r.Right - $r.Left; $ht = $r.Bottom - $r.Top
    if ($w -le 0 -or $ht -le 0) { Write-Host "bad rect for $path"; return }
    $bmp = New-Object System.Drawing.Bitmap $w, $ht
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $hdc = $g.GetHdc()
    [void][P2]::PrintWindow($h, $hdc, 0)
    $g.ReleaseHdc($hdc)
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    Write-Host ("shot -> " + $path + " " + $w + "x" + $ht)
}

# the original IDE is single-instance: a leftover e.exe would swallow the file and make the
# new process exit immediately ("exited early"), so clear any previous run first.
Start-Process -FilePath taskkill -ArgumentList '/f','/im','e.exe' -Wait -WindowStyle Hidden
Start-Sleep -Milliseconds 500
$p = Start-Process -FilePath $Exe -WorkingDirectory (Split-Path $Exe) -ArgumentList @($File) -PassThru
Start-Sleep -Seconds $WaitSec
if ($p.HasExited) { Write-Host "exited early"; exit 1 }

$script:frame = [IntPtr]::Zero
[void][P2]::EnumWindows([P2+EnumProc]{
    param($h, $q)
    $wpid = 0
    [void][P2]::GetWindowThreadProcessId($h, [ref]$wpid)
    if ($wpid -eq $p.Id -and [P2]::Cls($h) -eq 'ENewFrame') { $script:frame = $h; return $false }
    return $true
}, [IntPtr]::Zero)
if ($script:frame -eq [IntPtr]::Zero) { Write-Host "no frame"; $p.Kill(); exit 1 }
Write-Host ("frame=" + $script:frame + " pid=" + $p.Id)
Write-Host ("title BEFORE = " + [P2]::Txt($script:frame))

$dump = Join-Path $OutDir "trees.txt"
cmd /c "`"D:\eclone\tools\treeinfo.exe`" $($p.Id) > `"$dump`""
$idx = & python "D:\eclone\tools\tree_index.py" $dump $Root $Node
if ($LASTEXITCODE -ne 0) { Write-Host ("node not found: " + $Root + " / " + $Node); Stop-Process -Id $p.Id -Force; exit 2 }
$parts = $idx -split '\s+'
Write-Host ("tree=$($parts[0]) item=$($parts[1]) for |$Node|")

# TVM_GETITEMRECT only works while the tree is really shown: the work pane opens on the
# support-library tab, so click the work-pane tabs until the program tree reports visible.
$hx = & python "D:\eclone\tools\tree_pick.py" $dump $Root
if ($LASTEXITCODE -ne 0) { Write-Host "program tree hwnd not found"; Stop-Process -Id $p.Id -Force; exit 2 }
$script:tree = [IntPtr][Convert]::ToInt64($hx, 16)
$script:work = [IntPtr]::Zero
[void][P2]::EnumChildWindows($script:frame, [P2+EnumProc]{
    param($h, $q)
    if ([P2]::Cls($h) -eq 'AfxControlBar42s') {
        $r = New-Object P2+RECT
        [void][P2]::GetWindowRect($h, [ref]$r)
        if (($r.Bottom - $r.Top) -gt 300 -and ($r.Left -lt 300)) { $script:work = $h; return $false }
    }
    return $true
}, [IntPtr]::Zero)
if ($script:work -eq [IntPtr]::Zero) { Write-Host "no work pane"; $p.Kill(); exit 1 }
$tabs = New-Object System.Collections.Generic.List[IntPtr]
[void][P2]::EnumChildWindows($script:work, [P2+EnumProc]{
    param($h,$q)
    if ([P2]::Cls($h) -eq 'SysTabControl32') { $tabs.Add($h) }
    return $true
}, [IntPtr]::Zero)
if ($tabs.Count -eq 0) { Write-Host "no work tabs"; $p.Kill(); exit 1 }
$tab = $tabs[0]
$ntabs = [int][P2]::SendMessage($tab, 0x1304, [IntPtr]::Zero, [IntPtr]::Zero)   # TCM_GETITEMCOUNT
$cr = New-Object P2+RECT
[void][P2]::GetWindowRect($tab, [ref]$cr)
$ty = ($cr.Bottom - $cr.Top) - 10
$picked = -1
for ($i = 0; $i -lt $ntabs; $i++) {
    $tx = 20 + $i * 55
    [void][P2]::PostMessage($tab, 0x0201, [IntPtr]1, [IntPtr](($ty -shl 16) -bor $tx))
    Start-Sleep -Milliseconds 120
    [void][P2]::PostMessage($tab, 0x0202, [IntPtr]0, [IntPtr](($ty -shl 16) -bor $tx))
    Start-Sleep -Milliseconds 900
    if ([P2]::IsWindowVisible($script:tree)) { $picked = $i; break }
}
Write-Host ("work tabs=" + $ntabs + " picked=" + $picked)
if ($picked -lt 0) { Write-Host "program tree never became visible"; $p.Kill(); exit 1 }

Shot $script:frame (Join-Path $OutDir "frame_before.png")
$nodelog = Join-Path $OutDir ("node_" + $Node + ".txt")
cmd /c "`"D:\eclone\tools\treeinfo.exe`" $($p.Id) click $($parts[0]) $($parts[1]) > `"$nodelog`""
Start-Sleep -Seconds 2
Write-Host ("title AFTER  = " + [P2]::Txt($script:frame))
Write-Host ("node log -> " + $nodelog)

$lines = New-Object System.Collections.Generic.List[string]
[void][P2]::EnumChildWindows($script:frame, [P2+EnumProc]{
    param($h,$q)
    $lines.Add(("0x{0:X8} {1,-28} vis={2,-5} {3,-26} |{4}|" -f [int64]$h, [P2]::Cls($h), [P2]::IsWindowVisible($h), [P2]::Rect($h), [P2]::Txt($h)))
    return $true
}, [IntPtr]::Zero)
$lines | Set-Content -Encoding UTF8 (Join-Path $OutDir "windows.txt")

Shot $script:frame (Join-Path $OutDir ("frame_" + $Node + ".png"))

$script:ed = [IntPtr]::Zero
$mdi = New-Object System.Collections.Generic.List[IntPtr]
[void][P2]::EnumChildWindows($script:frame, [P2+EnumProc]{
    param($h,$q)
    if ([P2]::Cls($h) -eq 'AfxMDIFrame42s' -and [P2]::IsWindowVisible($h)) { $mdi.Add($h) }
    return $true
}, [IntPtr]::Zero)
foreach ($m in $mdi) {
    [void][P2]::EnumChildWindows($m, [P2+EnumProc]{
        param($h,$q)
        $c = [P2]::Cls($h)
        if (($c.StartsWith('Afx:400000:b') -or $c.StartsWith('Afx:400000:8')) -and [P2]::IsWindowVisible($h)) { $script:ed = $h; return $false }
        return $true
    }, [IntPtr]::Zero)
    if ($script:ed -ne [IntPtr]::Zero) { break }
}
if ($script:ed -ne [IntPtr]::Zero) {
    $r = New-Object P2+RECT
    [void][P2]::GetWindowRect($script:ed, [ref]$r)
    Shot $script:ed (Join-Path $OutDir ("view_" + $Node + ".png"))
} else {
    Write-Host "no document client found"
}
Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
