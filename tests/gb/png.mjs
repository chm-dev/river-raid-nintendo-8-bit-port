/* png.mjs - decode an 8-bit RGB/RGBA PNG (Gearboy screenshots) to {w, h, px(x, y) -> [r, g, b]}. */
import { inflateSync } from 'zlib';

export function decodePng(buf) {
    let pos = 8, w = 0, h = 0, ct = 0, bd = 0;
    const idat = [];
    while (pos < buf.length) {
        const len = buf.readUInt32BE(pos), type = buf.toString('ascii', pos + 4, pos + 8);
        const data = buf.subarray(pos + 8, pos + 8 + len);
        if (type === 'IHDR') { w = data.readUInt32BE(0); h = data.readUInt32BE(4); bd = data[8]; ct = data[9]; }
        if (type === 'IDAT') idat.push(data);
        pos += 12 + len;
    }
    if (bd !== 8 || (ct !== 2 && ct !== 6)) throw new Error(`unsupported PNG: depth ${bd} type ${ct}`);
    const bpp = ct === 6 ? 4 : 3, stride = w * bpp;
    const raw = inflateSync(Buffer.concat(idat)), out = Buffer.alloc(h * stride);
    for (let y = 0; y < h; y++) {
        const f = raw[y * (stride + 1)];
        for (let x = 0; x < stride; x++) {
            const v = raw[y * (stride + 1) + 1 + x];
            const a = x >= bpp ? out[y * stride + x - bpp] : 0;
            const b = y ? out[(y - 1) * stride + x] : 0;
            const c = x >= bpp && y ? out[(y - 1) * stride + x - bpp] : 0;
            let p = 0;
            if (f === 1) p = a; else if (f === 2) p = b; else if (f === 3) p = (a + b) >> 1;
            else if (f === 4) { const pa = Math.abs(b - c), pb = Math.abs(a - c), pc = Math.abs(a + b - 2 * c); p = pa <= pb && pa <= pc ? a : pb <= pc ? b : c; }
            out[y * stride + x] = (v + p) & 255;
        }
    }
    return { w, h, px: (x, y) => [out[y * stride + x * bpp], out[y * stride + x * bpp + 1], out[y * stride + x * bpp + 2]] };
}