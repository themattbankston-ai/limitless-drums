// A test plugin for Limitless Drums' plugin hosting: a gain with 64 samples of reported latency (the output is
// delayed by exactly that), and its gain saved in the state.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
class P : public juce::AudioProcessor
{
public:
    P() : AudioProcessor (BusesProperties().withInput ("In", juce::AudioChannelSet::stereo()).withOutput ("Out", juce::AudioChannelSet::stereo()))
    { addParameter (gain = new juce::AudioParameterFloat (juce::ParameterID { "gain", 1 }, "Gain", 0.0f, 2.0f, 0.5f)); }
    const juce::String getName() const override { return "LD Test Gain"; }
    void prepareToPlay (double, int) override { for (auto& d : delay) d.assign (64, 0.0f); pos = 0; setLatencySamples (64); }
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& l) const override { return l.getMainInputChannelSet() == l.getMainOutputChannelSet() && ! l.getMainInputChannelSet().isDisabled(); }
    void processBlock (juce::AudioBuffer<float>& b, juce::MidiBuffer&) override
    {
        const float g = gain->get();
        for (int n = 0; n < b.getNumSamples(); ++n)
        {
            for (int c = 0; c < std::min (2, b.getNumChannels()); ++c) { float x = b.getSample (c, n); b.setSample (c, n, delay[c][pos] * g); delay[c][pos] = x; }
            pos = (pos + 1) % 64;
        }
    }
    double getTailLengthSeconds() const override { return 0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return new juce::GenericAudioProcessorEditor (*this); }
    bool hasEditor() const override { return true; }
    int getNumPrograms() override { return 1; } int getCurrentProgram() override { return 0; } void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; } void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock& m) override { juce::MemoryOutputStream (m, false).writeFloat (gain->get()); }
    void setStateInformation (const void* d, int n) override { if (n >= 4) *gain = juce::MemoryInputStream (d, (size_t) n, false).readFloat(); }
    juce::AudioParameterFloat* gain; std::vector<float> delay[2]; int pos = 0;
};
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new P(); }
