# Probe the ORIGINAL e.exe: set a breakpoint with F9 and capture the code editor 1:1, so the
# breakpoint marker (colour + geometry) can be measured.
#
# Why this file exists: probe_bp.ps1 kept probing an EMPTY IDE. The original is single-instance,
# so a still-dying instance swallows the file given on the command line and the window title
# comes up as just "e - Windows..." with no file name; then the biggest Afx:400000:b child is
# the 210x636 program tree instead of the 1707x743 code editor (see HANDOFF 3.8-12/13).
# Fixes here: (1) wait until e.exe is really gone, (2) poll until the title contains ".e",
# (3) sanity-check the editor width before shooting.
#
# usage: powershell -NoProfile -ExecutionPolicy Bypass -File tools\probe_bp2.ps1
param(
    [string]$Exe = "C:\Program Files (x86)\e\e.exe",
    [string]$File = "C:\Program Files (x86)\e\lib\krnln\samples\.e",
    [string]$OutDir = "D:\eclone\shots\live_bp",
    [int]$ClickX = 200,
    [int]$ClickY = 60
)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System; using System.Text; using System.Collections.Generic; using System.Runtime.InteropServices;
public class BP2 {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  [DllImport("user32.dll")] public static extern IntPtr SetFocus(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr a, int x, int y, int cx, int cy, uint f);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, IntPtr e);
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, IntPtr extra);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public static string Cls(IntPtr h){ var sb=new StringBuilder(128); GetClassNameW(h,sb,128); return sb.ToString(); }
  public static string Txt(IntPtr h){ var sb=new StringBuilder(512); GetWindowTextW(h,sb,512); return sb.ToString(); }
  public static IntPtr Frame(int pid){ IntPtr f=IntPtr.Zero;
    EnumWindows(delegate(IntPtr h, IntPtr q){ uint w=0; GetWindowThreadProcessId(h, out w);
      if (w==(uint)pid && IsWindowVisible(h) && Cls(h)=="ENewFrame") f=h; return true; }, IntPtr.Zero);
    return f; }
  public static IntPtr BiggestChild(IntPtr top, string prefix){ IntPtr best=IntPtr.Zero; long area=0;
    EnumChildWindows(top, delegate(IntPtr h, IntPtr q){ if (IsWindowVisible(h) && Cls(h).StartsWith(prefix)) {
      RECT r; GetWindowRect(h, out r); long a=(long)(r.Right-r.Left)*(r.Bottom-r.Top); if (a>area){area=a;best=h;} } return true; }, IntPtr.Zero);
    return best; }
  public static void Click(int x,int y){ SetCursorPos(x,y); System.Threading.Thread.Sleep(200);
    mouse_event(2,0,0,0,IntPtr.Zero); System.Threading.Thread.Sleep(70);
    mouse_event(4,0,0,0,IntPtr.Zero); System.Threading.Thread.Sleep(250); }
  public static void Key(byte vk){ keybd_event(vk,0,0,IntPtr.Zero); System.Threading.Thread.Sleep(90);
    keybd_event(vk,0,2,IntPtr.Zero); System.Threading.Thread.Sleep(300); }
}
"@

# --- stop any instance and WAIT until it is really gone (single-instance hand-off is the bug) ---
Start-Process -FilePath taskkill -ArgumentList '/f','/im','e.exe' -Wait -WindowStyle Hidden -ErrorAction SilentlyContinue
for ($i = 0; $i -lt 40; $i++) {
    if (-not (Get-Process e -ErrorAction SilentlyContinue)) { break }
    Start-Sleep -Milliseconds 250
}
Start-Sleep -Milliseconds 700

$p = Start-Process -FilePath $Exe -WorkingDirectory (Split-Path $Exe) -ArgumentList $File -PassThru
# --- poll until the document is really loaded (title contains ".e") ---
$ok = $false
for ($i = 0; $i -lt 60; $i++) {
    Start-Sleep -Milliseconds 700
    if ($p.HasExited) { Write-Host "exited early"; exit 1 }
    $f = [BP2]::Frame($p.Id)
    if ($f -ne [IntPtr]::Zero) {
        $t = [BP2]::Txt($f)
        if ($t -like '*.e *') { $ok = $true; break }
    }
}
if (-not $ok) { Write-Host "FAIL: the sample never loaded (title has no .e)"; Stop-Process -Id $p.Id -Force; exit 1 }
$script:frame = [BP2]::Frame($p.Id)
Write-Host ("frame=" + $script:frame + " title=" + [BP2]::Txt($script:frame))
[void][BP2]::BringWindowToTop($script:frame)
[void][BP2]::SetWindowPos($script:frame, [IntPtr]::Zero, 0, 0, 0, 0, 0x0001 -bor 0x0002 -bor 0x0040)
[void][BP2]::SetForegroundWindow($script:frame)
Start-Sleep -Milliseconds 900

$ed = [BP2]::BiggestChild($script:frame, 'Afx:400000:b')
$r = New-Object BP2+RECT; [void][BP2]::GetWindowRect($ed, [ref]$r)
$w = $r.Right-$r.Left; $h = $r.Bottom-$r.Top
Write-Host ("editor=" + $ed + " rect=" + $r.Left + "," + $r.Top + " " + $w + "x" + $h)
if ($w -lt 800) { Write-Host "FAIL: the code editor is not the biggest view (got $w x $h)"; Stop-Process -Id $p.Id -Force; exit 1 }

function Shot([string]$path) {
    $bmp = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $hdc = $g.GetHdc()
    [void][BP2]::PrintWindow($ed, $hdc, 0)
    $g.ReleaseHdc($hdc)
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    Write-Host ("shot -> " + $path)
}

Shot (Join-Path $OutDir "before.png")
[void][BP2]::SetFocus($ed)
[BP2]::Click($r.Left + $ClickX, $r.Top + $ClickY)
Start-Sleep -Milliseconds 400
Shot (Join-Path $OutDir "clicked.png")
[BP2]::Key(0x78)                                  # VK_F9 =  (#32855)
Start-Sleep -Milliseconds 600
Shot (Join-Path $OutDir "after_f9.png")
[BP2]::Key(0x78)                                  # 
Start-Sleep -Milliseconds 600
Shot (Join-Path $OutDir "after_f9_again.png")
Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
