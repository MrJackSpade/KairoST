<#
.SYNOPSIS
Runs a fixed set of game runs with core/test/game_profile and writes each run's frame hashes, so two
builds can be shown to run the guest identically (KairoST#31).

.DESCRIPTION
Each run boots a game from roms/ with a fixed seed and scripted presses and writes every frame's
picture and sound hash to <Out>\<name>.txt. Run it with the build before a change and with the build
after it, then compare the two folders with compare-hashes.ps1: a change that is meant to be exact
must not change a single frame.

.EXAMPLE
tools\perf\verify-hashes.ps1 -Tool .build\host-debug\game_profile.exe -Out .build\verify\before
#>
param(
    [Parameter(Mandatory)][string]$Tool,
    [Parameter(Mandatory)][string]$Out,
    [int]$Frames = 1500
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$emutos = Join-Path $root 'third_party\emutos-1024k\etos1024k.img'
$tos104 = Join-Path $root 'bios\retrobios\tos104uk.img'
$tos162 = Join-Path $root 'bios\retrobios\tos162uk.img'
# name, TOS, media under roms/, machine, memory, script
$runs = @(
    @('gauntlet2', $emutos, 'GAUNTLE2.MSA', 'st', 1024, '100:+0x39,105:-0x39,400:+fire,410:-fire'),
    @('oxyd', $emutos, 'OXYD_COL.MSA', 'st', 1024, '-'),
    @('psyborg', $emutos, 'PSYBORG.MSA', 'st', 1024, '200:+0x39,205:-0x39'),
    @('goal', $tos104, 'GOAL.MSA', 'st', 1024, '300:+fire,310:-fire'),
    @('rotmc-ste', $tos162, 'ROTMCSTE.MSA', 'ste', 1024, '200:+0x39,205:-0x39'),
    @('outrun', $emutos, 'T_OUTRUN.MSA', 'st', 1024, '300:+fire,600:-fire')
)
New-Item -ItemType Directory -Force $Out | Out-Null
foreach ($run in $runs) {
    $name, $tos, $media, $machine, $memory, $script = $run
    $work = Join-Path $Out "work-$name"
    if (Test-Path $work) { Remove-Item -Recurse -Force $work }
    New-Item -ItemType Directory -Force $work | Out-Null
    $hashes = Join-Path $Out "$name.txt"
    $text = & (Join-Path $root 'shared\tools\run-limited.ps1') $Tool $tos $work (Join-Path $root "roms\$media") $machine $memory $Frames $script --boot-frames 600 --hashes $hashes 2>&1 | ForEach-Object { "$_" }
    # run-limited.ps1 shows the tool's own output and returns its exit line.
    if (-not ($text -match '^exit 0,')) { throw "$name failed: $($text -join "`n")" }
}
