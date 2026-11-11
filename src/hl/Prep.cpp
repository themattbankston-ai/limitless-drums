#include "Prep.h"
#include "Detect.h"
#include "Rules.h"
#include "Shape.h"
#include "Wav.h"
#include "Resolve.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <future>
#include <map>
#include <regex>

namespace ld::hl
{
namespace
{
using std::regex;
const auto IC = std::regex::ECMAScript | std::regex::icase;

std::string lower (std::string s) { for (auto& c : s) c = (char) std::tolower ((unsigned char) c); return s; }
bool isWord (char c) { return std::isalnum ((unsigned char) c) || c == '_'; }
/** s.replace(/\b\w/g, c => c.toUpperCase()) */
std::string capWords (std::string s)
{
    for (size_t i = 0; i < s.size(); ++i) if (isWord (s[i]) && (i == 0 || ! isWord (s[i - 1]))) s[i] = (char) std::toupper ((unsigned char) s[i]);
    return s;
}
/** ...then .replace(/\b(\w)(\w*)/g, (m, a, b) => a + b.toLowerCase()) */
std::string lowerTails (std::string s)
{
    for (size_t i = 0; i < s.size(); ++i) if (isWord (s[i]) && i > 0 && isWord (s[i - 1])) s[i] = (char) std::tolower ((unsigned char) s[i]);
    return s;
}
std::string trim (const std::string& s) { return juce::String (s).trim().toStdString(); }
std::string escapeRe (const std::string& w) { static const regex sp (R"([.^$|()\[\]{}*+?\\])"); return std::regex_replace (w, sp, R"(\$&)"); }
std::string rep (const std::string& s, const char* re, const char* with, bool icase = false) { return std::regex_replace (s, regex (re, icase ? IC : std::regex::ECMAScript), with); }
std::string rep1 (const std::string& s, const char* re, const char* with) { return std::regex_replace (s, regex (re), with, std::regex_constants::format_first_only); }
bool has (const std::string& s, const char* re, bool icase = false) { return std::regex_search (s, regex (re, icase ? IC : std::regex::ECMAScript)); }

const char* DRUM_WORDS = R"(\b(kick|snare|tom|floor|rack|hat|hats|hihat|ride|crash|china|splash|cym|cymbal)\b)";
const std::vector<std::pair<const char*, const char*>> MIC_MAP = {
    { "^(top|snare ?top|sn ?top)$", "Top" }, { "^(bottom|btm|bot|snare ?bottom|sn ?bot+)$", "Bottom" }, { "^(ovh|oh|overheads?|ohs?)$", "Overheads" },
    { "^(rm ?cls|room ?close|rm ?close|near ?room)$", "Room Close" }, { "^(rm ?far|room ?far|far ?room)$", "Room Far" },
    { "^(rm ?mono|room ?mono|mono ?room|mono)$", "Room Mono" }, { "^(rn ?crsh|rm ?crsh|room ?crush|crush|rm ?crush)$", "Room Crush" },
    { "^(in|kick ?in|kin)$", "Kick In" }, { "^(out|kick ?out|kout)$", "Kick Out" }, { "^(sub|kick ?sub|subkick)$", "Sub" }, { "^spot$", "Spot" } };

struct DrumDef { double shape, len, fmin, fmax; };
const std::map<std::string, DrumDef> DRUM_DEFS = {
    { "kick", { 0.8, 2.7, 35, 120 } }, { "snare", { 0.9, 2.5, 120, 400 } }, { "tom", { 0.75, 4.2, 60, 300 } }, { "hat", { 0.6, 2.4, 300, 4000 } },
    { "ride", { 0.85, 9.6, 250, 2500 } }, { "crash", { 0.45, 7.2, 200, 2500 } }, { "china", { 0.75, 4.8, 200, 2500 } }, { "splash", { 0.4, 2.4, 300, 3000 } },
    { "other", { 0.75, 3, 40, 1000 } } };
const double KIT_F[2] = { 40, 1000 };
bool pitched (const std::string& d) { return d == "kick" || d == "snare" || d == "tom"; }
const char* GROUPS[4] = { "A", "B", "C", "D" };
int groupIndex (const std::string& g) { for (int i = 0; i < 4; ++i) if (g == GROUPS[i]) return i; return -1; }

std::string roleOfTrack (const SessionTrack& tr) { return roleOf (tr.mic + " " + tr.name); }

Chs baseChOf (const SessionTrack& tr)
{
    if (tr.ch.size() != 2 || ! tr.lrLock || ! tr.lr || tr.lr->kept) return tr.ch;
    return { tr.ch[0], renderSeg (tr.ch[1], 0, (long) tr.ch[1].size(), tr.lr->lag, tr.lr->pol, 1, true) };
}
std::vector<int> toInt (const std::vector<long>& v) { return std::vector<int> (v.begin(), v.end()); }

void kitDefaults (Session& S)
{
    S.mode = "kit";
    auto nm = [] (const SessionTrack& t) { return t.name + " " + t.mic; };
    auto isKick = [&] (const SessionTrack& t) { return has (nm (t), R"(kick|\bbd\b|\bin\b|\bout\b|\bsub\b)", true) && ! has (t.name, "snare|tom", true); };
    int sn = -1, sn2 = -1;
    for (int i = 0; i < (int) S.tracks.size(); ++i) if (has (nm (S.tracks[(size_t) i]), "snare", true) && has (nm (S.tracks[(size_t) i]), R"(top|\bup\b)", true)) { sn = i; break; }
    if (sn >= 0) sn2 = sn;
    else for (int i = 0; i < (int) S.tracks.size(); ++i) if (has (nm (S.tracks[(size_t) i]), "snare", true)) { sn2 = i; break; }
    if (sn2 >= 0)
    {
        S.refIdx = sn2;
        if (S.refIdx > 0) { auto t = std::move (S.tracks[(size_t) S.refIdx]); S.tracks.erase (S.tracks.begin() + S.refIdx); S.tracks.insert (S.tracks.begin(), std::move (t)); S.refIdx = 0; }
    }
    std::map<std::string, std::string> toms;
    int g = 1;
    for (int i = 0; i < (int) S.tracks.size(); ++i)
    {
        if (i == S.refIdx) continue;
        auto& t = S.tracks[(size_t) i];
        const auto n = lower (nm (t));
        if (roleOfTrack (t) == "far") t.alignTo = "none";
        else if (isKick (t)) t.alignTo = "A";
        else if (has (n, R"(\btom|floor|rack)"))
        {
            std::smatch m;
            std::string id = has (n, "floor") ? "f" : "1";
            if (std::regex_search (n, m, regex (R"((?:\btom|floor|rack)\s*(\d+))"))) id = m[1];
            if (! toms.count (id) && g < 4) toms[id] = GROUPS[g++];
            t.alignTo = toms.count (id) ? toms[id] : "none";
        }
        else if (has (n, "snare")) t.alignTo = "lead";
        else if (has (n, R"(\boh\b|overhead|ovh)")) t.alignTo = "lead";
        else t.alignTo = "none";
    }
    S.exp.mode = "full";
}

int pickRef (const Session& S)
{
    auto rank = [] (const SessionTrack& t)
    {
        const auto n = lower (t.mic + " " + t.name);
        if (has (n, R"(\btop\b)")) return 0;
        if (has (n, R"(kick in|\bin\b)")) return 1;
        if (has (n, "spot|close") && ! has (n, "room")) return 2;
        if (has (n, R"(\bout\b)")) return 3;
        if (t.ch.size() == 1 && ! isRoomName (n)) return 4;
        return 9;
    };
    int best = 0;
    for (int i = 0; i < (int) S.tracks.size(); ++i) if (rank (S.tracks[(size_t) i]) < rank (S.tracks[(size_t) best])) best = i;
    return best;
}

int groupAnchor (const Session& S, const std::string& g)
{
    std::vector<int> mem;
    for (int i = 0; i < (int) S.tracks.size(); ++i) if (i != S.refIdx && S.tracks[(size_t) i].alignTo == g) mem.push_back (i);
    if (mem.empty()) return -1;
    const auto& want = S.groupAnchors[(size_t) groupIndex (g)];
    if (! want.empty() && want != "auto") for (int k : mem) if (S.tracks[(size_t) k].mic == want) return k;
    auto arr = [&] (int k) { const auto& a = S.tracks[(size_t) k].autoResult; return a ? (std::isfinite (a->arrival) ? a->arrival : a->lag) : INFINITY; };
    int best = mem[0];
    for (int k : mem) if (arr (k) < arr (best)) best = k;
    return best;
}
} // namespace

//==============================================================================
std::string cleanMicName (const std::string& file, const std::string& drumHint, const std::string& articHint)
{
    auto s = rep (file, R"(\.(wav|wave)$)", "", true);
    s = rep (s, R"(\.cm[\w.-]*$)", "", true);
    s = rep (s, R"((^|[_\-. ])(v|vel|rr)\s?\d{1,2}(?=$|[_\-. ]))", "$1", true);
    s = rep1 (s, R"([-_.]0\d$)", "");
    s = rep (s, R"([_\-.]+)", " ");
    s = rep (s, R"(\b(v|vel)\s?\d{1,2}\b)", "", true);
    s = rep (s, R"(\brr\s?\d{1,2}\b)", "", true);
    const auto low = lower (s);
    if (drumHint == "kit")
    {
        auto k = trim (rep (rep (low, R"(\b(kit|drums?)\b)", " "), R"(\s+)", " "));
        if (k.empty()) k = trim (low);
        k = capWords (k);
        k = rep1 (k, R"(\b(Oh|Ovh)\b)", "OH"); k = rep1 (k, R"(\bRm\b)", "Room"); k = rep1 (k, R"(\bBtm\b)", "Bottom");
        return k;
    }
    auto t = rep (low, DRUM_WORDS, " ");
    if (! articHint.empty())
    {
        juce::StringArray words; words.addTokens (juce::String (lower (articHint)), " \t\n_", "");
        for (auto& w : words) if (w.length() > 2) t = std::regex_replace (t, regex ("\\b" + escapeRe (w.toStdString()) + "\\b"), " ");
    }
    t = rep (t, R"(\b(hit|center|centre|rimshot|rim|sidestick|wires ?off|wiresoff|choke|closed|open|pedal|tip|shank|bell|bow|ching|tight|one shot)\b)", " ");
    t = trim (rep (t, R"(\s+)", " "));
    if (t.empty()) t = trim (low);
    for (auto& [re, name] : MIC_MAP) if (has (t, re)) return name;
    auto o = rep1 (capWords (t), R"(\bOh\b)", "OH");
    return o.empty() ? "Mic" : o;
}

std::string guessDrum (const std::string& path)
{
    const auto s = lower (path);
    const std::vector<std::pair<const char*, const char*>> M = { { "kick", R"(kick|\bbd\b)" }, { "snare", "snare" }, { "tom", R"(\btom|floor|rack)" },
        { "hat", "hat" }, { "ride", "ride" }, { "crash", "crash" }, { "china", "china" }, { "splash", "splash" } };
    for (auto& [k, re] : M) if (has (s, re)) return k;
    return "snare";
}

std::string guessArtic (const std::string& folder)
{
    auto s = rep (folder, R"([_-]?(v|vel)\s?\d{1,2}[_-]?rr\s?\d{1,2}|[_-]?rr\s?\d{1,2}[_-]?(v|vel)\s?\d{1,2})", "", true);
    s = rep (s, "[_-]+", " ");
    s = rep (s, DRUM_WORDS, " ");
    s = trim (rep (rep (s, R"(\b\d+\b)", " "), R"(\s+)", " "));
    return s.empty() ? "Hit" : lowerTails (capWords (s));
}

std::vector<SessionRecord> groupSessions (const std::vector<std::string>& relPaths, bool kit)
{
    std::map<std::string, std::vector<size_t>> groups;
    for (size_t i = 0; i < relPaths.size(); ++i)
    {
        juce::StringArray parts; parts.addTokens (juce::String (relPaths[i]), "/", "");
        parts.remove (parts.size() - 1);
        std::string key = kit ? (parts.isEmpty() ? "(files)" : parts[0].toStdString()) : (parts.isEmpty() ? "(files)" : parts.joinIntoString ("/").toStdString());
        groups[key].push_back (i);
    }
    std::vector<SessionRecord> out;
    for (auto& [key, fs] : groups)
    {
        SessionRecord r; r.key = key; r.files = fs;
        const auto folder = key == "(files)" ? std::string() : juce::String (key).fromLastOccurrenceOf ("/", false, false).toStdString();
        std::string names;
        for (auto i : fs) names += " " + juce::String (relPaths[i]).fromLastOccurrenceOf ("/", false, false).toStdString();
        r.drum = kit ? "kit" : guessDrum (key + names);
        if (kit) r.artic = folder.empty() ? "Performance" : folder;
        else if (! folder.empty()) r.artic = guessArtic (folder);
        else
        {
            const auto stem = rep (juce::String (relPaths[fs[0]]).fromLastOccurrenceOf ("/", false, false).toStdString(), R"(\.(wav|wave)$)", "", true);
            juce::StringArray bits; bits.addTokens (juce::String (stem), "_ ", "");
            const auto second = bits.size() > 1 ? bits[1].toStdString() : std::string();
            r.artic = guessArtic (second.empty() ? "Hit" : second);
        }
        out.push_back (r);
    }
    return out;
}

int expectedHits (const Session& S) { return S.det.V * S.det.RR; }
bool countsOk (const Session& S) { return S.mode != "kit" && ! S.onsets.empty() && (int) S.onsets.size() == expectedHits (S); }

void applyDrumDefaults (Session& S, const std::string& d)
{
    const auto it = DRUM_DEFS.find (d);
    const auto H = it != DRUM_DEFS.end() ? it->second : DRUM_DEFS.at ("other");
    S.drum = d; S.lev.shape = H.shape;
    if (d == "kit") S.mode = "kit";
    if (it != DRUM_DEFS.end() || d == "kit") { S.pitchOpt.fmin = d == "kit" ? KIT_F[0] : H.fmin; S.pitchOpt.fmax = d == "kit" ? KIT_F[1] : H.fmax; }
    if (! pitched (d)) S.tune.on = false;
    S.exp.maxLenS = std::max (8.0, std::ceil (H.len * 2.5));
    if (d == "kit") S.dnOn = false;
}

OpenResult openSession (std::vector<SourceFile> files, const std::string& drum, const std::string& artic)
{
    OpenResult R;
    auto& S = R.session;
    // Hitline's freshSession
    S.drum = drum; S.artic = artic; S.mode = "drum";
    S.det = Det(); S.lev = LevelSettings(); S.lev.shape = 0.8;
    S.exp = ExportSettings(); S.exp.maxLenS = 12;
    S.dnOn = true; S.dnDb = 18;
    double sr = 0;
    std::vector<std::string> names;
    for (auto& f : files)
    {
        auto& w = f.wav;
        if (! w.error.empty()) { R.errors.push_back (f.name + ": " + w.error); continue; }
        if (w.channels.size() > 2) { R.errors.push_back (f.name + ": " + std::to_string (w.channels.size()) + " channels"); continue; }
        if (sr != 0 && w.sampleRate != sr)
        {
            R.errors.push_back (f.name + " is " + juce::String (w.sampleRate / 1000).toStdString() + " kHz, the others are " + juce::String (sr / 1000).toStdString() + " kHz");
            continue;
        }
        if (sr == 0) sr = w.sampleRate;
        SessionTrack t;
        t.name = rep (f.name, R"(\.(wav|wave)$)", "", true);
        t.mic = cleanMicName (f.name, drum, artic);
        t.ch = std::move (w.channels);
        t.mono = monoOf (t.ch);
        t.len = (long) t.ch[0].size();
        t.label = w.label;
        t.alignTo = isRoomName (t.mic) ? "none" : "lead";
        S.tracks.push_back (std::move (t));
        names.push_back (f.rel.empty() ? f.name : f.rel);
    }
    std::map<std::string, int> seen;
    for (auto& t : S.tracks) { const auto k = lower (t.mic); if (++seen[k] > 1) t.mic += " " + std::to_string (seen[k]); }
    S.sr = sr;
    if (S.tracks.empty()) { if (R.errors.empty()) R.errors.push_back ("This articulation has no audio."); return R; }
    S.refIdx = pickRef (S);
    if (S.refIdx > 0) { auto t = std::move (S.tracks[(size_t) S.refIdx]); S.tracks.erase (S.tracks.begin() + S.refIdx); S.tracks.insert (S.tracks.begin(), std::move (t)); S.refIdx = 0; }
    if (const auto c = guessCounts (names)) { S.det.V = c->first; S.det.RR = c->second; }
    applyDrumDefaults (S, drum);
    for (auto& t : S.tracks) if (t.alignTo == "lead" && roleOfTrack (t) == "far") t.alignTo = "none";
    if (drum == "kit") kitDefaults (S);
    runDetect (S);
    return R;
}

std::vector<long> strongHits (const F32& x, double sr)
{
    DetectOptions o1; o1.expected = 1; o1.minGapMs = 60;
    const auto d0 = detectHits (x, sr, o1);
    DetectOptions o2; o2.expected = 0; o2.minGapMs = 60; o2.thrDb = d0.maxDb - 22;
    const auto r = detectHits (x, sr, o2);
    std::vector<std::pair<long, double>> cand;
    for (int on : r.onsets)
    {
        double m = 0;
        for (long i = on, e = std::min ((long) x.size(), on + (long) jsRound (0.01 * sr)); i < e; ++i) m = std::max (m, (double) std::abs (x[(size_t) i]));
        cand.push_back ({ on, m });
    }
    std::stable_sort (cand.begin(), cand.end(), [] (auto& a, auto& b) { return b.second < a.second; });
    std::vector<long> out;
    const long gap = (long) jsRound (0.3 * sr);
    for (auto& c : cand)
    {
        if (std::all_of (out.begin(), out.end(), [&] (long o) { return std::abs (o - c.first) >= gap; })) out.push_back (c.first);
        if (out.size() >= 48) break;
    }
    std::sort (out.begin(), out.end());
    return out;
}

void runDetect (Session& S)
{
    if (S.tracks.empty()) return;
    const auto& tr = S.tracks[(size_t) S.refIdx];
    if (S.mode == "kit")
    {
        S.kitOn = strongHits (tr.mono, S.sr); S.onsets = S.kitOn; S.detRan = true;
        if (S.phaseAnalyzed) S.phaseStale = true;
        return;
    }
    DetectOptions o;
    o.expected = expectedHits (S); o.minGapMs = S.minGapMs;
    if (! S.thrAuto) o.thrDb = S.thrDb;
    const auto r = detectHits (tr.mono, S.sr, o);
    S.onsets.assign (r.onsets.begin(), r.onsets.end());
    S.thrDb = jsRound (r.thrDb * 2) / 2; S.detRan = true;
    if (S.phaseAnalyzed) S.phaseStale = true;
    if (S.tuneAnalyzed) runPitch (S);
}

namespace { void applyRoomPolicy (Session&); }

void runAnalyze (Session& S)
{
    if (S.onsets.empty()) return;
    if (S.mode == "kit" && S.kitOn.empty()) S.kitOn = strongHits (S.tracks[(size_t) S.refIdx].mono, S.sr);
    const auto alignOns = toInt (S.mode == "kit" ? S.kitOn : S.onsets);
    const auto& ref = S.tracks[(size_t) S.refIdx].mono;
    const int nT = (int) S.tracks.size();
    for (int i = 0; i < nT; ++i)
    {
        auto& tr = S.tracks[(size_t) i];
        if (i == S.refIdx) continue;
        tr.role = detectRole (S, i);
        // what it really is decides how it's handled: a far room keeps its natural timing
        const auto r = effectiveRole (S, i);
        if (r == "far" && tr.alignTo == "lead" && ! tr.manualLag) tr.alignTo = "none";
        if (r != "far" && tr.alignTo == "none" && roleOfTrack (tr) == "far" && ! tr.manualLag && ! tr.roleOverride.empty()) tr.alignTo = "lead";
    }
    std::vector<int> todo;
    for (int i = 0; i < nT; ++i) if (i != S.refIdx) todo.push_back (i);
    std::vector<AlignResult> res (todo.size());
    {
        std::vector<std::future<void>> jobs;
        for (size_t j = 0; j < todo.size(); ++j)
            jobs.push_back (std::async (std::launch::async, [&, j]
            {
                const auto& tr = S.tracks[(size_t) todo[j]];
                AlignOptions o; o.role = roleOfTrack (tr);
                o = alignOptionsFor (effectiveRole (S, todo[j]));
                res[j] = alignMic (ref, baseChOf (tr), alignOns, S.sr, o);
            }));
        for (auto& j : jobs) j.get();
    }
    for (size_t j = 0; j < todo.size(); ++j) S.tracks[(size_t) todo[j]].autoResult = res[j];
    // rooms, measured where they sit: the polarity and phase EQ that sum best with the lead without moving them in time
    {
        std::vector<int> rooms;
        for (int i : todo) if (effectiveRole (S, i) == "far") rooms.push_back (i);
        std::vector<AlignResult> rr (rooms.size());
        std::vector<std::future<void>> jobs;
        for (size_t j = 0; j < rooms.size(); ++j)
            jobs.push_back (std::async (std::launch::async, [&, j]
            {
                AlignOptions o = alignOptionsFor ("far");
                o.inPlace = true; o.allpass = 1;
                rr[j] = alignMic (ref, baseChOf (S.tracks[(size_t) rooms[j]]), alignOns, S.sr, o);
            }));
        for (auto& j : jobs) j.get();
        for (auto& tr : S.tracks) tr.inPlace.reset();
        for (size_t j = 0; j < rooms.size(); ++j) S.tracks[(size_t) rooms[j]].inPlace = rr[j];
    }
    applyRoomPolicy (S);
    for (auto* g : GROUPS)
    {
        std::vector<int> mem;
        for (int i = 0; i < nT; ++i) if (i != S.refIdx && S.tracks[(size_t) i].alignTo == g) mem.push_back (i);
        if (mem.size() < 2) continue;
        const int a = groupAnchor (S, g);
        const auto& A = S.tracks[(size_t) a];
        const long shift = (long) jsRound (A.autoResult ? (std::isfinite (A.autoResult->arrival) ? A.autoResult->arrival : A.autoResult->lag) : 0);
        std::vector<int> ons;
        if (S.mode == "kit") ons = toInt (strongHits (A.mono, S.sr));
        else for (auto o : S.onsets) ons.push_back ((int) (o + shift));
        for (int i : mem)
        {
            if (i == a) continue;
            auto& tr = S.tracks[(size_t) i];
            AlignOptions o;
            {
                const auto r = effectiveRole (S, i);
                o = alignOptionsFor (r == "far" || effectiveRole (S, a) == "far" ? "far" : r);
            }
            o.minMs = -25;
            tr.grp = alignMic (A.mono, baseChOf (tr), ons, S.sr, o);
        }
    }
    resolvePhase (S);
    S.phaseAnalyzed = true; S.phaseStale = false;
}

namespace
{
/** Which of "as recorded" and "kept in time, phase fixed" each room is on, from the session's choice. A room moved or
    grouped by hand is left alone. */
void applyRoomPolicy (Session& S)
{
    for (int i = 0; i < (int) S.tracks.size(); ++i)
    {
        auto& tr = S.tracks[(size_t) i];
        if (i == S.refIdx || tr.manualLag || effectiveRole (S, i) != "far") continue;
        if (S.roomPhase && tr.alignTo == "none") tr.alignTo = "phase";
        else if (! S.roomPhase && tr.alignTo == "phase") tr.alignTo = "none";
    }
}
}

void setRoomPhase (Session& S, bool on)
{
    S.roomPhase = on;
    applyRoomPolicy (S);
    resolvePhase (S);
}

void resolvePhase (Session& S)
{
    const int nT = (int) S.tracks.size();
    std::vector<MicAlign> mics;
    for (auto& tr : S.tracks)
    {
        MicAlign m;
        m.mic = tr.mic; m.name = tr.name; m.alignTo = tr.alignTo; m.amount = tr.amount; m.apOn = tr.apOn;
        m.manualLag = tr.manualLag; m.manualPol = tr.manualPol; m.autoResult = tr.autoResult; m.grp = tr.grp; m.proPol = tr.proPol;
        m.lag = tr.lag; m.pol = tr.pol; m.ap = tr.ap; m.inPlace = tr.inPlace;
        mics.push_back (std::move (m));
    }
    if (std::any_of (S.tracks.begin(), S.tracks.end(), [] (auto& t) { return t.alignTo == "pro"; }))
    {
        static const std::vector<std::optional<MixRender::Scan>> none;
        ToneEngine E (S, none);
        for (int t = 0; t < nT; ++t) mics[(size_t) t].proLag = E.proLag (t);
    }
    S.flipped = resolveAlignment (mics, S.refIdx, S.leadFlipAuto, S.groupAnchors);
    for (int t = 0; t < nT; ++t)
    {
        auto& tr = S.tracks[(size_t) t];
        tr.lag = mics[(size_t) t].lag; tr.pol = mics[(size_t) t].pol; tr.ap = mics[(size_t) t].ap;
        if (tr.manualAp) tr.ap = tr.apOn && t != S.refIdx ? tr.userAp : std::vector<AllPass> {};   // bands set by hand outlive the measured ones
        tr.autoAsis = tr.autoResult ? tr.autoResult->asis : NAN;
    }
}

void runPitch (Session& S)
{
    if (S.onsets.empty()) return;
    const int nT = (int) S.tracks.size();
    const int t = S.tuneMic == "ref" ? S.refIdx : std::clamp (juce::String (S.tuneMic).getIntValue(), 0, nT - 1);
    const auto& tr = S.tracks[(size_t) t];
    const long lag = (long) jsRound (t == S.refIdx ? 0 : (tr.autoResult ? tr.autoResult->lag : tr.lag));
    std::vector<long> ons; for (auto o : S.onsets) ons.push_back (o + lag);
    if (S.drum == "tom" && S.tuneAutoWin)
    {
        auto o = S.pitchOpt; o.skipMs = 200; o.winMs = 400;
        auto r = sessionPitch (tr.mono, ons, S.sr, o);
        double w0 = 200, w1 = 400;
        if (std::isfinite (r.est) && r.est < 105) { o.skipMs = 150; o.winMs = 300; r = sessionPitch (tr.mono, ons, S.sr, o); w0 = 150; w1 = 300; }
        S.pitchOpt.skipMs = w0; S.pitchOpt.winMs = w1;
        S.pitch = std::move (r);
    }
    else S.pitch = sessionPitch (tr.mono, ons, S.sr, S.pitchOpt);
    S.tuneAnalyzed = true;
    computeTune (S);
}

void computeTune (Session& S)
{
    S.tinfo.reset(); S.tuneInfo.reset();
    if (! S.tuneAnalyzed || S.pitch.hits.empty() || S.pitch.hits.size() != S.onsets.size()) return;
    std::vector<int> velOf;
    const bool ok = countsOk (S);
    for (size_t i = 0; i < S.onsets.size(); ++i) velOf.push_back (ok ? S.velOfHit (i) : -1);
    auto ti = hl::computeTune (S.pitch, S.tune, S.det.V, velOf);
    if (! ti.valid) return;
    S.tinfo = TuneState { ti.ratios, ti.medF };
    S.tuneInfo = std::move (ti);
}

bool autoPrep (Session& S)
{
    if (S.tracks.empty()) return false;
    if (! countsOk (S) && S.mode != "kit") runDetect (S);
    if (S.mode == "kit" && S.onsets.empty()) runDetect (S);
    if (! countsOk (S) && S.mode != "kit") return false;
    runAnalyze (S);
    if (pitched (S.drum) && S.mode != "kit" && ! soundsLikeCymbal (S)) runPitch (S);
    if (S.mode == "kit") analyseSongOf (S);
    return true;
}

void analyseSongOf (Session& S)
{
    std::vector<F32> monos; std::vector<std::string> names;
    for (auto& t : S.tracks) { monos.push_back (t.mono.empty() ? monoOf (t.ch) : t.mono); names.push_back (t.mic + " " + t.name); }
    S.song.an = analyseSong (monos, names, S.sr);
    if (S.song.set.divisionAuto) S.song.set.division = S.song.an.suggestedDivision;
}

std::string hitCountAdvice (const Session& S)
{
    const int n = (int) S.onsets.size(), e = expectedHits (S);
    if (n == 0 || n == e) return {};
    std::vector<std::string> facts;
    for (int v = 2; v <= 12; ++v) if (n % v == 0 && n / v >= 1 && n / v <= 12) facts.push_back (std::to_string (v) + " \xc3\x97 " + std::to_string (n / v));
    std::string msg = "Found " + std::to_string (n) + " hits; the settings expect " + std::to_string (e) + " (" + std::to_string (S.det.V) + " \xc3\x97 " + std::to_string (S.det.RR) + "). ";
    if (! facts.empty())
    {
        std::string f;
        for (size_t i = 0; i < std::min<size_t> (3, facts.size()); ++i) f += (i ? " or " : "") + facts[i];
        msg += "If the take has " + std::to_string (n) + " hits, set velocities \xc3\x97 round robins to " + f + ".";
    }
    else msg += "Check the markers: Shift-click to add, double-click to remove.";
    return msg;
}

// ---------------------------------------------------------------------------- the Align step's controls
int alignRef (const Session& S, int t)
{
    const auto& tr = S.tracks[(size_t) t];
    if (t == S.refIdx || tr.alignTo == "none") return -1;
    if (tr.alignTo == "lead" || tr.alignTo == "pro" || tr.alignTo == "phase") return S.refIdx;
    const int a = groupAnchor (S, tr.alignTo);
    return a == t ? -1 : a;
}

void setTiming (Session& S, const std::string& mode)
{
    const int nT = (int) S.tracks.size();
    if (mode == "pro")
    {
        static const std::vector<std::optional<MixRender::Scan>> none;
        ToneEngine E (S, none);
        const auto& R = S.tracks[(size_t) S.refIdx];
        for (int t = 0; t < nT; ++t)
        {
            if (t == S.refIdx) continue;
            auto& tr = S.tracks[(size_t) t];
            if (groupIndex (tr.alignTo) >= 0) continue;
            const double lag = E.proLag (t);
            if (! std::isfinite (E.proTarget (t)) || ! std::isfinite (lag)) { tr.alignTo = "lead"; continue; }
            tr.alignTo = "pro"; tr.manualLag = false; tr.manualPol = false;
            // the polarity that sums fuller at that delay (Hitline's proPolarity)
            const double arr = tr.autoResult && std::isfinite (tr.autoResult->arrival) ? tr.autoResult->arrival : 0;
            const int leadPol = R.pol;
            const auto a = scoreMic (R.mono, tr.ch, toInt (S.onsets), S.sr, lag, leadPol, {}, arr);
            const auto b = scoreMic (R.mono, tr.ch, toInt (S.onsets), S.sr, lag, -leadPol, {}, arr);
            tr.proPol = a.valid && b.valid && b.score > a.score + 0.01 ? -1 : 1;
        }
    }
    else
        for (int t = 0; t < nT; ++t)
        {
            auto& tr = S.tracks[(size_t) t];
            if (t == S.refIdx || groupIndex (tr.alignTo) >= 0) continue;
            tr.alignTo = mode == "natural" ? "none" : "lead"; tr.manualLag = false; tr.manualPol = false;
        }
    S.timing = mode;
    applyRoomPolicy (S);
    resolvePhase (S);
}

void setAlignTo (Session& S, int t, const std::string& to)
{
    if (t == S.refIdx) return;
    auto& tr = S.tracks[(size_t) t];
    tr.alignTo = to; tr.manualLag = false; tr.manualPol = false;
    resolvePhase (S);
}

void nudgeLag (Session& S, int t, int dir)
{
    if (t == S.refIdx) return;
    auto& tr = S.tracks[(size_t) t];
    tr.lag = jsRound ((tr.lag - dir * 0.25) * 4) / 4; tr.manualLag = true;
    resolvePhase (S);
}

void setLag (Session& S, int t, double lag)
{
    if (t == S.refIdx) return;
    auto& tr = S.tracks[(size_t) t];
    tr.lag = lag; tr.manualLag = true;
    resolvePhase (S);
}

void flipMic (Session& S, int t)
{
    auto& tr = S.tracks[(size_t) t];
    tr.pol = -(tr.pol ? tr.pol : 1); tr.manualPol = true;
    resolvePhase (S);
}

void setPhaseEq (Session& S, int t, bool on) { S.tracks[(size_t) t].apOn = on; resolvePhase (S); }
void setAllPass (Session& S, int t, int band, double fc, double Q)
{
    if (t < 0 || t >= (int) S.tracks.size() || t == S.refIdx || band < 0 || band > 1) return;
    auto& tr = S.tracks[(size_t) t];
    if (! tr.manualAp) { tr.userAp = tr.ap; tr.manualAp = true; }   // start from what's playing
    if (band > (int) tr.userAp.size()) return;
    if (band == (int) tr.userAp.size()) tr.userAp.push_back ({});
    tr.userAp[(size_t) band] = { std::clamp (fc, 20.0, 0.45 * S.sr), std::clamp (Q, 0.3, 8.0) };
    tr.apOn = true;
    resolvePhase (S);
}

void removeAllPass (Session& S, int t, int band)
{
    if (t < 0 || t >= (int) S.tracks.size() || band < 0) return;
    auto& tr = S.tracks[(size_t) t];
    if (! tr.manualAp) { tr.userAp = tr.ap; tr.manualAp = true; }
    if (band < (int) tr.userAp.size()) tr.userAp.erase (tr.userAp.begin() + band);
    resolvePhase (S);
}

void setLrLock (Session& S, int t, bool on) { S.tracks[(size_t) t].lrLock = on; resolvePhase (S); }

void resetMic (Session& S, int t)
{
    auto& tr = S.tracks[(size_t) t];
    tr.manualLag = false; tr.manualPol = false; tr.apOn = true; tr.amount = 1; tr.manualAp = false; tr.userAp.clear();
    resolvePhase (S);
}

Heat overlayHeat (const Session& S, int t, int sel, bool spread)
{
    Heat H;
    if (t < 0 || t >= (int) S.tracks.size() || S.onsets.empty()) return H;
    const int r = alignRef (S, t) >= 0 ? alignRef (S, t) : S.refIdx;
    if (r == t) return H;
    const auto& R = S.tracks[(size_t) r]; const auto& M = S.tracks[(size_t) t];
    H.ref = r; H.far = roleOfTrack (M) == "far";
    const double sr = S.sr;
    H.win = (long) jsRound ((H.far ? 0.025 : 0.008) * sr); H.pre = (long) jsRound (0.0015 * sr);
    const F32 xR = R.ap.empty() ? baseChOf (R)[0] : applyAllpass (baseChOf (R)[0], sr, R.ap);
    const F32 xM = M.ap.empty() ? baseChOf (M)[0] : applyAllpass (baseChOf (M)[0], sr, M.ap);
    const double lagR = r == S.refIdx ? 0 : R.lag; const int pR = R.pol ? R.pol : 1;
    sel = std::clamp (sel, 0, (int) S.onsets.size() - 1);
    std::vector<int> hits { sel };
    if (spread && countsOk (S))
    {
        const int n = (int) S.onsets.size(), k = std::min (12, n);
        for (int j = 0; j < k; ++j) { const int h = (int) jsRound (j * (n - 1) / (double) std::max (1, k - 1)); if (std::find (hits.begin(), hits.end(), h) == hits.end()) hits.push_back (h); }
    }
    auto lp = [&] (const F32& x, double fc)
    {
        const double a = std::exp (-2 * M_PI * fc / sr); F32 y (x.size()); double s1 = 0, s2 = 0;
        for (size_t i = 0; i < x.size(); ++i) { s1 = (1 - a) * x[i] + a * s1; s2 = (1 - a) * s1 + a * s2; y[i] = (float) s2; }
        return y;
    };
    auto bands = [&] (const F32& x)
    {
        const auto l60 = lp (x, 60), l250 = lp (x, 250), l1k = lp (x, 1000), l4k = lp (x, 4000);
        std::array<F32, 3> b; for (auto& v : b) v.resize (x.size());
        for (size_t i = 0; i < x.size(); ++i) { b[0][i] = l250[i] - l60[i]; b[1][i] = l1k[i] - l250[i]; b[2][i] = l4k[i] - l1k[i]; }
        return b;
    };
    const auto BR = bands (xR), BM = bands (xM);
    struct Seg { long st; std::array<std::vector<double>, 3> a; std::array<double, 3> e; };
    std::vector<Seg> segs;
    for (int i : hits)
    {
        Seg sg; sg.st = (long) jsRound (S.onsets[(size_t) i] - H.pre + lagR);
        for (int k = 0; k < 3; ++k)
        {
            sg.a[(size_t) k].assign ((size_t) H.win, 0); double e = 0;
            for (long n = 0; n < H.win; ++n) { const long q = sg.st + n; const double v = (q >= 0 && q < (long) BR[(size_t) k].size() ? BR[(size_t) k][(size_t) q] : 0) * pR; sg.a[(size_t) k][(size_t) n] = v; e += v * v; }
            sg.e[(size_t) k] = e;
        }
        segs.push_back (std::move (sg));
    }
    const int pol = M.pol ? M.pol : 1;
    auto corrAt = [&] (double lag)
    {
        double tot = 0;
        for (int k = 0; k < 3; ++k)
        {
            const auto& hM = BM[(size_t) k];
            double sxy = 0, sxx = 0, syy = 0;
            for (auto& sg : segs)
            {
                const double base = sg.st - lagR + lag;
                for (long n = 0; n < H.win; ++n)
                {
                    const double s2 = base + n; const long i0 = (long) std::floor (s2); const double f = s2 - i0;
                    auto at = [&] (long q) { return q >= 0 && q < (long) hM.size() ? (double) hM[(size_t) q] : 0.0; };
                    const double y = (at (i0) * (1 - f) + at (i0 + 1) * f) * pol;
                    sxy += sg.a[(size_t) k][(size_t) n] * y; syy += y * y;
                }
                sxx += sg.e[(size_t) k];
            }
            tot += sxy / std::sqrt (sxx * syy + 1e-30);
        }
        return tot / 3;
    };
    H.cur = M.lag;
    const long range = (long) jsRound ((H.far ? 0.004 : 0.0015) * sr);
    for (double l = H.cur - range; l <= H.cur + range + 1e-9; l += 0.25) H.curve.push_back ({ l, corrAt (l) });
    for (auto& [l, c] : H.curve) if (! std::isfinite (H.bestC) || c > H.bestC) { H.best = l; H.bestC = c; }
    H.curC = corrAt (H.cur);
    return H;
}

// ------------------------------------------------------------------------------- is tuning recommended
bool soundsLikeCymbal (const Session& S)
{
    if (S.tracks.empty() || S.onsets.empty()) return false;
    const auto& x = S.tracks[(size_t) S.refIdx].mono;
    const double sr = S.sr;
    const double kl = std::exp (-2 * M_PI * 150 / sr), kh = std::exp (-2 * M_PI * 2000 / sr);
    double lo = 0, hi = 0, all = 0;
    const size_t step = std::max<size_t> (1, S.onsets.size() / 24);
    for (size_t k = 0; k < S.onsets.size(); k += step)
    {
        const long a = std::max (0L, (long) S.onsets[k]), b = std::min ((long) x.size(), a + (long) (0.3 * sr));
        double l1 = 0, l2 = 0, h1 = 0, h2 = 0;
        for (long i = a; i < b; ++i)
        {
            const double v = x[(size_t) i];
            l1 = (1 - kl) * v + kl * l1; l2 = (1 - kl) * l1 + kl * l2;
            h1 = (1 - kh) * v + kh * h1; h2 = (1 - kh) * h1 + kh * h2;
            lo += l2 * l2; hi += (v - h2) * (v - h2); all += v * v;
        }
    }
    // snares put 5-15 % of their energy above 2 kHz, toms about 1 %; hats, rides and crashes 70-95 %
    return all > 0 && hi / all > 0.45 && lo / all < 0.08;
}

std::string tuneAdvice (const Session& S)
{
    if (! pitched (S.drum) || soundsLikeCymbal (S)) return "cymbal";
    if (! S.tuneAnalyzed || S.pitch.hits.empty()) return "";
    int n = 0; std::vector<double> c;
    const double ref = std::isfinite (S.pitch.est) ? S.pitch.est : NAN;
    for (auto& h : S.pitch.hits) if (h && h->f > 0) { ++n; if (std::isfinite (ref)) c.push_back (1200 * std::log2 (h->f / ref)); }
    if (n < 0.6 * (double) S.pitch.hits.size()) return "nopitch";
    if (c.size() >= 4)
    {
        // harder hits ring higher, so the spread is judged inside each velocity when the counts are known
        std::sort (c.begin(), c.end());
        const double iqr = c[c.size() * 3 / 4] - c[c.size() / 4];
        if (iqr > 150) return "nopitch";
    }
    return "";
}

// ------------------------------------------------------------------------------- the Mix step's reference
Reference loadReference (const Session& S, const MixRender& M, const juce::File& f, int engine)
{
    Reference R; R.name = f.getFileName().toStdString(); R.engine = engine;
    const auto w = readWav (f);
    if (! w.error.empty() || w.channels.empty()) { R.error = w.error.empty() ? "no audio" : w.error; return R; }
    const bool v2 = engine >= 2;
    const double sr = w.sampleRate;
    const size_t nCh = w.channels.size();
    F32 m (w.channels[0].size(), 0.0f);
    std::vector<double> chPow (nCh, 0.0);
    if (v2)
    {
        // detection and the envelope come from the strongest channel as recorded: nothing is summed, so nothing cancels
        size_t strongest = 0;
        for (size_t c = 0; c < nCh; ++c) { for (float v : w.channels[c]) chPow[c] += (double) v * v; if (chPow[c] > chPow[strongest]) strongest = c; }
        if (! (chPow[strongest] > 0) || ! std::isfinite (chPow[strongest])) { R.error = "the reference is silent"; return R; }
        m = w.channels[strongest];
        m.resize (w.channels[0].size(), 0.0f);
    }
    else
        for (auto& c : w.channels) for (size_t i = 0; i < m.size() && i < c.size(); ++i) m[i] += c[i] / (float) nCh;
    DetectOptions o; o.minGapMs = 200;
    auto det = detectHits (m, sr, o);
    if (v2) { o.thrDb = det.maxDb - 30; det = detectHits (m, sr, o); }   // every hit within 30 dB of the loudest
    auto ons = det.onsets;
    if (ons.empty()) ons.push_back (0);
    const auto& B = bands3();
    auto median = [] (std::vector<double> v) -> double { if (v.empty()) return NAN; std::sort (v.begin(), v.end()); return v[v.size() / 2]; };
    std::vector<std::vector<double>> theirs (B.size()), mine (B.size());
    std::array<std::vector<double>, 3> envT, envM;
    auto addEnv = [] (std::array<std::vector<double>, 3>& e, const EnvStats& st)
    { if (std::isfinite (st.body) && std::isfinite (st.sus) && std::isfinite (st.tail)) { e[0].push_back (st.body); e[1].push_back (st.sus); e[2].push_back (st.tail); } };
    for (size_t k = 0; k < ons.size() && k < 12; ++k)
    {
        const double maxN = (double) ((k + 1 < ons.size() ? ons[k + 1] : (long) m.size()) - ons[k] + 64);
        F64 b;
        if (v2 && nCh > 1)
        {
            // each channel's spectrum, combined as power, weighted by how loud the channel is
            double tw = 0;
            F64 p (B.size(), 0.0);
            for (size_t c = 0; c < nCh; ++c)
            {
                if (! (chPow[c] > 0)) continue;
                const auto bc = bandSpec (w.channels[c], ons[k], sr, maxN);
                for (size_t j = 0; j < B.size(); ++j) p[j] += chPow[c] * std::pow (10.0, bc[j] / 10);
                tw += chPow[c];
            }
            for (size_t j = 0; j < B.size(); ++j) b.push_back (10 * std::log10 (p[j] / tw + 1e-30));
        }
        else
            b = bandSpec (m, ons[k], sr, maxN);
        for (size_t j = 0; j < B.size(); ++j) theirs[j].push_back (b[j]);
        addEnv (envT, envStats (m, ons[k], sr, maxN));
        ++R.hitsUsed;
    }
    // the sum of your mics on the hardest hits, after clean-up and before tone and shape
    // (engine 2: every round robin of the hardest layer, up to twelve, as many as the reference may give; the median
    // over them is what's compared, so one odd hit on either side moves nothing)
    const size_t maxTop = v2 ? 12 : 6;
    std::vector<size_t> top;
    for (size_t k = 0; k < S.onsets.size() && top.size() < maxTop; ++k) if (S.velOfHit (k) == S.det.V - 1) top.push_back (k);
    if (top.empty()) for (size_t k = 0; k < S.onsets.size() && k < maxTop; ++k) top.push_back (k);
    const long pre = (long) jsRound (S.det.prerollMs * S.sr / 1000), len = (long) jsRound (0.6 * S.sr);
    // the stretch bandSpec listens to (300 ms and a frame), as energy
    auto energy = [&] (const F32& x) { double e = 0; const size_t n = std::min (x.size(), (size_t) (0.3 * S.sr) + 4096); for (size_t q = 0; q < n; ++q) e += (double) x[q] * x[q]; return e; };
    std::vector<std::vector<double>> held (B.size());   // engine 2: per band, the sum against its mics' powers added up (dB)
    for (size_t i : top)
    {
        F32 mm ((size_t) (len + pre), 0.0f);
        F64 apart (B.size(), 0.0);
        for (size_t t = 0; t < S.tracks.size() && t < M.dn.size(); ++t)
        {
            F32 one (mm.size(), 0.0f);
            for (auto& c : M.dn[t])
            {
                const auto y = renderSeg (c, (long) S.onsets[i] - pre, len + pre, S.tracks[t].lag, S.tracks[t].pol ? S.tracks[t].pol : 1, 1.0 / (double) M.dn[t].size(), true);
                for (size_t q = 0; q < mm.size() && q < y.size(); ++q) { mm[q] += y[q]; one[q] += y[q]; }
            }
            if (v2 && S.tracks.size() > 1)
            {
                const auto bt = bandSpec (one, pre, S.sr);   // (each spectrum comes back against its own total: put the level back)
                const double e = energy (one);
                for (size_t j = 0; j < B.size(); ++j) apart[j] += e * std::pow (10.0, bt[j] / 10);
            }
        }
        const auto b = bandSpec (mm, pre, S.sr);
        for (size_t j = 0; j < B.size(); ++j) mine[j].push_back (b[j]);
        if (v2 && S.tracks.size() > 1)
        {
            const double e = energy (mm);
            for (size_t j = 0; j < B.size(); ++j) held[j].push_back (10 * std::log10 (e * std::pow (10.0, b[j] / 10) + 1e-30) - 10 * std::log10 (apart[j] + 1e-30));
        }
        addEnv (envM, envStats (mm, pre, S.sr));
    }
    if (! envT[0].empty() && ! envM[0].empty())
    {
        R.hasEnv = true;
        for (int k = 0; k < 3; ++k) R.env[(size_t) k] = std::round (std::clamp (median (envT[(size_t) k]) - median (envM[(size_t) k]), -6.0, 6.0) * 100) / 100;
    }
    F64 U (B.size()), D (B.size());
    std::vector<char> ok (B.size(), 0);   // engine 2: the bands there is evidence for
    // engine 2, a tuned take: the match is applied before the tuning, which then moves it with everything else. So the
    // band at g Hz is asked for what the reference has at g x ratio, where that band lands (RM05)
    double ratio = 1;
    if (v2 && S.tune.on && S.tinfo && ! S.tinfo->ratios.empty())
    {
        std::vector<double> rs;
        for (double r : S.tinfo->ratios) if (std::isfinite (r) && r > 0) rs.push_back (r);
        if (! rs.empty()) ratio = median (rs);
    }
    F64 TH (B.size());
    for (size_t j = 0; j < B.size(); ++j) TH[j] = median (theirs[j]);
    auto theirsAt = [&] (size_t j) -> double
    {
        if (std::abs (std::log2 (ratio)) < 1e-6) return TH[j];
        const double f = B[j] * ratio;
        if (f <= B.front()) return TH.front();
        if (f >= B.back()) return TH.back();
        size_t k = 0;
        while (k + 2 < B.size() && B[k + 1] < f) ++k;
        const double a = std::log2 (f / B[k]) / std::log2 (B[k + 1] / B[k]);
        return std::isfinite (TH[k]) && std::isfinite (TH[k + 1]) ? TH[k] + a * (TH[k + 1] - TH[k]) : NAN;
    };
    for (size_t j = 0; j < B.size(); ++j)
    {
        U[j] = median (mine[j]); const double th = v2 ? theirsAt (j) : TH[j];
        D[j] = B[j] < 60 || B[j] > 15000 || ! (th > -80) || ! std::isfinite (U[j]) ? 0 : th - U[j];
        ok[j] = B[j] >= 60 && B[j] <= 15000 && th > -60 && std::isfinite (U[j]) && U[j] > -80;
        // engine 2: where your mics cancel each other (the sum 6 dB or more under their powers added up) the hole is a
        // phase problem, not a tone to boost back: left alone (RM05)
        if (v2 && ok[j] && ! held[j].empty() && median (held[j]) < -6) { ok[j] = 0; R.cancelled.push_back (B[j]); }
        if (v2 && ! ok[j]) D[j] = 0;
    }
    double sw = 0, sd = 0;
    for (size_t j = 0; j < B.size(); ++j)
    {
        if (v2 && ! ok[j]) continue;   // a band with no evidence doesn't pull the level either
        const double e = std::pow (10.0, U[j] / 10); if (std::isfinite (e)) { sw += e; sd += e * D[j]; }
    }
    const double mean = sw > 0 ? sd / sw : 0;
    F64 c (B.size());
    for (size_t j = 0; j < B.size(); ++j) c[j] = (v2 ? ! ok[j] : (B[j] < 60 || B[j] > 15000)) ? 0 : D[j] - mean;
    for (int it = 0; it < 2; ++it)
    {
        const auto o2 = c;
        for (size_t j = 1; j + 1 < B.size(); ++j)
        {
            if (v2)
            {
                // smooth among the bands with evidence only, so a neutral neighbour isn't read as "0 dB wanted"
                if (! ok[j]) continue;
                double s2 = 0.5 * o2[j], ws = 0.5;
                if (ok[j - 1]) { s2 += 0.25 * o2[j - 1]; ws += 0.25; }
                if (ok[j + 1]) { s2 += 0.25 * o2[j + 1]; ws += 0.25; }
                c[j] = s2 / ws;
            }
            else c[j] = 0.25 * o2[j - 1] + 0.5 * o2[j] + 0.25 * o2[j + 1];
        }
    }
    if (v2)   // taper into the bands left alone: the last band with evidence moves half as far (of its capped value)
    {
        for (auto& v : c) v = std::clamp (v, -8.0, 8.0);
        const auto o2 = c;
        for (size_t j = 0; j < B.size(); ++j)
            if (ok[j] && ((j > 0 && ! ok[j - 1]) || (j + 1 < B.size() && ! ok[j + 1]))) c[j] = 0.5 * o2[j];
    }
    for (auto& v : c) v = std::round (std::clamp (v, -8.0, 8.0) * 100) / 100;
    R.curve = c;
    // its first hit, 1.6 s, at the session's rate
    const long a = ons[0], n = std::min ((long) m.size() - a, (long) jsRound (1.6 * sr));
    const double k = sr / S.sr;
    for (int ch = 0; ch < 2; ++ch)
    {
        const auto& src = w.channels[(size_t) std::min (ch, (int) w.channels.size() - 1)];
        F32 out ((size_t) std::max (0L, (long) std::floor (n / k)));
        if (v2 && std::abs (k - 1.0) > 1e-9 && ! out.empty())
        {
            // a windowed sinc, with the reference low-passed first when it has more bandwidth than the session
            const long pad = 64, a0 = std::max (0L, a - pad), lead = a - a0;
            F32 in (src.begin() + a0, src.begin() + std::min ((long) src.size(), a + n + pad));
            if (k > 1.0)
                for (int pass = 0; pass < 4; ++pass)
                {
                    juce::IIRFilter lp; lp.setCoefficients (juce::IIRCoefficients::makeLowPass (sr, 0.45 * S.sr));
                    lp.processSamples (in.data(), (int) in.size());
                }
            in.resize (in.size() + 256, 0.0f);
            const long skip = (long) std::llround ((double) lead / k);
            juce::Interpolators::WindowedSinc rs;
            const long lat = (long) std::llround ((double) rs.getBaseLatency() / k);   // the interpolator's own delay
            F32 y ((size_t) (skip + lat) + out.size(), 0.0f);
            const int room = (int) std::min ((double) y.size(), std::floor ((double) (in.size() - 128) / k));
            rs.process (k, in.data(), y.data(), std::max (0, room));
            for (size_t i = 0; i < out.size(); ++i) { const long q = (long) i + skip + lat; out[i] = q < (long) y.size() ? y[(size_t) q] : 0.0f; }
        }
        else
            for (size_t i = 0; i < out.size(); ++i) out[i] = src[(size_t) std::min ((long) src.size() - 1, a + (long) jsRound ((double) i * k))];
        R.audio.push_back (std::move (out));
    }
    return R;
}
} // namespace ld::hl
