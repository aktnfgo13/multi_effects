#pragma once
#include "engine/Engine.h"
#include <cmath>
#include <stdexcept>
#include <string>

inline void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
inline void require(bool condition, const std::string& message)
{
    if (!condition) throw std::runtime_error(message);
}
template<class F> void requireRejected(F action, const std::string& message)
{
    bool rejected = false;
    try { action(); } catch (const std::exception&) { rejected = true; }
    require(rejected, message);
}
inline multi_effects::Chain makeTestChain()
{
    multi_effects::Chain chain;
    for (const auto& type : {"noise_gate", "compressor", "gain", "distortion", "eq", "amp_sim", "cabinet", "delay", "reverb", "tremolo"})
        chain.effects.push_back(multi_effects::createEffect(type, std::string(type) + "_1"));
    return chain;
}
inline float testSignal(std::size_t sample, double rate)
{
    // Bursts, silence, DC and impulses exercise dynamics, tails and state recovery.
    const auto second = std::size_t(sample / rate);
    if (second % 5 == 4) return 0;
    return float(.13 * std::sin(6.283185307179586 * 110 * sample / rate)
                 + .04 * std::sin(6.283185307179586 * 2300 * sample / rate)
                 + (sample % 4093 == 0 ? .3 : 0) + (second % 5 == 2 ? .01 : 0));
}
