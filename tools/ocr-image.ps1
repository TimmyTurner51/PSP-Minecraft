# OCR a screenshot with the OCR engine built into Windows, so a PPSSPP crash screen
# (which cannot be copied as text) can be fed to tools/symbolize-crash.py.
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools/ocr-image.ps1 shot.png
#
# Prints the recognised text to stdout. Used by symbolize-crash.py --crash-image.

param([Parameter(Mandatory = $true)][string]$Path)

Add-Type -AssemblyName System.Runtime.WindowsRuntime
Add-Type -AssemblyName System.Drawing

$asTaskGeneric = ([System.WindowsRuntimeSystemExtensions].GetMethods() | Where-Object {
    $_.Name -eq 'AsTask' -and
    $_.GetParameters().Count -eq 1 -and
    $_.GetParameters()[0].ParameterType.Name -eq 'IAsyncOperation`1'
})[0]

function Await($WinRtTask, $ResultType) {
    $asTask = $asTaskGeneric.MakeGenericMethod($ResultType)
    $netTask = $asTask.Invoke($null, @($WinRtTask))
    $netTask.Wait(-1) | Out-Null
    $netTask.Result
}

[void][Windows.Storage.StorageFile, Windows.Storage, ContentType = WindowsRuntime]
[void][Windows.Graphics.Imaging.BitmapDecoder, Windows.Graphics.Imaging, ContentType = WindowsRuntime]
[void][Windows.Media.Ocr.OcrEngine, Windows.Foundation, ContentType = WindowsRuntime]

$resolved = (Resolve-Path -LiteralPath $Path).Path

# 3x upscale: the crash dialog is small on screen and OCR misses hex digits at 1x.
$img = [System.Drawing.Image]::FromFile($resolved)
$scale = 3
$bmp = New-Object System.Drawing.Bitmap ($img.Width * $scale), ($img.Height * $scale)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.DrawImage($img, 0, 0, $bmp.Width, $bmp.Height)
$g.Dispose()
$img.Dispose()

$tmp = Join-Path $env:TEMP ("mcpsp_ocr_" + [System.IO.Path]::GetFileNameWithoutExtension($resolved) + ".png")
$bmp.Save($tmp, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()

$file = Await ([Windows.Storage.StorageFile]::GetFileFromPathAsync($tmp)) ([Windows.Storage.StorageFile])
$stream = Await ($file.OpenAsync([Windows.Storage.FileAccessMode]::Read)) ([Windows.Storage.Streams.IRandomAccessStream])
$decoder = Await ([Windows.Graphics.Imaging.BitmapDecoder]::CreateAsync($stream)) ([Windows.Graphics.Imaging.BitmapDecoder])
$softwareBitmap = Await ($decoder.GetSoftwareBitmapAsync()) ([Windows.Graphics.Imaging.SoftwareBitmap])

$engine = [Windows.Media.Ocr.OcrEngine]::TryCreateFromUserProfileLanguages()
if ($null -eq $engine) {
    Write-Error "No Windows OCR engine available for the current user profile languages."
    exit 1
}

$result = Await ($engine.RecognizeAsync($softwareBitmap)) ([Windows.Media.Ocr.OcrResult])
foreach ($line in $result.Lines) { Write-Output $line.Text }
