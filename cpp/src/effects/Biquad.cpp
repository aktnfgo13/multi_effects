#include "effects/Biquad.h"

namespace multi_effects
{

Biquad::Biquad()
    : b0_(1.0f),
      b1_(0.0f),
      b2_(0.0f),
      a1_(0.0f),
      a2_(0.0f),
      x1_(0.0f),
      x2_(0.0f),
      y1_(0.0f),
      y2_(0.0f)
{
}


void Biquad::setCoefficients(
    float b0,
    float b1,
    float b2,
    float a1,
    float a2
)
{
    b0_ = b0;
    b1_ = b1;
    b2_ = b2;

    a1_ = a1;
    a2_ = a2;
}


void Biquad::reset()
{
    x1_ = 0.0f;
    x2_ = 0.0f;

    y1_ = 0.0f;
    y2_ = 0.0f;
}


float Biquad::processSample(
    float input
)
{
    const float output =
        b0_ * input
        + b1_ * x1_
        + b2_ * x2_
        - a1_ * y1_
        - a2_ * y2_;


    x2_ = x1_;
    x1_ = input;

    y2_ = y1_;
    y1_ = output;


    return output;
}


void Biquad::processBlock(
    float* samples,
    std::size_t numSamples
)
{
    for (
        std::size_t i = 0;
        i < numSamples;
        ++i
    )
    {
        samples[i] =
            processSample(
                samples[i]
            );
    }
}

} // namespace multi_effects