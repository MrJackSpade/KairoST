<#
.SYNOPSIS
Compares two folders of verify-hashes.ps1 output: every run's every frame must match.

.EXAMPLE
tools\perf\compare-hashes.ps1 .build\verify\before .build\verify\after
#>
param([Parameter(Mandatory)][string]$Before, [Parameter(Mandatory)][string]$After)
$ErrorActionPreference = 'Stop'
$different = 0
foreach ($file in Get-ChildItem $Before -Filter '*.txt') {
    $other = Join-Path $After $file.Name
    if (-not (Test-Path $other)) { throw "$($file.Name) is missing from $After" }
    $a = Get-Content $file.FullName
    $b = Get-Content $other
    if ($a.Count -ne $b.Count) { throw "$($file.Name): $($a.Count) frames before, $($b.Count) after" }
    $first = $null
    $count = 0
    for ($i = 0; $i -lt $a.Count; $i++) {
        if ($a[$i] -ne $b[$i]) { $count++; if ($null -eq $first) { $first = $i } }
    }
    if ($count) { $different++; "$($file.Name): $count of $($a.Count) frames differ, the first at line $first" }
    else { "$($file.Name): all $($a.Count) frames match" }
}
if ($different) { throw "$different runs differ" }
