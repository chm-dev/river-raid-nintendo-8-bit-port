/*
 * montage.mjs - tile PNG screenshots into one sheet, in file-name order, for looking at.
 *   node tests/gb/montage.mjs out.png columns in1.png in2.png ...
 */
import { readFileSync, writeFileSync } from 'fs';
import { deflateSync } from 'zlib';
import { decodePng } from './png.mjs';

const [out, colsArg, ...files] = process.argv.slice(2);
const cols = Number(colsArg), pad = 4, W = 160, H = 144;
const rows = Math.ceil(files.length / cols);
const sw = cols * (W + pad) + pad, sh = rows * (H + pad) + pad;
const img = Buffer.alloc(sw * sh * 3, 0x30);
files.forEach((f, i) => {
    const p = decodePng(readFileSync(f)), ox = pad + (i % cols) * (W + pad), oy = pad + Math.floor(i / cols) * (H + pad);
    for (let y = 0; y < p.h; y++) for (let x = 0; x < p.w; x++) img.set(p.px(x, y), ((oy + y) * sw + ox + x) * 3);
});
const crcT = Array.from({ length: 256 }, (_, n) => { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1; return c >>> 0; });
const crc = b => { let c = 0xFFFFFFFF; for (const v of b) c = crcT[(c ^ v) & 255] ^ (c >>> 8); return (c ^ 0xFFFFFFFF) >>> 0; };
const chunk = (t, d) => { const b = Buffer.concat([Buffer.from(t), d]), l = Buffer.alloc(4), k = Buffer.alloc(4); l.writeUInt32BE(d.length); k.writeUInt32BE(crc(b)); return Buffer.concat([l, b, k]); };
const raw = Buffer.alloc((sw * 3 + 1) * sh);
for (let y = 0; y < sh; y++) img.copy(raw, y * (sw * 3 + 1) + 1, y * sw * 3, (y + 1) * sw * 3);
const ihdr = Buffer.alloc(13); ihdr.writeUInt32BE(sw, 0); ihdr.writeUInt32BE(sh, 4); ihdr[8] = 8; ihdr[9] = 2;
writeFileSync(out, Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', ihdr), chunk('IDAT', deflateSync(raw)), chunk('IEND', Buffer.alloc(0))]));
console.log(`${out}: ${files.length} images, ${sw}x${sh}`);