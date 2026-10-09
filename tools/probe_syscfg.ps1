# Open the ORIGINAL IDE's "Tools -> system config" dialog (command 32876) and read every
# SysTabControl32 page label. Optional -PerPage also dumps which controls are visible on
# each page (needs TCM_SETCURSEL to be honoured by the dialog).
# Cross-process TCITEM buffer (VirtualAllocEx) because in-proc TCM_GETITEM cannot marshal.
# ASCII only (no non-ASCII byte is allowed in .ps1 on this box).
param(
    [string]$Exe = "C:\Program Files (x86)\e\e.exe",
    [string]$File = "D:\eclone\shots\e_samples\graphic.e",
    [int]$Cmd = 32876,
    [string]$OutDir = "D:\eclone\shots\live_syscfg",
    [int]$WaitSec = 14,
    [switch]$PerPage
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
public class SC {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern int GetDlgCtrlID(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  [DllImport("kernel32.dll")] public static extern IntPtr OpenProcess(uint a, bool b, uint pid);
  [DllImport("kernel32.dll")] public static extern IntPtr VirtualAllocEx(IntPtr p, IntPtr a, IntPtr sz, uint t, uint prot);
  [DllImport("kernel32.dll")] public static extern bool VirtualFreeEx(IntPtr p, IntPtr a, IntPtr sz, uint t);
  [DllImport("kernel32.dll")] public static extern bool WriteProcessMemory(IntPtr p, IntPtr a, byte[] b, IntPtr n, out IntPtr w);
  [DllImport("kernel32.dll")] public static extern bool ReadProcessMemory(IntPtr p, IntPtr a, byte[] b, IntPtr n, out IntPtr r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
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
  public static string RectOf(IntPtr h) {
    RECT r; GetWindowRect(h, out r);
    return string.Format("{0},{1} {2}x{3}", r.Left, r.Top, r.Right-r.Left, r.Bottom-r.Top);
  }
  // TCM_GETITEMW = 0x133C ; TCIF_TEXT = 1
  // Allocate the remote TCITEM buffer ONCE: repeated VirtualAllocEx into a 32-bit target
  // starts returning NULL after a couple of calls (observed), so keep it and reuse it.
  public static IntPtr TabMem = IntPtr.Zero;
  public static IntPtr TabProc = IntPtr.Zero;
  public static string TabText(IntPtr tab, uint pid, int idx) {
    if (TabProc == IntPtr.Zero) {
      TabProc = OpenProcess(0x1F0FFF, false, pid);
      if (TabProc == IntPtr.Zero) return "<no OpenProcess>";
      TabMem = VirtualAllocEx(TabProc, IntPtr.Zero, (IntPtr)1024, 0x1000, 0x04);
      if (TabMem == IntPtr.Zero) return "<no VirtualAllocEx>";
    }
    byte[] item = new byte[32];
    BitConverter.GetBytes((uint)1).CopyTo(item, 0);                      // mask = TCIF_TEXT
    BitConverter.GetBytes((int)(TabMem.ToInt64() + 512)).CopyTo(item, 16); // pszText (32-bit target)
    BitConverter.GetBytes((int)100).CopyTo(item, 24);                    // cchTextMax
    IntPtr w;
    WriteProcessMemory(TabProc, TabMem, item, (IntPtr)32, out w);
    // TCM_GETITEMA = 0x1305 (the original's labels are ANSI/GBK; TCM_GETITEMW mis-marshals here)
    SendMessage(tab, 0x1305, (IntPtr)idx, TabMem);
    byte[] buf = new byte[300];
    IntPtr rd;
    ReadProcessMemory(TabProc, (IntPtr)(TabMem.ToInt64() + 512), buf, (IntPtr)300, out rd);
    System.Text.StringBuilder sb = new System.Text.StringBuilder();
    for (int q = 0; q < 299; q++) {
      if (buf[q] == 0) break;
      sb.Append((char)buf[q]);
    }
    return sb.ToString();
  }
  public static void TabFree() {
    if (TabProc != IntPtr.Zero && TabMem != IntPtr.Zero) VirtualFreeEx(TabProc, TabMem, IntPtr.Zero, 0x8000);
    TabMem = IntPtr.Zero; TabProc = IntPtr.Zero;
  }
  public static void DumpVisible(IntPtr top, List<string> o) {
    RECT r; GetWindowRect(top, out r);
    o.Add(string.Format("DIALOG {0},{1} {2}x{3}", r.Left, r.Top, r.Right-r.Left, r.Bottom-r.Top));
    EnumChildWindows(top, delegate(IntPtr c, IntPtr p) {
      if (!IsWindowVisible(c)) return true;
      RECT cr; GetWindowRect(c, out cr);
      o.Add(string.Format("  {0,-18} id={1,-6} rel={2},{3} {4}x{5}  |{6}|",
          Cls(c), GetDlgCtrlID(c), cr.Left-r.Left, cr.Top-r.Top,
          cr.Right-cr.Left, cr.Bottom-cr.Top, Text(c)));
      return true;
    }, IntPtr.Zero);
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

Get-Process e -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 2
$p = Start-Process -FilePath $Exe -WorkingDirectory (Split-Path $Exe) -ArgumentList @($File) -PassThru
Start-Sleep -Seconds $WaitSec
if ($p.HasExited) { Write-Host "exited early"; exit 1 }

$script:frame = [IntPtr]::Zero
[void][SC]::EnumWindows([SC+EnumProc]{
    param($h, $p2)
    $wpid = 0
    [void][SC]::GetWindowThreadProcessId($h, [ref]$wpid)
    if ($wpid -eq $p.Id -and [SC]::Cls($h) -eq 'ENewFrame') { $script:frame = $h }
    return $true
}, [IntPtr]::Zero)
Write-Host ("frame=" + $script:frame + " title=[" + [SC]::Text($script:frame) + "]")
if ($script:frame -eq [IntPtr]::Zero) { Stop-Process -Id $p.Id -Force; Write-Host "FAIL: no ENewFrame"; exit 1 }

[void][SC]::PostMessage($script:frame, 0x111, [IntPtr]$Cmd, [IntPtr]::Zero)
Start-Sleep -Seconds 3

# Collect visible top-level windows; the config dialog is the one that owns a SysTabControl32.
$script:tops = New-Object System.Collections.Generic.List[IntPtr]
[void][SC]::EnumWindows([SC+EnumProc]{
    param($h, $p2)
    $wpid = 0
    [void][SC]::GetWindowThreadProcessId($h, [ref]$wpid)
    if ($wpid -eq $p.Id -and [SC]::IsWindowVisible($h)) { $script:tops.Add($h) }
    return $true
}, [IntPtr]::Zero)

$script:dlg = [IntPtr]::Zero
$script:tab = [IntPtr]::Zero
$i = 0
foreach ($t in $script:tops) {
    $script:hasTab = [IntPtr]::Zero
    [void][SC]::EnumChildWindows($t, [SC+EnumProc]{
        param($h, $p2)
        if ([SC]::Cls($h) -eq 'SysTabControl32') { $script:hasTab = $h }
        return $true
    }, [IntPtr]::Zero)
    Write-Host ("top[$i] " + [SC]::Cls($t) + " " + [SC]::RectOf($t) + " tab=" + $script:hasTab + " [" + [SC]::Text($t) + "]")
    [SC]::Shoot($t, "$OutDir\top$i.png")
    if ($script:hasTab -ne [IntPtr]::Zero -and $script:dlg -eq [IntPtr]::Zero) {
        $script:dlg = $t
        $script:tab = $script:hasTab
    }
    $i++
}

$dlgH = [IntPtr]$script:dlg.ToInt64()
$tabH = [IntPtr]$script:tab.ToInt64()
Write-Host ("dlg=" + $dlgH.ToInt64() + " rect=" + [SC]::RectOf($dlgH) + " title=[" + [SC]::Text($dlgH) + "] tab=" + $tabH.ToInt64())
if ($dlgH -eq [IntPtr]::Zero) { Stop-Process -Id $p.Id -Force; Write-Host "FAIL: no dialog with a tab control"; exit 1 }

$count = [int][SC]::SendMessage($tabH, 0x1304, [IntPtr]::Zero, [IntPtr]::Zero)   # TCM_GETITEMCOUNT
Write-Host ("tabcount=" + $count)

$all = New-Object System.Collections.Generic.List[string]
for ($k = 0; $k -lt $count; $k++) {
    $name = [SC]::TabText($tabH, $p.Id, $k)
    $all.Add("==== page $k  label=[$name] ====")
    Write-Host ("page $k label=[" + $name + "]")
    if ($PerPage) {
        [void][SC]::SendMessage($tabH, 0x130C, [IntPtr]$k, [IntPtr]::Zero)   # TCM_SETCURSEL
        Start-Sleep -Milliseconds 500
        [SC]::DumpVisible($dlgH, $all)
        [SC]::Shoot($dlgH, "$OutDir\page$k.png")
    }
}
$all | Set-Content -Path "$OutDir\pages.txt" -Encoding UTF8
Write-Host ("pid=" + $p.Id + " wrote $OutDir\pages.txt")
Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
