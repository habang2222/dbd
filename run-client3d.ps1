Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$Exe = Join-Path $RepoRoot "build-native\Debug\dbd_client3d.exe"

if (-not (Test-Path -LiteralPath $Exe)) {
    Write-Error "Missing dbd_client3d.exe. Run .\build-native.ps1 first."
}

Set-Location $RepoRoot
try {
    & $Exe
} catch {
    $Message = $_.Exception.Message
    if ($Message -like "*애플리케이션 제어 정책*" -or $Message -like "*application control policy*" -or $Message -like "*AppLocker*") {
        Write-Host ""
        Write-Host "DBDReboot executable launch was blocked by Windows application control policy." -ForegroundColor Yellow
        Write-Host "This is not a C++ build error. The exe exists, but Windows policy refused to start it." -ForegroundColor Yellow
        Write-Host "Run .\diagnose-run-policy.ps1 for the most recent AppLocker / Code Integrity events." -ForegroundColor Yellow
    }
    throw
}
