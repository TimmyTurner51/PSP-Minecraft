# One frame of the nether probe, captured from the game's own framebuffer AFTER the world is up.
#
# Why this exists: the perf counters prove the lighting flood stopped re-walking, and flat 33 ms frames
# are consistent with *both* "the flood is fixed" and "the flood does nothing any more". The counters
# cannot tell those apart on their own, because a world that never propagates light is also cheap. A
# picture of a lit nether can, so this runs the probe build and grabs the frame the game drew.
#
# TRAP, and the reason this waits on the log rather than on a clock. The first version slept a fixed 50s
# and captured the white Mojang splash -- the nether probe does region generation and a mesh build
# before its first lit frame, so a wall-clock wait is a race whose loser looks like "the screen is
# white". The probe prints `netherprobe: round trip complete, entering the frame loop` when it is
# actually there, so wait for THAT marker and the capture cannot land on the boot logo.
#
# Capture is capture.ps1's Save-EmuFrame: PrintWindow + PW_CLIENTONLY | PW_RENDERFULLCONTENT in a
# DPI-aware process, i.e. DWM's composited client area of the emulator window. See Tools/Emulator/capture.ps1
# for why the obvious methods (desktop screen grab, emulated-VRAM read, PPSSPP's own screenshot key) do
# not work here.

param(
    [string]$Tag = "nether",
    [int]$BootTimeoutSec = 120,
    [int]$SettleSec = 4
)

$ErrorActionPreference = "Stop"

$exe   = "C:\Program Files\PPSSPP\PPSSPPWindows64.exe"
$src   = Split-Path -Parent $PSScriptRoot
$eboot = Join-Path $src "EBOOT.PBP"
$log   = Join-Path $src "Log.txt"
$out   = "c:\Users\minec\Documents\MC PSP Project\Tools\Screenshots\$Tag.png"

. "C:\Users\minec\Documents\MC PSP Project\Tools\Emulator\capture.ps1"

if (-not (Test-Path $eboot)) { Write-Error "no EBOOT at $eboot -- build first"; exit 1 }

Get-Process -Name 'PPSSPP*' -ErrorAction SilentlyContinue | ForEach-Object { Stop-Process -Id $_.Id -Force }
Start-Sleep -Seconds 2
Remove-Item $log -ErrorAction SilentlyContinue

Write-Output "emu:   $exe"
Write-Output "eboot: $eboot"
Write-Output "out:   $out"
Start-Process -FilePath $exe -ArgumentList '--escape-exit', "`"$eboot`""

# Wait on the game's own progress marker, not the clock.
$marker = "round trip complete, entering the frame loop"
$waited = 0
$ready = $false
while ($waited -lt $BootTimeoutSec) {
    Start-Sleep -Seconds 1
    $waited++
    if ((Test-Path $log) -and ((Get-Content $log -Raw) -match [regex]::Escape($marker))) {
        Write-Output "probe reached the frame loop after ${waited}s"
        $ready = $true
        break
    }
}
if (-not $ready) {
    Write-Error "probe did not reach the frame loop within ${BootTimeoutSec}s -- not capturing a boot frame"
    Get-Process -Name 'PPSSPP*' -ErrorAction SilentlyContinue | ForEach-Object { Stop-Process -Id $_.Id -Force }
    exit 1
}
# A few more frames so the mesher has finished and the shot is the settled world.
Start-Sleep -Seconds $SettleSec

if (-not (Save-EmuFrame -Path $out)) { Write-Error "capture failed"; exit 1 }

# A nether that is lit reads as a mid-bright orange-brown; one the light never reached reads as near
# black. The average is enough to tell those apart; the screenshot carries the rest.
$img = New-Object System.Drawing.Bitmap $out
$sr = 0; $sg = 0; $sb = 0; $n = 0; $dark = 0
for ($y = 0; $y -lt $img.Height; $y += 4) {
    for ($x = 0; $x -lt $img.Width; $x += 4) {
        $c = $img.GetPixel($x, $y)
        $sr += $c.R; $sg += $c.G; $sb += $c.B; $n++
        if (($c.R + $c.G + $c.B) -lt 30) { $dark++ }
    }
}
$ar = [int]($sr / $n); $ag = [int]($sg / $n); $ab = [int]($sb / $n)
$hex = ($ar.ToString('X2')) + ($ag.ToString('X2')) + ($ab.ToString('X2'))
Write-Output ""
Write-Output ("size:           {0}x{1}" -f $img.Width, $img.Height)
Write-Output ("average colour: {0},{1},{2}  (#{3})" -f $ar, $ag, $ab, $hex)
Write-Output ("near-black:     {0}% of sampled pixels" -f [int](100 * $dark / $n))
$img.Dispose()

Get-Process -Name 'PPSSPP*' -ErrorAction SilentlyContinue | ForEach-Object { Stop-Process -Id $_.Id -Force }
Start-Sleep -Seconds 1
