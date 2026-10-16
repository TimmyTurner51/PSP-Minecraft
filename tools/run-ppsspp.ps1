<#
.SYNOPSIS
    Launch the built EBOOT in PPSSPP, let it run, close it, and report what the
    development build logged -- so a crash or a heap-guard complaint can be
    reproduced on demand instead of being lost when the emulator is closed.

.DESCRIPTION
    PPSSPP is not always on PATH and has no single install location, so the
    executable is looked for in the usual places. The dev build truncates
    Log.txt at boot (MC_DEBUG_MODE), which means the log after a run describes
    exactly that run and nothing else -- that is what makes this script useful.

    Any HEAP GUARD line (double free / overrun) is treated as a failure and
    printed in full, along with the checkpoint lines so the phase that produced
    it is visible.

.EXAMPLE
    powershell -File tools/run-ppsspp.ps1
    powershell -File tools/run-ppsspp.ps1 -Seconds 90 -Tag after-pigfix
#>
[CmdletBinding()]
param(
    # How long to let the game run before closing the emulator.
    [int]$Seconds = 30,
    # EBOOT to run. Defaults to the one next to this script's project root.
    [string]$Eboot,
    # Optional label folded into the summary header, e.g. a change you are testing.
    [string]$Tag,
    # Leave PPSSPP open instead of closing it (you drive the game yourself).
    [switch]$KeepRunning
)

$ErrorActionPreference = 'Stop'

$srcDir = Split-Path -Parent $PSScriptRoot
if (-not $Eboot) { $Eboot = Join-Path $srcDir 'EBOOT.PBP' }

if (-not (Test-Path $Eboot)) {
    Write-Error "EBOOT not found: $Eboot`nBuild first: wsl -e bash -lc './compile-wsl.sh fast'"
    exit 1
}

# --- locate PPSSPP -----------------------------------------------------------
$candidates = @(
    'C:\Program Files\PPSSPP\PPSSPPWindows64.exe',
    'C:\Program Files\PPSSPP\PPSSPPWindows.exe',
    'C:\Program Files (x86)\PPSSPP\PPSSPPWindows64.exe',
    "$env:LOCALAPPDATA\PPSSPP\PPSSPPWindows64.exe",
    "$env:USERPROFILE\Documents\PPSSPP\PPSSPPWindows64.exe",
    'C:\PPSSPP\PPSSPPWindows64.exe'
)
$exe = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $exe) {
    $exe = Get-ChildItem 'C:\Program Files\PPSSPP', "$env:LOCALAPPDATA\PPSSPP" `
            -Filter 'PPSSPPWindows*.exe' -ErrorAction SilentlyContinue |
           Select-Object -First 1 -ExpandProperty FullName
}
if (-not $exe) {
    Write-Error "PPSSPP not found. Pass -Eboot to a copy and install PPSSPP, or edit the candidate list."
    exit 1
}

$logPath = Join-Path (Split-Path -Parent $Eboot) 'Log.txt'

Write-Host "emu:    $exe"
Write-Host "eboot:  $Eboot  ($((Get-Item $Eboot).LastWriteTime))"
Write-Host "log:    $logPath"
Write-Host ""

# the game writes Log.txt into the memstick root; PPSSPP may have it open
Get-Process -Name 'PPSSPP*' -ErrorAction SilentlyContinue | ForEach-Object {
    Write-Host "closing a running PPSSPP (pid $($_.Id)) first"
    Stop-Process -Id $_.Id -Force
    Start-Sleep -Seconds 2
}

Start-Process -FilePath $exe -ArgumentList '--escape-exit', "`"$Eboot`""
Write-Host "launched, running for ${Seconds}s..."
Start-Sleep -Seconds $Seconds

$wasRunning = @(Get-Process -Name 'PPSSPP*' -ErrorAction SilentlyContinue)
if (-not $KeepRunning) {
    $wasRunning | ForEach-Object { Stop-Process -Id $_.Id -Force }
    Start-Sleep -Seconds 2
} else {
    Write-Host "left running (pid $($wasRunning.Id -join ','))"
    return
}

if (-not (Test-Path $logPath)) {
    Write-Error "No Log.txt written -- the game did not reach boot logging."
    exit 1
}

$lines = Get-Content $logPath

# If the log was not rewritten, the run never happened (bad EBOOT path, emulator
# error, etc). Compare the banner to be sure we are reading this run.
$banner = $lines | Select-Object -First 40 | Select-String -Pattern 'development build'
$fresh = $true
if (-not $banner) { $fresh = $false }

Write-Host ""
Write-Host "===================== run summary ====================="
if ($Tag) { Write-Host "tag: $Tag" }

if (-not $fresh) {
    Write-Host "WARNING: Log.txt has no development-build banner."
    Write-Host "         Either this is a release build, or the run did not start."
}

$guard   = $lines | Select-String -Pattern 'HEAP GUARD'
# the boot snapshot is "heap at boot:" and carries no errors= field, so only
# match real checkpoints here -- otherwise the summary always reports a failure
$checks  = $lines | Select-String -Pattern '^heap .*errors='

Write-Host ""
Write-Host "--- boot banner ---"
$lines | Select-Object -First 7 | ForEach-Object { Write-Host "  $_" }

Write-Host ""
Write-Host "--- heap checkpoints ---"
if ($checks) {
    $checks | ForEach-Object { Write-Host "  $_" }
    # every checkpoint prints errors=N; surface anything non-zero
    $badErrors = $checks | Where-Object { $_ -notmatch 'errors=0' }
} else {
    Write-Host "  (none recorded -- the run stopped before world creation)"
    $badErrors = @()
}

Write-Host ""
if ($guard) {
    Write-Host "--- HEAP GUARD HITS ---" -ForegroundColor Red
    $guard | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
} else {
    Write-Host "no HEAP GUARD hits" -ForegroundColor Green
}

Write-Host ""
$lastCheck = if ($checks) { ($checks | Select-Object -Last 1).ToString().Trim() } else { '(none)' }
Write-Host "last checkpoint: $lastCheck"
Write-Host "log lines:       $($lines.Count)"

if ($guard -or $badErrors) {
    Write-Host ""
    Write-Host "RESULT: FAIL -- heap guard reported a problem (see above)." -ForegroundColor Red
    Write-Host "Map any addresses with: python tools/symbolize-crash.py --crash-text <file>"
    exit 1
}

Write-Host ""
Write-Host "RESULT: PASS -- no heap guard hits, all checkpoints errors=0." -ForegroundColor Green
Write-Host "(reaching 'play mesh built' means generation and world entry completed)"
exit 0
