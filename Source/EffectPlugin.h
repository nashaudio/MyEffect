//
//  EffectPlugin.h
//  MyEffect Plugin Header File
//
//  Used to declare objects and data structures used by the plugin.
//

#pragma once

// Directory scanned by the standalone app's simulated-input file player.
// Relative paths are resolved from the directory containing the executable.
inline constexpr const char* standaloneAudioFileDirectory = "../../Tests";

#include "dsp.h"
using namespace DSP;

#include "EffectExtra.h"

class MyEffect : public DSP::Effect
{
public:
    MyEffect();     // constructor (initialise variables, etc.)
    ~MyEffect();    // destructor (clean up, free memory, etc.)

    void setSampleRate(float sampleRate){ stk::Stk::setSampleRate(sampleRate); }
    float getSampleRate() const { return stk::Stk::sampleRate(); };
    
    void process(const float** inputBuffers, float** outputBuffers, int numSamples);
    
    void presetLoaded(int iPresetNum, const char *sPresetName);
    void optionChanged(int iOptionMenu, int iItem);
    void buttonPressed(int iButton);
};
