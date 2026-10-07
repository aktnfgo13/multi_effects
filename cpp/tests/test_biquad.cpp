#include "effects/Biquad.h"

#include <cassert>
#include <cmath>
#include <iostream>


bool nearlyEqual(
    float a,
    float b,
    float tolerance = 0.000001f
)
{
    return std::fabs(
        a - b
    ) < tolerance;
}


int main()
{
    using multi_effects::Biquad;


    // =====================================================
    // Identity filter
    // =====================================================

    Biquad biquad;

    biquad.setCoefficients(
        1.0f,
        0.0f,
        0.0f,
        0.0f,
        0.0f
    );


    assert(
        nearlyEqual(
            biquad.processSample(0.5f),
            0.5f
        )
    );

    assert(
        nearlyEqual(
            biquad.processSample(-0.25f),
            -0.25f
        )
    );


    // =====================================================
    // Block processing
    // =====================================================

    biquad.reset();


    float samples[] = {
        -1.0f,
        -0.5f,
        0.0f,
        0.5f,
        1.0f
    };


    biquad.processBlock(
        samples,
        5
    );


    assert(
        nearlyEqual(samples[0], -1.0f)
    );

    assert(
        nearlyEqual(samples[1], -0.5f)
    );

    assert(
        nearlyEqual(samples[2], 0.0f)
    );

    assert(
        nearlyEqual(samples[3], 0.5f)
    );

    assert(
        nearlyEqual(samples[4], 1.0f)
    );


    // =====================================================
    // State test
    // y[n] = x[n] + 0.5 * x[n-1]
    // =====================================================

    biquad.reset();

    biquad.setCoefficients(
        1.0f,
        0.5f,
        0.0f,
        0.0f,
        0.0f
    );


    const float first =
        biquad.processSample(
            1.0f
        );

    const float second =
        biquad.processSample(
            0.0f
        );


    assert(
        nearlyEqual(
            first,
            1.0f
        )
    );

    assert(
        nearlyEqual(
            second,
            0.5f
        )
    );


    // =====================================================
    // Reset test
    // =====================================================

    biquad.reset();


    const float afterReset =
        biquad.processSample(
            0.0f
        );


    assert(
        nearlyEqual(
            afterReset,
            0.0f
        )
    );


    std::cout
        << "All Biquad tests passed."
        << std::endl;


    return 0;
}