# How many river cache tiles the exact river needs (PLAN_DMG.md task 1): real spawner, all 4 start
# levels, on-screen geometry in tests/host/tile_budget_main.c.  Needs zig (zig cc) on PATH.
# Usage: ./tests/host_tile_budget.ps1 [-Rows 60000] [-Speed 2] [-Cache 250] [-JobCols 10] [-ViewOff 82] [-Lag 0] [-Jitter 0] [-Only 0..3]
# -Cache, -ViewOff and -Lag set the real clobber check; the "need" table does not depend on them.
param([int]$Rows = 60000, [int]$Speed = 2, [int]$Cache = 250, [int]$JobCols = 10, [int]$ViewOff = 82, [int]$Lag = 0, [int]$Jitter = 0, [int[]]$Only = @(0, 1, 2, 3))
$ErrorActionPreference = 'Stop'
if ($Rows -gt 65000) { throw 'river_distance is 16 bits: use -Rows 65000 or fewer' }
if ($Cache -gt 250) { throw 'tile ids are 8 bits, and 253/254 are the road tiles: use -Cache 250 or fewer' }
$root = Split-Path $PSScriptRoot -Parent
$out = Join-Path $env:TEMP 'river_host_budget'
New-Item -ItemType Directory -Force $out | Out-Null
zig cc -std=c11 -O2 -w -DRIVER_HOST_DIRECT "-DRIVER_JOB_COLS=$JobCols" "-DTILE_RIVER_COUNT=${Cache}u" `
    -I"$PSScriptRoot/host" -I"$root/src" -include stdlib.h -o "$out/budget.exe" `
    "$PSScriptRoot/host/tile_budget_main.c" "$root/src/river.c" "$root/src/atari_data.c" "$root/src/entity_gfx.c"
if ($LASTEXITCODE) { throw 'host build failed' }
foreach ($variant in $Only) {
    & "$out/budget.exe" $Rows $variant $Speed $ViewOff $Lag $Jitter
    if ($LASTEXITCODE) { throw "harness failed (start index $variant)" }
}
