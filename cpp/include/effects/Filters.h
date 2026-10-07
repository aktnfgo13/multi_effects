#pragma once

#include "effects/Biquad.h"

#include <cstddef>

namespace multi_effects
{

class Filter
{
public:
    void setLowPass(
        float sampleRate,
        float cutoff = 8000.0f,
        float q = 0.707f
    );

    void setHighPass(
        float sampleRate,
        float cutoff = 80.0f,
        float q = 0.707f
    );

    void setLowShelf(
        float sampleRate,
        float frequency = 200.0f,
        float gainDb = 6.0f,
        float slope = 1.0f
    );

    void setHighShelf(
        float sampleRate,
        float frequency = 4000.0f,
        float gainDb = 6.0f,
        float slope = 1.0f
    );

    void setPeaking(
        float sampleRate,
        float frequency = 1000.0f,
        float gainDb = 6.0f,
        float q = 1.0f
    );

    void reset();

    float processSample(float input);

    void processBlock(
        float* samples,
        std::size_t numSamples
    );

private:
    Biquad biquad_;
};

}