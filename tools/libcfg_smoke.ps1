# Smoke test for "Tools -> support library config" (command 32906) and DYNAMIC library
# load/unload.  ASCII only on purpose (this machine reads .ps1 as ANSI).
#
# What it asserts:
#   1) the dialog template matches the original e.exe RT_DIALOG id=321 (489x464 px,
#      controls 1115/1011/1036/1001/65535/6/3/4/5/1/2)
#   2) typing in the search box filters the list
#   3) checking a library + OK loads it at runtime  (support-library tree grows)
#   4) unchecking it + OK unloads it again          (tree back to baseline)
#
# usage: powershell -NoProfile -ExecutionPolicy Bypass -File libcfg_smoke.ps1 [-Lib iext2]
param(
    [string]$Exe = "D:\eclone\bin\EClone.exe",
    [string]$Lib = "iext2",
    [string]$Tag = "libcfg",
    [int]$WaitSec = 7
)
$ErrorActionPreference = 'Continue'
$root = "D:\eclone"
$fail = 0

Add-Type @"
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public class LC2 {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern int GetDlgCtrlID(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h, int id);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public static string Cls(IntPtr h){ var sb=new StringBuilder(128); GetClassNameW(h,sb,128); return sb.ToString(); }
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
  public static IntPtr frame = IntPtr.Zero;
  public static IntPtr dlg = IntPtr.Zero;
  public static List<string> Find(int pid) {
    frame = IntPtr.Zero; dlg = IntPtr.Zero;
    var o = new List<string>();
    EnumWindows(delegate(IntPtr h, IntPtr p) {
      uint wpid = 0; GetWindowThreadProcessId(h, out wpid);
      if (wpid != (uint)pid) return true;
      string c = Cls(h);
      if (c == "ENewFrame") frame = h;
      if (c == "#32770" && IsWindowVisible(h)) dlg = h;
      return true;
    }, IntPtr.Zero);
    if (dlg != IntPtr.Zero) {
      RECT r; GetWindowRect(dlg, out r);
      o.Add(String.Format("DLG {0}x{1}", r.Right-r.Left, r.Bottom-r.Top));
      EnumChildWindows(dlg, delegate(IntPtr c, IntPtr p) {
        RECT cr; GetWindowRect(c, out cr);
        RECT dr; GetWindowRect(dlg, out dr);
        o.Add(String.Format("{0}|{1}|{2},{3}|{4}x{5}|{6}", Cls(c), GetDlgCtrlID(c),
              cr.Left-dr.Left, cr.Top-dr.Top, cr.Right-cr.Left, cr.Bottom-cr.Top, Text(c)));
        return true;
      }, IntPtr.Zero);
    }
    return o;
  }
  // NOTE: the target is a 32-bit process; a pointer allocated in this 64-bit host may be
  // above 4GB and useless to it.  So type the text with WM_CHAR, never WM_SETTEXT.
  public static bool SetText(IntPtr parent, int id, string s) {
    IntPtr h = GetDlgItem(parent, id);
    if (h == IntPtr.Zero) return false;
    SendMessage(h, 0x000C, IntPtr.Zero, IntPtr.Zero);            // WM_SETTEXT with NULL = clear
    for (int i = 0; i < s.Length; i++)
      SendMessage(h, 0x0102, (IntPtr)s[i], IntPtr.Zero);         // WM_CHAR
    SendMessage(parent, 0x0111, (IntPtr)((0x0300 << 16) | id), h);   // EN_CHANGE
    return true;
  }
  public static int ListCount(IntPtr parent, int id) {
    IntPtr h = GetDlgItem(parent, id);
    if (h == IntPtr.Zero) return -1;
    return (int)SendMessage(h, 0x018B, IntPtr.Zero, IntPtr.Zero);   // LB_GETCOUNT
  }
  // click item 0 of the list box (mouse messages go through the dialog's PreTranslateMessage).
  // y is computed from LB_GETITEMHEIGHT - no cross-process buffer needed.
  public static void ClickItem0(IntPtr parent, int id) {
    IntPtr h = GetDlgItem(parent, id);
    if (h == IntPtr.Zero) return;
    int ih = (int)SendMessage(h, 0x01A1, IntPtr.Zero, IntPtr.Zero);  // LB_GETITEMHEIGHT
    if (ih <= 0) ih = 16;
    int lp = ((ih / 2 + 1) << 16) | 12;
    SendMessage(h, 0x0201, (IntPtr)1, (IntPtr)lp);     // WM_LBUTTONDOWN
    PostMessage(h, 0x0202, IntPtr.Zero, (IntPtr)lp);   // WM_LBUTTONUP
  }
  public static void ClickButton(IntPtr parent, int id) {
    IntPtr h = GetDlgItem(parent, id);
    if (h != IntPtr.Zero) PostMessage(h, 0x00F5, IntPtr.Zero, IntPtr.Zero);   // BM_CLICK
  }
  public static int ListSel(IntPtr parent, int id) {
    IntPtr h = GetDlgItem(parent, id);
    if (h == IntPtr.Zero) return -2;
    return (int)SendMessage(h, 0x0188, IntPtr.Zero, IntPtr.Zero);   // LB_GETCURSEL
  }
  public static void DblClickItem0(IntPtr parent, int id) {
    IntPtr h = GetDlgItem(parent, id);
    if (h == IntPtr.Zero) return;
    int ih = (int)SendMessage(h, 0x01A1, IntPtr.Zero, IntPtr.Zero);  // LB_GETITEMHEIGHT
    if (ih <= 0) ih = 16;
    int lp = ((ih / 2 + 1) << 16) | 12;
    SendMessage(h, 0x0203, (IntPtr)1, (IntPtr)lp);     // WM_LBUTTONDBLCLK
    PostMessage(h, 0x0202, IntPtr.Zero, (IntPtr)lp);   // WM_LBUTTONUP
  }
}
"@

function TreeItems([string]$file) {
    if (-not (Test-Path $file)) { return -1 }
    $best = -1
    foreach ($m in Select-String -Path $file -Pattern 'items=(\d+)') {
        if ($m.Matches.Count -gt 0) {
            $n = [int]$m.Matches[0].Groups[1].Value
            if ($n -gt $best) { $best = $n }
        }
    }
    return $best
}

Get-Process EClone -ErrorAction SilentlyContinue | Stop-Process -Force
Remove-Item Env:\ECLONE_EXTRA_LIBS -ErrorAction SilentlyContinue
$p = Start-Process -FilePath $Exe -WorkingDirectory (Split-Path $Exe) -PassThru
Start-Sleep -Seconds $WaitSec
if ($p.HasExited) { Write-Host "[FAIL] EClone exited early"; exit 1 }

& "$root\tools\treeinfo.exe" EClone.exe > "$root\shots\${Tag}_tree_base.txt" 2>&1
$base = TreeItems "$root\shots\${Tag}_tree_base.txt"
Write-Host "baseline support-library tree items = $base"

# --- open the dialog
[void][LC2]::Find($p.Id)
if ([LC2]::frame -eq [IntPtr]::Zero) { Write-Host "[FAIL] no ENewFrame"; Stop-Process -Id $p.Id -Force; exit 1 }
[void][LC2]::PostMessage([LC2]::frame, 0x111, [IntPtr]32906, [IntPtr]::Zero)
Start-Sleep -Seconds 2

$dump = [LC2]::Find($p.Id)
$dump | Set-Content -Path "$root\shots\$Tag.txt" -Encoding UTF8
if ([LC2]::dlg -eq [IntPtr]::Zero) { Write-Host "[FAIL] support library config dialog did not open"; Stop-Process -Id $p.Id -Force; exit 1 }

$d = $dump -join "`n"
# 1) geometry / control set must match the original RT_DIALOG id=321
if ($d -match 'DLG 489x464') { Write-Host "[PASS] dialog size 489x464 px (original template 322x290 DLG)" }
else { Write-Host "[FAIL] dialog size wrong: $($dump[0])"; $fail++ }
$wanted = @(1115,1011,1036,1001,65535,6,3,4,5,1,2)
$missing = @()
foreach ($id in $wanted) { if ($d -notmatch "\|$id\|") { $missing += $id } }
if ($missing.Count -eq 0) { Write-Host "[PASS] all 11 original controls present (1115/1011/1036/1001/65535/6/3/4/5/1/2)" }
else { Write-Host "[FAIL] missing control ids: $($missing -join ',')"; $fail++ }

$n0 = [LC2]::ListCount([LC2]::dlg, 1036)
Write-Host "list items (unfiltered) = $n0"
if ($n0 -ge 5) { Write-Host "[PASS] list shows the .fne files found in the lib dir ($n0 items)" }
else { Write-Host "[FAIL] list too short ($n0)"; $fail++ }

Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing @"
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
public class LC3 {
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h, int id);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public static void Shoot(IntPtr h, string png) {
    RECT r; GetWindowRect(h, out r);
    int w = r.Right-r.Left, hh = r.Bottom-r.Top;
    if (w <= 0 || hh <= 0) return;
    Bitmap b = new Bitmap(w, hh, PixelFormat.Format24bppRgb);
    using (Graphics g = Graphics.FromImage(b)) {
      IntPtr dc = g.GetHdc(); PrintWindow(h, dc, 0); g.ReleaseHdc(dc);
    }
    b.Save(png, ImageFormat.Png); b.Dispose();
  }
  // count dark pixels inside the check box of list item 0.
  // The list box sits at dialog (14,59); its 2px border puts item 0's 11x11 box at
  // dialog (19,63)-(29,73) - measured from shots\libcfg.png.
  public static int CheckInk(IntPtr dlg, string png) {
    Shoot(dlg, png);
    using (Bitmap b = new Bitmap(png)) {
      int n = 0;
      for (int y = 64; y <= 72; y++)
        for (int x = 20; x <= 28; x++) {
          if (x >= b.Width || y >= b.Height) continue;
          Color c = b.GetPixel(x, y);
          if (c.R < 90 && c.G < 90 && c.B < 90) n++;
        }
      return n;
    }
  }
}
"@
[LC3]::Shoot([LC2]::dlg, "$root\shots\$Tag.png")
Write-Host "dialog screenshot -> $root\shots\$Tag.png"

# 2) search filter
[void][LC2]::SetText([LC2]::dlg, 1011, $Lib)
Start-Sleep -Milliseconds 800
$n1 = [LC2]::ListCount([LC2]::dlg, 1036)
Write-Host "list items (filter '$Lib') = $n1"
if ($n1 -eq 1) { Write-Host "[PASS] search box filters the list (1 match for '$Lib')" }
else { Write-Host "[FAIL] search filter gave $n1 items"; $fail++ }

# 3) click item 0 -> the check box glyph must appear (this is the click-toggle feature)
$lst = [LC3]::GetDlgItem([LC2]::dlg, 1036)
$ink0 = [LC3]::CheckInk([LC2]::dlg, "$root\shots\${Tag}_box_unchecked.png")
[void][LC2]::ClickItem0([LC2]::dlg, 1036)
Start-Sleep -Milliseconds 500
$ink1 = [LC3]::CheckInk([LC2]::dlg, "$root\shots\${Tag}_box_checked.png")
$sel = [LC2]::ListSel([LC2]::dlg, 1036)
Write-Host "list cursel after click = $sel"
if ($ink1 -le 1) {
    # fall back: double click (LBN_DBLCLK path)
    [void][LC2]::DblClickItem0([LC2]::dlg, 1036)
    Start-Sleep -Milliseconds 500
    $ink1 = [LC3]::CheckInk([LC2]::dlg, "$root\shots\${Tag}_box_checked.png")
    Write-Host "(after double click) ink=$ink1"
}
Write-Host "checkbox ink before=$ink0 after=$ink1"
if ($ink0 -le 1 -and $ink1 -gt 3) { Write-Host "[PASS] clicking a list row toggles its check box" }
else { Write-Host "[FAIL] click did not toggle the check box (ink $ink0 -> $ink1)"; $fail++ }

# 4) press OK -> dynamic load
[void][LC2]::ClickButton([LC2]::dlg, 1)
Start-Sleep -Seconds 3
& "$root\tools\treeinfo.exe" EClone.exe > "$root\shots\${Tag}_tree_load.txt" 2>&1
$after = TreeItems "$root\shots\${Tag}_tree_load.txt"
Write-Host "after load: support-library tree items = $after"
if ($after -gt $base) { Write-Host "[PASS] library dynamically loaded at runtime (tree $base -> $after)" }
else { Write-Host "[FAIL] dynamic load did not take effect"; $fail++ }

# 4) open again, uncheck, OK -> dynamic unload
[void][LC2]::Find($p.Id)
[void][LC2]::PostMessage([LC2]::frame, 0x111, [IntPtr]32906, [IntPtr]::Zero)
Start-Sleep -Seconds 2
$dump2 = [LC2]::Find($p.Id)
if ([LC2]::dlg -ne [IntPtr]::Zero) {
    [void][LC2]::SetText([LC2]::dlg, 1011, $Lib)
    Start-Sleep -Milliseconds 800
    $n2 = [LC2]::ListCount([LC2]::dlg, 1036)
    $ink2 = [LC3]::CheckInk([LC2]::dlg, "$root\shots\${Tag}_box_reopened.png")
    Write-Host "reopened: filter matches=$n2, checkbox ink=$ink2"
    if ($n2 -eq 1 -and $ink2 -gt 3) { Write-Host "[PASS] the loaded library shows up checked when the dialog reopens" }
    else { Write-Host "[FAIL] reopened dialog does not reflect the loaded state (n=$n2 ink=$ink2)"; $fail++ }
    [void][LC2]::ClickItem0([LC2]::dlg, 1036)
    Start-Sleep -Milliseconds 400
    [void][LC2]::ClickButton([LC2]::dlg, 1)
    Start-Sleep -Seconds 3
    & "$root\tools\treeinfo.exe" EClone.exe > "$root\shots\${Tag}_tree_unload.txt" 2>&1
    $back = TreeItems "$root\shots\${Tag}_tree_unload.txt"
    Write-Host "after unload: support-library tree items = $back"
    if ($back -eq $base) { Write-Host "[PASS] library dynamically unloaded (tree back to $base)" }
    else { Write-Host "[FAIL] unload did not take effect (tree = $back, baseline $base)"; $fail++ }
} else {
    Write-Host "[FAIL] dialog did not reopen"; $fail++
}

$proc = Get-Process EClone -ErrorAction SilentlyContinue
if ($proc -ne $null) {
    if ($proc.MainWindowTitle.Length -gt 0) { Write-Host "[PASS] survived the whole load/unload cycle" }
    else { Write-Host "[FAIL] main window lost"; $fail++ }
    Stop-Process -Id $proc.Id -Force
} else {
    Write-Host "[FAIL] EClone died during dynamic load/unload"; $fail++
}

if ($fail -eq 0) { Write-Host "[PASS] libcfg_smoke: all checks passed" }
else { Write-Host "[FAIL] libcfg_smoke: $fail check(s) failed" }
exit $fail
