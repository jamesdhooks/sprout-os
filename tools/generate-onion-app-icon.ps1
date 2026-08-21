param(
    [string]$Source = (Join-Path $PSScriptRoot '..\launcher\assets\sprout-startup-storybook.png'),
    [string]$Output = (Join-Path $PSScriptRoot '..\launcher\assets\icons\sprout-onion.png')
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

# The Onion Apps grid uses 74px square icons.  Keep only Sprout's mascot from
# the shared startup illustration: the wordmark and white canvas would be too
# small and visually noisy at this size.
$sourceImage = [System.Drawing.Bitmap]::new((Resolve-Path $Source).Path)
try {
    $crop = [System.Drawing.Rectangle]::new(188, 72, 270, 270)
    $icon = [System.Drawing.Bitmap]::new(74, 74, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        $graphics = [System.Drawing.Graphics]::FromImage($icon)
        try {
            $graphics.Clear([System.Drawing.Color]::Transparent)
            $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
            $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
            $graphics.DrawImage($sourceImage, [System.Drawing.Rectangle]::new(0, 0, 74, 74), $crop, [System.Drawing.GraphicsUnit]::Pixel)
        } finally {
            $graphics.Dispose()
        }

        for ($y = 0; $y -lt $icon.Height; $y++) {
            for ($x = 0; $x -lt $icon.Width; $x++) {
                $pixel = $icon.GetPixel($x, $y)
                $alpha = [Math]::Max(0, 255 - [Math]::Min($pixel.R, [Math]::Min($pixel.G, $pixel.B)))
                if ($alpha -lt 12) {
                    $icon.SetPixel($x, $y, [System.Drawing.Color]::Transparent)
                } else {
                    $icon.SetPixel($x, $y, [System.Drawing.Color]::FromArgb($alpha, $pixel.R, $pixel.G, $pixel.B))
                }
            }
        }

        $outputDirectory = Split-Path -Parent $Output
        [System.IO.Directory]::CreateDirectory($outputDirectory) | Out-Null
        $icon.Save($Output, [System.Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $icon.Dispose()
    }
} finally {
    $sourceImage.Dispose()
}
