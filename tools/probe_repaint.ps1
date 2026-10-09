# Who paints the dot grid of the original window designer?
#   1. splash magenta over the design form's client area on the SCREEN (destroys the dots)
#   2. invalidate ONLY the form   -> capture  (dots back => the form's own WM_PAINT paints them)
#   3. invalidate ONLY its parent -> capture  (dots back => the parent paints under the child)
# ASCII only.
param(
    [string]$Exe = "C:\Program Files (x86)\e\e.exe",
    [string]$File = "D:\eclone\shots\e_samples\graphic.e",
    [string]$Class = "WTWindow",
    [string]$ExeArg = "",
    [string]$HideChild = "",
    [string]$OutDir = "D:\eclone\shots\repaint",
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
public class RP {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern IntPtr GetParent(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  public static string Text(IntPtr h){
    var sb=new StringBuilder(512); GetWindowTextW(h,sb,512);
    if (sb.Length > 0) return sb.ToString();
    IntPtr buf = Marshal.AllocHGlobal(2048);
    for (int i=0;i<1024;i++) Marshal.WriteInt16(buf, i*2, 0);
    SendMessage(h, 0x000D, (IntPtr)1024, buf);
    string s = Marshal.PtrToStringUni(buf);
    Marshal.FreeHGlobal(buf);
    return s == null ? "" : s;
  }
  // MDI-activate the document view that owns this window (no text matching needed:
  // walk up until the parent is the MDIClient).
  public static IntPtr ActivateAncestorView(IntPtr wnd) {
    IntPtr q = wnd, mdi = IntPtr.Zero;
    while (q != IntPtr.Zero) {
      IntPtr par = GetParent(q);
      if (par != IntPtr.Zero && Cls(par) == "MDIClient") { mdi = par; break; }
      q = par;
    }
    if (q == IntPtr.Zero || mdi == IntPtr.Zero) return IntPtr.Zero;
    SendMessage(mdi, 0x0222, q, IntPtr.Zero);
    return q;
  }
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [DllImport("user32.dll")] public static extern bool InvalidateRect(IntPtr h, IntPtr r, bool erase);
  [DllImport("user32.dll")] public static extern bool UpdateWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool RedrawWindow(IntPtr h, IntPtr r, IntPtr rg, uint flags);
  [DllImport("gdi32.dll")] public static extern IntPtr CreateSolidBrush(uint c);
  [DllImport("user32.dll")] public static extern IntPtr GetDC(IntPtr h);
  [DllImport("user32.dll")] public static extern int ReleaseDC(IntPtr h, IntPtr dc);
  [DllImport("user32.dll")] public static extern int FillRect(IntPtr dc, ref RECT r, IntPtr b);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
  public static IntPtr HideChildrenOfClass(IntPtr h, string cls) {
    IntPtr first = IntPtr.Zero;
    EnumChildWindows(h, delegate(IntPtr c, IntPtr q) {
      if (Cls(c) == cls) { ShowWindow(c, 0); if (first == IntPtr.Zero) first = c; }
      return true;
    }, IntPtr.Zero);
    return first;
  }
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
  public static string Cls(IntPtr h){ var sb=new StringBuilder(256); GetClassNameW(h,sb,256); return sb.ToString(); }
  public static RECT ClientOnScreen(IntPtr h) {
    RECT rc; GetClientRect(h, out rc);
    POINT p; p.X = 0; p.Y = 0; ClientToScreen(h, ref p);
    RECT o; o.Left = p.X; o.Top = p.Y; o.Right = p.X + rc.Right; o.Bottom = p.Y + rc.Bottom;
    return o;
  }
  public static void Splash(RECT r, uint color) {
    IntPtr dc = GetDC(IntPtr.Zero);
    IntPtr br = CreateSolidBrush(color);
    FillRect(dc, ref r, br);
    ReleaseDC(IntPtr.Zero, dc);
  }
  public static void Grab(RECT r, string png) {
    int w = r.Right - r.Left, h = r.Bottom - r.Top;
    if (w <= 0 || h <= 0) return;
    Bitmap b = new Bitmap(w, h, PixelFormat.Format24bppRgb);
    using (Graphics g = Graphics.FromImage(b)) {
      g.CopyFromScreen(r.Left, r.Top, 0, 0, new Size(w, h), CopyPixelOperation.SourceCopy);
    }
    b.Save(png, ImageFormat.Png);
    b.Dispose();
  }
}
"@

$args2 = @()
if ($File -ne "" -and $File -ne "none") { $args2 += $File }
if ($ExeArg -ne "") { $args2 += $ExeArg }
$p = Start-Process -FilePath $Exe -WorkingDirectory (Split-Path $Exe) -ArgumentList $args2 -PassThru
Start-Sleep -Seconds $WaitSec
if ($p.HasExited) { Write-Host "exited early"; exit 1 }

$form = [IntPtr]::Zero
[void][RP]::EnumWindows([RP+EnumProc]{
    param($h, $p2)
    $wpid = 0
    [void][RP]::GetWindowThreadProcessId($h, [ref]$wpid)
    if ($wpid -eq $p.Id) {
        [void][RP]::EnumChildWindows($h, [RP+EnumProc]{
            param($c, $q)
            if ($form -eq [IntPtr]::Zero -and [RP]::Cls($c) -eq $Class) { $script:form = $c }
            return $true
        }, [IntPtr]::Zero)
    }
    return $true
}, [IntPtr]::Zero)
if ($form -eq [IntPtr]::Zero) { Write-Host "form not found"; Stop-Process -Id $p.Id -Force; exit 1 }
# bring the designer document to the front, otherwise it is covered by the code page
$act = [RP]::ActivateAncestorView($form)
Write-Host ("activated=" + $act)
Start-Sleep -Seconds 2
$parent = [RP]::GetParent($form)
$rc = [RP]::ClientOnScreen($form)
Write-Host ("form=" + $form + " parent=" + $parent + " " + [RP]::Cls($parent) + " client-on-screen=" + $rc.Left + "," + $rc.Top + " " + ($rc.Right-$rc.Left) + "x" + ($rc.Bottom-$rc.Top))

# a small strip a few px inside the form, away from the border and any component
$probe = New-Object RP+RECT
$probe.Left = $rc.Left + 2; $probe.Top = $rc.Top + 2
$probe.Right = $rc.Left + 34; $probe.Bottom = $rc.Top + 26
$big = New-Object RP+RECT
$big.Left = $rc.Left; $big.Top = $rc.Top; $big.Right = $rc.Right; $big.Bottom = $rc.Bottom

[RP]::Grab($probe, "$OutDir\0_before.png")
[RP]::Splash($big, 0x00FF00FF)      # magenta, BGR
Start-Sleep -Milliseconds 250
[RP]::Grab($probe, "$OutDir\1_splashed.png")

[void][RP]::InvalidateRect($form, [IntPtr]::Zero, $true)
[void][RP]::UpdateWindow($form)
Start-Sleep -Milliseconds 500
[RP]::Grab($probe, "$OutDir\2_after_invalidate_form.png")

[RP]::Splash($big, 0x00FF00FF)
Start-Sleep -Milliseconds 250
[void][RP]::InvalidateRect($parent, [IntPtr]::Zero, $true)
[void][RP]::UpdateWindow($parent)
Start-Sleep -Milliseconds 500
[RP]::Grab($probe, "$OutDir\3_after_invalidate_parent.png")

if ($HideChild -ne "") {
    $hidden = [RP]::HideChildrenOfClass($form, $HideChild)
    Write-Host ("hid child " + $HideChild + " = " + $hidden)
    Start-Sleep -Milliseconds 400
    [RP]::Splash($big, 0x00FF00FF)
    Start-Sleep -Milliseconds 250
    [void][RP]::InvalidateRect($form, [IntPtr]::Zero, $true)
    [void][RP]::UpdateWindow($form)
    Start-Sleep -Milliseconds 500
    [RP]::Grab($probe, "$OutDir\3b_after_hide_child.png")
    if ($hidden -ne [IntPtr]::Zero) { [void][RP]::ShowWindow($hidden, 5) }
}

# restore
[RP]::Splash($big, 0x00FF00FF)
Start-Sleep -Milliseconds 200
[void][RP]::RedrawWindow($parent, [IntPtr]::Zero, [IntPtr]::Zero, 0x0001 -bor 0x0080 -bor 0x0100)  # INVALIDATE|ALLCHILDREN|UPDATENOW
Start-Sleep -Milliseconds 500
[RP]::Grab($probe, "$OutDir\4_restored.png")

Write-Host ("pid=" + $p.Id)
Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
