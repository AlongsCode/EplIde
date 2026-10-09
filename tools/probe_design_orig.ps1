# Probe the ORIGINAL e.exe window designer: print the real design form (WTWindow,
# created by krnln with blInDesignMode=TRUE) and its container view 1:1.
# ASCII-only on purpose (this machine reads .ps1 as ANSI).
param(
    [string]$Exe = "C:\Program Files (x86)\e\e.exe",
    [string]$File = "D:\eclone\shots\e_samples\graphic.e",
    [string]$OutDir = "D:\eclone\shots\live_des_orig",
    [string]$Class = "WTWindow",
    [string]$ExeArg = "",
    [string]$Activate = "",
    [int]$WaitSec = 14,
    [switch]$Keep
)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
Add-Type -AssemblyName System.Drawing

Add-Type @"
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public class LV {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern IntPtr GetParent(IntPtr h);
  [DllImport("user32.dll")] public static extern int GetWindowLong(IntPtr h, int i);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public static string Text(IntPtr h){
    var sb=new StringBuilder(1024); GetWindowTextW(h,sb,1024);
    if (sb.Length>0) return sb.ToString();
    IntPtr buf = Marshal.AllocHGlobal(4096);
    for (int i=0;i<2048;i++) Marshal.WriteInt16(buf, i*2, 0);
    SendMessage(h, 0x000D, (IntPtr)2048, buf);
    string s = Marshal.PtrToStringUni(buf);
    Marshal.FreeHGlobal(buf);
    return s == null ? "" : s;
  }
  public static string Cls(IntPtr h){ var sb=new StringBuilder(256); GetClassNameW(h,sb,256); return sb.ToString(); }
}
"@

$args2 = @()
if ($File -ne "" -and $File -ne "none") { $args2 += $File }
if ($ExeArg -ne "") { $args2 += $ExeArg }
$p = Start-Process -FilePath $Exe -WorkingDirectory (Split-Path $Exe) -ArgumentList @($args2) -PassThru
Start-Sleep -Seconds $WaitSec
if ($p.HasExited) { Write-Host "exited early"; exit 1 }

$forms  = New-Object System.Collections.Generic.List[IntPtr]
$views  = New-Object System.Collections.Generic.List[IntPtr]
$all    = New-Object System.Collections.Generic.List[string]
[void][LV]::EnumWindows([LV+EnumProc]{
    param($h, $p2)
    $wpid = 0
    [void][LV]::GetWindowThreadProcessId($h, [ref]$wpid)
    if ($wpid -eq $p.Id) {
        [void][LV]::EnumChildWindows($h, [LV+EnumProc]{
            param($c, $q)
            $cl = [LV]::Cls($c)
            if ($cl -eq $Class) { $forms.Add($c) }
            if ($cl -like 'Afx:400000:b*') { $views.Add($c) }
            if ([LV]::IsWindowVisible($c)) {
                $r = New-Object LV+RECT
                [void][LV]::GetWindowRect($c, [ref]$r)
                $all.Add(("{0,-30} {1},{2} {3}x{4} style=0x{5:X8} {6}" -f $cl, $r.Left, $r.Top,
                          ($r.Right-$r.Left), ($r.Bottom-$r.Top), [LV]::GetWindowLong($c,-16), [LV]::Text($c)))
            }
            return $true
        }, [IntPtr]::Zero)
    }
    return $true
}, [IntPtr]::Zero)
$all | Set-Content -Path (Join-Path $OutDir "windows.txt") -Encoding UTF8

function Shot([string]$tag, [IntPtr]$hwnd, [switch]$Client) {
    $r = New-Object LV+RECT
    if ($Client) { [void][LV]::GetClientRect($hwnd, [ref]$r) } else { [void][LV]::GetWindowRect($hwnd, [ref]$r) }
    $w = $r.Right-$r.Left; $h = $r.Bottom-$r.Top
    if ($w -le 0 -or $h -le 0) { Write-Host "skip $tag (empty)"; return }
    $bmp = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $hdc = $g.GetHdc()
    [void][LV]::PrintWindow($hwnd, $hdc, 0)
    $g.ReleaseHdc($hdc)
    $png = Join-Path $OutDir $tag
    $bmp.Save($png, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    Write-Host ("shot: " + $png + " " + $w + "x" + $h)
}

Write-Host ("forms=" + $forms.Count + " views=" + $views.Count)
if ($Activate -ne "") {
    $mdi = [IntPtr]::Zero
    $frames = New-Object System.Collections.Generic.List[IntPtr]
    [void][LV]::EnumWindows([LV+EnumProc]{
        param($h, $p2)
        $wpid = 0
        [void][LV]::GetWindowThreadProcessId($h, [ref]$wpid)
        if ($wpid -eq $p.Id -and [LV]::Cls($h) -eq 'ENewFrame') { $frames.Add($h) }
        return $true
    }, [IntPtr]::Zero)
    foreach ($f in $frames) {
        [void][LV]::EnumChildWindows($f, [LV+EnumProc]{
            param($c, $q)
            if ([LV]::Cls($c) -eq 'MDIClient') { $script:mdi = $c }
            return $true
        }, [IntPtr]::Zero)
    }
    Write-Host ("mdiclient=" + $mdi)
    [void][LV]::EnumChildWindows($mdi, [LV+EnumProc]{
        param($c, $q)
        $t = [LV]::Text($c)
        if ($t -like "*$Activate*") {
            Write-Host ("activate '" + $t + "' hwnd=" + $c)
            [void][LV]::SendMessage($mdi, 0x0222, $c, [IntPtr]::Zero)
        }
        return $true
    }, [IntPtr]::Zero)
    Start-Sleep -Seconds 2
}
$i = 0
foreach ($v in $views) { Shot ("view$i.png") $v; Shot ("view${i}_client.png") $v -Client; $i++ }
$i = 0
foreach ($f in $forms) { Shot ("form$i.png") $f; $i++ }
Write-Host ("pid=" + $p.Id)
if (-not $Keep) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }
