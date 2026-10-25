<#
.SYNOPSIS
Trains KairoST's profile-guided build on the device (KairoST#9): runs an instrumented
game_profile over the training games and freezes what they record into core/pgo.

.DESCRIPTION
-Build is a device build folder of core/ configured with the NDK and
-DKAIROST_TOOLS=ON -DKAIROST_PGO_GENERATE=ON -DKAIRO_PGO=OFF, from the committed tree (the profile
records its code). Each training run boots a game and plays it with presses, writing its own
.profraw from the Hatari library (LLVM_PROFILE_FILE, %m per module); the runs are pulled to
<Out>\raw and merged with equal weight by the NDK's llvm-profdata, and shared/tools/pgo_freeze.py
writes the result to core/pgo as the profile the device build uses.

GAUNTLE2.MSA is kept out of the training set, so evaluations can include a game the profile has
never seen.

.EXAMPLE
tools\perf\pgo-train.ps1 -Device 192.168.1.118:5555 -Build .build\device-pgogen -Out .build\pgo
#>
param(
    [Parameter(Mandatory)][string]$Device,
    [Parameter(Mandatory)][string]$Build,
    [Parameter(Mandatory)][string]$Out,
    [string]$Ndk = 'F:\android-sdk\ndk\28.2.13676358',
    [int]$Frames = 2400
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$remote = '/data/local/tmp/kst-pgo'
$adb = (Get-Command adb -CommandType Application | Select-Object -First 1).Source
function Adb { $out = & $adb -s $Device @args 2>&1; if ($LASTEXITCODE -ne 0) { throw "adb $args failed: $out" }; $out }

$emutos = Join-Path $root 'third_party\emutos-1024k\etos1024k.img'
$tos104 = Join-Path $root 'bios\retrobios\tos104uk.img'
$tos162 = Join-Path $root 'bios\retrobios\tos162uk.img'
# Name, TOS, media under roms/, machine, script of presses after the boot.
$runs = @(
    @('oxyd', $emutos, 'OXYD_COL.MSA', 'st', '300:+fire,310:-fire'),
    @('psyborg', $emutos, 'PSYBORG.MSA', 'st', '200:+0x39,205:-0x39,600:+fire,900:-fire'),
    @('outrun', $emutos, 'T_OUTRUN.MSA', 'st', '300:+fire,1500:-fire'),
    @('goal', $tos104, 'GOAL.MSA', 'st', '300:+fire,310:-fire'),
    @('rotmc-ste', $tos162, 'ROTMCSTE.MSA', 'ste', '200:+0x39,205:-0x39'),
    @('ooze', $emutos, 'OOZE.MSA', 'st', '300:+0x39,305:-0x39')
)

$raw = Join-Path $Out 'raw'
if (Test-Path $raw) { Remove-Item -Recurse -Force $raw }
New-Item -ItemType Directory -Force $raw | Out-Null
Adb shell "rm -rf $remote; mkdir -p $remote/work $remote/pgo" | Out-Null
Adb push (Join-Path $Build 'game_profile') "$remote/game_profile" | Out-Null
Adb push (Join-Path $Build 'libkairost_hatari.so') "$remote/libkairost_hatari.so" | Out-Null
Adb shell "chmod 755 $remote/game_profile" | Out-Null
foreach ($run in $runs) {
    $name, $tos, $media, $machine, $script = $run
    Adb push $tos "$remote/tos.img" | Out-Null
    Adb push (Join-Path $root "roms\$media") "$remote/game$([IO.Path]::GetExtension($media))" | Out-Null
    $text = Adb shell "cd $remote && rm -rf work/run && mkdir -p work/run && LD_LIBRARY_PATH=$remote LLVM_PROFILE_FILE=$remote/pgo/$name-%m.profraw ./game_profile tos.img work/run game$([IO.Path]::GetExtension($media)) $machine 1024 $Frames $script --boot-frames 600"
    "${name}: " + (($text | Where-Object { $_ -match '^host |^FAIL' }) -join ' ')
}
Adb pull "$remote/pgo/." $raw | Out-Null
$files = @(Get-ChildItem $raw -Filter '*.profraw' | ForEach-Object FullName)
if (-not $files) { throw "no profiles were written to $remote/pgo" }
$profdata = Join-Path $Ndk 'toolchains\llvm\prebuilt\windows-x86_64\bin\llvm-profdata.exe'
$merged = Join-Path $Out 'kairost.profdata'
& $profdata merge -o $merged @files
if ($LASTEXITCODE -ne 0) { throw 'llvm-profdata merge failed' }
"merged $merged from $($files.Count) profiles"
$note = 'tools/perf/pgo-train.ps1: ' + (($runs | ForEach-Object { $_[0] }) -join ', ') +
    ", $Frames frames after a 600-frame boot each; Gauntlet II held out"
New-Item -ItemType Directory -Force (Join-Path $root 'core\pgo') | Out-Null
& python (Join-Path $root 'shared\tools\pgo_freeze.py') --profdata $merged --llvm-profdata $profdata --root $root `
    --text (Join-Path $root 'core\pgo\arm64.proftext') --manifest (Join-Path $root 'core\pgo\profile.json') `
    --ndk (Split-Path $Ndk -Leaf) --sparse --note $note @files
if ($LASTEXITCODE -ne 0) { throw 'pgo_freeze.py failed' }
