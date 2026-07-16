[CmdletBinding(DefaultParameterSetName = "StoreCertificate")]
param(
    [Parameter(ParameterSetName = "StoreCertificate")]
    [string]$CertificateThumbprint,

    [Parameter(ParameterSetName = "StoreCertificate")]
    [string]$CertificateSubject,

    [Parameter(ParameterSetName = "TrustedSigning")]
    [switch]$UseTrustedSigning,

    [Parameter(ParameterSetName = "TrustedSigning")]
    [string]$TrustedSigningProfile,

    [string]$TimestampServer = "http://timestamp.digicert.com",
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-RepoRoot {
    return Split-Path -Parent $PSScriptRoot
}

function Get-SignToolPath {
    $candidates = @(
        "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\signtool.exe",
        "C:\Program Files (x86)\Windows Kits\10\App Certification Kit\signtool.exe"
    )
    $path = $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    if ([string]::IsNullOrWhiteSpace($path)) {
        throw "signtool.exe was not found. Install Windows SDK signing tools."
    }
    return $path
}

function Get-TargetExecutables([string]$RepoRoot, [string]$Configuration) {
    $targets = @(
        (Join-Path $RepoRoot "build-native\$Configuration\dbd_play_session.exe"),
        (Join-Path $RepoRoot "build-native\$Configuration\dbd_client.exe"),
        (Join-Path $RepoRoot "build-native\$Configuration\dbd_client3d.exe"),
        (Join-Path $RepoRoot "build-native\$Configuration\dbd_server.exe")
    )
    foreach ($target in $targets) {
        if (!(Test-Path -LiteralPath $target)) {
            throw "Missing executable: $target"
        }
    }
    return $targets
}

function Get-StoreCertificate([string]$Thumbprint, [string]$Subject) {
    if ([string]::IsNullOrWhiteSpace($Thumbprint) -and [string]::IsNullOrWhiteSpace($Subject)) {
        throw "Provide -CertificateThumbprint or -CertificateSubject, or use -UseTrustedSigning."
    }

    $stores = @("Cert:\CurrentUser\My", "Cert:\LocalMachine\My")
    foreach ($store in $stores) {
        $candidates = Get-ChildItem $store -CodeSigningCert -ErrorAction SilentlyContinue
        if (-not [string]::IsNullOrWhiteSpace($Thumbprint)) {
            $match = $candidates | Where-Object { $_.Thumbprint -eq $Thumbprint } | Select-Object -First 1
        } else {
            $match = $candidates | Where-Object { $_.Subject -like "*$Subject*" } | Select-Object -First 1
        }
        if ($null -ne $match) {
            return $match
        }
    }

    throw "No matching trusted code-signing certificate found in CurrentUser\\My or LocalMachine\\My."
}

function Invoke-StoreCertificateSigning(
    [string]$SignTool,
    [System.Security.Cryptography.X509Certificates.X509Certificate2]$Certificate,
    [string]$TimestampServer,
    [string[]]$Executables
) {
    foreach ($exe in $Executables) {
        & $SignTool sign /fd SHA256 /td SHA256 /tr $TimestampServer /sha1 $Certificate.Thumbprint $exe
    }
}

function Invoke-TrustedSigningWrapper(
    [string]$TimestampServer,
    [string]$Configuration,
    [string[]]$Executables,
    [string]$TrustedSigningProfile
) {
    $wrapper = $env:DBD_TRUSTED_SIGNING_WRAPPER
    if ([string]::IsNullOrWhiteSpace($wrapper)) {
        throw @"
Trusted signing was requested, but no provider wrapper is configured.

Set DBD_TRUSTED_SIGNING_WRAPPER to a PowerShell script or executable that accepts:
  -Configuration
  -TimestampServer
  -TrustedSigningProfile
  -Files <string[]>

This keeps provider-specific logic out of the core repo while still giving DBDReboot
a stable trusted-signing entrypoint.
"@
    }

    if (!(Test-Path -LiteralPath $wrapper)) {
        throw "Trusted signing wrapper not found: $wrapper"
    }

    if ($wrapper.EndsWith(".ps1", [System.StringComparison]::OrdinalIgnoreCase)) {
        & powershell -NoProfile -ExecutionPolicy Bypass -File $wrapper `
            -Configuration $Configuration `
            -TimestampServer $TimestampServer `
            -TrustedSigningProfile $TrustedSigningProfile `
            -Files $Executables
    } else {
        & $wrapper `
            -Configuration $Configuration `
            -TimestampServer $TimestampServer `
            -TrustedSigningProfile $TrustedSigningProfile `
            -Files $Executables
    }
}

function Write-SigningSummary([string[]]$Executables, [string]$Mode, [string]$Detail) {
    Write-Host ""
    Write-Host "Signing mode: $Mode" -ForegroundColor Cyan
    Write-Host $Detail -ForegroundColor Cyan
    Write-Host ""
    Get-AuthenticodeSignature $Executables |
        Select-Object Path, Status, StatusMessage, SignerCertificate |
        Format-List
}

$repoRoot = Get-RepoRoot
$signTool = Get-SignToolPath
$executables = Get-TargetExecutables -RepoRoot $repoRoot -Configuration $Configuration

if ($UseTrustedSigning) {
    Invoke-TrustedSigningWrapper `
        -TimestampServer $TimestampServer `
        -Configuration $Configuration `
        -Executables $executables `
        -TrustedSigningProfile $TrustedSigningProfile
    Write-SigningSummary `
        -Executables $executables `
        -Mode "trusted-signing-wrapper" `
        -Detail ("Trusted signing wrapper executed{0}." -f ($(if ([string]::IsNullOrWhiteSpace($TrustedSigningProfile)) { "" } else { " with profile '$TrustedSigningProfile'" })))
    return
}

$certificate = Get-StoreCertificate -Thumbprint $CertificateThumbprint -Subject $CertificateSubject
Invoke-StoreCertificateSigning `
    -SignTool $signTool `
    -Certificate $certificate `
    -TimestampServer $TimestampServer `
    -Executables $executables

$issuerHint =
    if ($certificate.Issuer -eq $certificate.Subject) {
        "Warning: this certificate is self-signed. WDAC/Device Guard may still reject it for enterprise-signing requirements."
    } else {
        "Issuer: $($certificate.Issuer)"
    }

Write-SigningSummary `
    -Executables $executables `
    -Mode "certificate-store" `
    -Detail ("Certificate: $($certificate.Subject)`n$issuerHint")
