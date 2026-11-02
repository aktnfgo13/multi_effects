#include "engine/Engine.h"
#include "effects/TimeEffects.h"
#include "effects/AmpSim.h"
#include "effects/Cabinet.h"
#include "effects/NoiseGate.h"
#include "effects/Compressor.h"
#include "effects/Distortion.h"
#include <stdexcept>

namespace multi_effects
{
static_assert(std::atomic<float>::is_always_lock_free, "Realtime parameter requires lock-free float");
static_assert(std::atomic<bool>::is_always_lock_free, "Realtime bypass requires lock-free bool");
void Parameter::set(float value)
{
    if (std::isfinite(value)) target.store(std::clamp(value, info.minimum, info.maximum), std::memory_order_relaxed);
}
Parameter* Effect::parameter(const std::string& key)
{
    for (auto& p : parameters) if (p->info.id == key) return p.get();
    return nullptr;
}
class Builtin final : public Effect
{
public:
    explicit Builtin(const EffectDefinition& d) : definition_(d)
    {
        type = d.type; name = d.name;
        for (const auto& p : d.parameters) parameters.push_back(std::make_unique<Parameter>(p));
        values_.resize(parameters.size());
    }
    void prepare(ProcessSpec spec) override
    {
        if (!std::isfinite(spec.sampleRate) || spec.sampleRate < 8000 || spec.sampleRate > 192000 || !spec.maxBlockSize)
            throw std::invalid_argument("Unsupported process spec");
        spec_ = spec;
        reset();
    }
    void reset() override
    {
        invalidSampleCount = 0;
        for (std::size_t i = 0; i < parameters.size(); ++i) values_[i] = parameters[i]->target.load();
        processor_ = definition_.create(spec_.sampleRate, values_);
        if (type == "cabinet" && !impulseResponse.empty())
        {
            if (!std::isfinite(impulseSampleRate) || impulseSampleRate < 8000 || impulseSampleRate > 192000)
                throw std::invalid_argument("IR sample rate");
            const auto length = std::size_t(std::ceil(impulseResponse.size() * spec_.sampleRate / impulseSampleRate));
            if (length > 32768) throw std::invalid_argument("Resampled IR exceeds 32768 samples");
            std::vector<float> ir(length);
            // Offline windowed-sinc resampling; never invoked by process().
            const double ratio = impulseSampleRate / spec_.sampleRate;
            const double cutoff = std::min(1.0, 1.0 / ratio);
            for (std::size_t i = 0; i < length; ++i)
            {
                const double pos = i * ratio;
                double sum = 0, weight = 0;
                for (int k = -32; k <= 32; ++k)
                {
                    const auto index = static_cast<long>(pos) + k;
                    const double d = pos - index;
                    const double t = 3.141592653589793 * d * cutoff;
                    const double w = (std::abs(t) < 1e-12 ? 1 : std::sin(t)/t) * (.5 + .5 * std::cos(3.141592653589793 * d / 33));
                    if (index >= 0 && index < long(impulseResponse.size())) sum += impulseResponse[std::size_t(index)] * w;
                    weight += w;
                }
                ir[i] = float(sum / weight);
            }
            Cabinet cabinet; cabinet.prepare(ir);
            processor_ = [cabinet = std::move(cabinet)](float x, const auto& p) mutable { return cabinet.processSample(x) * std::pow(10.f, p[0]/20); };
        }
        coefficient_ = float(std::exp(-1.0 / (spec_.sampleRate * .02)));
        wet_ = bypass.load() ? 0.f : 1.f;
        dryDelay_.fill(0); dryIndex_ = 0;
    }
    void process(float* data, std::size_t count) noexcept override
    {
        if (!processor_) return;
        for (std::size_t n = 0; n < count; ++n)
        {
            for (std::size_t p = 0; p < values_.size(); ++p)
            {
                const float target = parameters[p]->target.load(std::memory_order_relaxed);
                values_[p] = parameters[p]->info.smooth ? target + coefficient_ * (values_[p] - target) : target;
            }
            const float targetWet = bypass.load(std::memory_order_relaxed) ? 0.f : 1.f;
            wet_ = targetWet + coefficient_ * (wet_ - targetWet);
            const float dry = data[n];
            const float processed = processor_(dry, values_);
            float alignedDry = dry;
            if (type == "cabinet")
            {
                alignedDry = dryDelay_[dryIndex_]; dryDelay_[dryIndex_] = dry;
                dryIndex_ = (dryIndex_ + 1) % dryDelay_.size();
            }
            data[n] = alignedDry + wet_ * (processed - alignedDry);
            if (!std::isfinite(data[n])) { ++invalidSampleCount; data[n] = 0; }
        }
    }
private:
    EffectDefinition definition_;
    ProcessSpec spec_;
    SampleProcessor processor_;
    std::vector<float> values_;
    float coefficient_ = 0, wet_ = 1;
    std::array<float, Cabinet::partition> dryDelay_{};
    std::size_t dryIndex_ = 0;
};
const std::vector<EffectDefinition>& registry()
{
    static const std::vector<EffectDefinition> definitions = {
        {"noise_gate", "Noise Gate", {{"threshold_db", "Threshold", "dB", -80, 0, -45}},
            [](double sr, const auto&) { NoiseGate g; g.setSampleRate(float(sr)); return [g](float x, const auto& p) mutable { g.setThresholdDb(p[0]); return g.processSample(x); }; }},
        {"compressor", "Compressor", {{"threshold_db", "Threshold", "dB", -60, 0, -18}, {"ratio", "Ratio", ":1", 1, 20, 4}, {"makeup_gain_db", "Makeup", "dB", -12, 18, 0}},
            [](double sr, const auto&) { Compressor c; c.setSampleRate(float(sr)); return [c](float x, const auto& p) mutable { c.setThresholdDb(p[0]); c.setRatio(p[1]); c.setMakeupGainDb(p[2]); return c.processSample(x); }; }},
        {"gain", "Gain (headroom)", {{"gain_db", "Gain", "dB", -60, 24, 0}},
            [](double, const auto&) { return [](float x, const auto& p) { return x * std::pow(10.f, p[0] / 20); }; }},
        {"distortion", "Distortion", {{"drive_db", "Drive", "dB", 0, 36, 6}, {"mix", "Mix", "", 0, 1, .25f}},
            [](double, const auto&) { Distortion d; return [d](float x, const auto& p) mutable { d.setDriveDb(p[0]); return x + p[1] * (d.processSample(x) - x); }; }},
        {"eq", "EQ 1 kHz", {{"gain_db", "Gain", "dB", -12, 12, 0}},
            [](double sr, const auto&) { Filter f; f.setRetainStateOnUpdate(true); return [f, sr, previous = 999.f](float x, const auto& p) mutable { if (p[0] != previous) { f.setPeaking(float(sr), std::min(1000.f, float(sr)*.45f), p[0]); previous = p[0]; } return f.processSample(x); }; }},
        {"amp_sim", "Amp Simulation", {{"gain_db", "Preamp", "dB", 0, 30, 12}, {"drive", "Drive", "", .1f, 4, 1.5f}, {"output_db", "Output", "dB", -36, 6, -6}},
            [](double sr, const auto&) { AmpSim a; a.prepare(float(sr)); return [a](float x, const auto& p) mutable { return a.processSample(x, p[0], p[1], .05f, -3, 1.2f, p[2]); }; }},
        {"cabinet", "Cabinet (synthetic IR)", {{"output_db", "Output", "dB", -24, 6, 0}},
            [](double sr, const auto&) { Filter hp, lp; hp.setHighPass(float(sr), 80); lp.setLowPass(float(sr), float(std::min(5000.0, sr * .45))); std::vector<float> ir(1024); for (std::size_t i = 0; i < ir.size(); ++i) ir[i] = lp.processSample(hp.processSample(i == 0 ? 1.f : 0.f)); Cabinet c; c.prepare(ir); return [c = std::move(c)](float x, const auto& p) mutable { return c.processSample(x) * std::pow(10.f, p[0] / 20); }; }},
        {"delay", "Delay", {{"delay_ms", "Time", "ms", 1, 2000, 400}, {"feedback", "Feedback", "", 0, .95f, .35f}, {"mix", "Mix", "", 0, 1, .2f}},
            [](double sr, const auto&) { Delay d; d.prepare(sr); return [d = std::move(d)](float x, const auto& p) mutable { return d.processSample(x, p[0], p[1], p[2]); }; }},
        {"reverb", "Reverb", {{"room_size", "Room", "", 0, 1, .6f}, {"damping", "Damping", "", 0, .99f, .4f}, {"mix", "Mix", "", 0, 1, .15f}},
            [](double sr, const auto&) { Reverb r; r.prepare(sr); return [r = std::move(r)](float x, const auto& p) mutable { return r.processSample(x, p[0], p[1], p[2]); }; }},
        {"tremolo", "Tremolo", {{"rate_hz", "Rate", "Hz", .1f, 20, 4}, {"depth", "Depth", "", 0, 1, .5f}},
            [](double sr, const auto&) { Tremolo t; t.prepare(sr); return [t](float x, const auto& p) mutable { return t.processSample(x, p[0], p[1]); }; }}
    };
    return definitions;
}
std::unique_ptr<Effect> createEffect(const std::string& type, const std::string& id)
{
    for (const auto& d : registry()) if (d.type == type) { auto e = std::make_unique<Builtin>(d); e->id = id; return e; }
    throw std::invalid_argument("Unknown effect: " + type);
}
void Chain::prepare(ProcessSpec spec) { for (auto& e : effects) e->prepare(spec); }
void Chain::reset() { for (auto& e : effects) e->reset(); }
void Chain::process(float* samples, std::size_t count) noexcept { for (auto& e : effects) e->process(samples, count); }
void Chain::move(std::size_t from, std::size_t to)
{
    if (from >= effects.size() || to >= effects.size()) throw std::out_of_range("Effect index");
    auto e = std::move(effects[from]); effects.erase(effects.begin() + from); effects.insert(effects.begin() + to, std::move(e));
}
}
