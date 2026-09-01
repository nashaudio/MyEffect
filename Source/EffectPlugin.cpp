//
//  EffectPlugin.cpp
//  MyEffect Plugin Source Code
//
//  Used to define the bodies of functions used by the plugin, as declared in EffectPlugin.h.
//

#include "EffectPlugin.h"

////////////////////////////////////////////////////////////////////////////
// EFFECT - represents the whole effect plugin
////////////////////////////////////////////////////////////////////////////

// Constructor: called when the effect is first created / loaded
MyEffect::MyEffect()
{       
    //==========================================================================
    // CONTROLS - Use this array to completely specify your UI
    // - tells the system what parameters you want, and how they are controlled
    // - add or remove parameters by adding or removing entries from the list
    // - each control should have an expressive label / caption
    // - controls can be of different types: ROTARY, BUTTON, TOGGLE, SLIDER, or MENU (see definitions)
    // - for rotary and linear sliders, you can set the range of values (make sure the initial value is inside the range)
    // - for menus, replace the three numeric values with a single array of option strings: e.g. { "one", "two", "three" }
    // - by default, the controls are laid out in a grid, but you can also move and size them manually
    //   i.e. replace AUTO_SIZE with { 50,50,100,100 } to place a 100x100 control at (50,50)

    parameters = {
        //  name,       type,              min, max, initial, size
        { "Group", { Dial("One"), Dial("Two"), Dial("Three") }},
        // Toggle("Param 2", 0),
        // Menu("Param 3", "Option 1", "Option 2", "Option 3"),
        // Dial("Param 4",  0.0, 1.0, 0.0),
        // Dial("Param 5",  0.0, 1.0, 0.0),
        // Dial("Param 6",  0.0, 1.0, 0.0),
        // Dial("Param 7",  0.0, 1.0, 0.0),
        // Dial("Param 8",  0.0, 1.0, 0.0),
        // Dial("Param 9",  0.0, 1.0, 0.0),
    };

    presets = {
        { "Preset 1", { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
        { "Preset 2", { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
        { "Preset 3", { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    };
}

// Destructor: called when the effect is terminated / unloaded
MyEffect::~MyEffect()
{
    // Put your own additional clean up code here (e.g. free memory)
}

// EVENT HANDLERS: handle different user input (button presses, preset selection, drop menus)

void MyEffect::presetLoaded(int iPresetNum, const char *sPresetName)
{
    // A preset has been loaded, so you could perform setup, such as retrieving parameter values
    // using getParameter and use them to set state variables in the plugin
}

void MyEffect::optionChanged(int iOptionMenu, int iItem)
{
    // An option menu, with index iOptionMenu, has been changed to the entry, iItem
}

void MyEffect::buttonPressed(int iButton)
{
    // A button, with index iButton, has been pressed
}

// Applies audio processing to a buffer of audio
// (inputBuffer contains the input audio, and processed samples should be stored in outputBuffer)
void MyEffect::process(const float** inputBuffers, float** outputBuffers, int numSamples)
{
    const float* input[2]  = { inputBuffers[0],  inputBuffers[1] };
          float* output[2] = { outputBuffers[0], outputBuffers[1] };
    
    //float fGain = parameters[0];
    
    while(numSamples--)
    {
        // input samples
        float in0 = *input[0]++;
        float in1 = *input[1]++;

        // output samples
        float &out0 = *output[0]++;
        float &out1 = *output[1]++;
        
        // Add your effect processing here
        float noise = ((float)rand() / (float)RAND_MAX) * 0.1f - 0.05f; // random value between -0.05 and 0.05
        out0 = in0 * parameters[0];
        out1 = in1 * parameters[0];
    }
}
