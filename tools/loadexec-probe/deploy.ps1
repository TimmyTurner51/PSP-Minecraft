# Deploys the sceKernelLoadExec probe to a PSP memory stick.
#
# The layout is dictated by the cases in probe.c, which look for their target at three different
# relative paths -- PAYLOAD.PBP and UPDATER.PBP next to the probe, and TARGET/EBOOT.PBP in a subfolder.
# All three copies are the *same* payload binary under different names, because whether the filename
# matters is one of the things being tested.
#
#   <drive>\PSP\GAME\MCPSP LoadExec Probe\
#       EBOOT.PBP            the probe
#       PAYLOAD.PBP          the payload, non-standard name, same folder
#       UPDATER.PBP          the payload, the name a real design would use
#       TARGET\EBOOT.PBP     the payload, canonical name in its own folder
#
# Nothing outside this one folder is touched, and nothing in it belongs to the game.

param(
    [string]$Drive = "I:"
)

$ErrorActionPreference = "Stop"

$src = $PSScriptRoot
$probe = Join-Path $src "EBOOT-probe.PBP"
$payload = Join-Path $src "EBOOT-payload.PBP"

if (-not (Test-Path $probe)) { throw "missing $probe -- run 'make both' first" }
if (-not (Test-Path $payload)) { throw "missing $payload -- run 'make both' first" }
if (-not (Test-Path $Drive)) { throw "$Drive not found -- pass the right drive letter with -Drive" }

$dst = Join-Path $Drive "PSP\GAME\MCPSP LoadExec Probe"
$dstTarget = Join-Path $dst "TARGET"

New-Item -ItemType Directory -Force -Path $dst | Out-Null
New-Item -ItemType Directory -Force -Path $dstTarget | Out-Null

Copy-Item $probe (Join-Path $dst "EBOOT.PBP") -Force
Copy-Item $payload (Join-Path $dst "PAYLOAD.PBP") -Force
Copy-Item $payload (Join-Path $dst "UPDATER.PBP") -Force
Copy-Item $payload (Join-Path $dstTarget "EBOOT.PBP") -Force

# Stale state is worse than no state, in two different ways, so this clears all of it:
#
#   the payload markers, because a leftover one would read as a success from a run that never happened;
#   the state file, because the probe deliberately resumes from it -- a fresh deploy should start over
#     from case 0 rather than continue wherever the last session stopped.
#
# Swept rather than listed, so a marker added later cannot be forgotten here. Nothing outside these
# loadexec-* names is touched.
$stale = @()
$stale += Get-ChildItem -Path $Drive -Filter "loadexec-*.txt" -File -ErrorAction SilentlyContinue
$stale += Get-ChildItem -Path $dst -Filter "loadexec-*.txt" -File -ErrorAction SilentlyContinue
$stale += Get-ChildItem -Path $dstTarget -Filter "loadexec-*.txt" -File -ErrorAction SilentlyContinue
foreach ($f in $stale) { Remove-Item $f.FullName -Force -ErrorAction SilentlyContinue }

if ($stale.Count -gt 0) {
    Write-Host "cleared $($stale.Count) stale file(s) from a previous session"
}

Write-Host "deployed to $dst"
Get-ChildItem $dst -File | ForEach-Object { Write-Host ("  {0,-14} {1,9:N0} bytes" -f $_.Name, $_.Length) }
Get-ChildItem $dstTarget -File | ForEach-Object { Write-Host ("  TARGET\{0,-7} {1,9:N0} bytes" -f $_.Name, $_.Length) }
Write-Host ""
Write-Host "On the PSP: Game -> Memory Stick -> MCPSP LoadExec Probe"
