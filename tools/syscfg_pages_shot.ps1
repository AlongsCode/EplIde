# Shoot every page of OUR system-config dialog (real mouse clicks on the tabs) so the layout
# can be eyeballed against the original's shots\live_syscfg\rects.txt.
# usage: powershell -NoProfile -ExecutionPolicy Bypass -File tools\syscfg_pages_shot.ps1
param(
    [string]$Exe = "D:\eclone\bin\EClone.exe",
    [string]$OutDir = "D:\eclone\shots\syscfg",
    [int]$WaitSec = 7
)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing @"
using System;
using System.Text;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
public class SP {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern int GetDlgCtrlID(IntPtr h);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, IntPtr e);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("kernel32.dll")] public static extern IntPtr OpenProcess(uint a, bool b, uint pid);
  [DllImport("kernel32.dll")] public static extern IntPtr VirtualAllocEx(IntPtr p, IntPtr a, IntPtr sz, uint t, uint prot);
  [DllImport("kernel32.dll")] public static extern bool WriteProcessMemory(IntPtr p, IntPtr a, byte[] b, IntPtr n, out IntPtr w);
  [DllImport("kernel32.dll")] public static extern bool ReadProcessMemory(IntPtr p, IntPtr a, byte[] b, IntPtr n, out IntPtr r);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public static string Cls(IntPtr h){ var sb=new StringBuilder(128); GetClassNameW(h,sb,128); return sb.ToString(); }
  public static bool ItemRect(IntPtr tab, int idx, uint pid, out RECT r) {
    r = new RECT();
    IntPtr hp = OpenProcess(0x1F0FFF, false, pid);
    if (hp == IntPtr.Zero) return false;
    IntPtr mem = VirtualAllocEx(hp, IntPtr.Zero, (IntPtr)32, 0x1000, 0x04);
    if (mem == IntPtr.Zero) return false;
    IntPtr w;
    WriteProcessMemory(hp, mem, new byte[16], (IntPtr)16, out w);
    SendMessage(tab, 0x130A, (IntPtr)idx, mem);
    byte[] b = new byte[16]; IntPtr rd;
    ReadProcessMemory(hp, mem, b, (IntPtr)16, out rd);
    r.Left = BitConverter.ToInt32(b,0); r.Top = BitConverter.ToInt32(b,4);
    r.Right = BitConverter.ToInt32(b,8); r.Bottom = BitConverter.ToInt32(b,12);
    return (r.Right > r.Left);
  }
  public static void Shoot(IntPtr h, string png) {
    RECT r; GetWindowRect(h, out r);
    int w = r.Right-r.Left, hh = r.Bottom-r.Top;
    if (w <= 0 || hh <= 0) return;
    Bitmap b = new Bitmap(w, hh, PixelFormat.Format24bppRgb);
    using (Graphics g = Graphics.FromImage(b)) {
      IntPtr dc = g.GetHdc();
      PrintWindow(h, dc, 0);
      g.ReleaseHdc(dc);
    }
    b.Save(png, ImageFormat.Png); b.Dispose();
  }
}
"@
Get-Process EClone -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 400
$p = Start-Process -FilePath $Exe -PassThru
Start-Sleep -Seconds $WaitSec
if ($p.HasExited) { Write-Host "exited early"; exit 1 }

$script:procId = $p.Id; $script:frame = [IntPtr]::Zero
[void][SP]::EnumWindows([SP+EnumProc]{
    param($h,$q)
    $wpid = 0; [void][SP]::GetWindowThreadProcessId($h, [ref]$wpid)
    if ($wpid -eq $script:procId -and [SP]::Cls($h) -eq 'ENewFrame') { $script:frame = $h }
    return $true
}, [IntPtr]::Zero)
[void][SP]::PostMessage($script:frame, 0x0111, [IntPtr]32876, [IntPtr]::Zero)
Start-Sleep -Seconds 3

$script:dlg = [IntPtr]::Zero
[void][SP]::EnumWindows([SP+EnumProc]{
    param($h,$q)
    $wpid = 0; [void][SP]::GetWindowThreadProcessId($h, [ref]$wpid)
    if ($wpid -eq $script:procId -and [SP]::IsWindowVisible($h) -and [SP]::Cls($h) -eq '#32770') { $script:dlg = $h }
    return $true
}, [IntPtr]::Zero)
if ($script:dlg -eq [IntPtr]::Zero) { Stop-Process -Id $p.Id -Force; Write-Host "no dialog"; exit 1 }
$script:tab = [IntPtr]::Zero
[void][SP]::EnumChildWindows($script:dlg, [SP+EnumProc]{
    param($c,$q)
    if ([SP]::Cls($c) -eq 'SysTabControl32') { $script:tab = $c }
    return $true
}, [IntPtr]::Zero)

$tr = New-Object SP+RECT
[void][SP]::GetWindowRect($script:tab, [ref]$tr)
for ($k = 0; $k -lt 7; $k++) {
    $ir = New-Object SP+RECT
    if ([SP]::ItemRect($script:tab, $k, $p.Id, [ref]$ir)) {
        $x = $tr.Left + [int](($ir.Left + $ir.Right) / 2)
        $y = $tr.Top + [int](($ir.Top + $ir.Bottom) / 2)
        [void][SP]::SetForegroundWindow($script:dlg)
        [void][SP]::SetCursorPos($x, $y)
        Start-Sleep -Milliseconds 120
        [SP]::mouse_event(0x0002, 0, 0, 0, [IntPtr]::Zero)
        Start-Sleep -Milliseconds 60
        [SP]::mouse_event(0x0004, 0, 0, 0, [IntPtr]::Zero)
        Start-Sleep -Milliseconds 500
    }
    [SP]::Shoot($script:dlg, (Join-Path $OutDir ("mine_page" + $k + ".png")))
    Write-Host ("mine_page" + $k + ".png")
}
Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
