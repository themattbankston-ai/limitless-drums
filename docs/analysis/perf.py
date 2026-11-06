# Full drum performance analysis: per mic what it is (by sound), hits per drum with bleed rejected,
# how hard each hit is, and when every other mic hears the kick and snare (and whether that holds all song).
import sys, os, re, json, numpy as np, soundfile as sf
from scipy.signal import butter, sosfiltfilt, sosfilt
folder, out = sys.argv[1], sys.argv[2]
files = sorted(f for f in os.listdir(folder) if f.lower().endswith(('.wav','.aif','.aiff')) and not f.startswith('._'))
want = re.compile(sys.argv[3], re.I) if len(sys.argv) > 3 else None
if want: files = [f for f in files if want.search(f)]
# a Pro Tools folder holds regions of every length: keep the full-length drum tracks (the biggest group of one length)
import collections
dur = {f: sf.info(os.path.join(folder, f)).frames for f in files}
grp = collections.defaultdict(list)
for f, d in dur.items():
    if d > 60 * sf.info(os.path.join(folder, f)).samplerate: grp[d].append(f)
drumname = re.compile(r'kick|kik|snare|sn\b|sn |tom|oh|overhead|room|rm|hat|hh|ride|crash|floor|rack|sub|amb|mono|bottom|top|ribbon|hall|ceiling', re.I)
if grp: files = sorted(max(grp.values(), key=lambda fs: sum(bool(drumname.search(f)) for f in fs)))
files = [f for f in files if drumname.search(f) and not re.search(r'sample|print|reverse', f, re.I)]
# pair .L/.R or " L"/" R" files into one stereo mic
mics = {}
for f in files:
    m = re.match(r'(.*?)[ ._-]([LR])\.(wav|aiff?)$', f, re.I)
    key = (m.group(1) if m else os.path.splitext(f)[0]).strip(' ._')
    key = re.sub(r'^\d+\s+', '', key); key = re.sub(r'[._]0?\d+$', '', key).strip(' ._')
    mics.setdefault(key, []).append(f)
sr = None; X = {}
for k, fs in mics.items():
    chans = []
    for f in fs:
        a, r = sf.read(os.path.join(folder, f), dtype='float32', always_2d=True); sr = r
        chans.append(a.mean(1))
    n = min(len(c) for c in chans); X[k] = np.mean([c[:n] for c in chans], 0)
n = min(len(v) for v in X.values()); X = {k: v[:n] for k, v in X.items()}
def kind(name):
    s = name.lower()
    for k, pat in [('sub','sub'),('kick',r'kick|kik|bd'),('snare',r'sn|snare'),('tom',r'tom|rack|floor'),('hat',r'hat|hh'),('ride',r'ride'),('crash','crash'),('room',r'room|rm|amb|hall|ceiling|ribbon|mono|crush|far'),('oh',r'oh|overhead')]:
        if re.search(pat, s): return k
    return 'other'
close = [k for k in X if kind(k) in ('kick','snare','tom','hat','ride','crash','sub') and not re.search(r'room|rm|oh\b|overhead|print', k, re.I)]
hp = butter(2, 60, 'hp', fs=sr, output='sos')
tomlp = butter(2, [60, 400], 'bp', fs=sr, output='sos')
def env(x, ms=1.0, tom=False):
    e = np.abs(sosfilt(tomlp if tom else hp, x)); w = max(1, int(sr*ms/1000))
    return np.convolve(e, np.ones(w)/w, 'same')
def onsets(x, tom=False):
    e = env(x, tom=tom); pk = np.percentile(e, 99.95)
    thr = pk * 10**((-14 if tom else -28)/20)   # toms are rarely ghosted, and open toms hear the whole kit
    above = e > thr; idx = np.flatnonzero(above[1:] & ~above[:-1]) + 1
    gap = int(0.045*sr); res = []
    for i in idx:
        if res and i - res[-1] < gap: continue
        seg = e[i:i+int(0.01*sr)]
        if len(seg) == 0: continue
        res.append(i)
    res = np.array(res, int)
    peaks = np.array([e[i:i+int(0.012*sr)].max() for i in res]) if len(res) else np.array([])
    return res, peaks, pk
H = {k: onsets(X[k], kind(k) == 'tom') for k in close}
# a hit belongs to the close mic that is loudest relative to its own loudest hits
def leadOf(kd, pat):
    c = [k for k in close if kind(k) == kd]
    return next((k for k in c if re.search(pat, k, re.I)), c[0] if c else None)
kickLead = leadOf('kick', r'\bin\b|kick ?1|^kick$'); snLead = leadOf('snare', r'top|snare ?1|^snare$')
primary = [k for k in (kickLead, snLead) if k]
for k in close:
    if k in primary: continue
    if kind(k) in ('kick','snare') and any(kind(j)==kind(k) for j in primary): continue
    primary.append(k)
hits = {k: [] for k in close}
# every onset from every drum's mic, grouped when they fall within 5 ms: the mics that are within 6 dB
# of the loudest (each against its own loudest hits) played; the rest is bleed
ev = sorted((int(t), k, float(v/H[k][2])) for k in primary for t, v in zip(H[k][0], H[k][1]))
i = 0; W = int(0.009*sr)   # bleed reaches a tom mic up to ~6 ms after the drum that made it
while i < len(ev):
    j = i
    while j + 1 < len(ev) and ev[j+1][0] - ev[i][0] <= W: j += 1
    grp = ev[i:j+1]; top = max(g[2] for g in grp)
    for t, k, r in grp:
        if r >= top * 10**(-6/20) and (not hits[k] or t - hits[k][-1][0] > int(0.045*sr)): hits[k].append((t, float(20*np.log10(r+1e-9))))
    i = j + 1
def isolated(lead_hits, lead, before=0.08, after=0.04):
    # strong hits with nothing else from any close mic 80 ms before or 40 ms after: clean to measure on
    others = np.sort(np.concatenate([np.array([t for t,_ in hits[j]], int) for j in close if j != lead and hits[j]] or [np.array([], int)]))
    out = []
    for t, v in hits[lead]:
        if v < -8: continue
        q = np.searchsorted(others, t - int(before*sr))
        if q < len(others) and others[q] < t + int(after*sr): continue
        out.append(t)
    return out
def arrival(x, ts, leadx):
    # when this mic hears the lead's hit: cross-correlation summed over clean hits, then each hit on its own
    L = int(0.04*sr); M = int(0.03*sr)
    acc = np.zeros(2*M+1); per = []
    for t in ts[:300]:
        if t - M < 0 or t + L + M >= n: continue
        a = leadx[t-int(0.002*sr):t-int(0.002*sr)+L]; b = x[t-int(0.002*sr)-M:t-int(0.002*sr)+L+M]
        na, nb = np.linalg.norm(a), np.linalg.norm(b[M:M+L])
        if na < 1e-6 or nb < 1e-6: continue
        c = np.correlate(b, a, 'valid') / (na*nb)
        acc += c; per.append(c)
    if len(per) < 4: return None
    j = int(np.argmax(np.abs(acc))); lag = (j - M)/sr*1000; pol = 1 if acc[j] > 0 else -1
    # stability: each hit's best lag within 1 ms of the overall one
    w = int(0.001*sr); near = []
    for c in per:
        lo = max(0, j-w); k = lo + int(np.argmax(pol*c[lo:j+w+1])); near.append((k-M)/sr*1000)
    near = np.array(near)
    return dict(ms=float(lag), flipped=pol < 0, corr=float(abs(acc[j])/len(per)), spread=float(np.percentile(near,75)-np.percentile(near,25)), n=len(per))
def balance(x, ref_hits):
    lo = sosfilt(butter(2,150,'lp',fs=sr,output='sos'), x); hi = sosfilt(butter(2,2000,'hp',fs=sr,output='sos'), x)
    el=eh=ea=0
    for t,_ in ref_hits[:200]:
        s = slice(t, t+int(0.3*sr)); el += np.sum(lo[s]**2); eh += np.sum(hi[s]**2); ea += np.sum(x[s]**2)
    return (el/ea, eh/ea) if ea else (0,0)
res = dict(folder=os.path.basename(folder.rstrip('/')), sr=sr, seconds=n/sr, mics={})
for k in X:
    m = dict(named=kind(k), close=k in close)
    if k in primary:
        v = np.array([h[1] for h in hits[k]])
        m['hits'] = len(v)
        if len(v): m['velocity_dB'] = dict(p10=float(np.percentile(v,10)), median=float(np.median(v)), p90=float(np.percentile(v,90)))
    for lead, nm in ((kickLead,'vs_kick'),(snLead,'vs_snare')):
        if lead and k != lead and hits.get(lead):
            a = arrival(X[k], isolated(hits[lead], lead, 0.04, 0.03) if nm == 'vs_kick' else isolated(hits[lead], lead, 0.025, 0.02), X[lead])
            if a and a['corr'] < 0.1: a = dict(a, note='barely hears it (gated or far off-axis)')
            if a: m[nm] = a
    ref = hits.get(snLead) if kind(k) in ('snare',) or not kickLead else hits.get(kickLead)
    if ref: lf, hf = balance(X[k], ref); m['low_share'] = float(lf); m['high_share'] = float(hf)
    res['mics'][k] = m
# tempo from kick + snare hits
allh = sorted([t for k in (kickLead, snLead) if k for t,_ in hits.get(k, [])])
if len(allh) > 20:
    iv = np.diff(allh)/sr; iv = iv[(iv>0.1)&(iv<1.5)]
    if len(iv): b = np.median(iv); res['beat_s_guess'] = float(b)
json.dump(res, open(out,'w'), indent=1)
print(res['folder'], f"{res['seconds']:.0f}s", sr)
for k, m in res['mics'].items():
    s = f"  {k:22s} {m['named']:6s}"
    if 'hits' in m: s += f" hits {m['hits']:4d}"
    if 'velocity_dB' in m: s += f" vel {m['velocity_dB']['p10']:+5.1f}/{m['velocity_dB']['median']:+5.1f}/{m['velocity_dB']['p90']:+5.1f}"
    for nm in ('vs_kick','vs_snare'):
        if nm in m: a=m[nm]; s += f"  {nm[3:]} " + ("(not heard)" if 'note' in a else f"{a['ms']:+6.2f}ms ±{a['spread']:.2f} {'flip' if a['flipped'] else '    '} r{a['corr']:.2f} n{a['n']}")
    if 'low_share' in m: s += f"  low {m['low_share']:.2f} high {m['high_share']:.3f}"
    print(s)
