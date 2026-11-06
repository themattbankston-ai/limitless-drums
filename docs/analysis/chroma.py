# Sam's chromatic set: for every drum and tuning, the pitch it really rings at against its label, how long it
# rings, how even the round robins are; for every mic, when it hears the hit after close mic 1, polarity,
# and what the sound-based detection calls it.
import sys, os, re, json, numpy as np, soundfile as sf
from multiprocessing import Pool
from scipy.signal import butter, sosfilt
ROOT = sys.argv[1]; GROUP = sys.argv[2]; OUT = sys.argv[3]
NOTES = {'C':0,'C#':1,'D':2,'D#':3,'E':4,'F':5,'F#':6,'G':7,'G#':8,'A':9,'A#':10,'B':11}
def hz(note):
    m = re.match(r'([A-G]#?)(-?\d)', note); return 440 * 2 ** ((NOTES[m.group(1)] + 12 * (int(m.group(2)) + 1) - 69) / 12)
def wavs(d):
    out = []
    for r, _, fs in os.walk(d):
        out += [os.path.join(r, f) for f in fs if f.lower().endswith('.wav') and not f.startswith('._')]
    return sorted(out)
def load(p):
    x, sr = sf.read(p, dtype='float32', always_2d=True); return x.mean(1), sr
def onset(x, sr):
    e = np.abs(x); pk = e.max()
    if pk <= 0: return None
    i = int(np.argmax(e > pk * 0.12)); return i
def pitch(x, sr, lo, hi):
    o = onset(x, sr)
    if o is None: return None
    seg = x[o + int(0.04 * sr): o + int(0.5 * sr)]
    if len(seg) < sr * 0.1: return None
    seg = seg * np.hanning(len(seg)); N = 1 << 18
    S = np.abs(np.fft.rfft(seg, N)); f = np.fft.rfftfreq(N, 1 / sr)
    m = (f >= lo) & (f <= hi)
    return float(f[m][np.argmax(S[m])])
def decay(x, sr):
    e = np.convolve(x * x, np.ones(int(sr * 0.01)) / int(sr * 0.01), 'same'); pk = int(np.argmax(e))
    db = 10 * np.log10(e[pk:] / e[pk] + 1e-12); j = np.argmax(db < -40)
    return float(j / sr) if j else float(len(db) / sr)
def bands(x, sr):
    o = onset(x, sr) or 0; s = x[o:o + int(0.3 * sr)]
    lo = sosfilt(butter(2, 150, 'lp', fs=sr, output='sos'), s); hi = sosfilt(butter(2, 2000, 'hp', fs=sr, output='sos'), s)
    a = np.sum(s * s); return (float(np.sum(lo * lo) / a), float(np.sum(hi * hi) / a)) if a else (0, 0)
def drum(d):
    """one drum: its mics, its tunings (or articulations)"""
    mics = sorted(m for m in os.listdir(d) if os.path.isdir(os.path.join(d, m)))
    lead = next((m for m in mics if m.startswith('Close 1')), mics[0])
    res = dict(drum=os.path.basename(d), group=GROUP, tunings={}, mics={})
    tunings = sorted(t for t in os.listdir(os.path.join(d, lead)) if os.path.isdir(os.path.join(d, lead, t)))
    leadHigh = []
    for t in tunings:
        fs = wavs(os.path.join(d, lead, t)); 
        if not fs: continue
        xs = [load(p) for p in fs]; sr = xs[0][1]
        r = dict(rr=len(xs))
        lv = [20 * np.log10(np.abs(x).max() + 1e-12) for x, _ in xs]; r['rr_level_spread_dB'] = float(max(lv) - min(lv))
        r['ring_s'] = float(np.median([decay(x, sr) for x, _ in xs]))
        m = re.search(r'([A-G]#?\d)', t)
        if m and GROUP in ('kick', 'snare', 'tom'):
            want = hz(m.group(1)); ps = [pitch(x, sr, 30, 600) for x, _ in xs]; ps = [p for p in ps if p]
            if ps:
                p = float(np.median(ps)); r['label'] = m.group(1); r['label_hz'] = want; r['strongest_hz'] = p
                c = 1200 * np.log2(p / want); r['cents_from_label'] = float(c)
                r['rr_pitch_spread_cents'] = float(1200 * np.log2(max(ps) / min(ps))) if len(ps) > 1 else 0.0
        res['tunings'][t] = r
        leadHigh.append(bands(xs[0][0], sr)[1])
    # every mic against close mic 1, on the first round robin of each tuning
    for mic in mics:
        arr, pol, lows, highs = [], [], [], []
        for t in tunings:
            a = wavs(os.path.join(d, lead, t)); b = wavs(os.path.join(d, mic, t))
            if not a or not b: continue
            xa, sr = load(a[0]); xb, _ = load(b[0]); n = min(len(xa), len(xb))
            oa = onset(xa, sr); ob = onset(xb, sr)
            if oa is None or ob is None: continue
            arr.append((ob - oa) / sr * 1000)
            L = int(0.03 * sr); M = int(0.035 * sr)
            if oa - 5 > M and oa + L + M < n:
                A = xa[oa - 5:oa - 5 + L]; B = xb[oa - 5 - M:oa - 5 + L + M]
                c = np.correlate(B, A, 'valid'); pol.append(np.sign(c[int(np.argmax(np.abs(c)))]))
            lf, hf = bands(xb, sr); lows.append(lf); highs.append(hf)
        if not arr: continue
        am = float(np.median(arr)); lf = float(np.median(lows)); hf = float(np.median(highs)); lh = float(np.median(leadHigh)) if leadHigh else 0
        role = 'sub' if lf > 0.75 and hf < 0.008 else 'far' if am > 4.5 else 'bottom' if GROUP != 'kick' and abs(am) < 2 and lh and hf > 1.5 * lh else 'near'
        res['mics'][mic] = dict(arrival_ms=am, arrival_spread_ms=float(np.ptp(arr)), flipped=float(np.mean(np.array(pol) < 0)) if pol else None, low=lf, high=hf, heard_as=role)
    return res
if __name__ == '__main__':
    gdir = next(os.path.join(ROOT, g) for g in os.listdir(ROOT) if g.lower().endswith(GROUP if GROUP != 'tom' else 'toms') or GROUP in g.lower())
    drums = sorted(os.path.join(gdir, d) for d in os.listdir(gdir) if os.path.isdir(os.path.join(gdir, d)))
    only = sys.argv[4] if len(sys.argv) > 4 else None
    if only: drums = [d for d in drums if re.search(only, d)]
    with Pool(4) as p: out = p.map(drum, drums)
    old = json.load(open(OUT)) if os.path.exists(OUT) else []
    names = {r['drum'] for r in out}
    json.dump([r for r in old if r['drum'] not in names] + out, open(OUT, 'w'), indent=1)
    for r in out:
        cs = [v['cents_from_label'] for v in r['tunings'].values() if 'cents_from_label' in v]
        print(f"{r['drum']}: {len(r['tunings'])} tunings", f"pitch vs label median {np.median(cs):+.0f}c (range {min(cs):+.0f}..{max(cs):+.0f})" if cs else "")
        for m, v in r['mics'].items(): print(f"   {m:28s} {v['arrival_ms']:+6.2f} ms {'flip' if (v['flipped'] or 0) > .5 else '    '} low {v['low']:.2f} high {v['high']:.3f} -> {v['heard_as']}")
