param(
    [string]$OutputDir = "assets/pixel"
)

$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.Drawing

$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$out = Join-Path $root $OutputDir
New-Item -ItemType Directory -Force -Path $out | Out-Null

function New-Canvas {
    $bmp = New-Object System.Drawing.Bitmap 32, 32, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.Clear([System.Drawing.Color]::Transparent)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::None
    return @{ Bitmap = $bmp; Graphics = $g }
}

function Fill-Rect($g, [int]$x, [int]$y, [int]$w, [int]$h, [string]$hex) {
    $brush = New-Object System.Drawing.SolidBrush ([System.Drawing.ColorTranslator]::FromHtml($hex))
    $g.FillRectangle($brush, $x, $y, $w, $h)
    $brush.Dispose()
}

function Save-Icon($name, [scriptblock]$draw) {
    $canvas = New-Canvas
    & $draw $canvas.Graphics
    $path = Join-Path $out "$name.png"
    $canvas.Bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $canvas.Graphics.Dispose()
    $canvas.Bitmap.Dispose()
    Write-Host "wrote $path"
}

Save-Icon "person" {
    param($g)
    Fill-Rect $g 12 3 8 8 "#d9a36e"
    Fill-Rect $g 10 11 12 10 "#2f6eea"
    Fill-Rect $g 7 12 4 10 "#2453b0"
    Fill-Rect $g 21 12 4 10 "#2453b0"
    Fill-Rect $g 11 21 5 8 "#162447"
    Fill-Rect $g 17 21 5 8 "#162447"
    Fill-Rect $g 10 29 6 2 "#1b120a"
    Fill-Rect $g 17 29 6 2 "#1b120a"
}

Save-Icon "raider" {
    param($g)
    Fill-Rect $g 12 3 8 8 "#b8795f"
    Fill-Rect $g 10 11 12 10 "#bd1e2d"
    Fill-Rect $g 7 12 4 10 "#7f1621"
    Fill-Rect $g 21 12 4 10 "#7f1621"
    Fill-Rect $g 11 21 5 8 "#2a1712"
    Fill-Rect $g 17 21 5 8 "#2a1712"
    Fill-Rect $g 9 2 14 3 "#2b1515"
}

Save-Icon "wood" {
    param($g)
    Fill-Rect $g 13 13 6 15 "#7b461f"
    Fill-Rect $g 8 6 16 12 "#228b32"
    Fill-Rect $g 5 10 22 9 "#176b29"
    Fill-Rect $g 11 3 10 8 "#2fa83e"
}

Save-Icon "stone" {
    param($g)
    Fill-Rect $g 7 15 18 10 "#8b9093"
    Fill-Rect $g 10 10 14 9 "#6f7579"
    Fill-Rect $g 16 7 10 8 "#a0a5a8"
    Fill-Rect $g 5 20 7 6 "#62676b"
}

Save-Icon "iron" {
    param($g)
    Fill-Rect $g 6 11 20 14 "#5e6368"
    Fill-Rect $g 9 8 14 8 "#3f4448"
    Fill-Rect $g 10 12 5 5 "#d36a30"
    Fill-Rect $g 19 17 4 4 "#f08a3c"
}

Save-Icon "depot" {
    param($g)
    Fill-Rect $g 5 13 22 13 "#7a4f27"
    Fill-Rect $g 3 9 26 5 "#2b7a3d"
    Fill-Rect $g 12 18 8 8 "#1a120a"
    Fill-Rect $g 7 15 5 4 "#c9904a"
}

Save-Icon "site" {
    param($g)
    Fill-Rect $g 6 23 20 4 "#7a431f"
    Fill-Rect $g 5 10 4 17 "#c17a32"
    Fill-Rect $g 23 10 4 17 "#c17a32"
    Fill-Rect $g 8 11 16 3 "#d9913b"
    Fill-Rect $g 10 17 12 3 "#d9913b"
}

Save-Icon "structure" {
    param($g)
    Fill-Rect $g 7 12 18 14 "#356c9e"
    Fill-Rect $g 9 8 14 5 "#1c344f"
    Fill-Rect $g 12 17 8 9 "#0f172a"
    Fill-Rect $g 15 5 6 4 "#8eb6df"
}

Save-Icon "cargo" {
    param($g)
    Fill-Rect $g 8 11 16 15 "#a86528"
    Fill-Rect $g 8 11 16 3 "#5c3517"
    Fill-Rect $g 15 11 3 15 "#5c3517"
    Fill-Rect $g 10 15 4 4 "#d18a3d"
}

Save-Icon "marker" {
    param($g)
    Fill-Rect $g 15 6 3 22 "#e5e7eb"
    Fill-Rect $g 18 7 9 6 "#ef4444"
    Fill-Rect $g 12 27 9 3 "#6b7280"
}

