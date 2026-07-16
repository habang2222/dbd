param(
    [Parameter(Mandatory = $true)]
    [string]$ManifestPath,

    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
)

$ErrorActionPreference = "Stop"

function Resolve-AssetPath {
    param([string]$Path)
    if ([string]::IsNullOrWhiteSpace($Path)) {
        return $null
    }
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return $Path
    }
    return Join-Path $RepoRoot $Path
}

function Get-JsonNumber {
    param($Value, [int]$Fallback)
    if ($null -eq $Value) {
        return $Fallback
    }
    return [int]$Value
}

$manifestFullPath = Resolve-AssetPath -Path $ManifestPath
if (!(Test-Path -LiteralPath $manifestFullPath)) {
    throw "Manifest not found: $manifestFullPath"
}

$manifest = Get-Content -LiteralPath $manifestFullPath -Raw | ConvertFrom-Json
$width = Get-JsonNumber -Value $manifest.canvas.width -Fallback 1024
$height = Get-JsonNumber -Value $manifest.canvas.height -Fallback 1024
$outputPath = Resolve-AssetPath -Path $manifest.output
if ([string]::IsNullOrWhiteSpace($outputPath)) {
    throw "Manifest output is required."
}

Add-Type -AssemblyName System.Drawing

$bitmap = New-Object System.Drawing.Bitmap $width, $height, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$graphics.Clear([System.Drawing.Color]::Transparent)
$graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceOver
$graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
$graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
$graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::Half

try {
    foreach ($layer in @($manifest.layers)) {
        if ($null -ne $layer.visible -and -not [bool]$layer.visible) {
            continue
        }
        $layerPath = Resolve-AssetPath -Path $layer.path
        if ($null -eq $layerPath) {
            continue
        }
        if (!(Test-Path -LiteralPath $layerPath)) {
            throw "Layer '$($layer.slot)' not found: $layerPath"
        }

        $image = [System.Drawing.Image]::FromFile($layerPath)
        try {
            $x = Get-JsonNumber -Value $layer.x -Fallback 0
            $y = Get-JsonNumber -Value $layer.y -Fallback 0
            $graphics.DrawImage($image, $x, $y, $image.Width, $image.Height)
            Write-Host "Layer $($layer.slot): $layerPath at $x,$y"
        } finally {
            $image.Dispose()
        }
    }

    $outputDir = Split-Path -Parent $outputPath
    New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
    $bitmap.Save($outputPath, [System.Drawing.Imaging.ImageFormat]::Png)
    Write-Host "Composite written: $outputPath"
} finally {
    $graphics.Dispose()
    $bitmap.Dispose()
}

