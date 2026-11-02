/*
 * mode_probe.mjs - which machine did Gearboy boot?  Run it on a freshly started (still paused) emulator:
 * at PC $0100 the boot ROM's A register is $11 on a GBC and $01 on a DMG.  get_media_info.is_cgb only
 * says what the header asks for, so it is printed beside it, not used.
 *
 *   node tests/gb/mode_probe.mjs [expect: dmg|gbc]      exit code 1 on a mismatch
 */
import * as c from './gearboy_client.mjs';

const txt = r => r.map(x => x.text || '').join('');
await c.init();
const info = JSON.parse(txt(await c.call('get_media_info', {})));
const cpu = JSON.parse(txt(await c.call('get_cpu_status', {})));
const mode = cpu.PC === '0100' ? (cpu.A === '11' ? 'gbc' : 'dmg') : 'unknown (already running)';
console.log(`mode ${mode} (A=${cpu.A} AF=${cpu.AF} PC=${cpu.PC}); ${info.file_name}, header cgb flag ${info.is_cgb}`);
const want = process.argv[2];
if (want && mode !== want) {
    console.log(`expected ${want}`);
    process.exit(1);
}