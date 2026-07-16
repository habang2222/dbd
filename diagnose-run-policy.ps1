Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$Executables = @(
    (Join-Path $RepoRoot "build-native\Debug\dbd_play_session.exe"),
    (Join-Path $RepoRoot "build-native\Debug\dbd_client.exe"),
    (Join-Path $RepoRoot "build-native\Debug\dbd_client3d.exe"),
    (Join-Path $RepoRoot "build-native\Debug\dbd_server.exe")
)

Write-Host "DBDReboot executable status" -ForegroundColor Cyan
foreach ($Exe in $Executables) {
    if (Test-Path -LiteralPath $Exe) {
        $Item = Get-Item -LiteralPath $Exe
        $Signature = Get-AuthenticodeSignature -LiteralPath $Exe
        $SignerSubject = if ($null -ne $Signature.SignerCertificate) { $Signature.SignerCertificate.Subject } else { "<none>" }
        $SignerIssuer = if ($null -ne $Signature.SignerCertificate) { $Signature.SignerCertificate.Issuer } else { "<none>" }
        Write-Host ("OK    {0} ({1:n0} bytes) signature={2}" -f $Item.FullName, $Item.Length, $Signature.Status)
        Write-Host ("      signer={0}" -f $SignerSubject)
        Write-Host ("      issuer={0}" -f $SignerIssuer)
    } else {
        Write-Host ("MISS  {0}" -f $Exe) -ForegroundColor Yellow
    }
}

Write-Host ""
Write-Host "Recent AppLocker events" -ForegroundColor Cyan
Get-WinEvent -LogName "Microsoft-Windows-AppLocker/EXE and DLL" -MaxEvents 8 -ErrorAction SilentlyContinue |
    Select-Object TimeCreated, Id, Message |
    Format-List

Write-Host ""
Write-Host "Recent Code Integrity events" -ForegroundColor Cyan
Get-WinEvent -LogName "Microsoft-Windows-CodeIntegrity/Operational" -MaxEvents 8 -ErrorAction SilentlyContinue |
    Select-Object TimeCreated, Id, Message |
    Format-List

Write-Host ""
Write-Host "If launch is blocked with an application control policy message but signature=Valid, Windows Device Guard/Smart App Control is requiring a stronger enterprise/reputation signing level or a different trusted issuer chain than the current signer." -ForegroundColor Yellow
