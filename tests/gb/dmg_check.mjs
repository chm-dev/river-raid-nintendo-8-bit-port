/*
 * dmg_check.mjs - is the -Dmg ROM, or the dual ROM booted as a DMG (run_emu.ps1 -Machine dmg), really running as a
 * DMG, and does its VRAM hold what the game means to show?  gbc_check.mjs is the GBC counterpart.  The
 * references are ones no ROM code produced:
 *   - the HUD tiles (bank 0 ids 229..254) against assets/atari/extracted/charset.bin;
 *   - sprite tiles: every logic id, through dmg_tile_map, against ent_gfx_chr in src/entity_gfx.c;
 *   - the river: the map cells on screen, through the tile patterns, against the edges collision
 *     uses (river_left_x / river_right_x for that ring slot, written by the generator).  A cell
 *     showing a stale or clobbered tile has its outline in the wrong place.
 *
 *   gearboy/Gearboy.exe --headless --mcp-http --mcp-http-port 7777 roms/X.gb roms/X.noi
 *   $env:GB_NOI='roms/X.noi'; node tests/gb/dmg_check.mjs [samples]
 *
 * Plays with an invulnerable jet at fast speed.  Exit code 1 on any failure.
 */
import { readFileSync } from 'fs';
import * as c from './gearboy_client.mjs';

const samples = Number(process.argv[2] || 30);
const sleep = ms => new Promise(r => setTimeout(r, ms));
const txt = r => r.map(x => x.text || '').join('');
const noi = readFileSync(process.env.GB_NOI || 'roms/river_raid_dmg.noi', 'utf8');
const sym = n => parseInt(new RegExp(`DEF ${n} 0x([0-9A-Fa-f]+)`).exec(noi)[1], 16);
const hex = v => v.toString(16).toUpperCase().padStart(4, '0');
const read = async (area, offset, size) => JSON.parse(txt(await c.call('read_memory', { area, offset: hex(offset), size })))
    .data.trim().split(/\s+/).map(h => parseInt(h, 16));
const wram = (a, size) => read(6, a - 0xC000, size);

await c.init();
const areas = txt(await c.call('list_memory_areas', {}));
const vramArea = JSON.parse(areas).areas.find(a => /vram/i.test(a.name)).id;
const vram = (a, size) => read(vramArea, a - 0x8000, size);
const bgTile = async id => vram((id < 128 ? 0x9000 : 0x8000) + id * 16, 16);

let fail = 0;
const info = txt(await c.call('get_media_info', {}));
console.log(info.split('\n').filter(l => /cgb|gbc|dmg|color|model|type/i.test(l)).join(' | ').slice(0, 300));


await c.call('debug_continue');
/* Boot done: the main loop counts frames.  How long it takes is a player-visible blank screen. */
const bootT0 = Date.now();
for (;;) {
    await sleep(50);
    const f = await wram(sym('_frame_counter'), 2);
    if (f[0] || f[1]) break;
    if (Date.now() - bootT0 > 10000) { console.log('no frame in 10 s'); process.exit(1); }
}
console.log(`boot ${Date.now() - bootT0} ms`);
/* The machine, from the ROM's own flag when it has one (the dual ROM) and the CPU's speed. */
const cpu = JSON.parse(txt(await c.call('get_cpu_status', {})));
if (/DEF _gb_cgb /.test(noi)) {
    const f = await wram(sym('_gb_cgb'), 1);
    console.log(`gb_cgb ${f[0]}, double speed ${cpu.double_speed}`);
    if (f[0] !== 0 || cpu.double_speed) { console.log('FAIL not running as a DMG'); fail++; }
}
await sleep(2000);

/* Sprite tiles */
const chrSrc = readFileSync('src/entity_gfx.c', 'utf8');
const chr = /ent_gfx_chr\[\d+\] = \{([^}]*)\}/.exec(chrSrc)[1].match(/0x[0-9A-Fa-f]{2}/g).map(h => parseInt(h, 16));
await c.call('debug_pause');
const map = await wram(sym('_dmg_tile_map'), 256);
const used = (await wram(sym('_dmg_spr_used'), 1))[0];
const spr = await vram(0x8000, 0x1000);
/* Colour indexes the DMG wants per object: balloon 1 -> 2, 2 -> 3; enemy jet and its explosion 2 -> 3. */
const table = n => new RegExp(`${n}\\[18\\] = \\{([^}]*)\\}`).exec(chrSrc)[1].match(/0x[0-9A-Fa-f]{2}/g).map(h => parseInt(h, 16));
const first = table('ent_gfx_first'), count = table('ent_gfx_count'), expFirst = table('ent_exp_first');
const BALLOON = 12, ENEMY_JET = 7;
let wrong = 0;
for (let i = 0; i * 16 < chr.length; i++) {
    const hw = map[88 + i];
    if (hw >= 128 && hw !== 255) wrong++;
    const balloon = i >= first[BALLOON] && i < first[BALLOON] + count[BALLOON];
    const jet = i === first[ENEMY_JET] || (i >= expFirst[ENEMY_JET] && i < expFirst[ENEMY_JET] + 5);
    for (let l = 0; l < 8; l++) {
        for (let x = 0; x < 8; x++) {
            const px = (lo, hi) => ((lo >> x) & 1) | (((hi >> x) & 1) << 1);
            let want = px(chr[i * 16 + l * 2], chr[i * 16 + l * 2 + 1]);
            if (balloon && want) want++;
            if (jet && want === 2) want = 3;
            if (px(spr[hw * 16 + l * 2], spr[hw * 16 + l * 2 + 1]) !== want) { wrong++; l = 8; break; }
        }
    }
}
/* HUD tiles: ink in both planes, ids 229.. (digits 230..239, E F plane B 249..252, HI 253 254) */
const charset = readFileSync('assets/atari/extracted/charset.bin');
const hudWant = [[230, 80], [231, 81], [232, 82], [233, 83], [234, 84], [235, 85], [236, 86], [237, 87], [238, 88], [239, 89],
                 [249, 5], [250, 6], [251, 0x5C], [252, 2], [253, 8], [254, 9]];
let hudWrong = 0;
for (const [id, g] of hudWant) {
    const t = await bgTile(id);
    for (let r = 0; r < 8; r++) {
        const want = charset[(g + Number(process.env.DMG_CHECK_NEG || 0)) * 8 + r];     /* DMG_CHECK_NEG=1: negative control */
        if (t[r * 2] !== want || t[r * 2 + 1] !== want) { hudWrong++; break; }
    }
}
console.log(`HUD tiles not matching charset.bin: ${hudWrong} of ${hudWant.length}`);
if (hudWrong) fail++;
console.log(`sprite tiles allocated ${used} (budget 129), entity tiles not matching ${wrong} of ${chr.length / 16}`);
if (used > 129 || wrong) fail++;
await c.call('debug_continue');

/* Play */
await c.call('controller_button', { action: 'press', button: 'start', player: 1 });
await sleep(300);
await c.call('controller_button', { action: 'release', button: 'start', player: 1 });
await sleep(4000);
await c.call('write_memory', { area: 6, offset: hex(sym('_player_invuln') - 0xC000), bytes: '01' });
await c.call('controller_button', { action: 'press', button: 'up', player: 1 });
await sleep(1500);

/* River: for each map row well inside the view, in each sample */
let rows = 0, bad = 0;
const tileCache = new Map();
for (let n = 0; n < samples; n++) {
    await sleep(500);
    await c.call('debug_pause');
    tileCache.clear();
    const scroll = (await wram(sym('_playfield_scroll_y'), 1))[0];
    const scy = (scroll + 82) & 255;
    const left = await wram(sym('_river_left_x'), 32), right = await wram(sym('_river_right_x'), 32);
    const isl = await wram(sym('_river_flags'), 32);
    const mapBytes = await vram(0x9800, 32 * 32);
    /* Whole rows only, away from the top: the bottom line of the view shows row (scy + 127) >> 3 */
    for (let k = 1; k < 15; k++) {
        const slot = ((scy >> 3) + k) & 31;
        if (right[slot] === 0 || isl[slot]) continue;     /* black start rows; islands and features have more edges */
        const line = [];
        for (let col = 0; col < 24; col++) {
            const id = mapBytes[slot * 32 + col];
            if (!tileCache.has(id)) tileCache.set(id, await bgTile(id));
            const t = tileCache.get(id);
            /* The 4th of the tile's 8 lines is the raw row river_left_x samples (block index 3 -> line 4). */
            const lo = t[4 * 2], hi = t[4 * 2 + 1];
            for (let b = 7; b >= 0; b--) line.push(((lo >> b) & 1) | (((hi >> b) & 1) << 1));
        }
        /* Water is colour 3, so the widest run of colour 3 has to span the row's channel. */
        let best = 0, bestAt = -1, run = 0;
        for (let x = 0; x <= line.length; x++) {
            if (x < line.length && line[x] === 3) run++;
            else { if (run > best) { best = run; bestAt = x - run; } run = 0; }
        }
        rows++;
        const mid = (left[slot] + right[slot]) >> 1;
        if (!(bestAt <= mid && mid < bestAt + best)) {
            bad++;
            if (bad < 6) console.log(`sample ${n} slot ${slot}: water ${bestAt}..${bestAt + best} but edges ${left[slot]}..${right[slot]}`);
        }
    }
    await c.call('debug_continue');
}
console.log(`river rows checked ${rows}, wrong ${bad}`);
if (!rows || bad) fail++;
if (fail) process.exit(1);
