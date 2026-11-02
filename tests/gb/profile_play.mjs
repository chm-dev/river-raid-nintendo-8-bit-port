/*
 * profile_play.mjs - profile the GBC ROM in play: boot, Start, invulnerable jet,
 * optional Up (fast speed), then 10 s of profiler data sorted by max.
 *
 *   gearboy/Gearboy.exe --headless --mcp-http --mcp-http-port 7777 roms/river_raid.gb roms/river_raid.noi
 *   node tests/gb/profile_play.mjs fast|cruise [count]
 *
 * Build with .\build_gbc.ps1 -ProfExport so static functions have names.
 * Profiler units are 70,224 per frame at any CPU speed: M-cycles at double
 * speed = units / 2.  The headline number is passes (river_update calls)
 * per frame: 1.0 = 60 Hz, 0.5 = the main loop locked at 30 Hz.
 */
import { readFileSync, writeFileSync } from 'fs';
import * as c from './gearboy_client.mjs';

const mode = process.argv[2] || 'fast';
const count = Number(process.argv[3] || 24);
const txt = r => r.map(x => x.text || '').join('');
const sleep = ms => new Promise(r => setTimeout(r, ms));

await c.init();
await c.call('set_profiler', { action: 'start' });
await c.call('debug_continue');
await sleep(3000);                               /* boot: Start before this is ignored */
/* GB_START_LEVEL=n: Up n times in the attract menu (0..3 = levels 1, 5?, 20, 30: atari_level_descriptors). */
for (let i = 0; i < Number(process.env.GB_START_LEVEL || 0); i++) {
    await c.call('controller_button', { action: 'press', button: 'up', player: 1 });
    await sleep(200);
    await c.call('controller_button', { action: 'release', button: 'up', player: 1 });
    await sleep(200);
}
await c.call('controller_button', { action: 'press', button: 'start', player: 1 });
await sleep(300);
await c.call('controller_button', { action: 'release', button: 'start', player: 1 });
await sleep(4000);
/* _player_invuln: its address moves with every WRAM change, so read the .noi. */
const noi = readFileSync(process.env.GB_NOI || 'roms/river_raid.noi', 'utf8');
const inv = parseInt(/DEF _player_invuln 0x([0-9A-Fa-f]+)/.exec(noi)[1], 16);
await c.call('write_memory', { area: 6, offset: (inv - 0xC000).toString(16).toUpperCase().padStart(4, '0'), bytes: '01' });
if (mode === 'fast') await c.call('controller_button', { action: 'press', button: 'up', player: 1 });
await sleep(2500);
await c.call('set_profiler', { action: 'reset' });
await sleep(10000);
const d = JSON.parse(txt(await c.call('get_profiler_data', { count, sort: 'max' })));
if (process.env.GB_PROF_JSON) writeFileSync(process.env.GB_PROF_JSON, JSON.stringify(d, null, 1));   /* full dump, every field */
const rd = name => c.call('read_memory', { area: 6, offset: (parseInt(new RegExp(`DEF ${name} 0x([0-9A-Fa-f]+)`).exec(noi)[1], 16) - 0xC000)
    .toString(16).toUpperCase().padStart(4, '0'), size: 1 }).then(r => txt(r).replace(/\s+/g, ' '));
console.log('river_clamps', await rd('_river_clamps'));
console.log('river_owed_max', await rd('_river_owed_max'));
if (/DEF _pl_oam_n /.test(noi)) {
    const rd16 = async n => { const o = JSON.parse(txt(await c.call('read_memory', { area: 6, offset: (parseInt(new RegExp(`DEF ${n} 0x([0-9A-Fa-f]+)`).exec(noi)[1], 16) - 0xC000).toString(16).toUpperCase().padStart(4, '0'), size: 2 }))); return parseInt(o.data.replace(/\s+/g, '').match(/../g).reverse().join(''), 16); };
    console.log('oamcheck: passes', await rd16('_pl_oam_n'), 'errors', await rd16('_pl_oam_err'), 'draw errors', await rd16('_pl_oam_draw_err'));
}
const w = d.functions.find(f => f.name === '_river_update');   /* one per pass; vsync() is skipped after an overrun */
console.log(`mode ${mode}: frames ${d.frames}, passes ${w ? w.calls : '?'}, passes/frame ${w ? (w.calls / d.frames).toFixed(2) : '?'}`);
for (const f of d.functions) {
    console.log(f.name.padEnd(26), 'calls', String(f.calls).padStart(5), 'avg', String(f.average_cycles).padStart(7),
        'max', String(f.max_cycles).padStart(7));
}
