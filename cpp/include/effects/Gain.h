#pragma once

#include <cstddef>

namespace multi_effects
{

class Gain
{
public:
    Gain();

    void setGainDb(float gainDb);

    float getGainDb() const;
    float getLinearGain() const;

    float processSample(float input) const;

    void processBlock(
        float* samples,
        std::size_t numSamples
    ) const;

private:
    float gainDb_;
    float linearGain_;

    void updateLinearGain();
};

} // namespace multi_effects