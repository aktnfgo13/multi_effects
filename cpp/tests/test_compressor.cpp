#include "effects/Compressor.h"

#include <cassert>
#include <cmath>
#include <iostream>


bool nearlyEqual(
    float a,
    float b,
    float tolerance = 0.0001f
)
{
    return std::fabs(a - b)
        < tolerance;
}


int main()
{
    using multi_effects::Compressor;


    // =====================================================
    // Ratio 1:1 = no compression
    // =====================================================

    Compressor identity;

    identity.setSampleRate(44100.0f);
    identity.setThresholdDb(-18.0f);
    identity.setRatio(1.0f);
    identity.setAttackMs(10.0f);
    identity.setReleaseMs(100.0f);
    identity.setMakeupGainDb(0.0f);


    const float identityOutput =
        identity.processSample(
            0.5f
        );


    assert(
        nearlyEqual(
            identityOutput,
            0.5f
        )
    );


    // =====================================================
    // Compression should reduce loud signal
    // =====================================================

    Compressor compressor;

    compressor.setSampleRate(44100.0f);
    compressor.setThresholdDb(-18.0f);
    compressor.setRatio(4.0f);
    compressor.setAttackMs(10.0f);
    compressor.setReleaseMs(100.0f);
    compressor.setMakeupGainDb(0.0f);


    float samples[2000];

    for (float& sample : samples)
    {
        sample = 1.0f;
    }


    compressor.processBlock(
        samples,
        2000
    );


    assert(
        samples[1999] < 1.0f
    );


    assert(
        compressor.getCurrentGainDb()
        < 0.0f
    );


    // =====================================================
    // Release should move gain back toward 0 dB
    // =====================================================

    const float gainAfterCompression =
        compressor.getCurrentGainDb();


    float silence[5000] = {};


    compressor.processBlock(
        silence,
        5000
    );


    const float gainAfterRelease =
        compressor.getCurrentGainDb();


    assert(
        gainAfterRelease
        > gainAfterCompression
    );


    // =====================================================
    // Reset
    // =====================================================

    compressor.reset();


    assert(
        nearlyEqual(
            compressor.getCurrentGainDb(),
            0.0f
        )
    );


    std::cout
        << "All Compressor tests passed."
        << std::endl;


    return 0;
}