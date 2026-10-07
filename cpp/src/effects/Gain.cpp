#include "effects/Gain.h"

#include <algorithm>
#include <cmath>

namespace multi_effects
{

Gain::Gain()
    : gainDb_(0.0f),
      linearGain_(1.0f)
{
}


void Gain::setGainDb(float gainDb)
{
    gainDb_ = gainDb;

    updateLinearGain();
}


float Gain::getGainDb() const
{
    return gainDb_;
}


float Gain::getLinearGain() const
{
    return linearGain_;
}


void Gain::updateLinearGain()
{
    linearGain_ = std::pow(
        10.0f,
        gainDb_ / 20.0f
    );
}


float Gain::processSample(float input) const
{
    const float output =
        input * linearGain_;

    return std::clamp(
        output,
        -1.0f,
        1.0f
    );
}


void Gain::processBlock(
    float* samples,
    std::size_t numSamples
) const
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