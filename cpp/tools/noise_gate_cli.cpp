#include "effects/NoiseGate.h"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

int main(int argc, char* argv[])
{
    try
    {
        if (argc != 5)
            throw std::invalid_argument("Usage: noise_gate_cli sample_rate threshold_db attack_ms release_ms < samples.txt");

        const float sampleRate = std::stof(argv[1]);
        const float threshold = std::stof(argv[2]);
        const float attack = std::stof(argv[3]);
        const float release = std::stof(argv[4]);
        if (!std::isfinite(sampleRate) || sampleRate <= 0.0f
            || !std::isfinite(threshold) || !std::isfinite(attack)
            || attack <= 0.0f || !std::isfinite(release) || release <= 0.0f)
            throw std::invalid_argument("Invalid gate parameters");

        multi_effects::NoiseGate gate;
        gate.setSampleRate(sampleRate);
        gate.setThresholdDb(threshold);
        gate.setAttackMs(attack);
        gate.setReleaseMs(release);
        std::cout << std::setprecision(std::numeric_limits<float>::max_digits10);
        float sample;
        while (std::cin >> sample)
        {
            if (!std::isfinite(sample))
                throw std::invalid_argument("Non-finite input");
            std::cout << gate.processSample(sample) << '\n';
        }
        if (!std::cin.eof())
            throw std::invalid_argument("Malformed input");
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
