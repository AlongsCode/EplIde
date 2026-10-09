# Smoke test for the code editor's mouse selection / clipboard / wheel:
#   1) a click puts the caret where you clicked (not always at end-of-line)
#   2) a mouse drag produces a real selection + the selected band is painted #000080
#   3) Ctrl+A selects everything, Ctrl+C puts the text on the clipboard
#   4) Ctrl+X cuts, Ctrl+V pastes it back (line count comes back)
#   5) a wheel notch changes the scroll offset
# ASCII-only on purpose (this machine reads .ps1 as ANSI).
param(
    [string]$Exe = "D:\eclone\bin\EClone.exe",
    [string]$OutDir = "D:\eclone\shots",
    [string]$Dump = "D:\eclone\shots\model_sel.txt"
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms

Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public class SS {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr a, int x, int y, int cx, int cy, uint f);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, IntPtr e);
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, IntPtr extra);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public static string Cls(IntPtr h){ var sb=new StringBuilder(256); GetClassNameW(h,sb,256); return sb.ToString(); }
  public static void Click(int x,int y){
    SetCursorPos(x,y); System.Threading.Thread.Sleep(200);
    mouse_event(2,0,0,0,IntPtr.Zero); System.Threading.Thread.Sleep(80);
    mouse_event(4,0,0,0,IntPtr.Zero); System.Threading.Thread.Sleep(250);
  }
  public static void Drag(int x0,int y0,int x1,int y1){
    SetCursorPos(x0,y0); System.Threading.Thread.Sleep(200);
    mouse_event(2,0,0,0,IntPtr.Zero); System.Threading.Thread.Sleep(120);
    int n=10;
    for(int i=1;i<=n;i++){ SetCursorPos(x0+(x1-x0)*i/n, y0+(y1-y0)*i/n); System.Threading.Thread.Sleep(35); }
    System.Threading.Thread.Sleep(200);
    mouse_event(4,0,0,0,IntPtr.Zero); System.Threading.Thread.Sleep(250);
  }
  public static void Ctrl(byte vk){
    keybd_event(0x11,0,0,IntPtr.Zero); System.Threading.Thread.Sleep(90);
    keybd_event(vk,0,0,IntPtr.Zero);   System.Threading.Thread.Sleep(90);
    keybd_event(vk,0,2,IntPtr.Zero);   System.Threading.Thread.Sleep(90);
    keybd_event(0x11,0,2,IntPtr.Zero); System.Threading.Thread.Sleep(200);
  }
  public static void KeyDown(byte vk){ keybd_event(vk,0,0,IntPtr.Zero); }
  public static void KeyUp(byte vk){ keybd_event(vk,0,2,IntPtr.Zero); }
}
"@

$fail = 0
function Check([string]$name, [bool]$ok) {
    if ($ok) { Write-Host ("[PASS] " + $name) }
    else { Write-Host ("[FAIL] " + $name); $script:fail = 1 }
}

$env:ECLONE_DUMP_MODEL = $Dump
$p = Start-Process -FilePath $Exe -WorkingDirectory (Split-Path $Exe) -PassThru
Start-Sleep -Seconds 5

$frame = [IntPtr]::Zero
for ($try = 0; $try -lt 20 -and $frame -eq [IntPtr]::Zero; $try++) {
    Start-Sleep -Seconds 1
    [SS]::EnumWindows([SS+EnumProc]{
        param($h,$q)
        $wpid = 0; [void][SS]::GetWindowThreadProcessId($h, [ref]$wpid)
        if ($wpid -eq $p.Id -and [SS]::Cls($h) -eq 'ENewFrame') { $script:frame = $h }
        return $true
    }, [IntPtr]::Zero) | Out-Null
}
$ed = [IntPtr]::Zero
[SS]::EnumChildWindows($frame, [SS+EnumProc]{
    param($h,$q)
    if ([SS]::Cls($h) -like 'Afx:400000:8:10005*') { $script:ed = $h }
    return $true
}, [IntPtr]::Zero) | Out-Null
if ($ed -eq [IntPtr]::Zero) { Write-Host "[FAIL] no editor window"; $p.Kill(); exit 1 }

[void][SS]::BringWindowToTop($frame)
[void][SS]::SetWindowPos($frame, [IntPtr]::Zero, 0, 0, 0, 0, 0x0001 -bor 0x0002 -bor 0x0040)
[void][SS]::SetForegroundWindow($frame)
Start-Sleep -Milliseconds 700

$fr = New-Object SS+RECT
[void][SS]::GetWindowRect($frame, [ref]$fr)
$er = New-Object SS+RECT
[void][SS]::GetWindowRect($ed, [ref]$er)

# The redump writes TWO files: the model (rows) and <same name>.fold.txt (scroll + caret).
$Fold = $Dump -replace '\.txt$', '.fold.txt'
function Redump() {
    [void][SS]::PostMessage($ed, 0x466, [IntPtr]::Zero, [IntPtr]::Zero)   # EC_WM_REDUMP
    Start-Sleep -Milliseconds 700
}
function ModelText() { return [System.IO.File]::ReadAllText($Dump, [System.Text.Encoding]::GetEncoding(936)) }
function FoldText()  { return [System.IO.File]::ReadAllText($Fold, [System.Text.Encoding]::GetEncoding(936)) }
function Dump() { Redump; return (FoldText) }
function Meta([string]$t) {
    $m = [regex]::Match($t, 'anchor=(-?\d+),(-?\d+) caret=(-?\d+),(-?\d+) lines=(\d+)')
    return @([int]$m.Groups[1].Value, [int]$m.Groups[2].Value, [int]$m.Groups[3].Value,
             [int]$m.Groups[4].Value, [int]$m.Groups[5].Value)
}
function FirstLine([string]$t) { return (($t -split "`n") | Where-Object { $_ -like 'scroll=*' } | Select-Object -First 1) }
function ScrollOf([string]$t) { return [int]([regex]::Match($t, 'scroll=(\d+)').Groups[1].Value) }

# PostMessage-based mouse input (client coords) -- works no matter which MDI page is on top,
# and is what tools\param_smoke.ps1 already uses.
$WM_MOUSEMOVE = 0x0200; $WM_LBUTTONDOWN = 0x0201; $WM_LBUTTONUP = 0x0202
function MkLp([int]$x, [int]$y) { return [IntPtr](($x -band 0xFFFF) -bor ($y -shl 16)) }
function ClickAt([int]$x, [int]$y) {
    [void][SS]::PostMessage($ed, $WM_MOUSEMOVE, [IntPtr]0, (MkLp $x $y))
    [void][SS]::PostMessage($ed, $WM_LBUTTONDOWN, [IntPtr]1, (MkLp $x $y))
    Start-Sleep -Milliseconds 120
    [void][SS]::PostMessage($ed, $WM_LBUTTONUP, [IntPtr]0, (MkLp $x $y))
    Start-Sleep -Milliseconds 400
}
function DragAt([int]$x0, [int]$y0, [int]$x1, [int]$y1) {
    [void][SS]::PostMessage($ed, $WM_MOUSEMOVE, [IntPtr]0, (MkLp $x0 $y0))
    [void][SS]::PostMessage($ed, $WM_LBUTTONDOWN, [IntPtr]1, (MkLp $x0 $y0))
    Start-Sleep -Milliseconds 150
    for ($i = 1; $i -le 8; $i++) {
        $x = $x0 + [int](($x1 - $x0) * $i / 8); $y = $y0 + [int](($y1 - $y0) * $i / 8)
        [void][SS]::PostMessage($ed, $WM_MOUSEMOVE, [IntPtr]1, (MkLp $x $y))
        Start-Sleep -Milliseconds 40
    }
    Start-Sleep -Milliseconds 150
    [void][SS]::PostMessage($ed, $WM_LBUTTONUP, [IntPtr]0, (MkLp $x1 $y1))
    Start-Sleep -Milliseconds 400
}
# Ctrl+<vk>: the injected Ctrl only lives in GetAsyncKeyState, so keep it down while the
# posted WM_KEYDOWN is being processed.
function CtrlKey([byte]$vk) {
    [SS]::KeyDown(0x11)
    Start-Sleep -Milliseconds 120
    [void][SS]::PostMessage($ed, 0x0100, [IntPtr]$vk, [IntPtr]1)
    Start-Sleep -Milliseconds 350
    [SS]::KeyUp(0x11)
    Start-Sleep -Milliseconds 250
}
function RowsOf([string]$t) {
    $out = @()
    foreach ($l in $t -split "`n") {
        $m = [regex]::Match($l, 'row\s+(\d+) kind=(\d+).*?y=\s*(\d+) h=\s*(\d+)')
        if ($m.Success) { $out += ,@([int]$m.Groups[1].Value, [int]$m.Groups[2].Value,
                                      [int]$m.Groups[3].Value, [int]$m.Groups[4].Value) }
    }
    return $out
}

$t0 = Dump
$meta0 = Meta $t0
$rows0 = RowsOf (ModelText)
# first code row (kind=1) in the view
$codeRow = $null
foreach ($r in $rows0) { if ($r[1] -eq 1) { $codeRow = $r; break } }
if ($codeRow -eq $null) { Write-Host "[FAIL] no code row in model"; $p.Kill(); exit 1 }
$cy = $codeRow[2] + [int]($codeRow[3] / 2) - $meta0[0]
Write-Host ("model: " + (FirstLine $t0))
Write-Host ("code row y=" + $codeRow[2] + " h=" + $codeRow[3] + " -> click client y=" + $cy)

# 1) click at client x=150 (text starts at 46) -> caret should land mid-line, not at end
ClickAt 150 $cy
$m1 = Meta (Dump)
Write-Host ("after click: " + (FirstLine (Dump)))
Check "click lands the caret inside the line (caretCol>1)" ($m1[3] -gt 1)

# 2) drag over that line -> a real selection
DragAt 60 $cy 240 $cy
$t2 = Dump
$m2 = Meta $t2
Write-Host ("after drag:  " + (FirstLine $t2))
Check "drag creates a selection (anchor<>caret)" (($m2[0] -ne $m2[2]) -or ($m2[1] -ne $m2[3]))

$bmp = New-Object System.Drawing.Bitmap ($er.Right-$er.Left), ($er.Bottom-$er.Top)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$hdc = $g.GetHdc()
[void][SS]::PrintWindow($ed, $hdc, 0)
$g.ReleaseHdc($hdc)
$png = Join-Path $OutDir "sel_drag.png"
$bmp.Save($png, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
# longest horizontal run of #000080 inside the row band (the selection bar); a bare
# keyword/paren is only 1-2 px wide, so require a real bar.
$best = 0
$im = [System.Drawing.Image]::FromFile($png)
$bm2 = New-Object System.Drawing.Bitmap $im
for ($y = $codeRow[2]; $y -lt ($codeRow[2] + 18); $y++) {
    $run = 0
    for ($x = 0; $x -lt $bm2.Width; $x++) {
        $c = $bm2.GetPixel($x, $y)
        if ($c.R -eq 0 -and $c.G -eq 0 -and $c.B -eq 128) { $run++; if ($run -gt $best) { $best = $run } }
        else { $run = 0 }
    }
}
$bm2.Dispose(); $im.Dispose()
Check ("selected band painted #000080 (longest bar = " + $best + " px > 24)") ($best -gt 24)

# 3) Ctrl+A then Ctrl+C
ClickAt 150 $cy
CtrlKey 0x41
$t3 = Dump
$m3 = Meta $t3
$lines = $m3[4]
Write-Host ("after ctrl+A: " + (FirstLine $t3))
Check "Ctrl+A selects the whole document" (($m3[0] -eq 0) -and ($m3[1] -eq 0) -and ($m3[2] -eq $lines - 1))
[System.Windows.Forms.Clipboard]::Clear()
CtrlKey 0x43
Start-Sleep -Milliseconds 400
$clip = [System.Windows.Forms.Clipboard]::GetText()
Check ("Ctrl+C puts the source on the clipboard (" + $clip.Length + " chars)") ($clip.Length -gt 40 -and $clip.Contains([char]0x4FE1))

# 4) Ctrl+X then Ctrl+V -> line count returns to the original
$before = $lines
CtrlKey 0x58
$m4 = Meta (Dump)
Write-Host ("after ctrl+X: " + (FirstLine (Dump)))
Check "Ctrl+X removes the selected lines" ($m4[4] -lt $before)
$cx = [System.Windows.Forms.Clipboard]::GetText()
Write-Host ("clipboard before paste: len=" + $cx.Length + " cr=" + ([regex]::Matches($cx, "`r").Count) + " lf=" + ([regex]::Matches($cx, "`n").Count))
CtrlKey 0x56
$m5 = Meta (Dump)
Write-Host ("after ctrl+V: " + (FirstLine (Dump)))
Check "Ctrl+V pastes them back" ($m5[4] -eq $before)

# 5) wheel
$s0 = ScrollOf (Dump)
$wp = [IntPtr]([int](-120) -shl 16)
$lp = [IntPtr]((150 -band 0xFFFF) -bor (300 -shl 16))
[void][SS]::PostMessage($ed, 0x020A, $wp, $lp)
Start-Sleep -Milliseconds 600
$t6 = Dump
$s1 = ScrollOf $t6
Write-Host ("after wheel: " + (FirstLine $t6))
Check ("mouse wheel scrolls (scroll " + $s0 + " -> " + $s1 + ")") ($s1 -gt $s0)

Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
if ($fail -ne 0) { Write-Host "[FAIL] sel_smoke"; exit 1 }
Write-Host "[PASS] selection / clipboard / wheel work"
