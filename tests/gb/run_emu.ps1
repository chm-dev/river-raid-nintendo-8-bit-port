# Start a fresh headless Gearboy on a ROM, run one node test against it, stop it.
#   .\tests\gb\run_emu.ps1 tests/gb/pass_log.mjs fast
#   .\tests\gb\run_emu.ps1 tests/gb/dmg_check.mjs 30 -Rom roms/river_raid -Machine dmg
# The scripts press Start themselves, so they need a freshly booted ROM every time.
# -Machine gbc|dmg picks the machine a $80 (dual) ROM boots as: ForceDMG in gearboy/config.ini, which
# Gearboy reads only with --portable.  The mode is then confirmed from the boot registers
# (tests/gb/mode_probe.mjs) and the run is skipped on a mismatch.  A DMG-only ROM needs -Machine dmg
# (or none); a GBC-only ROM -Machine gbc.
# Only the emulator this script started is stopped (by PID): other agents may run their own Gearboy.
# It listens on -Port (default 7790) so it does not meet another instance on 7777.
param([Parameter(Mandatory)][string]$Script, [string]$Mode = 'fast', [string]$Extra = '',
      [string]$Rom = 'roms/river_raid', [ValidateSet('gbc', 'dmg')][string]$Machine = 'gbc', [int]$Port = 7790)
Set-Location (Join-Path $PSScriptRoot '..\..')
$cfg = 'gearboy/config.ini'
$force = if ($Machine -eq 'dmg') { 'true' } else { 'false' }
(Get-Content $cfg -Raw) -replace 'ForceDMG = \w+', "ForceDMG = $force" | Set-Content $cfg -NoNewline
$emu = Start-Process -FilePath gearboy/Gearboy.exe -WindowStyle Hidden -PassThru -ArgumentList '--portable', '--headless', '--mcp-http',
    '--mcp-http-port', $Port, "$Rom.gb", "$Rom.noi"
$env:GEARBOY_MCP_URL = "http://127.0.0.1:$Port/mcp"
Start-Sleep 3
try {
    node tests/gb/mode_probe.mjs $Machine 2>&1
    if ($LASTEXITCODE -eq 0) { $env:GB_NOI = "$Rom.noi"; node $Script $Mode $Extra 2>&1 }
} finally {
    Stop-Process -Id $emu.Id -Force -ErrorAction SilentlyContinue
    (Get-Content $cfg -Raw) -replace 'ForceDMG = \w+', 'ForceDMG = false' | Set-Content $cfg -NoNewline
}