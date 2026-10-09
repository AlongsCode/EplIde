# Capture the EClone editor view 1:1 with Ctrl+A applied, so the select-all painting can be
# diffed pixel by pixel against the original's shots\live_ctrla\after_ctrla.png.
#
# usage: powershell -File tools/sel_shot.ps1 [-Tag selA] [-WaitSec 7] [-Resize WxH]
param(
    [string]$Exe = "D:\eclone\bin\EClone.exe",
    [string]$OutDir = "D:\eclone\shots",
    [int]$WaitSec = 7,
    [string]$Tag = "selA",
    [string]$Resize = ""
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public class SA2 {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint f);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr h, int x, int y, int cx, int cy, bool repaint);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, IntPtr e);
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, IntPtr extra);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public static string Cls(IntPtr h){ var sb=new StringBuilder(128); GetClassNameW(h,sb,128); return sb.ToString(); }
  public static void Click(int x, int y){
    SetCursorPos(x,y); System.Threading.Thread.Sleep(250);
    mouse_event(2,0,0,0,IntPtr.Zero); System.Threading.Thread.Sleep(80);
    mouse_event(4,0,0,0,IntPtr.Zero); System.Threading.Thread.Sleep(350);
  }
  public static void CtrlA(){
    keybd_event(0x11,0,0,IntPtr.Zero); System.Threading.Thread.Sleep(150);
    keybd_event(0x41,0,0,IntPtr.Zero); System.Threading.Thread.Sleep(150);
    keybd_event(0x41,0,2,IntPtr.Zero); System.Threading.Thread.Sleep(150);
    keybd_event(0x11,0,2,IntPtr.Zero); System.Threading.Thread.Sleep(500);
  }
  public static void Shot(IntPtr h, string path){
    RECT r; GetWindowRect(h, out r);
    int w = r.Right-r.Left, hh = r.Bottom-r.Top;
    var bmp = new System.Drawing.Bitmap(w, hh);
    var g = System.Drawing.Graphics.FromImage(bmp);
    IntPtr hdc = g.GetHdc();
    PrintWindow(h, hdc, 0);
    g.ReleaseHdc(hdc);
    bmp.Save(path, System.Drawing.Imaging.ImageFormat.Png);
    g.Dispose(); bmp.Dispose();
  }
}
"@

Get-Process EClone -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 400
$p = Start-Process -FilePath $Exe -WorkingDirectory (Split-Path $Exe) -PassThru
Start-Sleep -Seconds $WaitSec
if ($p.HasExited) { Write-Host "FAIL: exited early"; exit 1 }

$script:procId = $p.Id; $script:frame = [IntPtr]::Zero
[void][SA2]::EnumWindows({
    param($h,$q)
    $wpid = 0; [void][SA2]::GetWindowThreadProcessId($h, [ref]$wpid)
    if ($wpid -eq $script:procId -and [SA2]::Cls($h) -eq 'ENewFrame') { $script:frame = $h }
    return $true
}, [IntPtr]::Zero)
if ($script:frame -eq [IntPtr]::Zero) { Write-Host "FAIL: no frame"; Stop-Process -Id $p.Id -Force; exit 1 }

if ($Resize -ne "") {
    $parts = $Resize -split 'x'
    [void][SA2]::MoveWindow($script:frame, 100, 60, [int]$parts[0], [int]$parts[1], $true)
    Start-Sleep -Milliseconds 900
}

$script:ed = [IntPtr]::Zero
[void][SA2]::EnumChildWindows($script:frame, {
    param($h,$q)
    if ([SA2]::Cls($h).StartsWith('Afx:400000:8:10005')) { $script:ed = $h; return $false }
    return $true
}, [IntPtr]::Zero)
if ($script:ed -eq [IntPtr]::Zero) { Write-Host "FAIL: no editor"; Stop-Process -Id $p.Id -Force; exit 1 }

$er = New-Object SA2+RECT
[void][SA2]::GetWindowRect($script:ed, [ref]$er)
Write-Host ("editor rect " + $er.Left + "," + $er.Top + " " + ($er.Right-$er.Left) + "x" + ($er.Bottom-$er.Top))

[void][SA2]::SetForegroundWindow($script:frame)
Start-Sleep -Milliseconds 600
# put the caret in the first code line, then select all
[SA2]::Click(($er.Left + 60), ($er.Top + 90))
[SA2]::CtrlA()
Start-Sleep -Milliseconds 400
[void][SA2]::PostMessage($script:ed, 0x0008, [IntPtr]::Zero, [IntPtr]::Zero)   # WM_KILLFOCUS: drop caret
Start-Sleep -Milliseconds 400
[SA2]::Shot($script:ed, (Join-Path $OutDir ($Tag + "_sel.png")))
Write-Host ("shot -> " + (Join-Path $OutDir ($Tag + "_sel.png")))

# and the untouched view for reference
Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
