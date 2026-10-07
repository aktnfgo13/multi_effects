#include "effects/Filters.h"

#include <cmath>

namespace multi_effects
{

namespace
{
constexpr float kPi = 3.14159265358979323846f;
}


// =========================================================
// Low Pass
// =========================================================

void Filter::setLowPass(
    float sampleRate,
    float cutoff,
    float q
)
{
    const float omega =
        2.0f * kPi * cutoff / sampleRate;

    const float sinOmega = std::sin(omega);
    const float cosOmega = std::cos(omega);

    const float alpha =
        sinOmega / (2.0f * q);

    float b0 =
        (1.0f - cosOmega) / 2.0f;

    float b1 =
        1.0f - cosOmega;

    float b2 =
        (1.0f - cosOmega) / 2.0f;

    const float a0 =
        1.0f + alpha;

    float a1 =
        -2.0f * cosOmega;

    float a2 =
        1.0f - alpha;

    b0 /= a0;
    b1 /= a0;
    b2 /= a0;

    a1 /= a0;
    a2 /= a0;

    biquad_.setCoefficients(
        b0,
        b1,
        b2,
        a1,
        a2
    );

    biquad_.reset();
}


// =========================================================
// High Pass
// =========================================================

void Filter::setHighPass(
    float sampleRate,
    float cutoff,
    float q
)
{
    const float omega =
        2.0f * kPi * cutoff / sampleRate;

    const float sinOmega = std::sin(omega);
    const float cosOmega = std::cos(omega);

    const float alpha =
        sinOmega / (2.0f * q);

    float b0 =
        (1.0f + cosOmega) / 2.0f;

    float b1 =
        -(1.0f + cosOmega);

    float b2 =
        (1.0f + cosOmega) / 2.0f;

    const float a0 =
        1.0f + alpha;

    float a1 =
        -2.0f * cosOmega;

    float a2 =
        1.0f - alpha;

    b0 /= a0;
    b1 /= a0;
    b2 /= a0;

    a1 /= a0;
    a2 /= a0;

    biquad_.setCoefficients(
        b0,
        b1,
        b2,
        a1,
        a2
    );

    biquad_.reset();
}


// =========================================================
// Low Shelf
// =========================================================

void Filter::setLowShelf(
    float sampleRate,
    float frequency,
    float gainDb,
    float slope
)
{
    const float A =
        std::pow(
            10.0f,
            gainDb / 40.0f
        );

    const float omega =
        2.0f * kPi
        * frequency
        / sampleRate;

    const float sinOmega =
        std::sin(omega);

    const float cosOmega =
        std::cos(omega);

    const float alpha =
        sinOmega
        / 2.0f
        * std::sqrt(
            (A + 1.0f / A)
            * (1.0f / slope - 1.0f)
            + 2.0f
        );

    const float sqrtA =
        std::sqrt(A);

    float b0 =
        A
        * (
            A + 1.0f
            - (A - 1.0f) * cosOmega
            + 2.0f * sqrtA * alpha
        );

    float b1 =
        2.0f
        * A
        * (
            A - 1.0f
            - (A + 1.0f) * cosOmega
        );

    float b2 =
        A
        * (
            A + 1.0f
            - (A - 1.0f) * cosOmega
            - 2.0f * sqrtA * alpha
        );

    const float a0 =
        A + 1.0f
        + (A - 1.0f) * cosOmega
        + 2.0f * sqrtA * alpha;

    float a1 =
        -2.0f
        * (
            A - 1.0f
            + (A + 1.0f) * cosOmega
        );

    float a2 =
        A + 1.0f
        + (A - 1.0f) * cosOmega
        - 2.0f * sqrtA * alpha;

    b0 /= a0;
    b1 /= a0;
    b2 /= a0;

    a1 /= a0;
    a2 /= a0;

    biquad_.setCoefficients(
        b0,
        b1,
        b2,
        a1,
        a2
    );

    biquad_.reset();
}


// =========================================================
// High Shelf
// =========================================================

void Filter::setHighShelf(
    float sampleRate,
    float frequency,
    float gainDb,
    float slope
)
{
    const float A =
        std::pow(
            10.0f,
            gainDb / 40.0f
        );

    const float omega =
        2.0f * kPi
        * frequency
        / sampleRate;

    const float sinOmega =
        std::sin(omega);

    const float cosOmega =
        std::cos(omega);

    const float alpha =
        sinOmega
        / 2.0f
        * std::sqrt(
            (A + 1.0f / A)
            * (1.0f / slope - 1.0f)
            + 2.0f
        );

    const float sqrtA =
        std::sqrt(A);

    float b0 =
        A
        * (
            A + 1.0f
            + (A - 1.0f) * cosOmega
            + 2.0f * sqrtA * alpha
        );

    float b1 =
        -2.0f
        * A
        * (
            A - 1.0f
            + (A + 1.0f) * cosOmega
        );

    float b2 =
        A
        * (
            A + 1.0f
            + (A - 1.0f) * cosOmega
            - 2.0f * sqrtA * alpha
        );

    const float a0 =
        A + 1.0f
        - (A - 1.0f) * cosOmega
        + 2.0f * sqrtA * alpha;

    float a1 =
        2.0f
        * (
            A - 1.0f
            - (A + 1.0f) * cosOmega
        );

    float a2 =
        A + 1.0f
        - (A - 1.0f) * cosOmega
        - 2.0f * sqrtA * alpha;

    b0 /= a0;
    b1 /= a0;
    b2 /= a0;

    a1 /= a0;
    a2 /= a0;

    biquad_.setCoefficients(
        b0,
        b1,
        b2,
        a1,
        a2
    );

    biquad_.reset();
}


// =========================================================
// Peaking EQ
// =========================================================

void Filter::setPeaking(
    float sampleRate,
    float frequency,
    float gainDb,
    float q
)
{
    const float A =
        std::pow(
            10.0f,
            gainDb / 40.0f
        );

    const float omega =
        2.0f * kPi
        * frequency
        / sampleRate;

    const float alpha =
        std::sin(omega)
        / (2.0f * q);

    const float cosOmega =
        std::cos(omega);

    float b0 =
        1.0f + alpha * A;

    float b1 =
        -2.0f * cosOmega;

    float b2 =
        1.0f - alpha * A;

    const float a0 =
        1.0f + alpha / A;

    float a1 =
        -2.0f * cosOmega;

    float a2 =
        1.0f - alpha / A;

    b0 /= a0;
    b1 /= a0;
    b2 /= a0;

    a1 /= a0;
    a2 /= a0;

    biquad_.setCoefficients(
        b0,
        b1,
        b2,
        a1,
        a2
    );

    biquad_.reset();
}


// =========================================================
// Processing
// =========================================================

void Filter::reset()
{
    biquad_.reset();
}


float Filter::processSample(
    float input
)
{
    return biquad_.processSample(
        input
    );
}


void Filter::processBlock(
    float* samples,
    std::size_t numSamples
)
{
    biquad_.processBlock(
        samples,
        numSamples
    );
}

} // namespace multi_effects