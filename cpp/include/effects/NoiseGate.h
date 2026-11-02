#pragma once

#include <cstddef>

namespace multi_effects
{

class NoiseGate
{
public:
    NoiseGate();

    void setSampleRate(float sampleRate);
    void setThresholdDb(float thresholdDb);
    void setAttackMs(float attackMs);
    void setReleaseMs(float releaseMs);

    void reset();

    float processSample(float input);

    void processBlock(
        float* samples,
        std::size_t numSamples
    );

    float getCurrentGain() const;

private:
    float sampleRate_;

    float thresholdDb_;
    float threshold_;

    float attackMs_;
    float releaseMs_;

    float attackCoeff_;
    float releaseCoeff_;

    float currentGain_;

    void updateThreshold();
    void updateCoefficients();
};

} // namespace multi_effects