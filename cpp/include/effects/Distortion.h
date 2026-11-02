#pragma once

#include <cstddef>

namespace multi_effects
{

enum class DistortionMode
{
    HardClip,
    SoftClip
};


class Distortion
{
public:
    Distortion();

    void setMode(DistortionMode mode);
    void setDriveDb(float driveDb);
    void setThreshold(float threshold);

    DistortionMode getMode() const;
    float getDriveDb() const;
    float getLinearDrive() const;
    float getThreshold() const;

    float processSample(float input) const;

    void processBlock(
        float* samples,
        std::size_t numSamples
    ) const;

private:
    DistortionMode mode_;

    float driveDb_;
    float linearDrive_;
    float threshold_;

    void updateLinearDrive();
};

} // namespace multi_effects