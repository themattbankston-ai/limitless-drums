// Golden renders of Hitline 4.8's insert processors, from its own DSP code, on real snare audio.
// Usage: node golden_fx.js <session folder> <out dir>
const fs = require('fs'), path = require('path');
const html = fs.readFileSync(path.join(__dirname, 'hitline-4.8.html'), 'utf8');
const s0 = html.indexOf('<script id="dsp">') + 17, DSP = new Function(html.slice(s0, html.indexOf('</script>', s0)) + '\nreturn DSP;')();
const [dir, outDir] = process.argv.slice(2);
const load = f => { const b = fs.readFileSync(path.join(dir, f)); return DSP.parseWav(b.buffer.slice(b.byteOffset, b.byteOffset + b.length)); };
const top = load('SNARE_CENTER_TOP_RR06_V06.wav'), oh = load('SNARE_CENTER_OH_RR06_V06.wav'), sr = top.sampleRate;
const start = 778093 - 2000, len = Math.round(0.35*sr);
const cut = chs => chs.map(x => x.slice(start, start + len));
const T = cut(top.channels), O = cut(oh.channels);
const cases = [
  ['eq_mono_lin', 'eqRender', T, { phase: 'lin', out: -1.5, bands: [{ type: 'lowcut', f: 70, slope: 24 }, { type: 'bell', f: 220, g: 4, q: 1.2 }, { type: 'highshelf', f: 6000, g: 3, q: 0.7 }, { type: 'notch', f: 900, q: 6 }] }],
  ['eq_stereo_ms_dyn', 'eqRender', O, { phase: 'min', bands: [{ type: 'bell', f: 400, g: -3, q: 1, place: 'mid' }, { type: 'highshelf', f: 8000, g: 4, place: 'side' }, { type: 'bell', f: 2500, g: 0, q: 2, dyn: { on: true, thr: -36, range: -6, atk: 2, rel: 60 } }] }],
  ['pultec', 'pultecRender', T, { lf: 60, lfBoost: 6, lfAtten: 4, hf: 10, hfBoost: 5, bw: 4, hfa: 20, hfAtten: 2, meq: true, lpk: 3, lpf: 200, dip: 4, dipf: 700, hpk: 2, hpf: 5, drive: 1.5 }],
  ['width', 'widthRender', O, { width: 1.6, bassMono: 160, keep: true }],
  ['gate', 'gateRender', T, { thr: -30, hyst: 6, range: -50, hold: 30, atk: 0.2, rel: 90, hpf: 120, lpf: 9000, look: 1.5 }],
  ['comp_dist', 'compRender', T, { model: 'dist', dist: { input: 7, ratio: '6', attack: 4, release: 5, dist: 'd2', hp: true }, mix: 0.8, out: -2 }],
  ['comp_76_all', 'compRender', T, { model: '76', '76': { input: 10, ratio: 'all', attack: 4, release: 6 }, mix: 1 }],
  ['comp_fair', 'compRender', O, { model: 'fair', fair: { input: 4, thresh: 6, tc: 5 }, scHpf: 80 }],
  ['comp_160', 'compRender', T, { model: '160', '160': { thresh: -30, ratio: 5, overEasy: true } }],
  ['sat_decap_A', 'satRender', T, { model: 'decap', decap: { style: 'A', drive: 5, tone: 1.5, lowcut: 60, thump: true, highcut: 14000 }, mix: 0.9 }],
  ['sat_decap_T_punish', 'satRender', O, { model: 'decap', decap: { style: 'T', drive: 3, punish: true, steep: true, lowcut: 40 }, out: -3 }],
  ['sat_devil', 'satRender', T, { model: 'devil', devil: { crush: 6, darkness: 4, lowcut: true }, mix: 0.5 }],
  ['sat_saturn_tape', 'satRender', T, { model: 'saturn', saturn: { style: 'tape', drive: 6, tone: -1 } }],
  ['sat_saturn_transformer', 'satRender', O, { model: 'saturn', saturn: { style: 'transformer', drive: 8 } }],
  ['sat_fatso', 'satRender', T, { model: 'fatso', fatso: { input: 6, warmth: 4, comp: 'gp' }, auto: false, out: -6 }],
  ['verb_emt', 'verbRender', O, { model: 'emt', prog: 'A', decay: 0.6, pre: 12, hpf: 200, lpf: 9000, width: 1.3, mix: 0.3 }],
  ['verb_lex_room', 'verbRender', T, { model: 'lex', prog: 'Room A', decay: 0.4, early: 1.4, mix: 0.25 }],
  ['verb_city', 'verbRender', O, { model: 'city', prog: 'Re-mic', mix: 0.35 }],
  ['verb_vv_hall', 'verbRender', O, { model: 'vv', prog: 'Concert Hall', decay: 0.3, era: '1980s', mix: 0.2 }],
  ['verb_vv_nonlin', 'verbRender', T, { model: 'vv', prog: 'Nonlin', mix: 0.4 }],
  ['chain', 'insRender', T, [{ type: 'gate', p: { thr: -32, range: -30 } }, { type: 'eq', p: { bands: [{ type: 'bell', f: 200, g: 3, q: 1 }] } }, { type: 'comp', p: { model: '76', '76': { input: 8, ratio: '8' } } }, { type: 'sat', on: false, p: {} }, { type: 'sat', p: { model: 'saturn', saturn: { style: 'gentle', drive: 3 } } }]],
  ['comp_chan', 'compRender', T, { model: 'chan', chan: { thresh: -28, ratio: 4, knee: 8, attack: 6, release: 100 } }],
];
const meta = { sr, cases: [] }, bufs = [];
let off = 0;
for (const [name, fn, input, p] of cases){
  const out = fn === 'insRender' ? DSP.insRender(input.map(x => Float32Array.from(x)), sr, p, { trim: -2 }) : DSP[fn](input.map(x => Float32Array.from(x)), sr, p);
  meta.cases.push({ name, fn, input: input === O ? 'oh' : 'top', p, channels: out.length, offset: off, len });
  for (const y of out){ bufs.push(Buffer.from(Float32Array.from(y).buffer)); off += y.length; }
}
meta.start = start; meta.len = len;
fs.writeFileSync(path.join(outDir, 'fx.f32'), Buffer.concat(bufs));
fs.writeFileSync(path.join(outDir, 'fx.json'), JSON.stringify(meta, null, 1));
console.log('wrote', cases.length, 'insert renders');
