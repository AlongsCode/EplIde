# Open the original e.exe, run menu command 32876 (Tools -> System Config),
# then screenshot and enumerate the dialog's controls (class/text/id/rect).
# ASCII-only on purpose (this machine reads .ps1 as ANSI).
param(
    [string]$Exe = "C:\Program Files (x86)\e\e.exe",
    [string]$File = "",
    [string]$OutDir = "D:\eclone\shots\syscfg",
    [int]$WaitSec = 14,
    [int]$CmdId = 32876,
    [string]$DlgClass = "",
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
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern IntPtr GetParent(IntPtr h);
  [DllImport("user32.dll")] public static extern int GetWindowLong(IntPtr h, int i);
  [DllImport("user32.dll")] public static extern int GetDlgCtrlID(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern int SendMessageA(IntPtr h, uint m, int w, StringBuilder l);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public static string Text(IntPtr h){
    var sb=new StringBuilder(2048); GetWindowTextW(h,sb,2048);
    return sb.ToString();
  }
  public static string ComboText(IntPtr h, int i){
    var sb=new StringBuilder(1024);
    SendMessageA(h, 0x0148, i, sb);   // CB_GETLBTEXT
    return sb.ToString();
  }
  public static int ComboCount(IntPtr h){ return (int)SendMessage(h, 0x0146, IntPtr.Zero, IntPtr.Zero); } // CB_GETCOUNT
  public static int ComboSel(IntPtr h){ return (int)SendMessage(h, 0x0147, IntPtr.Zero, IntPtr.Zero); }   // CB_GETCURSEL
  public static int TabSel(IntPtr h){ return (int)SendMessage(h, 0x130B, IntPtr.Zero, IntPtr.Zero); }     // TCM_GETCURSEL
  public static string Cls(IntPtr h){ var sb=new StringBuilder(256); GetClassNameW(h,sb,256); return sb.ToString(); }
  public static void Dump(IntPtr top, List<string> o){
    IntPtr root = top;
    EnumChildWindows(top, delegate(IntPtr c, IntPtr p){
      int d = 0; IntPtr q = GetParent(c);
      while (q != IntPtr.Zero && q != root) { d++; q = GetParent(q); }
      RECT r; GetWindowRect(c, out r);
      RECT rt; GetWindowRect(root, out rt);
      o.Add(string.Format("{0}{1}|id={2}|vis={3}|{4},{5} {6}x{7}|style=0x{8:X8}|{9}",
          new string(' ', d*2), Cls(c), GetDlgCtrlID(c), IsWindowVisible(c),
          r.Left-rt.Left, r.Top-rt.Top, r.Right-r.Left, r.Bottom-r.Top,
          GetWindowLong(c,-16), Text(c)));
      return true;
    }, IntPtr.Zero);
  }
}
"@

$args2 = @()
if ($File -ne "") { $args2 += $File }
$p = Start-Process -FilePath $Exe -WorkingDirectory (Split-Path $Exe) -ArgumentList $args2 -PassThru
Start-Sleep -Seconds $WaitSec
if ($p.HasExited) { Write-Host "exited early"; exit 1 }

$list = New-Object System.Collections.Generic.List[object]
$cb = [LV+EnumProc]{
    param($h, $p2)
    $wpid = 0
    [void][LV]::GetWindowThreadProcessId($h, [ref]$wpid)
    if ($wpid -eq $p.Id -and [LV]::IsWindowVisible($h)) {
        $r = New-Object LV+RECT
        [void][LV]::GetWindowRect($h, [ref]$r)
        if (($r.Right-$r.Left) -gt 100) { $list.Add([pscustomobject]@{H=$h;C=[LV]::Cls($h);T=[LV]::Text($h)}) }
    }
    return $true
}
[void][LV]::EnumWindows($cb, [IntPtr]::Zero)
$frame = $list | Where-Object { $_.C -eq 'ENewFrame' } | Select-Object -First 1
if (-not $frame) { Write-Host "no frame"; if (-not $Keep) { $p.Kill() }; exit 1 }
Write-Host ("frame hwnd=" + $frame.H + " pid=" + $p.Id)

function Shot([string]$tag, [IntPtr]$hwnd) {
    $r = New-Object LV+RECT
    [void][LV]::GetWindowRect($hwnd, [ref]$r)
    $w = $r.Right-$r.Left; $h = $r.Bottom-$r.Top
    if ($w -le 0 -or $h -le 0) { return }
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

[void][LV]::SetForegroundWindow($frame.H)
Start-Sleep -Milliseconds 400
Shot "frame.png" $frame.H

# send the menu command to the frame
[void][LV]::SendMessage($frame.H, 0x0111, [IntPtr]$CmdId, [IntPtr]::Zero)
Start-Sleep -Seconds 3

# find the dialog: an owned visible top-level window of this process that is not the frame
$dlgs = New-Object System.Collections.Generic.List[object]
$cb2 = [LV+EnumProc]{
    param($h, $p2)
    $wpid = 0
    [void][LV]::GetWindowThreadProcessId($h, [ref]$wpid)
    if ($wpid -eq $p.Id -and [LV]::IsWindowVisible($h) -and $h -ne $frame.H) {
        $c = [LV]::Cls($h)
        if ($DlgClass -eq "" -or $c -eq $DlgClass) {
            $r = New-Object LV+RECT
            [void][LV]::GetWindowRect($h, [ref]$r)
            $dlgs.Add([pscustomobject]@{H=$h;C=$c;T=[LV]::Text($h);W=($r.Right-$r.Left);Ht=($r.Bottom-$r.Top)})
        }
    }
    return $true
}
[void][LV]::EnumWindows($cb2, [IntPtr]::Zero)
Write-Host ("dialogs=" + $dlgs.Count)
$lines = New-Object System.Collections.Generic.List[string]
foreach ($d in $dlgs) {
    Write-Host ("  dlg hwnd=" + $d.H + " class=" + $d.C + " text=" + $d.T + " " + $d.W + "x" + $d.Ht)
    $lines.Add("### hwnd=$($d.H) class=$($d.C) text=$($d.T) $($d.W)x$($d.Ht)")
    [LV]::Dump($d.H, $lines)
    Shot ("dlg_" + $d.C + ".png") $d.H
    # enumerate combo contents
    [LV]::EnumChildWindows($d.H, [LV+EnumProc]{
        param($c, $q)
        if ([LV]::Cls($c) -eq 'ComboBox') {
            $n = [LV]::ComboCount($c); $s = [LV]::ComboSel($c)
            $lines.Add("COMBO id=$([LV]::GetDlgCtrlID($c)) count=$n sel=$s")
            for ($i=0; $i -lt $n; $i++) { $lines.Add("   [$i] " + [LV]::ComboText($c,$i)) }
        }
        if ([LV]::Cls($c) -eq 'SysTabControl32') {
            $tc = [LV]::SendMessage($c, 0x1304, [IntPtr]::Zero, [IntPtr]::Zero)  # TCM_GETITEMCOUNT
            $lines.Add("TAB id=$([LV]::GetDlgCtrlID($c)) count=$tc sel=$([LV]::TabSel($c))")
            $buf = [Runtime.InteropServices.Marshal]::AllocHGlobal(512)
            for ($i=0; $i -lt $tc; $i++) {
                for ($k=0; $k -lt 256; $k++) { [Runtime.InteropServices.Marshal]::WriteInt16($buf, $k*2, 0) }
                [Runtime.InteropServices.Marshal]::WriteInt32($buf, 0, 0x0001 -bor 0x0004)  # TCIF_TEXT|TCIF_STATE
                [Runtime.InteropServices.Marshal]::WriteInt32($buf, 8, $buf.ToInt32() + 40)
                [void][LV]::SendMessage($c, 0x1305, [IntPtr]$i, $buf)  # TCM_GETITEM
                $lines.Add("   tab[$i] " + [Runtime.InteropServices.Marshal]::PtrToStringUni([Runtime.InteropServices.Marshal]::ReadInt32($buf, 8)))
            }
            [Runtime.InteropServices.Marshal]::FreeHGlobal($buf)
        }
        return $true
    }, [IntPtr]::Zero) | Out-Null
}
$lines | Set-Content -Path (Join-Path $OutDir "dlg.txt") -Encoding UTF8
Write-Host ("-> " + (Join-Path $OutDir "dlg.txt"))
if ($Keep) { Write-Host ("pid=" + $p.Id) } else { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }
