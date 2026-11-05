// Runs Hitline 4.8 headless on one session folder and writes its full analysis state as JSON:
// the answer key the C++ port is tested against.
// Usage: serve PARITY_ROOT (holding harness.html and the session folders) on PARITY_PORT, then
//   node run.js <session folder under PARITY_ROOT> <out.json>
const { chromium } = require('playwright');
const fs = require('fs'), path = require('path');
const sessDir = process.argv[2], out = process.argv[3];
(async () => {
  const b = await chromium.launch(); const pg = await b.newPage();
  pg.on('console', m => { if (m.type() === 'error') console.log('PAGE', m.text()); });
  pg.on('pageerror', e => console.log('PAGEERR', e.message));
  await pg.goto(`http://localhost:${process.env.PARITY_PORT || 8777}/harness.html`);
  await pg.waitForFunction(() => window.__HL);
  const files = fs.readdirSync(sessDir).filter(f => /\.wav$/i.test(f)).sort();
  const rel = path.relative(process.env.PARITY_ROOT || process.cwd(), sessDir);
  const res = await pg.evaluate(async ({ files, rel }) => {
    const H = window.__HL;
    const list = [];
    for (const f of files){ const r = await fetch('/' + rel.split('/').map(encodeURIComponent).join('/') + '/' + encodeURIComponent(f)); const blob = await r.blob(); const file = new File([blob], f); Object.defineProperty(file, '_rel', { value: rel.split('/').pop() + '/' + f }); list.push(file); }
    H.addSessionsFromFiles(list);
    for (let i = 0; i < 600 && !H.S.tracks.length; i++) await new Promise(r => setTimeout(r, 100));
    await new Promise(r => setTimeout(r, 500));
    const t0 = performance.now();
    const ok = await H.autoPrep(true);
    const S = H.S;
    const big = (k, v) => {
      if (v && (ArrayBuffer.isView(v) || (Array.isArray(v) && v.length > 4000 && typeof v[0] === 'number'))){
        const a = Array.from(v); if (a.length <= 4000) return a;
        let sum = 0, sq = 0; for (const x of a){ sum += x; sq += x * x; }
        return { _len: a.length, _sum: sum, _rms: Math.sqrt(sq / a.length), _head: a.slice(0, 8) };
      }
      if (v instanceof AudioBuffer || (typeof v === 'function')) return undefined;
      return v;
    };
    const pick = ['tracks','sr','refIdx','onsets','drum','artic','mode','det','phase','groupAnchors','lev','preset','tune','pitch','pitchEst','tinfo','exp','dn','tone','mix','shape','res','check','rejects'];
    const o = {}; for (const k of pick) o[k] = S[k];
    return JSON.parse(JSON.stringify({ ok, ms: performance.now() - t0, state: o }, big));
  }, { files, rel });
  fs.writeFileSync(out, JSON.stringify(res, null, 1)); console.log(res.ok, Math.round(res.ms), 'ms', res.state.onsets.length, 'onsets');
  await b.close();
})();
