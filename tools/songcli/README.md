# songcli

Runs Song mode's analysis (`src/hl/Song.cpp`) on real multitracks from the command line, without JUCE. It reports
hits per drum, ghost notes and flams, the tempo in each mode, the suggested grid, the tightening plan and the
problem finder's flags, and writes `hits.tsv` and `beats.txt`.

```
g++ -std=c++17 -O2 -Ishim -I../../src/hl songcli.cpp ../../src/hl/Song.cpp -o songcli
# each mic as raw mono float32 at the session rate, named as in the session
ffmpeg -i "Kick In.wav" -ac 1 -f f32le kick.f32
./songcli 48000 "Kick In=kick.f32" "Snare Top=snare.f32" ...
```
