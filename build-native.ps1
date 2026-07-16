Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $RepoRoot

$CMake = Get-Command cmake -ErrorAction SilentlyContinue
if ($null -ne $CMake) {
    $CMakeExe = $CMake.Source
} else {
    $Candidates = @(
        "C:\Program Files\Microsoft Visual Studio\18\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "C:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    )
    $CMakeExe = $Candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}

if ([string]::IsNullOrWhiteSpace($CMakeExe)) {
    Write-Error "Could not find cmake.exe. Install CMake or Visual Studio CMake tools."
}

& $CMakeExe -S . -B build-native
& $CMakeExe --build build-native --config Debug

Write-Host ""
Write-Host "Development signing mode: local self-signed certificate" -ForegroundColor Cyan
Write-Host "Use tools\\sign-release.ps1 for CA/enterprise/trusted-signing release builds." -ForegroundColor Cyan

$CertSubject = "CN=DBDReboot Local Dev Code Signing"
$Cert = Get-ChildItem Cert:\CurrentUser\My |
    Where-Object { $_.Subject -eq $CertSubject } |
    Select-Object -First 1

if ($null -eq $Cert) {
    $Cert = New-SelfSignedCertificate `
        -Type CodeSigningCert `
        -Subject $CertSubject `
        -CertStoreLocation Cert:\CurrentUser\My `
        -KeyExportPolicy Exportable `
        -KeyUsage DigitalSignature `
        -NotAfter (Get-Date).AddYears(3)

    $RootStore = New-Object System.Security.Cryptography.X509Certificates.X509Store("Root", "CurrentUser")
    $RootStore.Open("ReadWrite")
    $RootStore.Add($Cert)
    $RootStore.Close()

    $PublisherStore = New-Object System.Security.Cryptography.X509Certificates.X509Store("TrustedPublisher", "CurrentUser")
    $PublisherStore.Open("ReadWrite")
    $PublisherStore.Add($Cert)
    $PublisherStore.Close()
}

$SignToolCandidates = @(
    "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\signtool.exe",
    "C:\Program Files (x86)\Windows Kits\10\App Certification Kit\signtool.exe"
)
$SignTool = $SignToolCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1

if ($null -ne $SignTool) {
    $Executables = @(
        (Join-Path $RepoRoot "build-native\Debug\dbd_play_session.exe"),
        (Join-Path $RepoRoot "build-native\Debug\dbd_client.exe"),
        (Join-Path $RepoRoot "build-native\Debug\dbd_client3d.exe"),
        (Join-Path $RepoRoot "build-native\Debug\dbd_server.exe")
    )
    foreach ($Exe in $Executables) {
        if (Test-Path -LiteralPath $Exe) {
            $PreviousErrorActionPreference = $ErrorActionPreference
            $ErrorActionPreference = "Continue"
            $SignOutput = & $SignTool sign /fd SHA256 /td SHA256 /tr http://timestamp.digicert.com /sha1 $Cert.Thumbprint $Exe 2>&1
            $SignExitCode = $LASTEXITCODE
            $ErrorActionPreference = $PreviousErrorActionPreference
            if ($SignExitCode -ne 0) {
                Write-Warning "Timestamped signing failed for $Exe. Retrying local development signing without timestamp."
                & $SignTool sign /fd SHA256 /sha1 $Cert.Thumbprint $Exe | Out-Host
                if ($LASTEXITCODE -eq 0) {
                    $global:LASTEXITCODE = 0
                }
            } else {
                $SignOutput | Out-Host
            }
        }
    }
} else {
    Write-Warning "signtool.exe was not found. Build succeeded, but local application-control policy may block unsigned executables."
}
