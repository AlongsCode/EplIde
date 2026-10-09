# Convert all UTF-8 sources under <SrcDir> into GBK copies under <DstDir>.
# VC6 (cl.exe / rc.exe) reads sources as ANSI(GBK), so Chinese literals must be GBK.
# ASCII-only on purpose.
param(
    [string]$SrcDir = "D:\eclone\src",
    [string]$DstDir = "D:\eclone\gbk"
)
$ErrorActionPreference = 'Stop'
$enc = [System.Text.Encoding]::GetEncoding(936)
$utf8 = New-Object System.Text.UTF8Encoding($false)
if (-not (Test-Path $DstDir)) { New-Item -ItemType Directory -Force -Path $DstDir | Out-Null }
$n = 0; $c = 0
$textExt = @('.cpp', '.h', '.rc', '.txt', '.def', '.bat')
Get-ChildItem -Path $SrcDir -Recurse -File | ForEach-Object {
    $rel = $_.FullName.Substring((Resolve-Path $SrcDir).Path.Length).TrimStart('\')
    $dst = Join-Path $DstDir $rel
    $dd = Split-Path $dst -Parent
    if (-not (Test-Path $dd)) { New-Item -ItemType Directory -Force -Path $dd | Out-Null }
    if ($textExt -contains $_.Extension.ToLower()) {
        $txt = [System.IO.File]::ReadAllText($_.FullName, $utf8)
        $txt = $txt.Replace([string][char]0xFEFF, "")   # 去掉 BOM，否则 RC/CL 会看到垃圾字符
        # VC6 的 RC.EXE 只认 CRLF：LF-only 的 .rc 会让它解析错乱（报 RC2135 file not found: DIALOG）
        $txt = $txt.Replace("`r`n", "`n").Replace("`n", "`r`n")
        [System.IO.File]::WriteAllText($dst, $txt, $enc)
        $n++
    } else {
        Copy-Item $_.FullName $dst -Force   # 二进制资源（.bmp 等）原样拷贝，千万别转码
        $c++
    }
}
Write-Host "converted $n text + copied $c binary files -> $DstDir"
