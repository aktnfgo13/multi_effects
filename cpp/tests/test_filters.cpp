#include "effects/Filters.h"

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
    using multi_effects::Filter;

    constexpr float sampleRate =
        44100.0f;


    // =====================================================
    // Peaking EQ - 0 dB = Identity
    // =====================================================

    Filter peaking;

    peaking.setPeaking(
        sampleRate,
        1000.0f,
        0.0f,
        1.0f
    );

    float peakingSamples[] = {
        -1.0f,
        -0.5f,
        0.0f,
        0.5f,
        1.0f
    };

    peaking.processBlock(
        peakingSamples,
        5
    );

    assert(
        nearlyEqual(
            peakingSamples[0],
            -1.0f
        )
    );

    assert(
        nearlyEqual(
            peakingSamples[4],
            1.0f
        )
    );


    // =====================================================
    // Low Shelf - 0 dB = Identity
    // =====================================================

    Filter lowShelf;

    lowShelf.setLowShelf(
        sampleRate,
        200.0f,
        0.0f,
        1.0f
    );

    float lowShelfSamples[] = {
        0.25f,
        -0.25f,
        0.5f
    };

    lowShelf.processBlock(
        lowShelfSamples,
        3
    );

    assert(
        nearlyEqual(
            lowShelfSamples[0],
            0.25f
        )
    );


    // =====================================================
    // High Shelf - 0 dB = Identity
    // =====================================================

    Filter highShelf;

    highShelf.setHighShelf(
        sampleRate,
        4000.0f,
        0.0f,
        1.0f
    );

    float highShelfSamples[] = {
        0.25f,
        -0.25f,
        0.5f
    };

    highShelf.processBlock(
        highShelfSamples,
        3
    );

    assert(
        nearlyEqual(
            highShelfSamples[0],
            0.25f
        )
    );


    // =====================================================
    // Low Pass sanity test
    // =====================================================

    Filter lowPass;

    lowPass.setLowPass(
        sampleRate,
        8000.0f,
        0.707f
    );

    const float lp =
        lowPass.processSample(
            1.0f
        );

    assert(
        std::isfinite(lp)
    );


    // =====================================================
    // High Pass sanity test
    // =====================================================

    Filter highPass;

    highPass.setHighPass(
        sampleRate,
        80.0f,
        0.707f
    );

    const float hp =
        highPass.processSample(
            1.0f
        );

    assert(
        std::isfinite(hp)
    );


    std::cout
        << "All Filter tests passed."
        << std::endl;

    return 0;
}