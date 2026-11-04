#pragma once
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace multi_effects
{
struct ParameterInfo
{
    std::string id, name, unit;
    float minimum, maximum, initial;
    bool smooth = true;
    bool requiresPrepare = false;
};
struct Parameter
{
    ParameterInfo info;
    std::atomic<float> target;
    explicit Parameter(ParameterInfo p) : info(std::move(p)), target(info.initial) {}
    void set(float value);
};
struct ProcessSpec { double sampleRate = 44100; std::size_t maxBlockSize = 512; };
class Effect
{
public:
    virtual ~Effect() = default;
    virtual std::size_t latencySamples() const noexcept { return 0; }
    virtual void prepare(ProcessSpec) = 0;
    virtual void reset() = 0;
    virtual void process(float*, std::size_t) noexcept = 0;
    std::string id, type, name;
    std::vector<std::unique_ptr<Parameter>> parameters;
    std::atomic<bool> bypass{false};
    Parameter* parameter(const std::string& key);
    // Prepared only while callback is stopped. Raw asset data is serialized outside DSP.
    std::vector<float> impulseResponse;
    double impulseSampleRate = 44100;
    std::string impulseName; // Optional preset metadata; never read by processing.
    // Audio-thread diagnostic. Read only after processing is stopped.
    std::size_t invalidSampleCount = 0;
};
using SampleProcessor = std::function<float(float, const std::vector<float>&)>;
struct EffectDefinition
{
    std::string type, name;
    std::vector<ParameterInfo> parameters;
    std::function<SampleProcessor(double, const std::vector<float>&)> create;
};
const std::vector<EffectDefinition>& registry();
std::unique_ptr<Effect> createEffect(const std::string& type, const std::string& id);
class Chain
{
public:
    // Structural edits, prepare, reset and destruction require a stopped callback.
    std::vector<std::unique_ptr<Effect>> effects;
    void prepare(ProcessSpec spec);
    void reset();
    void process(float* samples, std::size_t count) noexcept;
    void move(std::size_t from, std::size_t to);
};
Chain cloneConfiguration(const Chain&); // Non-realtime snapshot; no DSP state copy.
}
