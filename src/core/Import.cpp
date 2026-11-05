#include "Import.h"
#include <map>

namespace ld
{

bool isAudioFile (const juce::File& f)
{
    return f.existsAsFile() && f.hasFileExtension ("wav;wave;aif;aiff;bwf;flac") && ! f.getFileName().startsWith ("._");
}

static bool looksLikeReference (const juce::File& f)
{
    const auto n = f.getFileNameWithoutExtension().toLowerCase();
    return n.contains ("one shot") || n.contains ("oneshot") || n.contains ("one-shot") || n.contains ("reference");
}

static ImportTake makeTake (const juce::String& title, const juce::File& folder, juce::Array<juce::File> files)
{
    ImportTake take;
    take.title = title;
    take.folder = folder;

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();

    juce::Array<juce::File> mics;
    for (auto& f : files)
    {
        if (looksLikeReference (f)) take.references.push_back (f);
        else mics.add (f);
    }

    juce::StringArray names;
    std::vector<double> lengths, rates;
    for (auto& f : mics)
    {
        names.add (f.getFileName());
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
        rates.push_back (r != nullptr ? r->sampleRate : 0.0);
        lengths.push_back (r != nullptr && r->sampleRate > 0 ? r->lengthInSamples / r->sampleRate : 0.0);
        if (r == nullptr) take.warnings.add ("Couldn't read " + f.getFileName());
    }

    std::vector<MicRole> roles;
    auto order = orderSession (names, roles);

    for (size_t i = 0; i < order.size(); ++i)
    {
        const int src = order[i];
        if (rates[(size_t) src] <= 0) continue;
        take.mics.push_back ({ roles[i].displayName, mics[src] });
        take.roles.push_back (roles[i]);
    }

    // Rate and length checks: mics of one take should match.
    double maxLen = 0, minLen = 1e12;
    std::map<double, int> rateCounts;
    for (size_t i = 0; i < rates.size(); ++i)
        if (rates[i] > 0)
        {
            rateCounts[rates[i]]++;
            maxLen = std::max (maxLen, lengths[i]);
            minLen = std::min (minLen, lengths[i]);
        }

    if (! rateCounts.empty())
        take.sampleRate = std::max_element (rateCounts.begin(), rateCounts.end(),
                                            [] (auto& a, auto& b) { return a.second < b.second; })->first;
    if (rateCounts.size() > 1)
        take.warnings.add ("These files use more than one sample rate; they'll play at "
                           + juce::String (take.sampleRate / 1000.0, 1) + " kHz.");
    take.lengthSeconds = maxLen;
    if (maxLen - minLen > 0.01)
        take.warnings.add ("The files aren't all the same length (" + juce::String (minLen, 2) + " to "
                           + juce::String (maxLen, 2) + " s). They're lined up at the start.");
    return take;
}

std::vector<ImportTake> planImport (const juce::Array<juce::File>& items)
{
    std::vector<ImportTake> takes;
    juce::Array<juce::File> loose;

    for (auto& item : items)
    {
        if (item.isDirectory())
        {
            // Every folder (this one and below) that directly holds audio files is a take.
            juce::Array<juce::File> folders { item };
            for (const auto& entry : juce::RangedDirectoryIterator (item, true, "*", juce::File::findDirectories))
                folders.add (entry.getFile());

            folders.sort();
            for (auto& folder : folders)
            {
                juce::Array<juce::File> audio;
                for (const auto& entry : juce::RangedDirectoryIterator (folder, false, "*", juce::File::findFiles))
                    if (isAudioFile (entry.getFile()))
                        audio.add (entry.getFile());

                if (! audio.isEmpty())
                {
                    audio.sort();
                    const auto title = folder == item ? folder.getFileName()
                                                      : folder.getRelativePathFrom (item.getParentDirectory()).replace ("/", " / ");
                    takes.push_back (makeTake (title, folder, audio));
                }
            }
        }
        else if (isAudioFile (item))
        {
            loose.add (item);
        }
    }

    if (! loose.isEmpty())
    {
        loose.sort();
        takes.insert (takes.begin(), makeTake (loose[0].getParentDirectory().getFileName(), loose[0].getParentDirectory(), loose));
    }

    // Drop takes made only of references (e.g. a "One Shot" folder) but keep their files on the first real take.
    std::vector<ImportTake> out;
    std::vector<juce::File> strayRefs;
    for (auto& t : takes)
    {
        if (t.mics.empty()) strayRefs.insert (strayRefs.end(), t.references.begin(), t.references.end());
        else out.push_back (std::move (t));
    }
    if (! out.empty())
        for (auto& t : out)
            t.references.insert (t.references.end(), strayRefs.begin(), strayRefs.end());

    return out;
}

} // namespace ld
