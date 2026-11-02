#include "effects/NoiseGate.h"

#include <cmath>

namespace multi_effects
{

NoiseGate::NoiseGate()
    : sampleRate_(44100.0f),
      thresholdDb_(-45.0f),
      threshold_(0.0f),
      attackMs_(5.0f),
      releaseMs_(100.0f),
      attackCoeff_(0.0f),
      releaseCoeff_(0.0f),
      currentGain_(0.0f)
{
    updateThreshold();
    updateCoefficients();
}


void NoiseGate::setSampleRate(
    float sampleRate
)
{
    sampleRate_ = sampleRate;
    updateCoefficients();
}


void NoiseGate::setThresholdDb(
    float thresholdDb
)
{
    thresholdDb_ = thresholdDb;
    updateThreshold();
}


void NoiseGate::setAttackMs(
    float attackMs
)
{
    attackMs_ = attackMs;
    updateCoefficients();
}


void NoiseGate::setReleaseMs(
    float releaseMs
)
{
    releaseMs_ = releaseMs;
    updateCoefficients();
}


void NoiseGate::updateThreshold()
{
    threshold_ =
        std::pow(
            10.0f,
            thresholdDb_ / 20.0f
        );
}


void NoiseGate::updateCoefficients()
{
    attackCoeff_ =
        std::exp(
            -1.0f
            / (
                sampleRate_
                * attackMs_
                / 1000.0f
            )
        );

    releaseCoeff_ =
        std::exp(
            -1.0f
            / (
                sampleRate_
                * releaseMs_
                / 1000.0f
            )
        );
}


float NoiseGate::processSample(
    float input
)
{
    const float level =
        std::abs(input);

    float targetGain;
    float coeff;


    if (level >= threshold_)
    {
        targetGain = 1.0f;
        coeff = attackCoeff_;
    }
    else
    {
        targetGain = 0.0f;
        coeff = releaseCoeff_;
    }


    currentGain_ =
        coeff * currentGain_
        + (1.0f - coeff)
        * targetGain;


    return input * currentGain_;
}


void NoiseGate::processBlock(
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


void NoiseGate::reset()
{
    currentGain_ = 0.0f;
}


float NoiseGate::getCurrentGain() const
{
    return currentGain_;
}

} // namespace multi_effects