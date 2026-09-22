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
    // PARAMETERS - Use this array to completely specify your UI
    // - tells the system what parameters you want, and how they are controlled
    // - add or remove parameters by adding or removing entries from the list
    // - each control should have an expressive label / caption
    // - controls can be of different types: Dial, Button, Toggle, Slider, or Menu (see definitions)
    // - default ranges are 0.0 to 1.0, but you can specify different ranges for dials and sliders, plus an initial value
    // - for menus, list the oprtions after the menu label: e.g. "one", "two", "three" 
    // - by default, the controls are laid out in a 5x2 grid, but you can also move and size them manually
    //   i.e. add { 50,50,100,100 } to place a 100x100 control at (50,50)
    // - controls can be grouped together by adding them to a group { "Group Name", ... }, which can also be sized and positioned manually

    parameters = {
        Dial("Param 0"), Dial("Param 1"), Dial("Param 2"), Dial("Param 3"), Dial("Param 4"),
        Dial("Param 5"), Dial("Param 6"), Dial("Param 7"), Dial("Param 8"), Dial("Param 9"),
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
    
    float gain = parameters[0];
    
    while(numSamples--)
    {
        // input / output samples
        float   in0 = *input[0]++,    in1 = *input[1]++;
        float &out0 = *output[0]++, &out1 = *output[1]++;
        
        // Add your effect processing here
        out0 = in0 * gain;
        out1 = in1 * gain;
    }
}
