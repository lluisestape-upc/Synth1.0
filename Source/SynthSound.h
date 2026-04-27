#pragma once
#include <JuceHeader.h>

// Marker class — applies to all MIDI notes and channels.
struct SynthSound : public juce::SynthesiserSound
{
    bool appliesToNote    (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};
