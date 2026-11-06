#include "Prep.h"
#include "Detect.h"
#include "Rules.h"
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
            o.role = roleOfTrack (tr) == "far" || roleOfTrack (A) == "far" ? "far" : "near";
            {
                const auto r = effectiveRole (S, i);
                o = alignOptionsFor (r == "far" || effectiveRole (S, a) == "far" ? "far" : r);
            }
            o.minMs = -25;
            tr.grp = alignMic (A.mono, baseChOf (tr), ons, S.sr, o);
        }
    }
    std::vector<MicAlign> mics;
    for (auto& tr : S.tracks)
    {
        MicAlign m;
        m.mic = tr.mic; m.name = tr.name; m.alignTo = tr.alignTo; m.amount = tr.amount; m.apOn = tr.apOn;
        m.manualLag = tr.manualLag; m.manualPol = tr.manualPol; m.autoResult = tr.autoResult; m.grp = tr.grp; m.proPol = tr.proPol;
        m.lag = tr.lag; m.pol = tr.pol; m.ap = tr.ap;
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
        tr.autoAsis = tr.autoResult ? tr.autoResult->asis : NAN;
    }
    S.phaseAnalyzed = true; S.phaseStale = false;
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
    if (pitched (S.drum) && S.mode != "kit") runPitch (S);
    return true;
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
} // namespace ld::hl
