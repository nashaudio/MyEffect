#pragma once

#include "PluginProcessor.h"
#include "AnalysisComponent.h"
#include "dsp.h"

using namespace juce;

class AnalysisWindow;

//==============================================================================
class PluginEditor : public AudioProcessorEditor,
                     public StandaloneHarnessEditor,
                     public Slider::Listener,
                     public ComboBox::Listener,
                     public Button::Listener,
                     private AudioProcessorListener,
                     private AsyncUpdater
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    void setParameters(MiniPlugin::Parameters* params);

    //==============================================================================
    void paint (Graphics&) override;
    void resized() override;

    void sliderValueChanged (Slider*) override;
    void comboBoxChanged (ComboBox*) override;
    void buttonClicked (Button*) override;
    void buttonStateChanged (Button*) override;
    void handleCommandMessage (int commandId) override;
    bool areStandaloneToolsExpanded() const noexcept override;

private:
    void audioProcessorParameterChanged (AudioProcessor*, int, float) override;
    void audioProcessorChanged (AudioProcessor*, const AudioProcessorListener::ChangeDetails&) override;
    void handleAsyncUpdate() override;
    void toggleAnalysisTools();
    void toggleAnalysisDetached();
    void updateAnalysisLayout();

    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    DSP::Parameters no_parameters;
    DSP::Parameters* parameters = nullptr;
    juce::Rectangle<float> group_bounds[32];

    std::unique_ptr<AnalysisComponent> analysisComponent;
    std::unique_ptr<AnalysisWindow> analysisWindow;
    bool analysisVisible = false;

    OwnedArray<Label> labels;
    Array<Component*> components;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
