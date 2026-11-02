# Host check of the DMG delay queue (PLATFORM_DMG in src/river.c).
#  1. Picture: the queue build must write the same pixels to the same map cells as the plain build.
#     tiles_main.c resolves ids to pixels; rows go out in the same order, later.  The margin cells
#     are written directly in both, at a different moment, and do not exist on a GB, so they are
#     left out.
#  2. Clobbers: with the real spawner and the game's schedule, DMG cache size, the queue's uploads
#     must not change a tile the screen is showing (tile_budget_main.c, decided from map row and SCY).
# Needs zig (zig cc) on PATH.  Usage: ./tests/host_river_queue.ps1 [-Rows 60000] [-Cache 226]
param([int]$Rows = 60000, [int]$Cache = 226, [int]$PictureRows = 20000, [int]$JobCols = 10)
$ErrorActionPreference = 'Stop'
if ($Rows -gt 65000) { throw 'river_distance is 16 bits: use -Rows 65000 or fewer' }
$root = Split-Path $PSScriptRoot -Parent
$out = Join-Path $env:TEMP 'river_host_queue'
New-Item -ItemType Directory -Force $out | Out-Null
$inc = @("-I$PSScriptRoot/host", "-I$root/src", '-include', 'stdlib.h', '-include', 'string.h')
$base = @('-std=c11', '-O2', '-w', '-DRIVER_NO_START_ROAD', "-DRIVER_JOB_COLS=$JobCols", '-DRIVER_HOST_DIRECT', "-DTILE_RIVER_COUNT=${Cache}u")
$src = @("$PSScriptRoot/host/tiles_main.c", "$root/src/river.c", "$root/src/atari_data.c")
zig cc @base @inc -o "$out/plain.exe" @src
if ($LASTEXITCODE) { throw 'host build (plain) failed' }
zig cc @base -DPLATFORM_DMG @inc -o "$out/queue.exe" @src
if ($LASTEXITCODE) { throw 'host build (queue) failed' }
$keep = { $_ -notlike 'row *' -and [int]($_ -split ' ')[1] -ge 4 -and [int]($_ -split ' ')[1] -le 27 }
foreach ($variant in 0..3) {
    & "$out/plain.exe" $PictureRows $variant 2>$null | Where-Object $keep | Set-Content "$out/plain.txt"
    & "$out/queue.exe" $PictureRows $variant 2>$null | Where-Object $keep | Set-Content "$out/queue.txt"
    # The queue build's last rows are still waiting when the run stops: compare what it wrote.
    $a = Get-Content "$out/plain.txt"; $b = Get-Content "$out/queue.txt"
    $n = [Math]::Min($a.Count, $b.Count)
    if ($n -lt 1000) { throw "too few writes ($n)" }
    for ($i = 0; $i -lt $n; $i++) {
        if ($a[$i] -ne $b[$i]) { throw "start ${variant}: line ${i} differs: '$($a[$i])' vs '$($b[$i])'" }
    }
    Write-Host "start ${variant}: picture identical ($n cells; plain $($a.Count), queue $($b.Count))"
}
zig cc -std=c11 -O2 -w -DRIVER_HOST_DIRECT "-DRIVER_JOB_COLS=$JobCols" "-DTILE_RIVER_COUNT=${Cache}u" -DPLATFORM_DMG `
    -I"$PSScriptRoot/host" -I"$root/src" -include stdlib.h -include string.h -o "$out/budget.exe" `
    "$PSScriptRoot/host/tile_budget_main.c" "$root/src/river.c" "$root/src/atari_data.c" "$root/src/entity_gfx.c"
if ($LASTEXITCODE) { throw 'host build (budget) failed' }
$bad = 0
foreach ($jitter in 0, 20, 7) {
    foreach ($variant in 0..3) {
        foreach ($lag in 0, 6) {
            $r = & "$out/budget.exe" $Rows $variant 2 82 $lag $jitter | Select-String 'clobbers'
            Write-Host "start $variant jitter $jitter :$r"
            if ("$r" -notmatch ': 0, youngest') { $bad++ }
        }
    }
}
if ($bad) { throw "$bad runs clobbered an on-screen tile" }
