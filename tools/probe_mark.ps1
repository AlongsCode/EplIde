# Does the little red marker in the original editor's gutter follow the caret?
# Start e.exe with a sample, PrintWindow the editor view, click on another row, capture again.
# ASCII only.
param(
    [string]$Exe = "C:\Program Files (x86)\e\e.exe",
    [string]$File = "D:\eclone\shots\e_samples\graphic.e",
    [string]$OutDir = "D:\eclone\shots\live_mark",
    [int]$WaitSec = 15
)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public class MK {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint f);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public static string Cls(IntPtr h){ var sb=new StringBuilder(128); GetClassNameW(h,sb,128); return sb.ToString(); }
  public static void Collect(IntPtr parent, string cls, List<IntPtr> o){
    EnumChildWindows(parent, delegate(IntPtr c, IntPtr p){ if (Cls(c) == cls) o.Add(c); return true; }, IntPtr.Zero);
  }
}
"@

$p = Start-Process -FilePath $Exe -WorkingDirectory (Split-Path $Exe) -ArgumentList @($File) -PassThru
Start-Sleep -Seconds $WaitSec
if ($p.HasExited) { Write-Host "exited early"; exit 1 }

$script:frame = [IntPtr]::Zero
[void][MK]::EnumWindows([MK+EnumProc]{
    param($h,$q)
    $wpid = 0; [void][MK]::GetWindowThreadProcessId($h, [ref]$wpid)
    if ($wpid -eq $p.Id -and [MK]::Cls($h) -eq 'ENewFrame') { $script:frame = $h; return $false }
    return $true
}, [IntPtr]::Zero)
if ($script:frame -eq [IntPtr]::Zero) { Write-Host "no frame"; $p.Kill(); exit 1 }
[void][MK]::SetForegroundWindow($script:frame)
Start-Sleep -Milliseconds 500

# editor view = Afx:400000:b:... child of AfxMDIFrame42s
$views = New-Object System.Collections.Generic.List[IntPtr]
[MK]::Collect($script:frame, 'AfxMDIFrame42s', $views)
$ed = [IntPtr]::Zero
$views2 = New-Object System.Collections.Generic.List[IntPtr]
foreach ($v in $views) { [MK]::Collect($v, 'Afx:400000:b:10003:900015:0', $views2) }
if ($views2.Count -gt 0) { $ed = $views2[0] }
if ($ed -eq [IntPtr]::Zero) { Write-Host "no editor"; $p.Kill(); exit 1 }
Write-Host ("editor=" + $ed)

function Shot([string]$path) {
    $r = New-Object MK+RECT
    [void][MK]::GetWindowRect($ed, [ref]$r)
    $w = $r.Right - $r.Left; $h = $r.Bottom - $r.Top
    $bmp = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $hdc = $g.GetHdc()
    [void][MK]::PrintWindow($ed, $hdc, 0)
    $g.ReleaseHdc($hdc)
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    Write-Host ("shot -> " + $path + " " + $w + "x" + $h)
}

Shot (Join-Path $OutDir "before.png")

# click somewhere low in the code area (client coords of the editor view)
$cx = 200; $cy = 300
$lp = [IntPtr](($cx -band 0xFFFF) -bor ($cy -shl 16))
[void][MK]::PostMessage($ed, 0x0201, [IntPtr]1, $lp)
Start-Sleep -Milliseconds 150
[void][MK]::PostMessage($ed, 0x0202, [IntPtr]0, $lp)
Start-Sleep -Seconds 1
Shot (Join-Path $OutDir "after_click300.png")

$cx = 200; $cy = 120
$lp = [IntPtr](($cx -band 0xFFFF) -bor ($cy -shl 16))
[void][MK]::PostMessage($ed, 0x0201, [IntPtr]1, $lp)
Start-Sleep -Milliseconds 150
[void][MK]::PostMessage($ed, 0x0202, [IntPtr]0, $lp)
Start-Sleep -Seconds 1
Shot (Join-Path $OutDir "after_click120.png")

Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
