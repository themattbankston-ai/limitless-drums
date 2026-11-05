#include "Conform.h"
#include <cmath>

namespace ld
{

double nearestGridTime (const te::TempoSequence& tempo, double t, double gridBeats)
{
    const double beats = tempo.toBeats (te::TimePosition::fromSeconds (t)).inBeats();
    const double snapped = std::round (beats / gridBeats) * gridBeats;
    return tempo.toTime (te::BeatPosition::fromBeats (snapped)).inSeconds();
}

ConformReport conformToGrid (te::Edit& edit, const std::vector<double>& triggers, const ConformSettings& s)
{
    ConformReport report;

    if (triggers.empty())
        return report;

    // Cut points shared by every track. Slice k (k >= 1) holds trigger k-1.
    std::vector<double> cuts, deltas;

    for (auto t : triggers)
    {
        cuts.push_back (std::max (0.0, t - s.preRollSec));
        deltas.push_back ((nearestGridTime (edit.tempoSequence, t, s.gridBeats) - t) * s.strength);
        report.maxMoveMs = std::max (report.maxMoveMs, std::abs (deltas.back()) * 1000.0);
    }

    const auto xf = te::TimeDuration::fromSeconds (s.crossfadeSec);

    for (auto* track : te::getAudioTracks (edit))
    {
        // 1. Split at every cut, left to right.
        auto clips = track->getClips();

        if (clips.isEmpty())
            continue;

        te::Clip* current = clips.getFirst();

        for (auto c : cuts)
        {
            const auto tp = te::TimePosition::fromSeconds (c);

            if (current == nullptr || tp <= current->getPosition().getStart() || tp >= current->getPosition().getEnd())
                continue;

            current = track->splitClip (*current, tp);
        }

        // 2. Collect the slices in time order.
        std::vector<te::AudioClipBase*> slices;

        for (auto* c : track->getClips())
            if (auto* ac = dynamic_cast<te::AudioClipBase*> (c))
                slices.push_back (ac);

        std::sort (slices.begin(), slices.end(), [] (auto* a, auto* b)
                   { return a->getPosition().getStart() < b->getPosition().getStart(); });

        // 3. Move slice k by delta[k-1]. Slice 0 is the lead-in before the first hit.
        for (size_t k = 1; k < slices.size() && k - 1 < deltas.size(); ++k)
        {
            auto* sl = slices[k];
            const auto start = sl->getPosition().getStart();
            sl->setStart (start + te::TimeDuration::fromSeconds (deltas[k - 1]), false, true);
        }

        // 4. Fill and crossfade: every slice runs on until the next one starts, plus the fade.
        for (size_t k = 0; k + 1 < slices.size(); ++k)
        {
            auto* a = slices[k];
            auto* b = slices[k + 1];
            const auto seam = b->getPosition().getStart();

            a->setEnd (seam + xf, false);
            a->setFadeOutType (te::AudioFadeCurve::convex);
            a->setFadeOut (xf);
            b->setFadeInType (te::AudioFadeCurve::convex);
            b->setFadeIn (xf);
        }

        report.slices = std::max (report.slices, (int) slices.size());
    }

    return report;
}

} // namespace ld
