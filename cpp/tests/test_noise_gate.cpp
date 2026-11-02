#include "effects/NoiseGate.h"

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
    using multi_effects::NoiseGate;


    // =====================================================
    // Gate should open for loud signal
    // =====================================================

    NoiseGate gate;

    gate.setSampleRate(44100.0f);
    gate.setThresholdDb(-45.0f);
    gate.setAttackMs(5.0f);
    gate.setReleaseMs(100.0f);


    float loudSignal[2000];

    for (float& sample : loudSignal)
    {
        sample = 1.0f;
    }


    gate.processBlock(
        loudSignal,
        2000
    );


    assert(
        gate.getCurrentGain()
        > 0.9f
    );


    assert(
        loudSignal[1999]
        > 0.9f
    );


    // =====================================================
    // Gate should close for quiet signal
    // =====================================================

    const float gainBeforeRelease =
        gate.getCurrentGain();


    float quietSignal[10000];

    for (float& sample : quietSignal)
    {
        sample = 0.0001f;
    }


    gate.processBlock(
        quietSignal,
        10000
    );


    const float gainAfterRelease =
        gate.getCurrentGain();


    assert(
        gainAfterRelease
        < gainBeforeRelease
    );


    // =====================================================
    // Reset
    // =====================================================

    gate.reset();


    assert(
        nearlyEqual(
            gate.getCurrentGain(),
            0.0f
        )
    );


    std::cout
        << "All NoiseGate tests passed."
        << std::endl;


    return 0;
}