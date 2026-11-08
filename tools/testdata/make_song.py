# A synthetic multitrack drum song with a known tempo map and known hits, for testing Song mode.
# Snare: real multi-mic hits from the parity snare session. Kick and hats: synthesized, bleeding into the overheads
# and rooms with their delays. Usage: make_song.py <out dir> <seconds> [seed]
import sys, os, json, math, glob
import numpy as np, soundfile as sf
out, secs = sys.argv[1], float(sys.argv[2]); seed = int(sys.argv[3]) if len(sys.argv) > 3 else 1
rng = np.random.default_rng(seed)
os.makedirs(out, exist_ok=True)
SR = 44100
src = '/home/claude/parity/sessions/SNARE 1 - MH/Center/'
mics = {'Snare Top': 'TOP', 'Snare Bottom': 'BOTTOM', 'OH': 'OH', 'Room Close': 'ROOM_CLOSE', 'Room Far': 'ROOM_FAR', 'Room Mono': 'ROOM_MONO', 'Room Crush': 'ROOM_CRUSH'}
sn = {k: sf.read(src + 'SNARE_CENTER_%s_V06_RR06.wav' % v, always_2d=True)[0] for k, v in mics.items()}
top = sn['Snare Top'][:, 0]
env = np.abs(top); on = []
i = 0
while i < len(top):
    if env[i] > 0.05: on.append(i); i += int(1.2 * SR)
    else: i += 1
on = [max(0, o - 200) for o in on]
pk = [float(np.max(np.abs(top[o:o + 4000]))) for o in on]
order = np.argsort(pk); L = int(1.0 * SR)
# where each snare hit really starts (first sample at 20 % of its peak), from its 200-sample pre-roll
start = []
for o in on:
    seg = np.abs(top[o:o + 6000]); p = seg.max(); start.append(int(np.argmax(seg >= 0.2 * p)))
last_off = [0]
def snare_hit(vel):  # pick a hit near the wanted loudness
    k = order[min(len(order) - 1, int(vel * (len(order) - 1)))]
    last_off[0] = start[k]
    return {m: a[on[k]:on[k] + L] for m, a in sn.items()}
N = int(secs * SR) + 2 * SR
tracks = {'Kick In': np.zeros((N, 1)), 'Hat': np.zeros((N, 1)), 'Tom 1': np.zeros((N, 1))}
for m, a in sn.items(): tracks[m] = np.zeros((N, a.shape[1]))
def add(name, x, at, gain=1.0):
    a = tracks[name]; at = int(at)
    if at < 0 or at >= N: return
    n = min(len(x), N - at)
    if x.ndim == 1: x = x[:, None]
    if a.shape[1] == 2 and x.shape[1] == 1: x = np.repeat(x, 2, axis=1)
    a[at:at + n] += x[:n] * gain
t = np.arange(int(0.6 * SR)) / SR
def kick(vel):
    f = 50 + 120 * np.exp(-t * 30); ph = 2 * np.pi * np.cumsum(f) / SR
    return (np.sin(ph) * np.exp(-t * 7) + 0.3 * rng.standard_normal(len(t)) * np.exp(-t * 300)) * 0.9 * vel
def hat(vel):
    n = rng.standard_normal(int(0.12 * SR)); n = np.diff(np.concatenate([[0], n]))
    return n * np.exp(-np.arange(len(n)) / SR * 45) * 0.25 * vel
def tom(vel):
    f = 110 + 40 * np.exp(-t * 20); ph = 2 * np.pi * np.cumsum(f) / SR
    return (np.sin(ph) * np.exp(-t * 5) + 0.4 * rng.standard_normal(len(t)) * np.exp(-t * 250)) * 0.7 * vel
# the tempo: drifts 118 -> 122 with a slow wobble; beats placed by integrating it
bpm = lambda x: 118 + 4 * x / secs + 1.5 * math.sin(2 * math.pi * x / 23)
beats = [0.5]
while beats[-1] < secs: beats.append(beats[-1] + 60 / bpm(beats[-1]))
hits = []
def play(drum, beat_pos, vel, jitter_ms, bias_ms=0):
    bi = int(beat_pos); fr = beat_pos - bi
    if bi + 1 >= len(beats): return
    grid = beats[bi] + fr * (beats[bi + 1] - beats[bi])
    tt = grid + (bias_ms + rng.normal(0, jitter_ms)) / 1000
    at = tt * SR
    hits.append({'drum': drum, 'grid': grid, 't': tt, 'vel': vel})
    if drum == 'kick':
        x = kick(vel); add('Kick In', x, at); add('OH', x * 0.12, at + 0.004 * SR); add('Room Close', x * 0.2, at + 0.008 * SR)
        add('Room Far', x * 0.12, at + 0.02 * SR); add('Room Mono', x * 0.15, at + 0.012 * SR); add('Room Crush', x * 0.2, at + 0.012 * SR); add('Snare Bottom', x * 0.08, at + 0.002 * SR)
    elif drum == 'snare':
        hit = snare_hit(vel); at -= last_off[0]   # the hit's real start lands on tt
        for m, x in hit.items(): add(m, x, at)
        add('Kick In', snare_hit(vel)['Snare Top'][:, 0] * 0.05, at + 0.002 * SR); add('Hat', snare_hit(vel)['Snare Top'][:, 0] * 0.15, at + 0.001 * SR)
    elif drum == 'hat':
        x = hat(vel); add('Hat', x, at); add('OH', np.stack([x * 0.35, x * 0.15], 1), at + 0.002 * SR); add('Snare Top', x * 0.1, at + 0.001 * SR)
    elif drum == 'tom':
        x = tom(vel); add('Tom 1', x, at); add('OH', x * 0.2, at + 0.003 * SR); add('Room Close', x * 0.2, at + 0.008 * SR); add('Snare Top', x * 0.05, at + 0.001 * SR)
nbars = (len(beats) - 2) // 4
for bar in range(nbars):
    b0 = bar * 4
    fill = bar % 8 == 7
    for e in range(8):
        if not (fill and e >= 4): play('hat', b0 + e / 2, 0.8 if e % 2 == 0 else 0.5, 5)
    play('kick', b0, 0.95, 6); play('kick', b0 + 2, 0.9, 6)
    if bar % 2 == 1: play('kick', b0 + 2.5, 0.8, 7)
    play('snare', b0 + 1, 0.9, 7, 4)
    if fill:
        for s in range(8): play('tom' if s < 4 else 'snare', b0 + 2 + s * 0.25, 0.7, 9)
    else:
        play('snare', b0 + 3, 0.92, 7, 4)
        if bar % 4 == 2: play('snare', b0 + 1.75, 0.15, 12)   # a ghost note
for name, a in tracks.items():
    a = a[:int(secs * SR)]
    sf.write(os.path.join(out, name + '.wav'), (a / max(1.0, np.max(np.abs(a)) * 1.05)).astype(np.float32), SR, subtype='PCM_24')
json.dump({'sr': SR, 'beats': [b for b in beats if b < secs], 'hits': sorted([h for h in hits if h['t'] < secs - 0.2], key=lambda h: h['t'])}, open(os.path.join(out, 'truth.json'), 'w'))
print('wrote', len(tracks), 'tracks,', len(hits), 'hits,', len(beats), 'beats')
