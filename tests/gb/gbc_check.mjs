/*
 * gbc_check.mjs - is the dual ROM, booted as a Game Boy Color, showing what the game means to show?
 * dmg_check.mjs is the DMG counterpart.  References no ROM code produced:
 *   - sprite tiles in VRAM bank 1 against ent_gfx_chr in src/entity_gfx.c (the art, unmodified);
 *   - HUD tiles in bank 1 against assets/atari/extracted/charset.bin, the HUD window's attribute map
 *     against the layout in hud_gbc.c's header comment;
 *   - OAM: every sprite has the bank bit, and a colour palette the generated table gives its object;
 *   - river attribute rows (bank 1): one value per row, 0 / 2 / 3, road rows exactly where the generator
 *     says the road is (river_road_slot), the land colour changing only beside a road;
 *   - colours: every colour on a screenshot is one the old GBC ROM (roms/ref/river_raid.ref.gb, the one the user
 *     accepted) shows through the same emulator (Gearboy colour-corrects, so RGB555 is not compared).
 *     tests/gb/gbc_colours.json is made by `--make-ref` on that ROM.
 *
 *   .\tests\gb\run_emu.ps1 tests/gb/gbc_check.mjs 80 -Rom roms/rr_dual_prof -Machine gbc
 *   (arguments: samples; -Extra --make-ref writes the colour reference, on a -GbcOnly -ProfExport build)
 *
 * Plays with an invulnerable jet at fast speed.  Exit code 1 on any failure.  Needs a -ProfExport ROM
 * (river_road_slot and river_land_alt are statics).
 */
import { readFileSync, writeFileSync, existsSync } from 'fs';
import * as c from './gearboy_client.mjs';
import { decodePng } from './png.mjs';

const samples = Number(process.argv[2] || 80);
const makeRef = process.argv[3] === '--make-ref';
const sleep = ms => new Promise(r => setTimeout(r, ms));
const txt = r => r.map(x => x.text || '').join('');
const noi = readFileSync(process.env.GB_NOI || 'roms/rr_dual_prof.noi', 'utf8');
const sym = n => parseInt(new RegExp(`DEF ${n} 0x([0-9A-Fa-f]+)`).exec(noi)[1], 16);
const hex = v => v.toString(16).toUpperCase().padStart(4, '0');
const read = async (area, offset, size) => JSON.parse(txt(await c.call('read_memory', { area, offset: hex(offset), size })))
    .data.trim().split(/\s+/).map(h => parseInt(h, 16));
const wram = (a, size) => read(6, a - 0xC000, size);
const VRAM1 = 18, VRAM0 = 17, OAM = 7;

let fail = 0;
const bad = msg => { fail++; console.log('FAIL ' + msg); };

await c.init();
const lcd = JSON.parse(txt(await c.call('get_lcd_status', {})));
await c.call('debug_continue');
const bootT0 = Date.now();
for (;;) {
    await sleep(50);
    const f = await wram(sym('_frame_counter'), 2);
    if (f[0] || f[1]) break;
    if (Date.now() - bootT0 > 10000) { console.log('no frame in 10 s'); process.exit(1); }
}
await sleep(1500);

/* The tables the checks below share. */
const chrSrc = readFileSync('src/entity_gfx.c', 'utf8');
const nums = s => s.match(/0x[0-9A-Fa-f]{2}/g).map(h => parseInt(h, 16));
const chr = nums(/ent_gfx_chr\[\d+\] = \{([^}]*)\}/.exec(chrSrc)[1]);
const table = (n, k = 0) => nums([...chrSrc.matchAll(new RegExp(`${n}\\[18\\] = \\{([^}]*)\\}`, 'g'))][k][1]);
const first = table('ent_gfx_first'), count = table('ent_gfx_count'), expFirst = table('ent_exp_first');
const gbcPal = table('ent_gfx_pal', 1);           /* the second: the first is the DMG-only ROM's */
if (JSON.stringify(gbcPal) === JSON.stringify(table('ent_gfx_pal', 0)) || gbcPal[12] !== 0) bad('ent_gfx_pal: GBC table not found');

/* --make-ref runs on the old GBC ROM (a -GbcOnly -ProfExport build): other VRAM layout, no gb_cgb. */
if (!makeRef) {
/* A. the mode */
const cpu = JSON.parse(txt(await c.call('get_cpu_status', {})));
const gbCgb = (await wram(sym('_gb_cgb'), 1))[0];
console.log(`mode: gb_cgb ${gbCgb}, double speed ${cpu.double_speed}, lcd is_cgb ${lcd.is_cgb}`);
if (gbCgb !== 1 || !cpu.double_speed || !lcd.is_cgb) bad('not running as a GBC at double speed');

/* B. sprite tiles, bank 1 */
await c.call('debug_pause');
const sprBank1 = await read(VRAM1, 0, 0x1000);
let wrong = 0;
for (let i = 0; i * 16 < chr.length; i++) {
    for (let b = 0; b < 16; b++) if (sprBank1[(88 + i) * 16 + b] !== chr[i * 16 + b]) { wrong++; break; }
}
console.log(`sprite tiles in bank 1 not matching entity_gfx.c: ${wrong} of ${chr.length / 16}`);
if (wrong) bad('sprite tiles');

/* C. HUD tiles (bank 1 ids 0..35, $9000 mode) and the window attribute map */
const charset = readFileSync('assets/atari/extracted/charset.bin');
const glyph = g => [...charset.subarray(g * 8, g * 8 + 8)];
const hudBank1 = await read(VRAM1, 0x1000, 36 * 16);
const expectTile = (id, rows, both) => {
    for (let r = 0; r < 8; r++) {
        const p0 = both ? rows[r] : 0, p1 = rows[r];
        if (hudBank1[id * 16 + r * 2] !== p0 || hudBank1[id * 16 + r * 2 + 1] !== p1) { bad(`HUD tile ${id} row ${r}`); return; }
    }
};
for (let d = 0; d < 10; d++) { expectTile(1 + d, glyph(80 + d), false); expectTile(26 + d, glyph(80 + d), true); }
expectTile(20, glyph(1 + 4), false); expectTile(21, glyph(1 + 5), false);
expectTile(22, glyph(0x5C), false); expectTile(23, glyph(1 + 1), false);
expectTile(24, glyph(1 + 7), true); expectTile(25, glyph(1 + 8), true);
const winAttr = await read(VRAM1, 0x1C00, 32 * 2), winTile = await read(VRAM0, 0x1C00, 32 * 2);
for (let col = 0; col < 20; col++) {
    const a0 = 0x08 | (col >= 17 ? 4 : 1);
    const a1 = 0x08 | (col === 5 ? 5 : col >= 6 && col < 14 ? 6 : col === 14 ? 4 : 1);
    if (winAttr[col] !== a0 || winAttr[32 + col] !== a1) bad(`window attribute col ${col}: ${winAttr[col].toString(16)} ${winAttr[32 + col].toString(16)}`);
    if (winTile[col] > 35 || winTile[32 + col] > 35) bad(`window tile id col ${col}`);
}
await c.call('debug_continue');
}

/* Play (GB_START_LEVEL = n: Up n times in the attract menu, as profile_play.mjs does) */
for (let i = 0; i < Number(process.env.GB_START_LEVEL || 0); i++) {
    await c.call('controller_button', { action: 'press', button: 'up', player: 1 });
    await sleep(150);
    await c.call('controller_button', { action: 'release', button: 'up', player: 1 });
    await sleep(250);
}
await c.call('controller_button', { action: 'press', button: 'start', player: 1 });
await sleep(300);
await c.call('controller_button', { action: 'release', button: 'start', player: 1 });
await sleep(1200);
await c.call('write_memory', { area: 6, offset: hex(sym('_player_invuln') - 0xC000), bytes: '01' });
await c.call('controller_button', { action: 'press', button: 'up', player: 1 });

const colours = new Map();               /* "r,g,b" -> pixels over all screenshots, playfield and HUD apart */
const hudColours = new Map();
const seen = { rows: 0, roadRows: 0, roads: 0, flips: 0, oam: 0, shots: 0, bridges: 0 };
const typeOfTile = [];
for (let t = 7; t <= 15; t++) {
    for (let k = 0; k < count[t]; k++) typeOfTile.push([first[t] + k, t]);
    if (expFirst[t] !== 255) for (let k = 0; k < count[t]; k++) typeOfTile.push([expFirst[t] + k, t]);
}
for (let n = 0; n < samples; n++) {
    await sleep(n < 6 ? 350 : 500);
    await c.call('debug_pause');
    const scroll = (await wram(sym('_playfield_scroll_y'), 1))[0];
    const scy = (scroll + 82) & 255;
    const roadSlot = (await wram(sym('_river_road_slot'), 1))[0];
    const attrs = await read(VRAM1, 0x1800, 32 * 32), ids = await read(VRAM0, 0x1800, 32 * 32);
    const rowsSeen = [];
    for (let k = 0; k <= 16; k++) {
        const slot = ((scy >> 3) + k) & 31, row = attrs.slice(slot * 32, slot * 32 + 24);
        seen.rows++;
        if (row.some(v => v !== row[0])) bad(`sample ${n} slot ${slot}: attribute differs along the row ${row.join(',')}`);
        if (![0, 2, 3].includes(row[0])) bad(`sample ${n} slot ${slot}: attribute ${row[0]}`);
        const black = ids.slice(slot * 32, slot * 32 + 24).every(v => v === 2);   /* TILE_BLACK: the empty world at the start */
        rowsSeen.push([slot, row[0], black]);
    }
    /* road rows: exactly the generator's pair, when visible */
    const roadSlots = roadSlot === 255 ? [] : [roadSlot, (roadSlot + 31) & 31];
    for (const [slot, a] of rowsSeen) {
        const should = roadSlots.includes(slot);
        if (should !== (a === 3)) bad(`sample ${n} slot ${slot}: attribute ${a}, generator's road slots ${roadSlots}`);
        if (a === 3) seen.roadRows++;
    }
    if (rowsSeen.some(([, a]) => a === 3)) seen.roads++;
    /* land colour: changes only across a road */
    const drawn = rowsSeen.filter(r => !r[2]);
    for (let i = 1; i < drawn.length; i++) {
        const [slot, a] = drawn[i], p = drawn[i - 1][1];
        if (a !== 3 && p !== 3 && a !== p) bad(`sample ${n}: land colour changes ${p} -> ${a} at slot ${slot} with no road between`);
    }
    if (new Set(drawn.filter(r => r[1] !== 3).map(r => r[1])).size > 1) seen.flips++;
    /* OAM */
    const oam = await read(OAM, 0, 160);
    for (let s = 0; s < 40; s++) {
        const y = oam[s * 4], tile = oam[s * 4 + 2], prop = oam[s * 4 + 3];
        if (y === 0 || y >= 160) continue;
        seen.oam++;
        if (!(prop & 8)) bad(`sample ${n} OAM ${s}: bank bit clear (tile ${tile} prop ${prop.toString(16)})`);
        const owners = typeOfTile.filter(([t]) => t + 88 === tile).map(([, ty]) => ty);
        if (owners.length) {
            const okPals = new Set(owners.map(ty => gbcPal[ty]));
            if (owners.includes(12)) okPals.add(1);            /* the balloon's basket */
            if (!okPals.has(prop & 7)) bad(`sample ${n} OAM ${s}: tile ${tile} palette ${prop & 7}, expected ${[...okPals]}`);
        }
    }
    /* colours */
    if (n % 3 === 0) {
        for (const x of await c.call('get_screenshot', {})) {
            if (x.type !== 'image') continue;
            const png = decodePng(Buffer.from(x.data, 'base64'));
            seen.shots++;
            for (let yy = 0; yy < png.h; yy++) for (let xx = 0; xx < png.w; xx++) {
                const k = png.px(xx, yy).join(',');
                const m = yy < 128 ? colours : hudColours;
                m.set(k, (m.get(k) || 0) + 1);
            }
        }
    }
    await c.call('debug_continue');
}
console.log(`river rows ${seen.rows}, road rows ${seen.roadRows} in ${seen.roads} samples, land colour flips seen ${seen.flips} samples, OAM sprites ${seen.oam}, screenshots ${seen.shots}`);
if (!seen.roads) bad('no road was ever on screen: the road check did not run');

const refPath = 'tests/gb/gbc_colours.json';
const norm = m => Object.fromEntries([...m].filter(([, v]) => v >= 4));      /* ignore a stray few pixels */
if (makeRef) {
    const old = existsSync(refPath) ? JSON.parse(readFileSync(refPath, 'utf8')) : { playfield: {}, hud: {} };
    const merge = (a, b) => { for (const [k, v] of Object.entries(b)) a[k] = (a[k] || 0) + v; return a; };      /* several runs add up */
    writeFileSync(refPath, JSON.stringify({ playfield: merge(old.playfield, norm(colours)), hud: merge(old.hud, norm(hudColours)) }, null, 1));
    console.log(`wrote ${refPath}: ${Object.keys(norm(colours)).length} playfield colours, ${Object.keys(norm(hudColours)).length} HUD colours`);
} else {
    const ref = JSON.parse(readFileSync(refPath, 'utf8'));
    for (const [name, m, r] of [['playfield', colours, ref.playfield], ['HUD', hudColours, ref.hud]]) {
        const extra = Object.keys(norm(m)).filter(k => !(k in r));
        const missing = Object.keys(r).filter(k => (m.get(k) || 0) < 4 && r[k] > 400);   /* common ones only */
        console.log(`${name}: ${Object.keys(norm(m)).length} colours, not in the reference ${extra.length ? extra.join(' ') : 'none'}, common reference colours missing ${missing.length ? missing.join(' ') : 'none'}`);
        if (extra.length) bad(`${name} colours the old GBC ROM never shows`);
        if (missing.length) bad(`${name} colours the old GBC ROM shows a lot are missing`);
    }
}
process.exit(fail ? 1 : 0);