Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

Write-Host "DBDReboot launch is currently blocked by Windows Smart App Control / Code Integrity." -ForegroundColor Yellow
Write-Host ""
Write-Host "Free unblock path for this development laptop:" -ForegroundColor Cyan
Write-Host "1. Windows Security opens now."
Write-Host "2. Go to App & browser control."
Write-Host "3. Open Smart App Control settings."
Write-Host "4. Turn Smart App Control Off if you accept the tradeoff."
Write-Host ""
Write-Host "Important: Microsoft documents that Smart App Control may not be turnable back on without resetting or reinstalling Windows." -ForegroundColor Yellow
Write-Host "Use this only as a development-machine decision, not as a shipping/signing solution." -ForegroundColor Yellow
Write-Host ""

Start-Process "windowsdefender://appbrowser"
