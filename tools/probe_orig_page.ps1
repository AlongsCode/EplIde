# Probe the ORIGINAL e.exe: click a program-tree node BY TEXT (e.g. 全局变量) and
# capture the editor view 1:1, so the document page's layout can be measured.
# ASCII-only on purpose (this machine reads .ps1 as ANSI).
# usage: powershell -File probe_orig_page.ps1 -File x.e -Node 全局变量 -OutDir d -Tag gvar
#        -NodeHex 5168...  is an alternative ASCII way to pass a GBK string.
param(
    [string]$Exe = "C:\Program Files (x86)\e\e.exe",
    [string]$File = "D:\eclone\shots\e_samples\jy.e",
    [string]$OutDir = "D:\eclone\shots\live_page",
    [string]$Node = "",
    [int]$WaitSec = 16,
    [int]$WhichTree = -1
)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public class PG {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern IntPtr GetParent(IntPtr h);
  [DllImport("kernel32.dll")] public static extern IntPtr OpenProcess(uint a, bool i, int pid);
  [DllImport("kernel32.dll")] public static extern IntPtr VirtualAllocEx(IntPtr p, IntPtr a, IntPtr s, uint t, uint pr);
  [DllImport("kernel32.dll")] public static extern bool VirtualFreeEx(IntPtr p, IntPtr a, IntPtr s, uint t);
  [DllImport("kernel32.dll")] public static extern bool WriteProcessMemory(IntPtr p, IntPtr a, byte[] b, IntPtr s, out IntPtr w);
  [DllImport("kernel32.dll")] public static extern bool ReadProcessMemory(IntPtr p, IntPtr a, byte[] b, IntPtr s, out IntPtr r);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public static string Cls(IntPtr h){ var sb=new StringBuilder(128); GetClassNameW(h,sb,128); return sb.ToString(); }
  public static string Txt(IntPtr h){ var sb=new StringBuilder(512); GetClassNameW(h,sb,0); return sb.ToString(); }
  // TVM_GETITEMW across processes: the item struct and its pszText must live in the target
  public static string ItemText(IntPtr tree, IntPtr item, int pid){
    IntPtr proc = OpenProcess(0x1F0FFF, false, pid);
    if (proc == IntPtr.Zero) return "";
    int sz = 512;
    IntPtr remote = VirtualAllocEx(proc, IntPtr.Zero, (IntPtr)(sz + 64), 0x3000, 0x04);
    if (remote == IntPtr.Zero) return "";
    IntPtr rtext = (IntPtr)((long)remote + 64);
    byte[] buf = new byte[sz + 64];
    // TVITEMA layout: mask(0) hItem(4) state(8) pszText(12) cchTextMax(16) iImage(20)
    // (the earlier attempt wrote pszText at 8 by mistake, which is why the text came back empty)
    BitConverter.GetBytes(0x11).CopyTo(buf, 0);
    BitConverter.GetBytes((int)item).CopyTo(buf, 4);
    BitConverter.GetBytes((int)rtext).CopyTo(buf, 12);
    BitConverter.GetBytes(511).CopyTo(buf, 16);
    IntPtr wr;
    WriteProcessMemory(proc, remote, buf, (IntPtr)(sz + 64), out wr);
    // The original IDE is an ANSI app: its tree control only implements TVM_GETITEMA
    // (0x110E) with a TVITEMA struct + ANSI char buffer, so ask for that and decode GBK.
    SendMessage(tree, 0x110E, IntPtr.Zero, remote);   // TVM_GETITEMA
    byte[] outb = new byte[sz];
    IntPtr rd;
    ReadProcessMemory(proc, rtext, outb, (IntPtr)sz, out rd);
    VirtualFreeEx(proc, remote, IntPtr.Zero, 0x8000);
    // the original IDE is an ANSI app: TVM_GETITEMW actually hands back GBK bytes
    string s = Encoding.GetEncoding(936).GetString(outb);
    int z = s.IndexOf('\0');
    return z >= 0 ? s.Substring(0, z) : s;
  }
}
"@

$enc = [System.Text.Encoding]::GetEncoding(936)
$wantBytes = $enc.GetBytes($Node)
$want = $enc.GetString($wantBytes)

$p = Start-Process -FilePath $Exe -WorkingDirectory (Split-Path $Exe) -ArgumentList @($File) -PassThru
Start-Sleep -Seconds $WaitSec
if ($p.HasExited) { Write-Host "exited early"; exit 1 }

$script:frame = [IntPtr]::Zero
[void][PG]::EnumWindows([PG+EnumProc]{
    param($h, $q)
    $wpid = 0
    [void][PG]::GetWindowThreadProcessId($h, [ref]$wpid)
    if ($wpid -eq $p.Id -and [PG]::Cls($h) -eq 'ENewFrame') { $script:frame = $h; return $false }
    return $true
}, [IntPtr]::Zero)
if ($script:frame -eq [IntPtr]::Zero) { Write-Host "no frame"; $p.Kill(); exit 1 }
Write-Host ("frame=" + $script:frame + " pid=" + $p.Id)

# ---------------------------------------------------------------------------
# Find the target node's VISIBLE ROW the simple way: the IDE's tree opens with only
# the root expanded, so the visible rows are the root's direct children in order.
# treeinfo.exe (32-bit helper) dumps the whole tree in GBK; python then tells us the
# direct-child index of the wanted text. (Reading node text from a 64-bit PowerShell
# via TVM_GETITEM does not work reliably across the 32/64-bit boundary.)
$dump = Join-Path $OutDir "trees.txt"
cmd /c "`"D:\eclone\tools\treeinfo.exe`" $($p.Id) > `"$dump`""
Write-Host ("trees -> " + $dump)
$row = & python "D:\eclone\tools\tree_row.py" $dump $Node
if ($LASTEXITCODE -ne 0) { Write-Host ("node not found: " + $Node); Stop-Process -Id $p.Id -Force; exit 2 }
$row = [int]$row
Write-Host ("target row = " + $row + " (want=" + $Node + ")")

# work pane -> click tab 2 so the program tree is the visible page
$script:work = [IntPtr]::Zero
[void][PG]::EnumChildWindows($script:frame, [PG+EnumProc]{
    param($h, $q)
    if ([PG]::Cls($h) -eq 'AfxControlBar42s') {
        $r = New-Object PG+RECT
        [void][PG]::GetWindowRect($h, [ref]$r)
        if (($r.Bottom - $r.Top) -gt 300 -and ($r.Left -lt 300)) { $script:work = $h; return $false }
    }
    return $true
}, [IntPtr]::Zero)
if ($script:work -eq [IntPtr]::Zero) { Write-Host "no work pane"; $p.Kill(); exit 1 }
$tab = [PG]::SendMessage($script:work, 0x0000, [IntPtr]::Zero, [IntPtr]::Zero)  # placeholder
$tabs = New-Object System.Collections.Generic.List[IntPtr]
[void][PG]::EnumChildWindows($script:work, [PG+EnumProc]{
    param($h,$q)
    if ([PG]::Cls($h) -eq 'SysTabControl32') { $tabs.Add($h) }
    return $true
}, [IntPtr]::Zero)
$tab = $tabs[0]
$cr = New-Object PG+RECT
[void][PG]::GetWindowRect($tab, [ref]$cr)
$lpt = [IntPtr]((( [int](($cr.Left+$cr.Right)/2 - $cr.Left) -band 0xFFFF) -bor (((($cr.Bottom-$cr.Top) - 8) -band 0xFFFF) -shl 16)))
foreach ($x in @(78, 84, 70, 90, 60, 100)) {
    $y = ($cr.Bottom - $cr.Top) - 10
    [void][PG]::PostMessage($tab, 0x0201, [IntPtr]1, [IntPtr](($y -shl 16) -bor $x))
    Start-Sleep -Milliseconds 120
    [void][PG]::PostMessage($tab, 0x0202, [IntPtr]0, [IntPtr](($y -shl 16) -bor $x))
    Start-Sleep -Milliseconds 700
    if ([int][PG]::SendMessage($tab, 0x130B, [IntPtr]::Zero, [IntPtr]::Zero) -eq 1) { break }
}
Write-Host ("work tab cursel=" + [int][PG]::SendMessage($tab, 0x130B, [IntPtr]::Zero, [IntPtr]::Zero))

# pick the program tree (largest visible SysTreeView32 in the work pane)
$trees = New-Object System.Collections.Generic.List[IntPtr]
[void][PG]::EnumChildWindows($script:work, [PG+EnumProc]{
    param($h,$q)
    if ([PG]::Cls($h) -eq 'SysTreeView32' -and [PG]::IsWindowVisible($h)) { $trees.Add($h) }
    return $true
}, [IntPtr]::Zero)
$tree = $trees[0]
Write-Host ("tree=" + $tree + " count=" + $trees.Count)

# click the target row (the IDE's tree opens with only the root expanded, so the visible
# rows are the root's direct children: row0 = root, row1 = first child, ...)
$itemH = [int][PG]::SendMessage($tree, 0x1111, [IntPtr]::Zero, [IntPtr]::Zero)
if ($itemH -le 0) { $itemH = 16 }
$cx = 60
$cy = 2 + $row * $itemH + [int]($itemH / 2)
Write-Host ("click tree row " + $row + " at client " + $cx + "," + $cy + " itemH=" + $itemH)
$lp = [IntPtr](($cx -band 0xFFFF) -bor ($cy -shl 16))
[void][PG]::PostMessage($tree, 0x0201, [IntPtr]1, $lp)
Start-Sleep -Milliseconds 150
[void][PG]::PostMessage($tree, 0x0202, [IntPtr]0, $lp)
Start-Sleep -Seconds 2

# editor view 1:1
$script:ed = [IntPtr]::Zero
$mdi = New-Object System.Collections.Generic.List[IntPtr]
[void][PG]::EnumChildWindows($script:frame, [PG+EnumProc]{
    param($h,$q)
    if ([PG]::Cls($h) -eq 'AfxMDIFrame42s') { $mdi.Add($h) }
    return $true
}, [IntPtr]::Zero)
foreach ($m in $mdi) {
    [void][PG]::EnumChildWindows($m, [PG+EnumProc]{
        param($h,$q)
        if ([PG]::Cls($h).StartsWith('Afx:400000:b') -and [PG]::IsWindowVisible($h)) { $script:ed = $h; return $false }
        return $true
    }, [IntPtr]::Zero)
    if ($script:ed -ne [IntPtr]::Zero) { break }
}
if ($script:ed -eq [IntPtr]::Zero) { Write-Host "no editor"; Stop-Process -Id $p.Id -Force; exit 1 }
[void][PG]::PostMessage($script:ed, 0x0008, [IntPtr]::Zero, [IntPtr]::Zero)
Start-Sleep -Milliseconds 300
$r = New-Object PG+RECT
[void][PG]::GetWindowRect($script:ed, [ref]$r)
$w = $r.Right-$r.Left; $h = $r.Bottom-$r.Top
$bmp = New-Object System.Drawing.Bitmap $w, $h
$g = [System.Drawing.Graphics]::FromImage($bmp)
$hdc = $g.GetHdc()
[void][PG]::PrintWindow($script:ed, $hdc, 0)
$g.ReleaseHdc($hdc)
$png = Join-Path $OutDir ("view_" + $Node + ".png")
$bmp.Save($png, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
Write-Host ("page shot -> " + $png + " " + $w + "x" + $h)
Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
