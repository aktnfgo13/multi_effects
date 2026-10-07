#include "effects/Distortion.h"

#include <cassert>
#include <cmath>
#include <iostream>


bool nearlyEqual(
    float a,
    float b,
    float tolerance = 0.0001f
)
{
    return std::fabs(
        a - b
    ) < tolerance;
}


int main()
{
    using multi_effects::Distortion;
    using multi_effects::DistortionMode;


    Distortion distortion;


    // =====================================================
    // Hard Clipping
    // =====================================================

    distortion.setMode(
        DistortionMode::HardClip
    );

    distortion.setDriveDb(
        0.0f
    );

    distortion.setThreshold(
        0.5f
    );


    assert(
        nearlyEqual(
            distortion.processSample(0.25f),
            0.5f
        )
    );


    assert(
        nearlyEqual(
            distortion.processSample(0.75f),
            1.0f
        )
    );


    assert(
        nearlyEqual(
            distortion.processSample(-0.75f),
            -1.0f
        )
    );


    // =====================================================
    // Soft Clipping
    // =====================================================

    distortion.setMode(
        DistortionMode::SoftClip
    );

    distortion.setDriveDb(
        0.0f
    );


    assert(
        nearlyEqual(
            distortion.processSample(0.5f),
            std::tanh(0.5f)
        )
    );


    // =====================================================
    // Drive
    // =====================================================

    distortion.setDriveDb(
        6.0206f
    );


    assert(
        nearlyEqual(
            distortion.getLinearDrive(),
            2.0f,
            0.001f
        )
    );


    // =====================================================
    // Block Processing
    // =====================================================

    distortion.setMode(
        DistortionMode::HardClip
    );

    distortion.setDriveDb(
        0.0f
    );

    distortion.setThreshold(
        0.5f
    );


    float samples[] = {
        -0.75f,
        -0.25f,
        0.0f,
        0.25f,
        0.75f,
    };


    distortion.processBlock(
        samples,
        5
    );


    assert(
        nearlyEqual(
            samples[0],
            -1.0f
        )
    );

    assert(
        nearlyEqual(
            samples[1],
            -0.5f
        )
    );

    assert(
        nearlyEqual(
            samples[2],
            0.0f
        )
    );

    assert(
        nearlyEqual(
            samples[3],
            0.5f
        )
    );

    assert(
        nearlyEqual(
            samples[4],
            1.0f
        )
    );


    std::cout
        << "All Distortion tests passed."
        << std::endl;


    return 0;
}