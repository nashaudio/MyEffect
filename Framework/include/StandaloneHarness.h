#pragma once

// Optional control surface implemented by the MyEffect adapter and discovered
// by JUCE's standalone wrapper. Keeping this interface independent of JUCE
// lets the harness bypass only student DSP while preserving file playback and
// post-output analysis.
class StandaloneHarnessControl
{
public:
    virtual ~StandaloneHarnessControl() = default;

    virtual void setStandaloneHarnessBypassed (bool shouldBypass) noexcept = 0;
    virtual bool isStandaloneHarnessBypassed() const noexcept = 0;
};
