#include "effects/Compressor.h"

#include <algorithm>
#include <cmath>

namespace multi_effects
{

namespace
{

float dbToLinear(float db)
{
    return std::pow(
        10.0f,
        db / 20.0f
    );
}


float linearToDb(float value)
{
    return 20.0f
        * std::log10(
            std::max(
                value,
                1.0e-12f
            )
        );
}

} // namespace


Compressor::Compressor()
    : sampleRate_(44100.0f),
      thresholdDb_(-18.0f),
      ratio_(4.0f),
      attackMs_(10.0f),
      releaseMs_(100.0f),
      makeupGainDb_(0.0f),
      attackCoeff_(0.0f),
      releaseCoeff_(0.0f),
      currentGainDb_(0.0f)
{
    updateCoefficients();
}


void Compressor::setSampleRate(
    float sampleRate
)
{
    sampleRate_ = sampleRate;

    updateCoefficients();
}


void Compressor::setThresholdDb(
    float thresholdDb
)
{
    thresholdDb_ = thresholdDb;
}


void Compressor::setRatio(
    float ratio
)
{
    ratio_ = ratio;
}


void Compressor::setAttackMs(
    float attackMs
)
{
    attackMs_ = attackMs;

    updateCoefficients();
}


void Compressor::setReleaseMs(
    float releaseMs
)
{
    releaseMs_ = releaseMs;

    updateCoefficients();
}


void Compressor::setMakeupGainDb(
    float makeupGainDb
)
{
    makeupGainDb_ = makeupGainDb;
}


void Compressor::updateCoefficients()
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


float Compressor::calculateTargetGainDb(
    float detectorLevel
) const
{
    const float levelDb =
        linearToDb(
            detectorLevel
        );


    if (levelDb <= thresholdDb_)
    {
        return 0.0f;
    }


    return (
        thresholdDb_
        + (
            levelDb
            - thresholdDb_
        ) / ratio_
        - levelDb
    );
}


float Compressor::processSample(
    float input
)
{
    const float detectorLevel =
        std::abs(input);


    const float targetGainDb =
        calculateTargetGainDb(
            detectorLevel
        );


    const float coeff =
        targetGainDb < currentGainDb_
        ? attackCoeff_
        : releaseCoeff_;


    currentGainDb_ =
        coeff * currentGainDb_
        + (1.0f - coeff)
        * targetGainDb;


    const float totalGainDb =
        currentGainDb_
        + makeupGainDb_;


    const float gainLinear =
        dbToLinear(
            totalGainDb
        );


    return input * gainLinear;
}


void Compressor::processBlock(
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


void Compressor::reset()
{
    currentGainDb_ = 0.0f;
}


float Compressor::getCurrentGainDb() const
{
    return currentGainDb_;
}

} // namespace multi_effects