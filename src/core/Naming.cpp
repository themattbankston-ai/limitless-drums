#include "Naming.h"
#include <algorithm>
#include <map>
#include <numeric>

namespace ld
{

namespace
{
// Split into lower-case word tokens; letters and digits are separated ("Kick1" -> kick, 1).
juce::StringArray tokenise (const juce::String& s)
{
    juce::StringArray out;
    juce::String cur;
    int curType = 0; // 1 letter, 2 digit

    auto flush = [&] { if (cur.isNotEmpty()) out.add (cur.toLowerCase()); cur.clear(); curType = 0; };

    for (auto c : s)
    {
        const int t = juce::CharacterFunctions::isLetter (c) ? 1 : juce::CharacterFunctions::isDigit (c) ? 2 : 0;

        if (t == 0) { flush(); continue; }
        if (curType != 0 && t != curType) flush();
        cur += c;
        curType = t;
    }

    flush();
    return out;
}

bool has (const juce::StringArray& t, std::initializer_list<const char*> words)
{
    for (auto* w : words)
        if (t.contains (w))
            return true;
    return false;
}

int firstNumber (const juce::StringArray& t)
{
    for (int i = 0; i < t.size(); ++i)
        if (t[i].containsOnly ("0123456789"))
        {
            // Skip sample-file counters (V06, RR06) and mic model numbers (57, 421, 414, 47, 67, 91)
            const auto prev = i > 0 ? t[i - 1] : juce::String();
            if (prev == "v" || prev == "rr" || prev == "vel" || prev == "beta" || prev == "md" || prev == "c" || prev == "u" || prev == "ns")
                continue;
            const int v = t[i].getIntValue();
            if (v >= 1 && v <= 9)
                return v;
        }
    return 0;
}

juce::String sideOf (const juce::StringArray& t)
{
    if (has (t, { "l", "left", "lft" })) return "L";
    if (has (t, { "r", "right" })) return "R";
    return {};
}

juce::String titleCase (const juce::String& s)
{
    juce::StringArray words;
    words.addTokens (s, " ", {});
    for (auto& w : words)
        w = w.substring (0, 1).toUpperCase() + w.substring (1).toLowerCase();
    return words.joinIntoString (" ");
}

double baseRank (MicRole::Kind k)
{
    using K = MicRole::Kind;
    switch (k)
    {
        case K::kickIn: return 10; case K::kickOut: return 11; case K::kickSub: return 12; case K::kick: return 13;
        case K::snareTop: return 20; case K::snareBottom: return 21; case K::snare: return 22;
        case K::hiHat: return 30; case K::tom: return 40; case K::floorTom: return 50;
        case K::ride: return 60; case K::cymbal: return 65;
        case K::overheads: return 70;
        case K::roomMono: return 80; case K::roomClose: return 81; case K::room: return 82;
        case K::roomFar: return 83; case K::roomCrush: return 84; case K::ambient: return 90;
        case K::print: return 95; case K::other: return 99;
    }
    return 99;
}
} // namespace

juce::String cleanFileName (const juce::String& fileName)
{
    auto s = juce::File::createFileWithoutCheckingPath ("/" + fileName).getFileNameWithoutExtension();

    // Leading track numbers: "01 Kick", "01_Kick", "1-Kick"
    int i = 0;
    while (i < s.length() && juce::CharacterFunctions::isDigit (s[i])) ++i;
    if (i > 0 && i < s.length() && (s[i] == ' ' || s[i] == '_' || s[i] == '-' || s[i] == '.'))
        s = s.substring (i + 1);

    // Sample counters at the end: _V06_RR06, _RR06_V06
    juce::StringArray parts;
    parts.addTokens (s, "_ ", {});
    while (parts.size() > 1)
    {
        auto last = parts[parts.size() - 1].toUpperCase();
        if ((last.startsWith ("RR") || last.startsWith ("V")) && last.substring (last.startsWith ("RR") ? 2 : 1).containsOnly ("0123456789")
            && last.length() > 1)
            parts.remove (parts.size() - 1);
        else
            break;
    }

    return parts.joinIntoString (" ").replaceCharacter ('-', ' ').trim();
}

MicRole classifyMic (const juce::String& fileName)
{
    using K = MicRole::Kind;
    MicRole r;
    const auto clean = cleanFileName (fileName);
    const auto t = tokenise (clean);
    r.side = sideOf (t);
    r.index = firstNumber (t);
    r.confident = true;

    auto instrumentWord = [&]() -> juce::String
    {
        if (has (t, { "hat", "hh", "hihat", "hats" })) return "Hat";
        if (has (t, { "ride", "rd" })) return "Ride";
        if (has (t, { "crash" })) return "Crash";
        if (has (t, { "china" })) return "China";
        if (has (t, { "kick", "kik" })) return "Kick";
        if (has (t, { "snare", "snr" })) return "Snare";
        return {};
    };

    // Prints, samples and effects first: "BB Kick Print" is a print, not a kick mic.
    if (has (t, { "print", "prints", "sample", "samples", "trigger", "trig", "reverse", "rev", "swell", "fx", "sfx", "loop", "bounce", "stem" }))
    {
        r.kind = K::print;
        r.confident = false;
    }
    // Rooms before drums: "Kick Room" is a room.
    else if (has (t, { "crush", "crushed", "trash", "smash", "smashed", "squash", "comp", "compressed", "destroy", "nuke" }))
        r.kind = K::roomCrush;
    else if (has (t, { "room", "rm", "rooms", "far", "distant", "mono", "close", "near", "hall", "ceiling", "chamber",
                       "stair", "stairs", "hallway", "amb", "ambient", "ambience", "ambi", "booth", "bathroom", "garage" })
             && ! has (t, { "oh", "ohs", "overhead", "overheads" }))
    {
        const bool roomWord = has (t, { "room", "rm", "rooms" });

        if (has (t, { "far", "distant" }))                       r.kind = K::roomFar;
        else if (has (t, { "close", "near" }))                   r.kind = K::roomClose;
        else if (has (t, { "mono", "mn", "center", "ctr" }) && (roomWord || t.size() <= 3)) r.kind = K::roomMono;
        else if (roomWord)
        {
            r.kind = K::room;
            // Keep a nickname: "Room Violet", "Mega Room"
            juce::StringArray extra;
            for (auto& w : t)
                if (w != "room" && w != "rm" && w != "rooms" && ! w.containsOnly ("0123456789") && w != "l" && w != "r"
                    && w != "left" && w != "right" && w != "snare" && w != "kick" && w != "center" && w != "v" && w != "rr")
                    extra.add (w);
            r.detail = titleCase (extra.joinIntoString (" "));
        }
        else
        {
            r.kind = K::ambient;
            r.confident = false;
        }
    }
    else if (has (t, { "oh", "ohs", "overhead", "overheads", "over", "ovh", "overs" }))
    {
        r.kind = K::overheads;
        r.detail = instrumentWord();
        if (r.detail == "Kick" || r.detail == "Snare") r.detail.clear(); // "SNARE_CENTER_OH" = Overheads
    }
    else if (has (t, { "kick", "kik", "kck", "bd", "bassdrum" }) || (has (t, { "bass" }) && has (t, { "drum" })))
    {
        if (has (t, { "sub", "subkick", "ns", "ns10", "yamaha" }))        r.kind = K::kickSub;
        else if (has (t, { "out", "outside", "front", "reso" }))          r.kind = K::kickOut;
        else if (has (t, { "in", "inside", "beater", "inner" }))          r.kind = K::kickIn;
        else                                                              r.kind = K::kick;
    }
    else if (has (t, { "snare", "snr", "sn", "sd", "snares" }))
    {
        if (has (t, { "bottom", "bot", "btm", "bttm", "under", "below", "bt" }))  r.kind = K::snareBottom;
        else if (has (t, { "top", "up", "upper" }))                             r.kind = K::snareTop;
        else                                                                    r.kind = K::snare;
    }
    else if (has (t, { "hat", "hh", "hihat", "hats", "hihats" }))
        r.kind = K::hiHat;
    else if (has (t, { "floor", "ft", "flr", "floortom" }))
        r.kind = K::floorTom;
    else if (has (t, { "tom", "toms", "tm", "rack", "racktom", "rt" }))
        r.kind = K::tom;
    else if (has (t, { "ride", "rd" }))
        r.kind = K::ride;
    else if (has (t, { "crash", "china", "splash", "cymbal", "cym", "cymbals", "stack", "bell" }))
    {
        r.kind = K::cymbal;
        r.confident = false; // keep "Crash L", "China"
    }
    else
    {
        r.kind = K::other;
        r.confident = false;
    }

    r.rank = baseRank (r.kind) + r.index * 0.01 + (r.side == "R" ? 0.002 : r.side == "L" ? 0.001 : 0.0);

    // Name
    juce::String name;
    switch (r.kind)
    {
        case K::kickIn:      name = "Kick In"; break;
        case K::kickOut:     name = "Kick Out"; break;
        case K::kickSub:     name = "Kick Sub"; break;
        case K::kick:        name = "Kick"; break;
        case K::snareTop:    name = "Snare Top"; break;
        case K::snareBottom: name = "Snare Bottom"; break;
        case K::snare:       name = "Snare"; break;
        case K::hiHat:       name = "Hi-Hat"; break;
        case K::tom:         name = "Tom"; break;
        case K::floorTom:    name = "Floor Tom"; break;
        case K::ride:        name = "Ride"; break;
        case K::overheads:   name = r.detail.isNotEmpty() ? "Overheads " + r.detail : "Overheads"; break;
        case K::roomMono:    name = "Room Mono"; break;
        case K::roomClose:   name = "Room Close"; break;
        case K::roomFar:     name = "Room Far"; break;
        case K::roomCrush:   name = "Room Crush"; break;
        case K::room:        name = r.detail.isNotEmpty() ? "Room " + r.detail : "Room"; break;
        default:             name = (clean == clean.toUpperCase() ? titleCase (clean) : clean); break; // keep "BB Kick Print", tidy "CRASH L"
    }

    if (r.confident)
    {
        const bool numbered = r.kind == K::tom || r.kind == K::floorTom || r.kind == K::kick || r.kind == K::snare;
        if (numbered && r.index > 0) name << " " << r.index;
        if (r.side.isNotEmpty()) name << " " << r.side;
    }

    r.displayName = name;
    return r;
}

std::vector<int> orderSession (const juce::StringArray& fileNames, std::vector<MicRole>& rolesOut)
{
    std::vector<MicRole> roles;
    for (auto& f : fileNames)
        roles.push_back (classifyMic (f));

    std::vector<int> order ((size_t) fileNames.size());
    std::iota (order.begin(), order.end(), 0);
    std::stable_sort (order.begin(), order.end(), [&] (int a, int b) { return roles[(size_t) a].rank < roles[(size_t) b].rank; });

    rolesOut.clear();
    for (int i : order)
        rolesOut.push_back (roles[(size_t) i]);

    // Number duplicate names in display order: Tom, Tom -> Tom 1, Tom 2
    std::map<juce::String, int> counts, seen;
    for (auto& r : rolesOut) counts[r.displayName]++;
    for (auto& r : rolesOut)
        if (counts[r.displayName] > 1)
        {
            const auto base = r.displayName;
            const int n = ++seen[base];
            if (r.side.isNotEmpty() && base.endsWith (" " + r.side))
                r.displayName = base.dropLastCharacters (r.side.length() + 1) + " " + juce::String (n) + " " + r.side;
            else
                r.displayName = base + " " + juce::String (n);
        }

    return order;
}

} // namespace ld
