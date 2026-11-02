# Host check of the 2600 band fast path: the picture it draws must equal the exact path's,
# tile by tile (pixels, not ids).  Needs zig (zig cc) on PATH.  Usage: ./tests/host_river_fast.ps1 [tile_rows]
param([int]$Rows = 3000, [switch]$Features, [int]$JobCols = 255, [string[]]$Extra = @())   # JobCols 10 = the GBC build's per-pass budget (exercises yield/resume)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$out = Join-Path $env:TEMP 'river_host_fast'
New-Item -ItemType Directory -Force $out | Out-Null
$base = @('-std=c11', '-O1', '-w', '-DRIVER_NO_START_ROAD', "-DRIVER_JOB_COLS=$JobCols", '-DRIVER_BLOCKY', '-DRIVER_HOST_DIRECT', '-DRIVER_STATS')
if (-not $Features) { $base += '-DRIVER_NOFEAT' }
$inc = @("-I$PSScriptRoot/host", "-I$root/src", '-include', 'stdlib.h')
$src = @("$PSScriptRoot/host/tiles_main.c", "$root/src/river.c", "$root/src/atari_data.c")
zig cc @base @inc -o "$out/exact.exe" @src
if ($LASTEXITCODE) { throw 'host build (exact) failed' }
zig cc @base @Extra -DRIVER_FAST @inc -o "$out/fast.exe" @src
if ($LASTEXITCODE) { throw 'host build (fast) failed' }
foreach ($variant in 0..3) {
    # the "row" markers move with the job timing (JobCols), the writes themselves must not
    & "$out/exact.exe" $Rows $variant | Where-Object { $_ -notlike 'row *' } | Set-Content "$out/exact.txt"
    & "$out/fast.exe" $Rows $variant | Where-Object { $_ -notlike 'row *' } | Set-Content "$out/fast.txt"
    $a = (Get-FileHash "$out/exact.txt").Hash; $b = (Get-FileHash "$out/fast.txt").Hash
    if ($a -ne $b) {
        $d = Compare-Object (Get-Content "$out/exact.txt") (Get-Content "$out/fast.txt") | Select-Object -First 6
        $d | Out-String | Write-Host
        throw "fast path differs from the exact path (start level variant $variant)"
    }
    Write-Host "variant ${variant}: identical ($((Get-Content "$out/exact.txt").Count) lines)"
}






