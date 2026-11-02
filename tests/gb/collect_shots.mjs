/*
 * collect_shots.mjs - play and save one screenshot per game state, for looking at (nothing is compared):
 * attract, the start road, the first time each entity type is on screen (jets, helicopters, ships, balloon, bridge,
 * fuel, explosions), the bridge flash, low fuel, pause, crash and restart.
 *
 *   .\tests\gb\run_emu.ps1 tests/gb/collect_shots.mjs 70 shots/gbc -Rom roms/rr_dual_prof -Machine gbc
 *   (arguments: flight samples, output prefix; GB_START_LEVEL = n as in profile_play.mjs)
 */
import { readFileSync, writeFileSync } from 'fs';
import * as c from './gearboy_client.mjs';
import { decodePng } from './png.mjs';

const samples = Number(process.argv[2] || 70);
const prefix = process.argv[3] || 'shots/x';
const sleep = ms => new Promise(r => setTimeout(r, ms));
const txt = r => r.map(x => x.text || '').join('');
const noi = readFileSync(process.env.GB_NOI, 'utf8');
const sym = n => parseInt(new RegExp(`DEF ${n} 0x([0-9A-Fa-f]+)`).exec(noi)[1], 16);
const hex = v => v.toString(16).toUpperCase().padStart(4, '0');
const wram = async (a, size) => JSON.parse(txt(await c.call('read_memory', { area: 6, offset: hex(a - 0xC000), size })))
    .data.trim().split(/\s+/).map(h => parseInt(h, 16));
const poke = (s, v) => c.call('write_memory', { area: 6, offset: hex(sym(s) - 0xC000), bytes: v });
const btn = (action, button) => c.call('controller_button', { action, button, player: 1 });
const tap = async b => { await btn('press', b); await sleep(200); await btn('release', b); };
const isFlat = png => { const first = png.px(0, 0).join(); return [...Array(144).keys()].every(y => [0, 80, 159].every(xx => png.px(xx, y).join() === first)); };
const shot = async name => {
    for (const x of await c.call('get_screenshot', {})) {
        if (!x.type || x.type !== 'image') continue;
        const buf = Buffer.from(x.data, 'base64');
        writeFileSync(`${prefix}_${name}.png`, buf);
        if (isFlat(decodePng(buf))) {         /* one colour from top to bottom: the LCD was off, or something is wrong */
            const regs = JSON.parse(txt(await c.call('get_lcd_registers', {})));
            const r = Object.fromEntries(regs.registers.map(([n, , v]) => [n, v]));
            const later = [];
            for (let k = 0; k < 8; k++) {       /* does it last? */
                await sleep(200);
                for (const y of await c.call('get_screenshot', {})) if (y.type === 'image') later.push(isFlat(decodePng(Buffer.from(y.data, 'base64'))) ? 'F' : '.');
            }
            console.log(`BLANK later frames ${later.join('')}`);
            console.log(`BLANK ${name}: player_state ${(await wram(sym('_player_state'), 1))[0]}, ${JSON.stringify(r)}, bridge_flash ${(await wram(sym('_bridge_flash'), 1))[0]}, frame ${(await wram(sym('_frame_counter'), 2)).join(',')}`);
        }
    }
    console.log('shot', name);
};
const NAMES = { 7: 'jet', 8: 'heli_a', 9: 'heli_b', 10: 'heli_c', 11: 'heli_d', 12: 'balloon', 13: 'ship', 14: 'bridge', 15: 'fuel' };

await c.init();
await c.call('debug_continue');
await sleep(3500);
await shot('01_attract');
for (let i = 0; i < Number(process.env.GB_START_LEVEL || 0); i++) { await tap('up'); await sleep(250); }
await tap('start');
await sleep(1400);
await shot('02_start_road');
await poke('_player_invuln', '01');
await btn('press', 'up');
await btn('press', 'a');                    /* fire: enemies explode, bridges fall */
const got = new Set();
let flashShots = 0;
for (let n = 0; n < samples; n++) {
    await sleep(350);
    await c.call('debug_pause');
    const type = await wram(sym('_ent_type'), 11), y = await wram(sym('_ent_y'), 11);
    const flash = (await wram(sym('_bridge_flash'), 1))[0];
    for (let i = 0; i < 11; i++) {
        const t = type[i];
        if (t === 0 || y[i] < 128 || y[i] > 205) continue;          /* well in the view */
        const key = t >= 7 && t <= 15 ? NAMES[t] : (t >= 2 && t <= 6 ? `explosion${t}` : null);
        if (key && !got.has(key)) { got.add(key); await shot(`10_${key}`); }
    }
    if (flash && flashShots < 3) { flashShots++; await shot(`20_bridge_flash${flashShots}`); }
    await c.call('debug_continue');
}
console.log('types seen:', [...got].join(' '));
await btn('release', 'a');
await btn('release', 'up');

/* low fuel (invulnerability refills the tank every pass: off for a moment, flying straight) */
await poke('_player_invuln', '00');
await poke('_hud_fuel', '30');
await sleep(1100);
await shot('30_low_fuel_a');
await sleep(380);
await shot('31_low_fuel_b');
await sleep(380);
await shot('32_low_fuel_c');
await poke('_player_invuln', '01');
await sleep(500);

/* pause */
await tap('start');
await sleep(600);
{
    const regs = JSON.parse(txt(await c.call('get_lcd_registers', {})));
    console.log(`pause: player_state ${(await wram(sym('_player_state'), 1))[0]}, LCDC ${regs.registers[0][2]}`);
}
await shot('40_pause');
await tap('start');
await sleep(600);

/* crash and restart: no invulnerability, steer into the bank */
await poke('_player_invuln', '00');
await btn('press', 'up');
await btn('press', 'left');
await sleep(2600);
await shot('50_crash');
await btn('release', 'left');
await sleep(2500);
await shot('51_after_crash');
await sleep(2500);
await shot('52_restart');