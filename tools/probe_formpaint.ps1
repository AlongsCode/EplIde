# Decisive experiment: does the design form paint its own background + dot grid, or
# is it transparent (NULL_BRUSH) and the dots belong to the designer view behind it?
#
# We pre-fill a memory DC with SENTINEL magenta, then send WM_PRINTCLIENT / WM_PRINT
# to the form and count the resulting colors.  Magenta left = the form painted nothing
# there; F0F0F0 + black dots = the form painted them itself.
# ASCII only.
param(
    [string]$Exe = "C:\Program Files (x86)\e\e.exe",
    [string]$File = "D:\eclone\shots\e_samples\graphic.e",
    [string]$Class = "WTWindow",
    [string]$ExeArg = "",
    [string]$OutDir = "D:\eclone\shots\formpaint",
    [int]$WaitSec = 12
)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing @"
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public class FP {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern IntPtr GetParent(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern int GetWindowLong(IntPtr h, int i);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  public static string Chain(IntPtr h) {
    StringBuilder sb = new StringBuilder();
    IntPtr q = h; int n = 0;
    while (q != IntPtr.Zero && n < 8) {
      RECT r; GetWindowRect(q, out r);
      sb.Append(Cls(q) + "[" + r.Left + "," + r.Top + " " + (r.Right-r.Left) + "x" + (r.Bottom-r.Top)
                + " ex=0x" + GetWindowLong(q, -20).ToString("X") + "]");
      q = GetParent(q); n++;
      if (q != IntPtr.Zero) sb.Append("  <-  ");
    }
    return sb.ToString();
  }
  public static string Siblings(IntPtr h) {
    IntPtr par = GetParent(h);
    if (par == IntPtr.Zero) return "(no parent)";
    StringBuilder sb = new StringBuilder("siblings of " + Cls(h) + " under " + Cls(par) + ":\n");
    EnumChildWindows(par, delegate(IntPtr c, IntPtr p) {
      RECT r; GetWindowRect(c, out r);
      sb.Append("   " + Cls(c) + " vis=" + IsWindowVisible(c) + " " + r.Left + "," + r.Top + " "
                + (r.Right-r.Left) + "x" + (r.Bottom-r.Top) + " ex=0x" + GetWindowLong(c,-20).ToString("X") + "\n");
      return true;
    }, IntPtr.Zero);
    return sb.ToString();
  }
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("gdi32.dll")] public static extern IntPtr CreateCompatibleDC(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr GetDC(IntPtr h);
  [DllImport("user32.dll")] public static extern int ReleaseDC(IntPtr h, IntPtr dc);
  [DllImport("gdi32.dll")] public static extern IntPtr CreateCompatibleBitmap(IntPtr dc, int w, int h);
  [DllImport("gdi32.dll")] public static extern IntPtr SelectObject(IntPtr dc, IntPtr o);
  [DllImport("gdi32.dll")] public static extern bool DeleteDC(IntPtr dc);
  [DllImport("gdi32.dll")] public static extern bool DeleteObject(IntPtr o);
  [DllImport("gdi32.dll")] public static extern uint GetPixel(IntPtr dc, int x, int y);
  [DllImport("gdi32.dll")] public static extern uint SetPixel(IntPtr dc, int x, int y, uint c);
  [DllImport("gdi32.dll")] public static extern IntPtr CreateSolidBrush(uint c);
  [DllImport("user32.dll")] public static extern int FillRect(IntPtr dc, ref RECT r, IntPtr b);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public static string Cls(IntPtr h){ var sb=new StringBuilder(256); GetClassNameW(h,sb,256); return sb.ToString(); }
  public static string Run(IntPtr hwnd, uint msg, uint flags, string tag, string png) {
    RECT rc; GetClientRect(hwnd, out rc);
    int w = rc.Right, h = rc.Bottom;
    IntPtr scr = GetDC(IntPtr.Zero);
    IntPtr dc = CreateCompatibleDC(scr);
    IntPtr bmp = CreateCompatibleBitmap(scr, w, h);
    IntPtr old = SelectObject(dc, bmp);
    IntPtr br = CreateSolidBrush(0x00FF00FF);      // BGR magenta
    RECT full; full.Left = 0; full.Top = 0; full.Right = w; full.Bottom = h;
    FillRect(dc, ref full, br);
    SendMessage(hwnd, msg, dc, (IntPtr)flags);
    StringBuilder sb = new StringBuilder();
    sb.Append("-- " + tag + " msg=0x" + msg.ToString("X") + " flags=0x" + flags.ToString("X") + " " + w + "x" + h + "\n");
    // histogram
    var hist = new Dictionary<uint,int>();
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
      uint c = GetPixel(dc, x, y) & 0x00FFFFFF;
      if (!hist.ContainsKey(c)) hist[c] = 0;
      hist[c]++;
    }
    var keys = new List<uint>(hist.Keys);
    keys.Sort(delegate(uint a, uint b) { return hist[b].CompareTo(hist[a]); });
    for (int i = 0; i < keys.Count && i < 6; i++) {
      uint c = keys[i];
      sb.Append(String.Format("   #{0:X2}{1:X2}{2:X2} x{3}\n", c & 0xFF, (c >> 8) & 0xFF, (c >> 16) & 0xFF, hist[c]));
    }
    // save
    IntPtr hbmpSave = bmp;
    System.Drawing.Bitmap outb = System.Drawing.Image.FromHbitmap(hbmpSave);
    outb.Save(png, System.Drawing.Imaging.ImageFormat.Png);
    outb.Dispose();
    SelectObject(dc, old); DeleteObject(bmp); DeleteObject(br); DeleteDC(dc); ReleaseDC(IntPtr.Zero, scr);
    return sb.ToString();
  }
}
"@

$args2 = @()
if ($File -ne "" -and $File -ne "none") { $args2 += $File }
if ($ExeArg -ne "") { $args2 += $ExeArg }
$p = Start-Process -FilePath $Exe -WorkingDirectory (Split-Path $Exe) -ArgumentList $args2 -PassThru
Start-Sleep -Seconds $WaitSec
if ($p.HasExited) { Write-Host "exited early"; exit 1 }

$targets = New-Object System.Collections.Generic.List[IntPtr]
[void][FP]::EnumWindows([FP+EnumProc]{
    param($h, $p2)
    $wpid = 0
    [void][FP]::GetWindowThreadProcessId($h, [ref]$wpid)
    if ($wpid -eq $p.Id) {
        [void][FP]::EnumChildWindows($h, [FP+EnumProc]{
            param($c, $q)
            if ([FP]::Cls($c) -eq $Class) { $targets.Add($c) }
            return $true
        }, [IntPtr]::Zero)
    }
    return $true
}, [IntPtr]::Zero)
Write-Host ("targets=" + $targets.Count)
$i = 0
foreach ($t in $targets) {
    Write-Host ([FP]::Chain($t))
    Write-Host ([FP]::Siblings($t))
    Write-Host ([FP]::Run($t, 0x0318, 0x0000000C, "WM_PRINTCLIENT(CLIENT|ERASEBKGND)", "$OutDir\pc$i.png"))
    Write-Host ([FP]::Run($t, 0x0317, 0x0000001E, "WM_PRINT(CLIENT|ERASEBK|NONCLIENT|CHILDREN)", "$OutDir\pw$i.png"))
    $i++
}
Write-Host ("pid=" + $p.Id)
Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
