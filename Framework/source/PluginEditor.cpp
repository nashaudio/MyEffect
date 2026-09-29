#include "PluginEditor.h"

using namespace juce;

static const Colour klangblue(0xFF00AEEF);

namespace
{
constexpr int analysisToolsToggleCommandId = 0x4d594154; // "MYAT"
constexpr int editorWidth = 400;
constexpr int editorHeight = 300;
constexpr int analysisPanelWidth = 500;
}

class AnalysisWindow final : public juce::DocumentWindow
{
public:
    explicit AnalysisWindow (AnalysisComponent& content)
        : DocumentWindow ("Analysis Tools",
                          juce::Colour (0xff24272d),
                          DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar (true);
        setResizable (true, true);
        setResizeLimits (500, 260, 4096, 2160);
        setContentNonOwned (&content, false);
        centreWithSize (800, 500);
    }

    void closeButtonPressed() override
    {
        setVisible (false);
    }
};

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p)
{
    if (p.wrapperType == juce::AudioProcessor::wrapperType_Standalone)
    {
        analysisComponent = std::make_unique<AnalysisComponent> (p.getAnalysisBuffer());
        analysisComponent->setDetachCallback ([this] { toggleAnalysisDetached(); });
        addChildComponent (*analysisComponent);
    }

    setSize (editorWidth, editorHeight);

    MiniPlugin::Effect* effect = p.getEffect();
    setParameters(&effect->parameters);
}

PluginEditor::~PluginEditor()
{
    if (analysisWindow != nullptr)
        analysisWindow->clearContentComponent();

    analysisWindow.reset();

    for(int c=0; c<components.size(); c++){
        delete components[c];
        components.getReference(c) = NULL;
    }
}

void PluginEditor::handleCommandMessage (int commandId)
{
    if (commandId == analysisToolsToggleCommandId)
        toggleAnalysisTools();
    else
        AudioProcessorEditor::handleCommandMessage (commandId);
}

void PluginEditor::toggleAnalysisTools()
{
    if (analysisComponent == nullptr)
        return;

    if (analysisWindow != nullptr)
    {
        const auto shouldShow = ! analysisWindow->isVisible();
        analysisWindow->setVisible (shouldShow);

        if (shouldShow)
            analysisWindow->toFront (true);

        return;
    }

    analysisVisible = ! analysisVisible;
    analysisComponent->setVisible (analysisVisible);
    updateAnalysisLayout();
}

void PluginEditor::toggleAnalysisDetached()
{
    if (analysisComponent == nullptr)
        return;

    if (analysisWindow != nullptr)
    {
        analysisWindow->clearContentComponent();
        analysisWindow.reset();
        analysisComponent->setDetached (false);
        addAndMakeVisible (*analysisComponent);
        analysisVisible = true;
        updateAnalysisLayout();
        return;
    }

    removeChildComponent (analysisComponent.get());
    analysisComponent->setDetached (true);
    analysisWindow = std::make_unique<AnalysisWindow> (*analysisComponent);
    analysisWindow->setVisible (true);
    analysisWindow->toFront (true);
    analysisVisible = false;
    updateAnalysisLayout();
}

void PluginEditor::updateAnalysisLayout()
{
    const auto docked = analysisComponent != nullptr
                     && analysisWindow == nullptr
                     && analysisVisible;
    setSize (editorWidth + (docked ? analysisPanelWidth : 0), editorHeight);
    resized();
}

class Meter : public Slider
{
public:
    String getTextFromValue (double value) override
    {
        return "";
    }
};

class Wheel : public Slider
{
    float initialValue;
        
public:
    Wheel() : Slider(), initialValue(0) { }
    virtual ~Wheel() { }
    
    void setInitialValue(float value){
        initialValue = value;
    }
    
    void stoppedDragging() {
        if(initialValue != 0.0)
            setValue(initialValue);
    }
};

void PluginEditor::setParameters(MiniPlugin::Parameters* pparams) {
    parameters = pparams;

    // using namespace DSP;

    const DSP::Parameters& params = *parameters;
    const int kNumberOfControls = (int)params.size();
    labels.clear(true);

    for(int c=0; c<components.size(); c++){
        delete components[c];
        components.getReference(c) = NULL;
    }

    components.resize(kNumberOfControls);

    // add controls..
    for(int c=0; c<kNumberOfControls; c++){
        const DSP::Parameter& parameter = params[c];
        labels.add(new Label());
        
        switch(parameter.type){
        case DSP::Parameter::WHEEL:
        case DSP::Parameter::ROTARY:
        case DSP::Parameter::SLIDER:
        case DSP::Parameter::METER:
        {   components.getReference(c) = parameter.type == DSP::Parameter::METER ? new ::Meter()
                                       : parameter.type == DSP::Parameter::WHEEL ? new Wheel()
                                       : new juce::Slider();
            juce::Slider* pSlider = (juce::Slider*)components[c];
        
            addAndMakeVisible (pSlider);
            
            switch((int)parameter.type){
            case DSP::Parameter::WHEEL:  ((Wheel*)pSlider)->setInitialValue(parameter.initial);
            case DSP::Parameter::ROTARY: pSlider->setSliderStyle (juce::Slider::Rotary); break;
            case DSP::Parameter::METER:  pSlider->setEnabled(false);
                                    pSlider->setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
            case DSP::Parameter::SLIDER: pSlider->setSliderStyle (parameter.size.width > parameter.size.height ? juce::Slider::LinearBar : juce::Slider::LinearBarVertical); break;
                                    break;
            }
            if (parameter.size.width > 0 && parameter.size.width < 40)
                pSlider->setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
            else
                pSlider->setTextBoxStyle (parameter.type == DSP::Parameter::WHEEL ? juce::Slider::NoTextBox : juce::Slider::TextBoxBelow, false, 50, 17);
            pSlider->addListener (this);
            pSlider->setRange (parameter.min, parameter.max, 0.001);
            pSlider->setColour(juce::Slider::thumbColourId, parameter.type == DSP::Parameter::METER ? Colour(255,255,255) : Colour(255,255,255));
            pSlider->setColour(juce::Slider::rotarySliderFillColourId, Colour(255,255,255));
            pSlider->setDoubleClickReturnValue(true, parameter.initial);
            pSlider->setValue (static_cast<float> (parameter), dontSendNotification);
            
            // add a label
            labels[c]->setInterceptsMouseClicks(false, false);
            labels[c]->setText(parameter.name.c_str(), dontSendNotification);
            labels[c]->attachToComponent(components[c], false);
            if(parameter.type == DSP::Parameter::WHEEL){
                labels[c]->setFont (Font (13.0f, Font::bold));
                labels[c]->setColour(Label::textColourId, Colour(128,128,128));
            }else{
                labels[c]->setFont (Font (13.0f));
            }
            labels[c]->setJustificationType(Justification::centred);
        } break;
        case DSP::Parameter::TOGGLE:
        case DSP::Parameter::BUTTON:
        {   components.getReference(c) = new TextButton();
            TextButton* pButton = (TextButton*)components[c];
                
            addAndMakeVisible (pButton);
            pButton->addListener (this);
            pButton->setButtonText(parameter.name.c_str());
            pButton->setClickingTogglesState(parameter.type == DSP::Parameter::TOGGLE);
            pButton->setToggleState (static_cast<float> (parameter) >= 0.5f,
                                     dontSendNotification);

            pButton->setColour(TextButton::buttonColourId, Colour(255,255,255));
            pButton->setColour(TextButton::buttonOnColourId, klangblue);
        } break;
        case DSP::Parameter::MENU:
        {
            components.getReference(c) = new ComboBox();
            ComboBox* pList = (ComboBox*)components[c];
            addAndMakeVisible(pList);
            const unsigned int nbOptions = parameter.options.size();
            for(unsigned int i=0; /*i<8 &&*/ i<nbOptions; i++)
                pList->addItem(parameter.options[i].c_str(), i+1);
            pList->addListener (this);
            pList->setColour(ComboBox::buttonColourId, Colour(192,192,192));
            pList->setSelectedId (juce::roundToInt (static_cast<float> (parameter)) + 1,
                                  dontSendNotification);
            
            // add a label
            labels[c]->setText(parameter.name.c_str(), dontSendNotification);
            labels[c]->attachToComponent(components[c], false);
            labels[c]->setFont (Font (13.0f));
            labels[c]->setJustificationType(Justification::centred);
        }
        }

        // labels[c]->setLookAndFeel(&pluginLF);
        // components[c]->setLookAndFeel(&pluginLF);
    }

    resized();

    klang::Array<klang::Control::Group, 10>& groups = parameters->groups;
    for (unsigned int g = 0; g < groups.count; g++) {
        juce::Rectangle <float>& bounds = group_bounds[g];
        bounds.setBounds(0, 0, 0, 0);
        const klang::Control::Group& group = groups[g];
        if (group.size.x == -1) {
            for (unsigned int c = 0; c < group.length; c++) {
                if (!parameters || group.start + c >= parameters->count) {
                    groups.clear();
                    return;
                }
                juce::Component* control = components[group.start + c];
                juce::Rectangle<int> control_bounds = control->getBounds();
                if (labels[group.start + c]->getTextValue() == "")
                    control_bounds.setTop(control_bounds.getY() + 15);
                bounds = bounds.getUnion(control_bounds.toFloat());
            }
            bounds.expand(7, 7);
            bounds.setTop(bounds.getY() - 30);// size = { bounds.getX(), bounds.getY() - 25, bounds.getWidth(), bounds.getHeight() + 25 };
        }
        else if (group.size.width == -1) {
            juce::Rectangle<int> control_bounds(0, 0, 0, 0);
            int margin = 0;
            for (unsigned int c = 0; c < group.length; c++) {
                if (!parameters || group.start + c >= parameters->count) {
                    groups.clear();
                    return;
                }

                // juce::Component* control = components[group.start + c];
                const DSP::Parameter& param = (*parameters)[group.start + c];
                juce::Rectangle<int> bounds(param.size.x == -1 ? 75 * c + 7 : param.size.x,
                    param.size.y == -1 ? 0 : param.size.y,
                    (param.size.width == -1 ? 50 : param.size.width),
                    (param.size.height == -1 ? 60 : param.size.height) + 44);
                if (c == 0)
                    margin = bounds.getX();
                //control_bounds.setPosition(group.size.x + (param.size.x == -1 ? 75 * c : param.size.x) - 5.f, 
                //                           group.size.y + (param.size.y == -1 ? 0 : param.size.y) - 7.5f);
                //control_bounds.setSize(param.size.width == -1 ? 50.f : (float)param.size.width,
                //                       param.size.height == -1 ? 60.f : (float)param.size.height);
                //if (labels[group.start + c]->getTextValue() == "")
                //    control_bounds.setTop(control_bounds.getY() + 15.f);
                control_bounds = control_bounds.getUnion(bounds);
            }
            bounds.setPosition((float)group.size.x, (float)group.size.y);
            bounds.setSize((float)control_bounds.getRight(), (float)control_bounds.getBottom());
            bounds.removeFromRight((float)-margin);
        }
        else {
            bounds.setBounds((float)group.size.x, (float)group.size.y, (float)group.size.width, (float)group.size.height);
        }
    }
}

void PluginEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    // g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    auto area = getLocalBounds();
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);

    g.setColour(Colour(116,116,116));
    g.fillAll();
    
    if (parameters) {
		klang::Array<klang::Control::Group, 10>& groups = parameters->groups;
        for (unsigned int grp = 0; grp < groups.count; grp++) {
            klang::Control::Group& group = groups[grp];
            juce::Rectangle<float> bounds = group_bounds[grp].toFloat();// ((float)group.size.x, (float)group.size.y, (float)group.size.width, (float)group.size.height);
            if (bounds.getWidth() <= 0)
                continue;
            g.setColour(Colour(0xFF666666));
            g.fillRoundedRectangle(bounds, 3.f);

            g.setColour(Colours::white.withAlpha(0.8f));
            g.setFont(Font(11.f, Font::bold));
            g.drawText(group.name.c_str(), bounds.withTrimmedTop(2.5f), Justification::centredTop, false);

            bounds.setTop(bounds.getY() + 15.f);
            bounds.reduce(1.f, 1.f);
            g.setColour(Colours::white.withAlpha(0.5f));
            g.fillRoundedRectangle(bounds, 3.f);
        }
    }

}

void PluginEditor::resized()
{
    if (analysisComponent != nullptr && analysisWindow == nullptr && analysisVisible)
        analysisComponent->setBounds (getLocalBounds().removeFromRight (analysisPanelWidth).reduced (6));

    juce::Rectangle<int> size;
    const MiniPlugin::Parameters& params = *parameters;

    for(int c=0; c<components.size(); c++){
        if (!components[c]) break;
        const DSP::Parameter& parameter = params[c];

        const klang::Control::Group* group = parameters->getGroup(c);
        const bool relativeToGroup = group && group->size.isRelative();
        const DSP::Parameter::Size offset = relativeToGroup ? DSP::Parameter::Size(group->size.x - (group->size.isAuto() ? 5 : 0) + (parameter.size.x == -1 ? 7 : 0),
                                                                                   group->size.y - (group->size.isAuto() ? 30 : -37),
                                                                                   group->size.width == -1 ? 400 : group->size.width, 
                                                                                   group->size.height == -1 ? 250 : group->size.height )
                                                            : DSP::Parameter::Size(12.5, 50, 400, 250);
        const int columnsPerRow = relativeToGroup ? int((offset.width - 7.5) / 75) : 5;
        if(parameter.size.isAuto()){
			const int gc = relativeToGroup ? c - group->start : c;
            const int column = gc % columnsPerRow;
            const int row = gc  / columnsPerRow;
            
            switch (parameter.type) {
            case DSP::Parameter::WHEEL:
            case DSP::Parameter::ROTARY:
            case DSP::Parameter::SLIDER:
            case DSP::Parameter::METER:
                size.setBounds(offset.x + 75 * column, offset.y + 115 * row, 75, 90);
                break;
            case DSP::Parameter::TOGGLE:
            case DSP::Parameter::BUTTON:
                size.setBounds(offset.x + 75 * column, offset.y + 115 * row, 75, 90);
                break;
            case DSP::Parameter::MENU:
                size.setBounds(offset.x + 75 * column, offset.y + 115 * row, 75, 30);
                break;
            }
        }else{
            if(relativeToGroup)
                size.setBounds(offset.x + parameter.size.x, offset.y + parameter.size.y, parameter.size.width, parameter.size.height);
            else
                size.setBounds(parameter.size.x, parameter.size.y, parameter.size.width, parameter.size.height);
        }

        components[c]->setBounds (size.getX(), size.getY(), size.getWidth(), size.getHeight());
        if (labels[c]->isVisible()) {
            labels[c]->setTopLeftPosition(size.getX() - 20, size.getY() - 20);
            labels[c]->setSize(size.getWidth() + 40, 20);
        } else {
            labels[c]->setTopLeftPosition(size.getX() - 20, size.getY());
            labels[c]->setSize(size.getWidth() + 40, 0);
        }
    }
}

void PluginEditor::sliderValueChanged (juce::Slider* slider)
{
    for(int c=0; c<components.size(); c++){
        if(components[c] == slider){
            DSP::Parameter& parameter = (*parameters)[c];
            parameter.value = (float)slider->getValue();
            break;
        }
    }
}

void PluginEditor::buttonClicked (juce::Button* button)
{
    for(int c=0; c<components.size(); c++){
        if(components[c] == button){
            DSP::Parameter& parameter = (*parameters)[c];
            parameter.value = (float)button->getToggleState();
            break;
        }
    }
}

void PluginEditor::comboBoxChanged (juce::ComboBox* comboBox)
{
    for(int c=0; c<components.size(); c++){
        if(components[c] == comboBox){
            DSP::Parameter& parameter = (*parameters)[c];
            parameter.value = (float)comboBox->getSelectedId() - 1.f;
            break;
        }
    }
}

void PluginEditor::buttonStateChanged (juce::Button* button)
{
    for(int c=0; c<components.size(); c++){
        if(components[c] == button){
            DSP::Parameter& parameter = (*parameters)[c];
            parameter.value = (float)button->getToggleState();
            break;
        }
    }
}
