#include "effects/Distortion.h"

#include <algorithm>
#include <cmath>

namespace multi_effects
{

Distortion::Distortion()
    : mode_(DistortionMode::SoftClip),
      driveDb_(12.0f),
      linearDrive_(1.0f),
      threshold_(0.7f)
{
    updateLinearDrive();
}


void Distortion::setMode(
    DistortionMode mode
)
{
    mode_ = mode;
}


void Distortion::setDriveDb(
    float driveDb
)
{
    driveDb_ = driveDb;

    updateLinearDrive();
}


void Distortion::setThreshold(
    float threshold
)
{
    threshold_ = threshold;
}


DistortionMode Distortion::getMode() const
{
    return mode_;
}


float Distortion::getDriveDb() const
{
    return driveDb_;
}


float Distortion::getLinearDrive() const
{
    return linearDrive_;
}


float Distortion::getThreshold() const
{
    return threshold_;
}


void Distortion::updateLinearDrive()
{
    linearDrive_ = std::pow(
        10.0f,
        driveDb_ / 20.0f
    );
}


float Distortion::processSample(
    float input
) const
{
    const float drivenSignal =
        input * linearDrive_;


    if (
        mode_
        == DistortionMode::HardClip
    )
    {
        const float output =
            std::clamp(
                drivenSignal,
                -threshold_,
                threshold_
            );

        return output / threshold_;
    }


    return std::tanh(
        drivenSignal
    );
}


void Distortion::processBlock(
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