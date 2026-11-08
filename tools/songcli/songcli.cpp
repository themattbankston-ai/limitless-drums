// Runs Song mode's analysis on raw float32 mono files: songcli <sr> <name=file.f32>...
#include "Song.h"
#include <cstdio>
#include <fstream>
#include <map>
using namespace ld::hl;
int main (int argc, char** argv)
{
    const double sr = atof (argv[1]);
    std::vector<F32> monos; std::vector<std::string> names;
    for (int i = 2; i < argc; ++i)
    {
        std::string a = argv[i]; auto eq = a.find ('=');
        names.push_back (a.substr (0, eq));
        std::ifstream f (a.substr (eq + 1), std::ios::binary | std::ios::ate); size_t n = (size_t) f.tellg() / 4; f.seekg (0);
        F32 x (n); f.read ((char*) x.data(), (std::streamsize) (n * 4)); monos.push_back (std::move (x));
        printf ("mic %-14s -> %s\n", names.back().c_str(), drumName (drumOfMic (names.back())));
    }
    auto A = analyseSong (monos, names, sr);
    std::map<std::string, int> cnt; int ghosts = 0, flams = 0;
    for (auto& h : A.hits) { cnt[drumName (h.drum)]++; ghosts += h.ghost; flams += h.flam; }
    printf ("hits %zu:", A.hits.size()); for (auto& [k, v] : cnt) printf (" %s %d", k.c_str(), v); printf (" | ghosts %d flams %d\n", ghosts, flams);
    for (const char* mode : { "keep", "smooth", "flatten" })
    {
        SongSettings s; s.tempoMode = mode;
        auto b = gridBeats (A, s); auto bpm = bpmOf (b);
        double lo = 1e9, hi = 0, m = 0; for (double v : bpm) { lo = std::min (lo, v); hi = std::max (hi, v); m += v; }
        printf ("%-8s beats %zu  bpm avg %.2f  min %.2f  max %.2f\n", mode, b.size(), bpm.empty() ? 0 : m / bpm.size(), lo, hi);
    }
    printf ("downbeat %d  suggested grid %d\n", A.downbeat, A.suggestedDivision);
    SongSettings s; s.tightness = 0.6; s.division = A.suggestedDivision;
    auto E = planEdits (A, s); double mv = 0; for (auto& e : E) mv += std::abs ((double) e.shift); 
    printf ("tightness 60%%: %zu edits, mean move %.1f ms\n", E.size(), E.empty() ? 0 : mv / E.size() / sr * 1000);
    auto P = findProblems (A, s); std::map<std::string, int> pk; for (auto& p : P) pk[p.kind]++;
    printf ("problems %zu:", P.size()); for (auto& [k, v] : pk) printf (" %s %d", k.c_str(), v); printf ("\n");
    for (size_t i = 0; i < P.size() && i < 8; ++i) printf ("  %.2f s %s: %s\n", P[i].pos / sr, P[i].kind.c_str(), P[i].text.c_str());
    // hit list for checking
    FILE* o = fopen ("hits.tsv", "w"); for (auto& h : A.hits) fprintf (o, "%.4f\t%s\t%.2f\t%d\t%d\n", h.pos / sr, drumName (h.drum), h.vel, (int) h.ghost, (int) h.flam); fclose (o);
    FILE* bo = fopen ("beats.txt", "w"); for (double b : A.beats) fprintf (bo, "%.4f\n", b); fclose (bo);
}
