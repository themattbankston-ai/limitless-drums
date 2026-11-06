#include "Engine.h"

namespace ld
{
std::shared_ptr<EngineState> openTake (const std::vector<juce::File>& files, const juce::String& folderName, const TakeOptions& o)
{
    auto st = std::make_shared<EngineState>();
    std::vector<std::string> rel;
    for (auto& f : files) rel.push_back ((folderName + "/" + f.getFileName()).toStdString());
    const auto recs = hl::groupSessions (rel, o.drum == "kit");
    const auto drum = ! o.drum.empty() ? o.drum : recs.empty() ? std::string ("snare") : recs[0].drum;
    const auto artic = ! o.artic.empty() ? o.artic : recs.empty() ? std::string ("Hit") : recs[0].artic;
    std::vector<hl::SourceFile> src;
    for (size_t i = 0; i < files.size(); ++i) src.push_back ({ files[i].getFileName().toStdString(), rel[i], hl::readWav (files[i]) });
    auto opened = hl::openSession (std::move (src), drum, artic);
    st->session = std::move (opened.session);
    st->errors = std::move (opened.errors);
    auto& S = st->session;
    if (S.tracks.empty()) return st;
    hl::applySettings (S, o.settings);
    if (o.vel > 0 && o.rr > 0 && (o.vel != S.det.V || o.rr != S.det.RR)) { S.det.V = o.vel; S.det.RR = o.rr; }
    return st;
}

std::shared_ptr<EngineState> prepareTake (const std::vector<juce::File>& files, const juce::String& folderName, const TakeOptions& o)
{
    auto st = openTake (files, folderName, o);
    if (st->session.tracks.empty()) return st;
    st->prepared = hl::autoPrep (st->session);
    if (! st->prepared) st->advice = hl::hitCountAdvice (st->session);
    return st;
}

void renderAndMeasure (EngineState& st)
{
    if (st.session.tracks.empty()) return;
    st.mix = hl::renderMix (st.session);
    if (st.session.mode != "kit") st.levels = hl::measureLevels (st.session, *st.mix);
}

} // namespace ld
