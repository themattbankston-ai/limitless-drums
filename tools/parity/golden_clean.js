// Golden outputs of Hitline 4.8's clean-up, tone/shape and tail routines, from its own DSP code, on the
// first hits of a real session. Long outputs are stored every STEP-th sample plus their energy.
// Usage: node golden_clean.js <session folder> <onsets json (answer key)> <out dir>
const fs = require('fs'), path = require('path');
const html = fs.readFileSync(path.join(__dirname, 'hitline-4.8.html'), 'utf8');
const s0 = html.indexOf('<script id="dsp">') + 17, DSP = new Function(html.slice(s0, html.indexOf('</script>', s0)) + '\nreturn DSP;')();
const [dir, keyPath, outDir] = process.argv.slice(2);
const key = JSON.parse(fs.readFileSync(keyPath, 'utf8')).state;
const load = f => { const b = fs.readFileSync(path.join(dir, f)); return DSP.parseWav(b.buffer.slice(b.byteOffset, b.byteOffset + b.length)); };
const sr = 44100, NH = 8, onsAll = key.onsets, end = onsAll[NH] - 200, ons = onsAll.slice(0, NH);
const cut = w => w.channels.map(x => x.slice(0, end));
const top = cut(load('SNARE_CENTER_TOP_RR06_V06.wav')), oh = cut(load('SNARE_CENTER_OH_RR06_V06.wav')), far = cut(load('SNARE_CENTER_ROOM-FAR_RR06_V06.wav'));
const STEP = 97, out = { sr, end, ons, STEP, cases: {} };
const pack = chs => chs.map(y => { let e = 0; for (const v of y) e += v*v; const s = []; for (let i = 0; i < y.length; i += STEP) s.push(y[i]); return { len: y.length, energy: e, s }; });
// noise print and denoise (room far has the most floor)
const np = DSP.noisePrint(far[0], sr, ons);
out.cases.noisePrint = np ? { runs: np.runs, floorDb: np.floorDb, trust: np.trust, hum: np.hum, psdHead: Array.from(np.psd.slice(0, 400)) } : null;
out.cases.denoise = pack([DSP.denoise(far[0], sr, np, { reductionDb: 14, oversub: 1.6, releaseS: 0.08 })]);
// tails on hit 3 of every mic together
const h = 3, a = ons[h] - 88, b = ons[h + 1] - 88, seg = [...top, ...oh, ...far].map(x => x.slice(a, b));
const plan = DSP.tailPlan(seg, sr, { floorRel: -60 }); out.cases.tailPlan = plan; out.cases.applyTail = pack(DSP.applyTail(seg, plan, sr));
// click repair on a copy with clicks added in the tails
const clicked = Float32Array.from(top[0]); for (let k = 0; k < NH; k++){ const p = ons[k] + Math.round(0.3*sr); clicked[p] += 0.3; clicked[p + 1] -= 0.25; }
const fixedX = Float32Array.from(clicked); out.cases.repairCount = DSP.repairClicks(fixedX, sr, ons); out.cases.repair = pack([fixedX]);
out.cases.clickedInput = 'top[0] + 0.3/-0.25 at onset+0.3 s, samples p and p+1';
// bleed gate on the overheads
out.cases.bleedGate = pack(DSP.bleedGate(oh, sr, 12));
// tone and shape pass: two kernels, a cut, a shape table, loudness match
const K = 2048, bins = K/2 + 1, mk = fn => Array.from({ length: bins }, (_, k) => fn(k*sr/K));
const kern = [mk(f => 3*Math.exp(-Math.pow(Math.log2(f/220), 2)*4) - 2*Math.exp(-Math.pow(Math.log2(f/4000), 2)*3)), mk(f => -6*Math.exp(-Math.pow(Math.log2(f/900), 2)*40))];
const env = Array.from({ length: 800 }, (_, i) => i < 40 ? 1 + i/40*0.4 : 1.4*Math.exp(-(i - 40)/1200));
const fxPlan = { K, ons, kern, tone: ons.map((_, i) => i % 3 === 2 ? -1 : 0), cut: ons.map((_, i) => i % 2 ? 1 : -1), hitK: ons.map((_, i) => 0.5 + 0.1*i), spare: 0.3, rampMs: 20, env, envStepMs: 0.25, envT0: -1, comp: { win: Math.round(0.06*sr), lag: 0 }, repair: true };
out.cases.fxPlan = fxPlan; out.cases.fx = pack(DSP.fxRenderMulti(oh.map(x => Float32Array.from(x)), sr, fxPlan));
// linked taming across three mics
const tame = DSP.tameMulti([top, oh, far], sr, { ons, bnd: [...ons.slice(1), end], lag: [0, 22.7, 1254], depth: 0.6, sharp: 0.4 });
out.cases.tame = { gains: tame.gains, curveHead: Array.from(tame.curve.slice(0, 200)), mo: tame.mo, tracks: tame.tracks.map(pack) };
// resonance scan and library measurements
const rs = DSP.resScan(top[0], ons, [...ons.slice(1), end], sr, { ring: true, autoProtect: true });
out.cases.resScan = rs;
out.cases.bandSpec = DSP.bandSpec(top[0], ons[2], sr); out.cases.envStats = DSP.envStats(top[0], ons[2], sr);
fs.writeFileSync(path.join(outDir, 'clean.json'), JSON.stringify(out));
console.log('wrote clean goldens', fs.statSync(path.join(outDir, 'clean.json')).size, 'bytes');
