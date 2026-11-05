// Renders golden outputs from Hitline 4.8's own DSP code (extracted from hitline-4.8.html) for DSP
// routines the Auto prep key doesn't cover, e.g. the tuning resampler with tuning switched on.
// Usage: node golden_dsp.js <Snare Top wav> <onset sample> <out dir>
const fs = require('fs'), path = require('path');
const html = fs.readFileSync(path.join(__dirname, 'hitline-4.8.html'), 'utf8');
const src = html.slice(html.indexOf('<script id="dsp">') + 17, html.indexOf('</script>', html.indexOf('<script id="dsp">')));
const DSP = new Function(src + '\nreturn DSP;')();
const [wavPath, onsetStr, outDir] = process.argv.slice(2), onset = Number(onsetStr);
const buf = fs.readFileSync(wavPath), w = DSP.parseWav(buf.buffer.slice(buf.byteOffset, buf.byteOffset + buf.length));
const x = w.channels[0], sr = w.sampleRate, pre = Math.round(0.002*sr), outLen = Math.round(0.5*sr);
const cases = [ ['tuned_up_60c', Math.pow(2, 60/1200)], ['tuned_down_150c', Math.pow(2, -150/1200)] ];
const meta = { onset, pre, outLen, sr, lag: 3.25, pol: -1, g: 0.8, lo: onset - pre, hi: onset + Math.round(0.45*sr), fadeN: Math.round(0.003*sr), cases: [] };
for (const [name, step] of cases){
  const out = DSP.renderTuned(x, onset, pre, outLen, meta.lag, meta.pol, meta.g, step, meta.lo, meta.hi, meta.fadeN, 32);
  fs.writeFileSync(path.join(outDir, name + '.f32'), Buffer.from(out.buffer));
  meta.cases.push({ name, step });
}
fs.writeFileSync(path.join(outDir, 'tuned.json'), JSON.stringify(meta, null, 1));
console.log('wrote', meta.cases.length, 'golden renders');
