#include "Session.h"

namespace ld
{

std::unique_ptr<te::Edit> createSessionEdit (te::Engine& engine,
                                             const std::vector<MicFile>& mics,
                                             double bpm,
                                             te::Edit::EditRole role)
{
    auto edit = te::Edit::createSingleTrackEdit (engine, role);
    edit->ensureNumberOfAudioTracks ((int) mics.size());
    edit->tempoSequence.getTempo (0)->setBpm (bpm);
    edit->getMasterVolumePlugin()->setVolumeDb (0.0f);

    auto tracks = te::getAudioTracks (*edit);

    for (size_t i = 0; i < mics.size(); ++i)
    {
        te::AudioFile af (engine, mics[i].file);

        if (! af.isValid())
            return {};

        auto* track = tracks[(int) i];
        track->setName (mics[i].name);

        const auto length = te::TimeDuration::fromSeconds (af.getLength());
        te::ClipPosition pos { te::TimeRange (te::TimePosition(), length), te::TimeDuration() };

        auto clip = track->insertWaveClip (mics[i].name, mics[i].file, pos, false);

        if (clip == nullptr)
            return {};

        clip->setAutoCrossfade (false);
    }

    return edit;
}

juce::AudioBuffer<float> renderTrack (te::Edit& edit, int trackIndex, double sampleRate)
{
    juce::TemporaryFile temp (".wav");

    auto tracks = te::getAllTracks (edit);
    auto audioTracks = te::getAudioTracks (edit);
    auto* target = audioTracks[trackIndex];

    juce::BigInteger tracksToDo;
    tracksToDo.setBit (tracks.indexOf (target));

    te::Renderer::Parameters params (edit);
    params.destFile = temp.getFile();
    params.audioFormat = edit.engine.getAudioFileFormatManager().getWavFormat();
    params.bitDepth = 32;
    params.sampleRateForAudio = sampleRate;
    params.blockSizeForAudio = 512;
    params.time = te::TimeRange (te::TimePosition(), edit.getLength());
    params.tracksToDo = tracksToDo;
    params.usePlugins = false;
    params.useMasterPlugins = false;
    params.mustRenderInMono = true;
    params.ditheringEnabled = false;

    te::Renderer::renderToFile ("Render track", params);

    return readMono (temp.getFile());
}

juce::AudioBuffer<float> readMono (const juce::File& f, double* sampleRateOut)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (f));

    if (reader == nullptr)
        return {};

    juce::AudioBuffer<float> buffer ((int) reader->numChannels, (int) reader->lengthInSamples);
    reader->read (&buffer, 0, buffer.getNumSamples(), 0, true, true);

    if (sampleRateOut != nullptr)
        *sampleRateOut = reader->sampleRate;

    juce::AudioBuffer<float> mono (1, buffer.getNumSamples());
    mono.copyFrom (0, 0, buffer, 0, 0, buffer.getNumSamples());
    return mono;
}

} // namespace ld
