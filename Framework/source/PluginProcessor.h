#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "AnalysisComponent.h"
#include "StandaloneHarness.h"
#include "dsp.h"

#if (MSVC)
#include "ipps.h"
#endif

//class MyEffect;

class PluginProcessor : public juce::AudioProcessor,
                        public StandaloneHarnessControl
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    MiniPlugin::Effect* getEffect() { return effect; }
    AnalysisBuffer& getAnalysisBuffer() noexcept { return analysisBuffer; }
    void setStandaloneHarnessBypassed (bool shouldBypass) noexcept override
    {
        standaloneHarnessBypassed.store (shouldBypass, std::memory_order_release);
    }
    bool isStandaloneHarnessBypassed() const noexcept override
    {
        return standaloneHarnessBypassed.load (std::memory_order_acquire);
    }
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)

    MiniPlugin::Effect* effect = nullptr;
    AnalysisBuffer analysisBuffer;
    std::atomic<bool> standaloneHarnessBypassed { false };
};
