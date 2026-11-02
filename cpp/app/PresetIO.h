#pragma once
#include "engine/Engine.h"
#include <juce_audio_formats/juce_audio_formats.h>

// Non-realtime application services shared by the GUI and offline tests.
// No audio-device or GUI initialization is required.
namespace multi_effects::app
{
struct ImpulseAsset
{
    std::vector<float> samples;
    double sampleRate;
};
juce::var serializePreset(const Chain& chain);
void savePreset(const Chain& chain, const juce::File& file);
Chain loadPreset(const juce::File& file);
ImpulseAsset loadImpulse(const juce::File& file);
}
