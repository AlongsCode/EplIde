$ErrorActionPreference = 'Continue'
for ($k = 0; $k -lt 6; $k++) {
    $ps = @(Get-Process EClone -ErrorAction SilentlyContinue)
    if ($ps.Count -eq 0) { break }
    foreach ($q in $ps) {
        try { taskkill /f /pid $q.Id 2>&1 | Out-Null } catch { }
        try { $q.Kill() } catch { }
    }
    Start-Sleep -Milliseconds 700
}
$left = @(Get-Process EClone -ErrorAction SilentlyContinue)
Write-Host ("left=" + $left.Count)
$left | Select-Object Id | Format-Table -AutoSize
