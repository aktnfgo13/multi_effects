#include "effects/NoiseGate.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>

int main()
{
    std::array<float, 8192> input{};
    for (std::size_t i = 0; i < input.size(); ++i)
        input[i] = i < 4000 ? 0.5f : 0.0001f;
    auto whole = input;
    auto split = input;
    multi_effects::NoiseGate a, b;
    a.processBlock(whole.data(), whole.size());
    b.processBlock(nullptr, 0);
    for (std::size_t offset = 0; offset < split.size();)
    {
        const auto count = std::min<std::size_t>(1 + offset % 257, split.size() - offset);
        b.processBlock(split.data() + offset, count);
        offset += count;
    }
    if (whole != split || a.getCurrentGain() != b.getCurrentGain())
    {
        std::cerr << "Block partition changed state/output\n";
        return 1;
    }
    b.reset();
    split = input;
    b.processBlock(split.data(), split.size());
    if (whole != split)
    {
        std::cerr << "Reset did not reproduce initial response\n";
        return 1;
    }
    multi_effects::NoiseGate boundary;
    boundary.setThresholdDb(0.0f);
    if (!(boundary.processSample(1.0f) > 0.0f))
    {
        std::cerr << "Threshold equality must open gate\n";
        return 1;
    }
    return 0;
}
