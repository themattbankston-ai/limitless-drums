// Runs a whole Hitline 4.8 export headless, with the Mix step switched on (clean-up, tone, shape, tame, inserts,
// bus, shared room, tuning), and records every file it writes. The C++ export is tested against it.
// Usage: serve PARITY_ROOT (holding harness.html and the session folders) on PARITY_PORT, then
//   node golden_export.js <session folder under PARITY_ROOT> <config A|B> <out.json> [answer key json]
// Each file is kept as its byte length, CRC-32, energy per channel and every STEP-th sample per channel; the
// analysis state Hitline exported from (onsets, alignment, tuning, settings) is stored with it.
const { chromium } = require('playwright');
const fs = require('fs'), path = require('path');
const [sessDir, cfgName, out, keyPath] = process.argv.slice(2);
// optional answer key: its alignment is used when Auto prep leaves the mics unaligned in this run
const keyAl = keyPath ? JSON.parse(fs.readFileSync(keyPath, 'utf8')).state.tracks.map(t => ({ mic: t.mic, lag: t.lag, pol: t.pol, ap: t.ap })) : null;
const STEP = 211;
(async () => {
  const b = await chromium.launch(); const pg = await b.newPage();
  // the tame pass splits its hits across (cores - 1) workers; fix the count so the split is reproducible
  await pg.addInitScript(() => Object.defineProperty(navigator, 'hardwareConcurrency', { get: () => 4 }));
  pg.on('console', m => { if (m.type() === 'error') console.log('PAGE', m.text()); });
  pg.on('pageerror', e => console.log('PAGEERR', e.message));
  await pg.goto(`http://localhost:${process.env.PARITY_PORT || 8777}/harness.html`);
  await pg.waitForFunction(() => window.__HL);
  const files = fs.readdirSync(sessDir).filter(f => /\.wav$/i.test(f)).sort();
  const rel = path.relative(process.env.PARITY_ROOT || process.cwd(), sessDir);
  const res = await pg.evaluate(async ({ files, rel, cfgName, STEP, keyAl }) => {
    const H = window.__HL;
    const list = [];
    for (const f of files){ const r = await fetch('/' + rel.split('/').map(encodeURIComponent).join('/') + '/' + encodeURIComponent(f)); const blob = await r.blob(); const file = new File([blob], f); Object.defineProperty(file, '_rel', { value: rel.split('/').pop() + '/' + f }); list.push(file); }
    H.addSessionsFromFiles(list);
    for (let i = 0; i < 600 && !H.S.tracks.length; i++) await new Promise(r => setTimeout(r, 100));
    await new Promise(r => setTimeout(r, 500));
    const ok0 = await H.autoPrep(true);
    const ok = ok0;
    if (!H.S.phase.analyzed && keyAl) H.S.tracks.forEach(tr => { const k = keyAl.find(a => a.mic === tr.mic); if (k){ tr.lag = k.lag; tr.pol = k.pol; tr.ap = k.ap; } });
    const S = H.S, T = n => S.tracks.findIndex(t => t.mic === n);
    const STRIP = () => ({ trim: 0, pol: false, pan: 0, tone: { match: 0, bright: 0, crack: 0, smooth: 0, tame: 0 }, shape: { atk: 0, dec: 0, sus: 0, rel: 0, match: 0 }, inserts: [] });
    S.tracks.forEach(tr => { tr.strip = STRIP(); });
    const band = (type, f, g, q, x = {}) => ({ on: true, type, f, g, q, slope: 24, place: 'st', dyn: { on: false, thr: -30, range: -6, atk: 3, rel: 80 }, ...x });
    const curve = Array.from({ length: 30 }, (_, j) => +(2.5*Math.sin(j/3.1) - 0.08*j).toFixed(2));
    if (cfgName === 'A'){
      // everything on: clean-up, tone, shape, tame, strip and bus inserts, the shared room, tuning, both file kinds
      S.dn.on = true; S.dn.db = 18;
      Object.assign(S.tone, { match: 0.6, bright: 0.3, crack: 0.25, smooth: 0.5, tame: 0.4, repair: true, ref: { name: 'ref', curve }, refAmt: 0.5 });
      Object.assign(S.shape, { atk: 0.4, dec: -0.2, sus: 0.3, rel: -0.3, match: 0.5 });
      const top = S.tracks[T('Top')].strip, bot = S.tracks[T('Bottom')].strip, oh = S.tracks[T('Overheads')].strip, rc = S.tracks[T('Room Close')].strip, rm = S.tracks[T('Room Mono')].strip;
      top.tone.bright = -0.2; top.trim = 2; top.send = -12;
      top.inserts = [{ type: 'eq', on: true, p: { phase: 'min', out: 0, bands: [band('lowcut', 70, 0, 0.71, { slope: 18 }), band('bell', 200, 2, 1.2), band('bell', 900, -2.5, 2), band('bell', 5000, 2.5, 1), band('highshelf', 10000, 2, 0.7)] } },
        { type: 'comp', on: true, p: { model: 'dist', mix: 1, scHpf: 20, out: 0, dist: { input: 6, ratio: '6', attack: 5, release: 2, dist: 'd2', hp: false } } },
        { type: 'sat', on: true, p: { model: 'decap', mix: 0.6, out: 0, auto: true, decap: { drive: 4.5, style: 'E', tone: 1, lowcut: 0, highcut: 0, thump: false, steep: false, punish: false } } }];
      bot.pol = true; bot.shape.atk = 0.2;
      bot.inserts = [{ type: 'gate', on: true, p: { thr: -32, range: -18, ratio: 0, atk: 0.3, hold: 50, rel: 150, hyst: 4, look: 1.5, hpf: 150, lpf: 20000, keyT: T('Top') } },
        { type: 'pultec', on: true, p: { eqp: true, meq: true, lf: 100, lfBoost: 3, lfAtten: 0, hf: 10, bw: 6, hfBoost: 4, hfa: 20, hfAtten: 0, lpf: 300, lpk: 0, dipf: 500, dip: 3, hpf: 3, hpk: 0, tube: true, drive: 1, phase: 'min', out: 0 } }];
      oh.send = -18;
      oh.inserts = [{ type: 'eq', on: true, p: { phase: 'min', out: 0, bands: [band('lowcut', 150, 0, 0.71, { slope: 12, place: 'side' }), band('highshelf', 8000, 2, 0.7, { place: 'side' })] } },
        { type: 'width', on: true, p: { width: 1.3, bassMono: 120, keep: true } }];
      rc.inserts = [{ type: 'comp', on: true, p: { model: '76', mix: 0.5, scHpf: 20, out: 0, '76': { input: 14, ratio: 'all', attack: 7, release: 7 } } }];
      rm.shape.atk = -0.3; rm.tone.smooth = 0.3;
      S.mix.busIns = [{ type: 'eq', on: true, p: { phase: 'min', out: 0, bands: [band('bell', 400, -1.5, 1), band('highshelf', 12000, 1.5, 0.7)] } },
        { type: 'comp', on: true, p: { model: 'chan', mix: 1, scHpf: 90, out: 0, chan: { thresh: -18, ratio: 2, knee: 8, attack: 30, release: 150 } } }];
      S.mix.room = { type: 'verb', on: true, ret: -6, p: { model: 'lex', prog: 'Chamber', decay: 1, pre: 0, early: 1, era: 'Now', hpf: 100, lpf: 14000, width: 1, mix: 1, out: 0 } };
      S.tune.on = true;
      Object.assign(S.exp, { mode: 'both', tailMode: 'natural', tailFloor: -90, bits: 32, rate: 'same', maxLenS: 8 });
    } else {
      // the other paths: the Panda reference set, kit levels with the peak trim, gated tails, 48 kHz 24-bit
      S.dn.on = false;
      Object.assign(S.tone, { match: 0.8, bright: -0.2, crack: 0, smooth: 0.3, tame: 0, repair: false, proSrc: 'panda' });
      Object.assign(S.shape, { atk: 0, dec: 0.3, sus: 0, rel: 0.4, match: 0 });
      S.tracks[T('Room Far')].strip.tone.tame = 0.5;
      S.tracks[T('Overheads')].strip.trim = -3;
      S.tracks[T('Overheads')].strip.inserts = [{ type: 'sat', on: true, p: { model: 'fatso', mix: 1, out: 0, auto: true, fatso: { input: 4, warmth: 3, tranny: true, comp: 'off' } } }];
      S.lev.std = 'kit';
      Object.assign(S.exp, { mode: 'hits', tailMode: 'fade', tailDb: -60, fadeOutMs: 40, fadeInMs: 0.5, bits: 24, rate: '48000', trim: 'auto', ceilDb: -1, maxLenS: 6 });
    }
    await H.runDenoise(); await H.readyMix(); H.computeAll(); await H.readyMix(); H.computeAll();
    // a sink that keeps every file the export writes
    const crcT = new Uint32Array(256); for (let n = 0; n < 256; n++){ let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1; crcT[n] = c >>> 0; }
    const crc = u8 => { let c = 0xFFFFFFFF; for (let i = 0; i < u8.length; i++) c = crcT[(c ^ u8[i]) & 255] ^ (c >>> 8); return (c ^ 0xFFFFFFFF) >>> 0; };
    const got = [];
    const sink = { add: async (p, u8) => {
      const dv = new DataView(u8.buffer, u8.byteOffset, u8.byteLength); let q = 12, fmt = null, data = null;
      while (q + 8 <= u8.length){ const id = String.fromCharCode(u8[q], u8[q+1], u8[q+2], u8[q+3]), sz = dv.getUint32(q + 4, true); if (id === 'fmt ') fmt = q + 8; if (id === 'data'){ data = [q + 8, sz]; break; } q += 8 + sz + (sz & 1); }
      const e = { path: p, bytes: u8.length, crc: crc(u8) };
      if (fmt != null && data){ const nch = dv.getUint16(fmt + 2, true), sr = dv.getUint32(fmt + 4, true), bits = dv.getUint16(fmt + 14, true), bps = bits/8, n = data[1]/(nch*bps);
        const rd = (i, c) => { const o = data[0] + (i*nch + c)*bps; if (bits === 32) return dv.getFloat32(o, true); let v = u8[o] | (u8[o+1] << 8) | (u8[o+2] << 16); if (v & 0x800000) v -= 0x1000000; return v; };
        Object.assign(e, { nch, sr, bits, n, energy: [], s: [] });
        for (let c = 0; c < nch; c++){ let en = 0; const s = []; for (let i = 0; i < n; i++){ const v = rd(i, c); en += v*v; if (i % STEP === 0) s.push(v); } e.energy.push(en); e.s.push(s); } }
      got.push(e); return p; } };
    const r = await H.writeSession(sink, '', null);
    const pick = ['sr', 'refIdx', 'onsets', 'drum', 'artic', 'mode', 'det', 'phase', 'lev', 'preset', 'tune', 'tinfo', 'exp', 'dn', 'tone', 'shape', 'mix', 'rejects'];
    const st = {}; for (const k of pick) st[k] = S[k];
    st.tracks = S.tracks.map(t => ({ mic: t.mic, name: t.name, len: t.len, chans: t.ch.length, alignTo: t.alignTo, lag: t.lag, pol: t.pol, ap: t.ap, clip: t.clip, fader: t.fader, mute: t.mute, lrLock: t.lrLock, lr: t.lr, auto: t.auto ? { asis: t.auto.asis, arrival: t.auto.arrival } : null, strip: t.strip }));
    st.res = { gainsDbT: S.res.gainsDbT, trimDb: S.res.trimDb, regions: S.res.regions, lvlRaw: S.res.lvlRaw };
    return { ok, total: r.total, state: st, files: got, STEP };
  }, { files, rel, cfgName, STEP, keyAl });
  fs.writeFileSync(out, JSON.stringify(res)); console.log(res.ok, res.total, 'files', fs.statSync(out).size, 'bytes');
  await b.close();
})();
