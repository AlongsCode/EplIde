# ui_smoke.ps1 -- smoke test for the round-23 UI work.
# ASCII ONLY (this box reads .ps1 as ANSI/GBK; one non-ASCII byte breaks parsing).
# Chinese is built from code points with [char]0xXXXX.
#
# Asserts:
#   [1] toolbar / pane toggles   (#59392 standard bar, #129 work pane)
#   [2] new form / switch form   (#32831 adds a real support-library WTWindow, its size comes
#                                 from the system config, and Ctrl+Tab cycles the forms)
#   [3] table document pages     (#32817..#32827 switch the MDI document title)
#   [4] whole search             (#32934 dialog = original RT_DIALOG id=30773, and the result
#                                 really lands in the status pane "search 1")
#
# usage: powershell -NoProfile -ExecutionPolicy Bypass -File tools\ui_smoke.ps1
$ErrorActionPreference = "Stop"
$ROOT = "D:\eclone"
$EXE  = Join-Path $ROOT "bin\EClone.exe"
$FAIL = 0

function Check($name, $ok) {
    if ($ok) { Write-Host ("[PASS] " + $name) }
    else     { Write-Host ("[FAIL] " + $name); $script:FAIL = $script:FAIL + 1 }
}

Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public class UI {
  [DllImport("user32.dll")] public static extern IntPtr PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr l);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern int GetDlgCtrlID(IntPtr h);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h, int id);
  [DllImport("user32.dll")] public static extern IntPtr GetWindow(IntPtr h, uint cmd);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll", CharSet=CharSet.Auto)] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, string l);
  [DllImport("user32.dll", CharSet=CharSet.Auto)] public static extern int GetClassName(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet=CharSet.Auto)] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr SendMessageW(IntPtr h, uint m, IntPtr w, StringBuilder l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr SendMessageW(IntPtr h, uint m, IntPtr w, string l);
  [DllImport("user32.dll")] public static extern void keybd_event(byte bVk, byte bScan, uint dwFlags, IntPtr dwExtraInfo);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int l, t, r, b; }
  public delegate bool EnumProc(IntPtr h, IntPtr l);
}
"@

function Get-Frame([int]$thepid) {
    $script:frame = [IntPtr]::Zero
    $cb = [UI+EnumProc]{
        param($h, $l)
        $p = 0
        [void][UI]::GetWindowThreadProcessId($h, [ref]$p)
        if ($p -eq $thepid) {
            $cls = New-Object System.Text.StringBuilder 64
            [void][UI]::GetClassName($h, $cls, 64)
            if ($cls.ToString() -eq "ENewFrame") { $script:frame = $h }
        }
        return $true
    }
    [void][UI]::EnumWindows($cb, [IntPtr]::Zero)
    return $script:frame
}

function Get-ChildIds([IntPtr]$frame) {
    $map = @{}
    $cb = [UI+EnumProc]{
        param($h, $l)
        $id = [UI]::GetDlgCtrlID($h)
        if (-not $map.ContainsKey($id)) { $map[$id] = $h }
        return $true
    }
    [void][UI]::EnumChildWindows($frame, $cb, [IntPtr]::Zero)
    return $map
}

function Get-Visible([IntPtr]$h) {
    if ($h -eq [IntPtr]::Zero) { return $false }
    return [UI]::IsWindowVisible($h)
}

function Get-Title([IntPtr]$h) {
    $sb = New-Object System.Text.StringBuilder 512
    [void][UI]::GetWindowText($h, $sb, 512)
    return $sb.ToString()
}

function Send-Cmd([IntPtr]$frame, [int]$id) {
    [void][UI]::PostMessage($frame, 0x111, [IntPtr]$id, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 700
}

function Get-Windows([IntPtr]$frame, [string]$cls) {
    $list = New-Object System.Collections.ArrayList
    $cb = [UI+EnumProc]{
        param($h, $l)
        $c = New-Object System.Text.StringBuilder 64
        [void][UI]::GetClassName($h, $c, 64)
        if ($c.ToString() -eq $cls) {
            [void]$list.Add([pscustomobject]@{ hwnd = $h; vis = [UI]::IsWindowVisible($h); title = (Get-Title $h) })
        }
        return $true
    }
    [void][UI]::EnumChildWindows($frame, $cb, [IntPtr]::Zero)
    return $list
}

function Get-Dialog([int]$thepid, [int]$wantW, [int]$wantH) {
    $script:dlg = [IntPtr]::Zero
    $cb = [UI+EnumProc]{
        param($h, $l)
        $p = 0
        [void][UI]::GetWindowThreadProcessId($h, [ref]$p)
        if ($p -eq $thepid) {
            $c = New-Object System.Text.StringBuilder 64
            [void][UI]::GetClassName($h, $c, 64)
            if ($c.ToString() -eq "#32770") {
                $r = New-Object UI+RECT
                [void][UI]::GetWindowRect($h, [ref]$r)
                if (($r.r - $r.l) -eq $wantW -and ($r.b - $r.t) -eq $wantH) { $script:dlg = $h }
            }
        }
        return $true
    }
    [void][UI]::EnumWindows($cb, [IntPtr]::Zero)
    return $script:dlg
}

# ---- start EClone ----------------------------------------------------------
Get-Process EClone -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 500
$proc = Start-Process -FilePath $EXE -PassThru
Start-Sleep -Seconds 3
$thepid = $proc.Id
$frame = Get-Frame $thepid
Check "EClone frame (ENewFrame) is up  pid=$thepid" ($frame -ne [IntPtr]::Zero)
if ($frame -eq [IntPtr]::Zero) { exit 1 }

# ============================ [1] bar toggles ==============================
$ids = Get-ChildIds $frame
Check "control bars found (59392 standard / 129 work pane / 130 status pane)" `
      ($ids.ContainsKey(59392) -and $ids.ContainsKey(129) -and $ids.ContainsKey(130))

$stdBefore = Get-Visible $ids[59392]
Send-Cmd $frame 59392
$ids = Get-ChildIds $frame
$stdAfter = Get-Visible $ids[59392]
Check ("menu #59392 toggles the standard toolbar (visible $stdBefore -> $stdAfter)") ($stdAfter -ne $stdBefore)
Send-Cmd $frame 59392
$ids = Get-ChildIds $frame
Check "menu #59392 toggles it back" ((Get-Visible $ids[59392]) -eq $stdBefore)

$workBefore = Get-Visible $ids[129]
Send-Cmd $frame 129
$ids = Get-ChildIds $frame
$workAfter = Get-Visible $ids[129]
Check ("menu #129 toggles the work pane (visible $workBefore -> $workAfter)") ($workAfter -ne $workBefore)
Send-Cmd $frame 129
$ids = Get-ChildIds $frame
Check "menu #129 toggles it back" ((Get-Visible $ids[129]) -eq $workBefore)

# ======================= [2] new form / switch form ========================
# Runs FIRST on purpose: the designer document page is only current at startup (see the header).
# NOTE: the form captions are Chinese and this project is a non-Unicode build, so reading them
#       back across processes gives mojibake -> assert counts / visibility / geometry only.
Send-Cmd $frame 32831
$forms = Get-Windows $frame "WTWindow"
$vis1 = @($forms | Where-Object { $_.vis }).Count
Check ("menu #32831 created a second real support-library form (WTWindow count=" + $forms.Count + ")") ($forms.Count -ge 2)
Check ("exactly one form window is visible after creating a new one (visible=" + $vis1 + ")") ($vis1 -eq 1)

$newForm = $forms | Where-Object { $_.vis } | Select-Object -First 1
$wr = New-Object UI+RECT
[void][UI]::GetWindowRect($newForm.hwnd, [ref]$wr)
Check ("the new form uses the configured default size 380x250 (got " + ($wr.r - $wr.l) + "x" + ($wr.b - $wr.t) + ")") `
      (($wr.r - $wr.l) -eq 380 -and ($wr.b - $wr.t) -eq 250)

Send-Cmd $frame 32831
$forms = Get-Windows $frame "WTWindow"
$vis2 = @($forms | Where-Object { $_.vis }).Count
Check ("a third form was created (WTWindow count=" + $forms.Count + ")") ($forms.Count -ge 3)
Check ("still exactly one visible form (visible=" + $vis2 + ")") ($vis2 -eq 1)

# switch form: Ctrl+Tab cycles the designer's forms (CECChildFrame::PreTranslateMessage).
# form 2 is visible now; one Ctrl+Tab must make form 0 visible (2 -> 0 wraps).
$script:mdiChild = $null
[void][UI]::EnumChildWindows($frame, {
    param($h,$q)
    $c = New-Object System.Text.StringBuilder 64
    [void][UI]::GetClassName($h, $c, 64)
    if ($c.ToString() -eq 'AfxMDIFrame42s') { $script:mdiChild = $h }
    return $true
}, [IntPtr]::Zero)
Check "found the MDI child frame" ($script:mdiChild -ne $null)

if ($script:mdiChild -ne $null) {
    $before = Get-Windows $frame "WTWindow"
    $visBefore = ($before | Where-Object { $_.vis } | Select-Object -First 1).hwnd

    [UI]::keybd_event(0x11, 0, 0, [IntPtr]::Zero)          # Ctrl down
    Start-Sleep -Milliseconds 150
    [void][UI]::PostMessage($script:mdiChild, 0x0100, [IntPtr]0x09, [IntPtr]::Zero)   # VK_TAB
    Start-Sleep -Milliseconds 900
    [UI]::keybd_event(0x11, 0, 2, [IntPtr]::Zero)          # Ctrl up
    Start-Sleep -Milliseconds 300

    $after = Get-Windows $frame "WTWindow"
    $visAfter = @($after | Where-Object { $_.vis })
    Check ("after Ctrl+Tab exactly one form is visible (visible=" + $visAfter.Count + ")") ($visAfter.Count -eq 1)
    Check ("Ctrl+Tab switched to a different form (" + $visBefore + " -> " + $visAfter[0].hwnd + ")") ($visAfter[0].hwnd -ne $visBefore)

    [UI]::keybd_event(0x11, 0, 0, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 150
    [void][UI]::PostMessage($script:mdiChild, 0x0100, [IntPtr]0x09, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 900
    [UI]::keybd_event(0x11, 0, 2, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 300
    $after2 = Get-Windows $frame "WTWindow"
    $visAfter2 = @($after2 | Where-Object { $_.vis })
    Check "still exactly one visible form after the second Ctrl+Tab" ($visAfter2.Count -eq 1)
    Check "the second Ctrl+Tab moved to another form again" ($visAfter2[0].hwnd -ne $visAfter[0].hwnd)
}

# ======================= [3] table document pages ==========================
# command id -> expected document title (from EcDocTitleText / EcDocTabText)
$tbl = @(
    @(32817, 0x81EA, 0x5B9A, 0x4E49, 0x6570, 0x636E, 0x7C7B, 0x578B, 0x8868),  # custom data type table
    @(32818, 0x5168, 0x5C40, 0x53D8, 0x91CF, 0x8868),                          # global variable table
    @(32819, 0x0044, 0x006C, 0x006C, 0x547D, 0x4EE4, 0x5B9A, 0x4E49, 0x8868),  # Dll command table
    @(32825, 0x5E38, 0x91CF, 0x6570, 0x636E, 0x8868),                          # constant table
    @(32826, 0x56FE, 0x7247, 0x8D44, 0x6E90, 0x8868),                          # image resource table
    @(32827, 0x58F0, 0x97F3, 0x8D44, 0x6E90, 0x8868)                           # sound resource table
)
foreach ($row in $tbl) {
    $cmd = $row[0]
    $want = -join ($row[1..($row.Count - 1)] | ForEach-Object { [char]$_ })
    Send-Cmd $frame $cmd
    $t = Get-Title $frame
    Check ("menu #" + $cmd + " opens the page '" + $want + "'  (title='" + $t + "')") ($t.Contains($want))
}

# =========================== [4] whole search ==============================
Send-Cmd $frame 32934
$dlg = Get-Dialog $thepid 387 274
Check "menu #32934 opens a #32770 dialog of the id=30773 template size (387x274)" ($dlg -ne [IntPtr]::Zero)
if ($dlg -ne [IntPtr]::Zero) {
    $wantTitle = -join (0x6574,0x4F53,0x641C,0x5BFB,0x5BF9,0x8BDD,0x6846 | ForEach-Object { [char]$_ })
    Check ("its title is the original's ('" + $wantTitle + "')") ((Get-Title $dlg) -eq $wantTitle)

    foreach ($id in @(1151, 1152, 1041, 1042, 1101, 1102, 1047, 1, 2)) {
        $c = [UI]::GetDlgItem($dlg, $id)
        Check ("control id=" + $id + " exists") ($c -ne [IntPtr]::Zero)
    }
    $r1 = Get-Title ([UI]::GetDlgItem($dlg, 1101))
    $r2 = Get-Title ([UI]::GetDlgItem($dlg, 1102))
    $wantP1 = -join (0x201C,0x641C,0x5BFB,0x0031,0x201D,0x7CFB,0x7EDF,0x8F93,0x51FA,0x5939 | ForEach-Object { [char]$_ })
    $wantP2 = -join (0x201C,0x641C,0x5BFB,0x0032,0x201D,0x7CFB,0x7EDF,0x8F93,0x51FA,0x5939 | ForEach-Object { [char]$_ })
    Check ("radio 1101 text is the original's ('" + $wantP1 + "')") ($r1 -eq $wantP1)
    Check ("radio 1102 text is the original's ('" + $wantP2 + "')") ($r2 -eq $wantP2)

    # ---- functional: type a needle, press the default button, look in the pane ----
    # 1151 is a ComboBox -> WM_SETTEXT must go to its EDIT child (GetWindow GW_CHILD = 5).
    # Read back with WM_GETTEXT (GetWindowText* gives "" for these across processes).
    $ed = [UI]::GetDlgItem($dlg, 1151)
    $edit = [UI]::GetWindow($ed, 5)
    if ($edit -eq [IntPtr]::Zero) { $edit = $ed }
    $needle = "abc"
    [void][UI]::SendMessageW($edit, 0x000C, [IntPtr]::Zero, $needle)               # WM_SETTEXT
    $rb = New-Object System.Text.StringBuilder 64
    [void][UI]::SendMessageW($edit, 0x000D, [IntPtr]64, $rb)                       # WM_GETTEXT
    Check ("the combo edit took the needle ('" + $needle + "')  got='" + $rb.ToString() + "'") ($rb.ToString() -eq $needle)

    [void][UI]::PostMessage($dlg, 0x111, [IntPtr]1, [IntPtr]::Zero)                # IDOK
    Start-Sleep -Milliseconds 900
    Check "the dialog closed after pressing the search button" (-not (Get-Visible $dlg))

    # status pane page 5 = "search 1": its content control id is IDC_EDIT_OUTPUT + 5 = 1115
    $ids = Get-ChildIds $frame
    $len5 = 0
    if ($ids.ContainsKey(1115)) {
        $sb0 = New-Object System.Text.StringBuilder 4
        $len5 = [UI]::SendMessageW($ids[1115], 0x000E, [IntPtr]::Zero, $sb0).ToInt32()   # WM_GETTEXTLENGTH
    }
    Check ("the search result landed in the status pane 'search 1' (id=1115 text length=" + $len5 + ")") ($len5 -gt 0)
    Check "the status pane switched to the 'search 1' page" ((Get-Visible $ids[1115]))
}

# ---- shutdown --------------------------------------------------------------
Stop-Process -Id $thepid -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 500

if ($FAIL -eq 0) { Write-Host "[PASS] ui_smoke: all checks passed" }
else             { Write-Host ("[FAIL] ui_smoke: " + $FAIL + " check(s) failed") }
exit $FAIL
