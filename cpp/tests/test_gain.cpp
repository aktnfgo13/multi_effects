#include "effects/Gain.h"

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
    using multi_effects::Gain;

    Gain gain;


    // ========================================
    // Test 1
    // 0 dB
    // ========================================

    gain.setGainDb(
        0.0f
    );

    assert(
        nearlyEqual(
            gain.processSample(0.5f),
            0.5f
        )
    );


    // ========================================
    // Test 2
    // +6.0206 dB ≈ x2
    // ========================================

    gain.setGainDb(
        6.0206f
    );

    assert(
        nearlyEqual(
            gain.processSample(0.25f),
            0.5f
        )
    );


    // ========================================
    // Test 3
    // -6.0206 dB ≈ x0.5
    // ========================================

    gain.setGainDb(
        -6.0206f
    );

    assert(
        nearlyEqual(
            gain.processSample(0.5f),
            0.25f
        )
    );


    // ========================================
    // Test 4
    // Clipping
    // ========================================

    gain.setGainDb(
        20.0f
    );

    assert(
        nearlyEqual(
            gain.processSample(0.5f),
            1.0f
        )
    );


    // ========================================
    // Test 5
    // Block Processing
    // ========================================

    gain.setGainDb(
        0.0f
    );

    float samples[] = {
        -0.5f,
        0.0f,
        0.25f,
        0.5f,
    };

    gain.processBlock(
        samples,
        4
    );

    assert(
        nearlyEqual(
            samples[0],
            -0.5f
        )
    );

    assert(
        nearlyEqual(
            samples[2],
            0.25f
        )
    );


    std::cout
        << "All Gain tests passed."
        << std::endl;

    return 0;
}