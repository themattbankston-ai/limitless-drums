#include "Library.h"

namespace ld
{
namespace
{
const juce::uint32 palette[] { 0xff9b6bff, 0xffff3da8, 0xff2ee6ff, 0xffffb23e, 0xff3dffd2, 0xff7cff9b, 0xffff6a3d, 0xffff7ad9 };
juce::var obj() { return juce::var (new juce::DynamicObject()); }
}

LibraryStore::LibraryStore (juce::File f) : file (std::move (f)) { load(); }

juce::File LibraryStore::defaultFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Limitless Drums").getChildFile ("Libraries.json");
}

void LibraryStore::load()
{
    libraries.clear();
    const auto v = juce::JSON::parse (file);
    if (auto* libs = v["libraries"].getArray())
        for (auto& l : *libs)
        {
            Library L;
            L.name = l["name"].toString();
            L.colour = (juce::uint32) (juce::int64) l["colour"];
            if (auto* ss = l["sessions"].getArray())
                for (auto& s : *ss)
                {
                    LibrarySession S;
                    S.id = s["id"].toString(); S.name = s["name"].toString(); S.drum = s["drum"].toString(); S.artic = s["artic"].toString();
                    S.project = s["project"].toString(); S.vel = s["vel"]; S.rr = s["rr"]; S.settings = s["settings"];
                    S.modified = (juce::int64) s["modified"];
                    L.sessions.push_back (std::move (S));
                }
            if (L.name.isNotEmpty()) libraries.push_back (std::move (L));
        }
    current = v["current"].toString();
    if (libraries.empty()) libraries.push_back ({ "My Library", palette[0], {} });
    if (find (current) == nullptr) current = libraries.front().name;
}

bool LibraryStore::save() const
{
    auto root = obj();
    juce::Array<juce::var> libs;
    for (auto& L : libraries)
    {
        auto l = obj();
        l.getDynamicObject()->setProperty ("name", L.name);
        l.getDynamicObject()->setProperty ("colour", (juce::int64) L.colour);
        juce::Array<juce::var> ss;
        for (auto& S : L.sessions)
        {
            auto s = obj(); auto* d = s.getDynamicObject();
            d->setProperty ("id", S.id); d->setProperty ("name", S.name); d->setProperty ("drum", S.drum); d->setProperty ("artic", S.artic);
            d->setProperty ("project", S.project); d->setProperty ("vel", S.vel); d->setProperty ("rr", S.rr); d->setProperty ("settings", S.settings);
            d->setProperty ("modified", S.modified);
            ss.add (s);
        }
        l.getDynamicObject()->setProperty ("sessions", ss);
        libs.add (l);
    }
    root.getDynamicObject()->setProperty ("libraries", libs);
    root.getDynamicObject()->setProperty ("current", current);
    file.getParentDirectory().createDirectory();
    return file.replaceWithText (juce::JSON::toString (root));
}

Library* LibraryStore::find (const juce::String& name)
{
    for (auto& L : libraries) if (L.name == name) return &L;
    return nullptr;
}

bool LibraryStore::add (const juce::String& raw)
{
    const auto name = raw.trim();
    if (name.isEmpty() || find (name) != nullptr) return false;
    libraries.push_back ({ name, palette[libraries.size() % std::size (palette)], {} });
    return true;
}

void LibraryStore::put (const juce::String& library, const LibrarySession& s)
{
    for (auto& L : libraries)
        L.sessions.erase (std::remove_if (L.sessions.begin(), L.sessions.end(), [&] (auto& x) { return x.id == s.id; }), L.sessions.end());
    if (find (library) == nullptr) add (library);
    if (auto* L = find (library)) L->sessions.push_back (s);
}

const LibrarySession* LibraryStore::findSession (const juce::String& id) const
{
    for (auto& L : libraries) for (auto& s : L.sessions) if (s.id == id) return &s;
    return nullptr;
}
} // namespace ld
