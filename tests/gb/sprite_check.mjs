/*
 * sprite_check.mjs - does the hardware OAM show the entities where the game logic says they are?
 * The ground truth is ent_type/ent_x/ent_y, which no sprite writer produces, so a mistake shared by
 * the C and the assembly shows here (the -OamCheck ROM compares those two with each other only).
 *
 *   gearboy/Gearboy.exe --headless --mcp-http --mcp-http-port 7777 roms/X.gb roms/X.noi
 *   $env:GB_NOI='roms/X.noi'; node tests/gb/sprite_check.mjs [samples] [out.png]
 *
 * Plays with an invulnerable jet at fast speed; exit code 1 on any miss.  Needs the player ROM's
 * .noi only.  The jet (logic slot 0, hardware slot 0) gives the X offset of the camera.
 */
import { readFileSync, writeFileSync } from 'fs';
import * as c from './gearboy_client.mjs';

const samples = Number(process.argv[2] || 12);
const png = process.argv[3];
const sleep = ms => new Promise(r => setTimeout(r, ms));
const txt = r => r.map(x => x.text || '').join('');
const noi = readFileSync(process.env.GB_NOI || 'roms/river_raid.noi', 'utf8');
const sym = n => parseInt(new RegExp(`DEF ${n} 0x([0-9A-Fa-f]+)`).exec(noi)[1], 16);
const hex = v => v.toString(16).toUpperCase().padStart(4, '0');
const bytes = async (a, size) => JSON.parse(txt(await c.call('read_memory', { area: 6, offset: hex(a - 0xC000), size })))
    .data.trim().split(/\s+/).map(h => parseInt(h, 16));
const JET = 7, FUEL = 15, VIEW_TOP = 112, VIEW_BOTTOM = 232, TILE_BASE = 88;
/* DMG ROM: logic tile ids go through dmg_tile_map, so check the hardware id against the mapped set. */
const dmgMap = /DEF _dmg_tile_map /.test(noi);

await c.init();
await c.call('debug_continue');
await sleep(3000);
await c.call('controller_button', { action: 'press', button: 'start', player: 1 });
await sleep(300);
await c.call('controller_button', { action: 'release', button: 'start', player: 1 });
await sleep(4000);
await c.call('write_memory', { area: 6, offset: hex(sym('_player_invuln') - 0xC000), bytes: '01' });
await c.call('controller_button', { action: 'press', button: 'up', player: 1 });
await sleep(1500);

let entityTile = t => t >= TILE_BASE;
if (dmgMap) {
    const map = await bytes(sym('_dmg_tile_map'), 256);
    const set = new Set(map.slice(TILE_BASE, TILE_BASE + 141));
    entityTile = t => set.has(t);
}

let checked = 0, missed = 0;
for (let n = 0; n < samples; n++) {
    await sleep(700);
    await c.call('debug_pause');
    const type = await bytes(sym('_ent_type'), 11), ex = await bytes(sym('_ent_x'), 11), ey = await bytes(sym('_ent_y'), 11);
    const jetX = (await bytes(sym('_gbc_logic_oam'), 2))[1];
    /* Both pages, as either may be the one that matches this moment of the pass. */
    const pages = [await bytes(0xC000, 160), await bytes(0xD000, 160)];
    for (let i = 0; i < 11; i++) {
        /* Well inside the view only: at the edges a sprite is legitimately culled. */
        if (type[i] < JET || type[i] > FUEL || ey[i] < VIEW_TOP + 8 || ey[i] >= VIEW_BOTTOM - 32) continue;
        checked++;
        const hit = pages.some(p => {
            const dx = p[1] - jetX;             /* hardware x - logic x, from the jet */
            for (let s = 2; s < 40; s++) {
                const y = p[s * 4], x = p[s * 4 + 1], tile = p[s * 4 + 2];
                const offs = (x - ((ex[i] + dx) & 255)) & 255;
                if (entityTile(tile) && offs % 8 === 0 && offs <= 24 && Math.abs(y - (ey[i] - 96)) <= 32) return true;
            }
            return false;
        });
        if (!hit) {
            missed++;
            console.log(`sample ${n}: slot ${i} type ${type[i]} at (${ex[i]},${ey[i]}) has no sprite`);
        }
    }
    if (png && n === samples - 1) {
        for (const x of await c.call('get_screenshot', {})) {
            if (x.type === 'image') writeFileSync(png, Buffer.from(x.data, 'base64'));
        }
    }
    await c.call('debug_continue');
}
console.log(`entities checked ${checked}, without a sprite ${missed}`);
if (checked === 0 || missed) process.exit(1);
