# build_gbc.ps1 - GBC build (GBDK-2020 sm83) plus the checks AGENTS.md asks for.
#
#   .\build_gbc.ps1                      # default module list
#   .\build_gbc.ps1 -Sources a.c,b.c     # custom list (benchmarks, trial links)
#
# Banked modules: entities, entity_gfx get -Wf-bo1; player gets -Wf-bo2.  The link is MBC5 with 4
# banks.  After the link the script fails the build unless:
#   - the header says $80 (the dual ROM), CGB-only $C0 (-GbcOnly, -River2600) or no CGB flag (-Dmg), MBC5+RAM+BATTERY ($0147 = $1B), 8 KB SRAM ($0149 = 2), the right
#     ROM size code ($0148) and a valid header checksum ($014D);
#   - the file is exactly Banks * 16 KB;
#   - every ROM area in the .map sits inside its bank window (the linker does
#     not reliably fail on overflow, AGENTS.md rule 8) and WRAM stays in range;
#   - the .noi exists and holds the symbols tests read.
param(
    [string[]]$Sources = @(
        'src/gbc/main.c', 'src/gbc/sound_gbc.c', 'src/gbc/save_gbc.c', 'src/gbc/hud_gbc.c', 'src/atari_data.c',
        'src/collision.c', 'src/river.c', 'src/entities.c', 'src/entity_gfx.c', 'src/player.c'),
    [string]$Name = 'river_raid',
    [int]$Banks = 4,
    [string[]]$RequireSymbols = @('_frame_counter', '_gbc_logic_oam'),
    [switch]$ProfExport,
    [switch]$OamCheck,
    [switch]$RiverCheck,        # every river row through the assembly and the C, compared (rchk_err)
    [switch]$PassLog,           # per-pass log for tests/gb/pass_log.mjs (costs ~2k units per pass)
    [switch]$River2600,         # 2600 river: -DRIVER_BLOCKY -DRIVER_NOFEAT -DRIVER_FAST (docs/MOD_2600_SPEC.md)
    [switch]$DmgProxy,          # no cpu_fast(): the DMG CPU proxy (proposal row 8)
    [switch]$Dmg,               # a DMG-only ROM: no CGB flag, PLATFORM_DMG (PLAN_DMG.md)
    [switch]$GbcOnly,           # the original GBC-only ROM (CGB-only header, no queue); -River2600 implies it
    [string[]]$Define = @()     # extra -D flags, e.g. RIVER_GRID=4
)

$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
$dual = -not $Dmg -and -not $GbcOnly -and -not $River2600   # the ROM that ships: GBC and DMG in one
$lcc = Join-Path $PSScriptRoot 'gbdk\bin\lcc.exe'
$objDir = Join-Path $PSScriptRoot 'build\gbc'
$outDir = Join-Path $PSScriptRoot 'roms'
New-Item -ItemType Directory -Force $objDir, $outDir | Out-Null
$rom = Join-Path $outDir "$Name.gb"

$failures = New-Object System.Collections.Generic.List[string]
function Fail([string]$msg) { $failures.Add($msg); Write-Host "  FAIL  $msg" -ForegroundColor Red }
function Pass([string]$msg) { Write-Host "  ok    $msg" }

# ---- compile ----
$objs = @()
foreach ($src in $Sources) {
    $base = [IO.Path]::GetFileNameWithoutExtension($src)
    $obj = Join-Path $objDir "$base.o"
    $a = @('-msm83:gb', '-Isrc')
    if ($base -in 'entities', 'entity_gfx') { $a += '-Wf-bo1' }
    if ($base -eq 'player') { $a += '-Wf-bo2' }
    if ($base -in 'sound_gbc', 'save_gbc') { $a += '-Wf-bo3' }
    if ($ProfExport) { $a += '-DPROF_EXPORT_STATICS' }
    if ($OamCheck) { $a += '-DPROF_OAMCHECK' }
    if ($RiverCheck) { $a += '-DPROF_RIVERCHECK' }
    if ($PassLog) { $a += '-DPROF_PASSLOG' }
    if ($River2600) { $a += '-DRIVER_BLOCKY', '-DRIVER_NOFEAT', '-DRIVER_FAST' }
    if ($DmgProxy) { $a += '-DDMG_PROXY' }
    if ($Dmg) { $a += '-DPLATFORM_DMG' }
    elseif ($dual) { $a += '-DPLATFORM_DMG', '-DPLATFORM_DUAL' }
    foreach ($d in $Define) { $a += "-D$d" }
    # Game logic writes sprites in NES coordinates into gbc_logic_oam; only
    # gbc/main.c sees the real shadow_OAM and translates (camera, +16/+8).
    if ($src -notlike '*gbc/main.c') { $a += '-Dshadow_OAM=gbc_logic_oam' }
    $a += @('-c', '-o', $obj, $src)
    & $lcc @a
    if ($LASTEXITCODE -ne 0) { Write-Host "compile failed: $src" -ForegroundColor Red; exit 1 }
    $objs += $obj
}

# ---- link ----
$l = @('-msm83:gb', '-Wl-j', '-Wl-m', '-Wl-yt0x1B', '-Wl-ya1', "-Wl-yo$Banks") +
     $(if ($Dmg) { @() } elseif ($dual) { @('-Wm-yc') } else { @('-Wm-yC') }) + @('-Wm-ynRIVERRAID', '-o', $rom) + $objs
& $lcc @l
if ($LASTEXITCODE -ne 0) { Write-Host 'link failed' -ForegroundColor Red; exit 1 }

# ---- checks ----
Write-Host "Checks for $rom"
$bytes = [IO.File]::ReadAllBytes($rom)

if ($bytes.Length -eq $Banks * 16384) { Pass "size $($bytes.Length) bytes = $Banks banks" }
else { Fail "size $($bytes.Length) bytes, expected $($Banks * 16384)" }

if ($Dmg) {
    if (($bytes[0x143] -band 0x80) -eq 0) { Pass ('header $0143 = ${0:X2} (no CGB flag)' -f $bytes[0x143]) }
    else { Fail ('header $0143 = ${0:X2}, expected no CGB flag' -f $bytes[0x143]) }
} elseif ($dual) {
    if ($bytes[0x143] -eq 0x80) { Pass 'header $0143 = $80 (GBC and DMG)' }
    else { Fail ('header $0143 = ${0:X2}, expected $80 (GBC and DMG)' -f $bytes[0x143]) }
} elseif ($bytes[0x143] -eq 0xC0) { Pass 'header $0143 = $C0 (CGB only)' }
else { Fail ('header $0143 = ${0:X2}, expected $C0 (CGB only)' -f $bytes[0x143]) }

if ($bytes[0x147] -eq 0x1B) { Pass 'header $0147 = $1B (MBC5+RAM+BATTERY)' }
else { Fail ('header $0147 = ${0:X2}, expected $1B (MBC5+RAM+BATTERY)' -f $bytes[0x147]) }

if ($bytes[0x149] -eq 0x02) { Pass 'header $0149 = $02 (8 KB SRAM)' }
else { Fail ('header $0149 = ${0:X2}, expected $02 (8 KB SRAM)' -f $bytes[0x149]) }

# ROM size code: 0 = 32 KB (2 banks), 1 = 64 KB, 2 = 128 KB ...
$sizeCode = [int][Math]::Log($Banks / 2, 2)
if ($bytes[0x148] -eq $sizeCode) { Pass ('header $0148 = ${0:X2} ({1} banks)' -f $bytes[0x148], $Banks) }
else { Fail ('header $0148 = ${0:X2}, expected ${1:X2} for {2} banks' -f $bytes[0x148], $sizeCode, $Banks) }

$sum = 0
for ($i = 0x134; $i -le 0x14C; $i++) { $sum = ($sum - $bytes[$i] - 1) -band 0xFF }
if ($sum -eq $bytes[0x14D]) { Pass ('header checksum $014D = ${0:X2}' -f $sum) }
else { Fail ('header checksum $014D = ${0:X2}, computed ${1:X2}' -f $bytes[0x14D], $sum) }

# .map: "<area> <addr 8 hex> <size 8 hex> = ..."; addr = bank << 16 | offset.
$map = [IO.Path]::ChangeExtension($rom, '.map')
if (-not (Test-Path $map)) { Fail "missing $map (link with -Wl-m)" }
else {
    $rx = '^(_\w+)\s+([0-9A-Fa-f]{8})\s+([0-9A-Fa-f]{8})\s+='
    $report = @()
    $badAreas = 0
    foreach ($line in Get-Content $map) {
        if ($line -notmatch $rx) { continue }
        $area = $Matches[1]
        if ($area -like '_HEADER*' -or $area -like '_CABS*' -or $area -like '_DABS*') { continue }
        $addr = [Convert]::ToInt32($Matches[2], 16)
        $size = [Convert]::ToInt32($Matches[3], 16)
        if ($size -eq 0) { continue }
        $bank = $addr -shr 16
        $off = $addr -band 0xFFFF
        $end = $off + $size
        if ($area -eq '_HRAM') {
            $ok = ($off -ge 0xFF80) -and ($end -le 0xFFFF)
            $win = 'HRAM $FF80-$FFFE'
        } elseif ($area -match '^_(DATA|BSS|INITIALIZED|HEAP)') {
            $ok = ($off -ge 0xC000) -and ($end -le 0xE000)
            $win = 'WRAM $C000-$DFFF'
        } elseif ($bank -eq 0) {
            $ok = $end -le 0x4000
            $win = 'bank 0 $0000-$3FFF'
        } else {
            $ok = ($bank -lt $Banks) -and ($off -ge 0x4000) -and ($end -le 0x8000)
            $win = "bank $bank `$4000-`$7FFF"
        }
        $line2 = '{0,-14} bank {1} ${2:X4}-${3:X4} ({4} bytes)' -f $area, $bank, $off, ($end - 1), $size
        if ($ok) { $report += $line2 } else { $badAreas++; Fail "$line2 outside $win" }
    }
    if ($badAreas -eq 0) { Pass 'every map area is inside its window' }
    $report | ForEach-Object { Write-Host "          $_" }
}

$noi = [IO.Path]::ChangeExtension($rom, '.noi')
if (-not (Test-Path $noi)) { Fail "missing $noi (link with -Wl-j)" }
else {
    $noiText = Get-Content $noi -Raw
    foreach ($sym in $RequireSymbols) {
        if ($noiText -match "DEF $([regex]::Escape($sym))\s") { Pass "symbol $sym in .noi" }
        else { Fail "symbol $sym missing from .noi" }
    }
}

if ($failures.Count -gt 0) {
    Write-Host "$($failures.Count) check(s) failed" -ForegroundColor Red
    exit 1
}
Write-Host "Built $rom" -ForegroundColor Green

