#pragma once

#include <cstddef>

namespace multi_effects
{

class Biquad
{
public:
    Biquad();

    void setCoefficients(
        float b0,
        float b1,
        float b2,
        float a1,
        float a2
    );

    void reset();

    float processSample(float input);

    void processBlock(
        float* samples,
        std::size_t numSamples
    );

private:
    float b0_;
    float b1_;
    float b2_;
    float a1_;
    float a2_;

    float x1_;
    float x2_;
    float y1_;
    float y2_;
};

} // namespace multi_effects