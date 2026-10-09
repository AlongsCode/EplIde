# Open the ORIGINAL IDE's "Tools -> support library config" dialog (command id 32906),
# shoot it 1:1 and dump every child control with class/text/rect/id.
# ASCII only.
param(
    [string]$Exe = "C:\Program Files (x86)\e\e.exe",
    [string]$File = "D:\eclone\shots\e_samples\graphic.e",
    [int]$Cmd = 32906,
    [string]$OutDir = "D:\eclone\shots\live_libcfg",
    [int]$WaitSec = 12
)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing @"
using System;
using System.Text;
using System.Drawing;
using System.Drawing.Imaging;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public class LC {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern int GetDlgCtrlID(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public static string Cls(IntPtr h){ var sb=new StringBuilder(256); GetClassNameW(h,sb,256); return sb.ToString(); }
  public static string Text(IntPtr h){
    var sb=new StringBuilder(1024); GetWindowTextW(h,sb,1024);
    if (sb.Length > 0) return sb.ToString();
    IntPtr buf = Marshal.AllocHGlobal(4096);
    for (int i=0;i<2048;i++) Marshal.WriteInt16(buf, i*2, 0);
    SendMessage(h, 0x000D, (IntPtr)2048, buf);
    string s = Marshal.PtrToStringUni(buf);
    Marshal.FreeHGlobal(buf);
    return s == null ? "" : s;
  }
  public static void Dump(IntPtr top, List<string> o, int depth) {
    RECT r; GetWindowRect(top, out r);
    o.Add(new string(' ', depth*2) + string.Format("{0,-24} id={1,-6} vis={2,-6} {3},{4} {5}x{6}  {7}",
        Cls(top), GetDlgCtrlID(top), IsWindowVisible(top), r.Left, r.Top,
        r.Right-r.Left, r.Bottom-r.Top, Text(top)));
    EnumChildWindows(top, delegate(IntPtr c, IntPtr p) {
      Dump2(c, o, depth + 1);
      return true;
    }, IntPtr.Zero);
  }
  public static void Dump2(IntPtr c, List<string> o, int depth) {
    RECT r; GetWindowRect(c, out r);
    o.Add(new string(' ', depth*2) + string.Format("{0,-24} id={1,-6} vis={2,-6} {3},{4} {5}x{6}  {7}",
        Cls(c), GetDlgCtrlID(c), IsWindowVisible(c), r.Left, r.Top,
        r.Right-r.Left, r.Bottom-r.Top, Text(c)));
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
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
}
"@

$p = Start-Process -FilePath $Exe -WorkingDirectory (Split-Path $Exe) -ArgumentList @($File) -PassThru
Start-Sleep -Seconds $WaitSec
if ($p.HasExited) { Write-Host "exited early"; exit 1 }

$frame = [IntPtr]::Zero
[void][LC]::EnumWindows([LC+EnumProc]{
    param($h, $p2)
    $wpid = 0
    [void][LC]::GetWindowThreadProcessId($h, [ref]$wpid)
    if ($wpid -eq $p.Id -and [LC]::Cls($h) -eq 'ENewFrame') { $script:frame = $h }
    return $true
}, [IntPtr]::Zero)
Write-Host ("frame=" + $frame)
[void][LC]::PostMessage($frame, 0x111, [IntPtr]$Cmd, [IntPtr]::Zero)
Start-Sleep -Seconds 3

$dlgs = New-Object System.Collections.Generic.List[IntPtr]
[void][LC]::EnumWindows([LC+EnumProc]{
    param($h, $p2)
    $wpid = 0
    [void][LC]::GetWindowThreadProcessId($h, [ref]$wpid)
    if ($wpid -eq $p.Id -and [LC]::IsWindowVisible($h)) {
        $c = [LC]::Cls($h)
        if ($c -eq '#32770') { $dlgs.Add($h) }
    }
    return $true
}, [IntPtr]::Zero)
Write-Host ("dialogs=" + $dlgs.Count)
$lines = New-Object System.Collections.Generic.List[string]
$i = 0
foreach ($d in $dlgs) {
    [LC]::Dump($d, $lines, 0)
    [LC]::Shoot($d, "$OutDir\dlg$i.png")
    Write-Host ("shot dlg$i.png")
    $i++
}
$lines | Set-Content -Path "$OutDir\dialog.txt" -Encoding UTF8
Write-Host ("pid=" + $p.Id)
Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
