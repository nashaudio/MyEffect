#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "../../Source/EffectPlugin.h"

#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <vector>

// Keep the effect implementation identical for the supplied VS/Xcode projects
// and the CMake build. These are the default DSP translation units.
#include "../../Source/EffectExtra.cpp"
#include "../../Source/EffectPlugin.cpp"

#include "include.cpp"

//==============================================================================
PluginProcessor::PluginProcessor()
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
{
    effect = new MyEffect();
}

PluginProcessor::~PluginProcessor()
{
    delete effect;
}

//==============================================================================
const juce::String PluginProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PluginProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool PluginProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool PluginProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double PluginProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PluginProcessor::getNumPrograms()
{
    return 1 + static_cast<int> (effect->presets.count);
}

int PluginProcessor::getCurrentProgram()
{
    return currentProgram.load (std::memory_order_acquire);
}

void PluginProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms()))
        return;

    {
        const juce::ScopedLock lock (getCallbackLock());
        auto& parameters = effect->parameters;

        if (index == 0)
        {
            for (unsigned int p = 0; p < parameters.count; ++p)
                parameters[p].set (parameters[p].initial);
        }
        else
        {
            const auto& preset = effect->presets[index - 1];
            for (unsigned int p = 0; p < juce::jmin (parameters.count, preset.values.count); ++p)
                parameters[p].set (preset.values[p]);
        }

        currentProgram.store (index, std::memory_order_release);
        effect->onPreset (index - 1);
        effect->presetLoaded (index - 1, getProgramName (index).toRawUTF8());
    }

    updateHostDisplay (juce::AudioProcessorListener::ChangeDetails{}.withProgramChanged (true));
}

const juce::String PluginProcessor::getProgramName (int index)
{
    if (index == 0)
        return "Default";

    if (juce::isPositiveAndBelow (index, getNumPrograms()))
        return effect->presets[index - 1].name.c_str();

    return {};
}

void PluginProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    ::stk::Stk::setSampleRate(float(sampleRate));
    analysisBuffer.setSampleRate (sampleRate);
    juce::ignoreUnused (sampleRate, samplesPerBlock);
}

void PluginProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);

    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // This is the place where you'd normally do the guts of your plugin's
    // audio processing...
    // Make sure to reset the state if your inner loop is processing
    // the samples and the outer loop is handling the channels.
    // Alternatively, you can process the samples with the channels
    // interleaved by keeping the same state.

    if (! standaloneHarnessBypassed.load (std::memory_order_acquire))
        effect->process((const float**)buffer.getArrayOfReadPointers(),
                        (float**)buffer.getArrayOfWritePointers(),
                        buffer.getNumSamples());

    if (wrapperType == juce::AudioProcessor::wrapperType_Standalone)
        analysisBuffer.push (buffer);

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer (channel);
        juce::ignoreUnused (channelData);
        // ..do something to the data...
    }
}

//==============================================================================
bool PluginProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

//==============================================================================
void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const juce::ScopedLock lock (getCallbackLock());
    juce::ValueTree state ("MyEffectState");
    state.setProperty ("program", getCurrentProgram(), nullptr);
    const auto& parameters = effect->parameters;

    for (unsigned int index = 0; index < parameters.count; ++index)
        state.setProperty ("parameter" + juce::String (index),
                           static_cast<float> (parameters[index]), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr || ! xml->hasTagName ("MyEffectState"))
        return;

    const auto state = juce::ValueTree::fromXml (*xml);
    {
        const juce::ScopedLock lock (getCallbackLock());
        auto& parameters = effect->parameters;

        for (unsigned int index = 0; index < parameters.count; ++index)
        {
            const auto property = juce::Identifier ("parameter" + juce::String (index));

            if (state.hasProperty (property))
                parameters[index].set (static_cast<float> (state[property]));
        }

        currentProgram.store (juce::jlimit (0, getNumPrograms() - 1,
                                           static_cast<int> (state.getProperty ("program", 0))),
                              std::memory_order_release);
    }

    updateHostDisplay (juce::AudioProcessorListener::ChangeDetails{}.withProgramChanged (true));
}

std::string PluginProcessor::getStandaloneSettingsText() const
{
    std::vector<float> values;
    values.reserve (effect->parameters.count);

    {
        const juce::ScopedLock lock (getCallbackLock());
        for (unsigned int p = 0; p < effect->parameters.count; ++p)
        {
            const auto& parameter = effect->parameters[p];
            values.push_back (parameter.type == DSP::Parameter::METER || parameter.type == DSP::Parameter::BUTTON
                                  ? 0.f : static_cast<float> (parameter));
        }
    }

    std::ostringstream text;
    text.imbue (std::locale::classic());
    text << "{ \"Copied Preset\", { ";

    for (std::size_t i = 0; i < values.size(); ++i)
    {
        if (i != 0)
            text << ", ";

        const auto value = values[i];
        if (! std::isfinite (value))
            text << "0";
        else if (value == 0.f && std::signbit (value))
            text << "-0.f";
        else
        {
            std::ostringstream literal;
            literal.imbue (std::locale::classic());
            literal << std::setprecision (std::numeric_limits<float>::max_digits10) << value;
            const auto scalar = literal.str();
            text << scalar;
            if (scalar.find_first_of (".eE") != std::string::npos)
                text << 'f';
        }
    }

    text << " } },";
    return text.str();
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
