#pragma once

#include <cstddef>

namespace multi_effects
{

class Compressor
{
public:
    Compressor();

    void setSampleRate(float sampleRate);

    void setThresholdDb(float thresholdDb);
    void setRatio(float ratio);
    void setAttackMs(float attackMs);
    void setReleaseMs(float releaseMs);
    void setMakeupGainDb(float makeupGainDb);

    void reset();

    float processSample(float input);

    void processBlock(
        float* samples,
        std::size_t numSamples
    );

    float getCurrentGainDb() const;

private:
    float sampleRate_;

    float thresholdDb_;
    float ratio_;

    float attackMs_;
    float releaseMs_;

    float makeupGainDb_;

    float attackCoeff_;
    float releaseCoeff_;

    float currentGainDb_;

    void updateCoefficients();

    float calculateTargetGainDb(
        float detectorLevel
    ) const;
};

} // namespace multi_effects