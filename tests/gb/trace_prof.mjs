/*
 * trace_prof.mjs - record a cycle-exact CPU trace of the ROM in play to disk.  The function
 * profiler cannot see into banked calls (they enter through `jp hl`, so entities_update and
 * player_update are charged to .call_hl); the trace can.  Analyse with tools/trace_hist.py.
 *
 *   gearboy/Gearboy.exe --headless --mcp-http --mcp-http-port 7777 roms/X.gb roms/X.noi
 *   $env:GB_NOI='roms/X.noi'; node tests/gb/trace_prof.mjs fast|cruise <out dir> [seconds]
 *
 * A second of fast flight is ~30 MB at the proxy clock.
 */
import { readFileSync } from 'fs';
import { resolve } from 'path';
import * as c from './gearboy_client.mjs';

const mode = process.argv[2] || 'fast';
const out = resolve(process.argv[3]);
const secs = Number(process.argv[4] || 6);
const sleep = ms => new Promise(r => setTimeout(r, ms));

await c.init();
await c.call('debug_continue');
await sleep(3000);                               /* boot: Start before this is ignored */
for (let i = 0; i < Number(process.env.GB_START_LEVEL || 0); i++) {               /* as in profile_play.mjs */
    await c.call('controller_button', { action: 'press', button: 'up', player: 1 });
    await sleep(200);
    await c.call('controller_button', { action: 'release', button: 'up', player: 1 });
    await sleep(200);
}
await c.call('controller_button', { action: 'press', button: 'start', player: 1 });
await sleep(300);
await c.call('controller_button', { action: 'release', button: 'start', player: 1 });
await sleep(4000);
const noi = readFileSync(process.env.GB_NOI || 'roms/river_raid.noi', 'utf8');
const inv = parseInt(/DEF _player_invuln 0x([0-9A-Fa-f]+)/.exec(noi)[1], 16);
await c.call('write_memory', { area: 6, offset: (inv - 0xC000).toString(16).toUpperCase().padStart(4, '0'), bytes: '01' });
if (mode === 'fast') await c.call('controller_button', { action: 'press', button: 'up', player: 1 });
await sleep(2500);
await c.call('set_trace_log', { enabled: true, output: 'disk', output_path: out, disk_size: '1GB', filters: ['cpu.instructions'] });
await sleep(secs * 1000);
console.log(JSON.stringify(await c.call('set_trace_log', { enabled: false })));
