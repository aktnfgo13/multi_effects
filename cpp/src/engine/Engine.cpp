#include "engine/Engine.h"
#include "effects/TimeEffects.h"
#include "effects/AmpSim.h"
#include "effects/Cabinet.h"
#include "effects/NoiseGate.h"
#include "effects/Compressor.h"
#include "effects/Distortion.h"
#include "effects/CustomDrive.h"
#include <stdexcept>

namespace multi_effects
{
static_assert(std::atomic<float>::is_always_lock_free, "Realtime parameter requires lock-free float");
static_assert(std::atomic<bool>::is_always_lock_free, "Realtime bypass requires lock-free bool");
void Parameter::set(float value)
{
    if (std::isfinite(value)) target.store(std::clamp(info.requiresPrepare ? std::round(value) : value, info.minimum, info.maximum), std::memory_order_relaxed);
}
Parameter* Effect::parameter(const std::string& key)
{
    for (auto& p : parameters) if (p->info.id == key) return p.get();
    return nullptr;
}
struct CustomDriveLane {
    CustomDrive drive;
    float operator()(float x,const std::vector<float>& p) {
        return drive.processSample(x,{p[0],p[1],p[2],p[3],p[4],p[5],p[6]});
    }
};
class Builtin final : public Effect
{
public:
    explicit Builtin(const EffectDefinition& d) : definition_(d), cabinetLatency_(d.type == "cabinet")
    {
        type = d.type; name = d.name;
        for (const auto& p : d.parameters) parameters.push_back(std::make_unique<Parameter>(p));
        values_.resize(parameters.size());
    }
    std::size_t latencySamples() const noexcept override { return latency_; }
    void prepare(ProcessSpec spec) override
    {
        if (!std::isfinite(spec.sampleRate) || spec.sampleRate < 8000 || spec.sampleRate > 192000 || !spec.maxBlockSize || (spec.channels != 1 && spec.channels != 2))
            throw std::invalid_argument("Unsupported process spec");
        spec_ = spec;
        reset();
    }
    void reset() override
    {
        invalidSampleCount = 0;
        for (std::size_t i = 0; i < parameters.size(); ++i) values_[i] = parameters[i]->target.load();
        latency_ = cabinetLatency_ ? 128 : 0;
        if (type == "distortion" || type == "amp_sim" || type == "custom_drive")
            latency_ = values_.back() == 0 ? 0 : (type == "amp_sim" ? 128 : 64);
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
        // std::function capture copies own their vectors/filter histories. Copy only
        // during reset/prepare, before either lane has processed any audio.
        rightProcessor_ = spec_.channels == 2 ? processor_ : SampleProcessor{};
        if (spec_.channels == 2 && type == "delay") {
            Delay d; d.prepare(spec_.sampleRate);
            rightProcessor_ = [d = std::move(d)](float x, const auto& p) mutable {
                return d.processSample(x, std::min(2000.f, p[0] * 1.03f), p[1], p[2]);
            };
        }
        if (spec_.channels == 2 && type == "reverb") {
            Reverb r; r.prepare(spec_.sampleRate, 1000.0 * 23 / 44100);
            rightProcessor_ = [r = std::move(r)](float x, const auto& p) mutable {
                return r.processSample(x, p[0], p[1], p[2]);
            };
        }
        rightDryDelay_.fill(0);
        coefficient_ = float(std::exp(-1.0 / (spec_.sampleRate * .02)));
        wet_ = bypass.load() ? 0.f : 1.f;
        dryDelay_.fill(0); dryIndex_ = 0;
    }
    float appliedGrain() const noexcept override {
        const auto* lane=processor_.target<CustomDriveLane>();
        return lane ? lane->drive.appliedGrain() : -1.f;
    }
    float appliedMix() const noexcept override {
        const auto* lane=processor_.target<CustomDriveLane>();
        return lane ? lane->drive.appliedMix() : -1.f;
    }
    float appliedHeadroom() const noexcept override {
        const auto* lane=processor_.target<CustomDriveLane>();
        return lane ? lane->drive.appliedHeadroom() : -1.f;
    }
    StereoPolicy stereoPolicy() const noexcept override {
        return type == "delay" || type == "reverb" ? StereoPolicy::Decorrelated : StereoPolicy::DualMono;
    }
    void process(float* data, std::size_t count) noexcept override { render(data, nullptr, count); }
    void processStereo(float* left, float* right, std::size_t count) noexcept override {
        if (!left || !right || left == right || !rightProcessor_) return;
        render(left, right, count);
    }
    void render(float* data, float* right, std::size_t count) noexcept
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
            // Reject invalid input before it can poison filter/feedback histories.
            if (!std::isfinite(data[n])) { ++invalidSampleCount; data[n] = 0; }
            if (right && !std::isfinite(right[n])) { ++invalidSampleCount; right[n] = 0; }
            const float dry = data[n];
            const float processed = processor_(dry, values_);
            float alignedDry = dry;
            if (latency_)
            {
                alignedDry = dryDelay_[dryIndex_]; dryDelay_[dryIndex_] = dry;
            }
            data[n] = alignedDry + wet_ * (processed - alignedDry);
            if (!std::isfinite(data[n])) { ++invalidSampleCount; data[n] = 0; }
            if (right) {
                const float input = right[n];
                const float wet = rightProcessor_(input, values_);
                float aligned = input;
                if (latency_) { aligned = rightDryDelay_[dryIndex_]; rightDryDelay_[dryIndex_] = input; }
                right[n] = aligned + wet_ * (wet - aligned);
                if (!std::isfinite(right[n])) { ++invalidSampleCount; right[n] = 0; }
            }
            if (latency_) dryIndex_ = (dryIndex_ + 1) % latency_;
        }
    }
private:
    EffectDefinition definition_;
    const bool cabinetLatency_;
    std::size_t latency_ = 0;
    ProcessSpec spec_;
    SampleProcessor processor_, rightProcessor_;
    std::vector<float> values_;
    float coefficient_ = 0, wet_ = 1;
    std::array<float, Cabinet::partition> dryDelay_{}, rightDryDelay_{};
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
        {"distortion", "Distortion", {{"drive_db", "Drive", "dB", 0, 36, 6}, {"mix", "Mix", "", 0, 1, .25f}, {"oversampling", "Oversampling", "0=1x 1=2x 2=4x", 0, 2, 0, false, true}},
            [](double, const auto& initial) { Distortion d; Oversampler os; os.prepare(1 << int(initial[2])); SampleDelay dry; dry.reset(os.latencySamples());
                return [d, os, dry](float x, const auto& p) mutable { d.setDriveDb(p[0]); const float wet=os.processSample(x,[&](float v){return d.processSample(v);}); const float aligned=dry.process(x); return aligned + p[1] * (wet-aligned); }; }},
        {"eq", "EQ 1 kHz", {{"gain_db", "Gain", "dB", -12, 12, 0}},
            [](double sr, const auto&) { Filter f; f.setRetainStateOnUpdate(true); return [f, sr, previous = 999.f](float x, const auto& p) mutable { if (p[0] != previous) { f.setPeaking(float(sr), std::min(1000.f, float(sr)*.45f), p[0]); previous = p[0]; } return f.processSample(x); }; }},
        {"amp_sim", "Amp Simulation", {{"gain_db", "Preamp", "dB", 0, 30, 12}, {"drive", "Drive", "", .1f, 4, 1.5f}, {"output_db", "Output", "dB", -36, 6, -6}, {"oversampling", "Oversampling", "0=1x 1=2x 2=4x", 0, 2, 0, false, true}},
            [](double sr, const auto& initial) { AmpSim a; a.prepare(float(sr),0,0,0,0,70,9000,1 << int(initial[3])); return [a](float x, const auto& p) mutable { return a.processSample(x, p[0], p[1], .05f, -3, 1.2f, p[2]); }; }},
        {"cabinet", "Cabinet (synthetic IR)", {{"output_db", "Output", "dB", -24, 6, 0}},
            [](double sr, const auto&) { Filter hp, lp; hp.setHighPass(float(sr), 80); lp.setLowPass(float(sr), float(std::min(5000.0, sr * .45))); std::vector<float> ir(1024); for (std::size_t i = 0; i < ir.size(); ++i) ir[i] = lp.processSample(hp.processSample(i == 0 ? 1.f : 0.f)); Cabinet c; c.prepare(ir); return [c = std::move(c)](float x, const auto& p) mutable { return c.processSample(x) * std::pow(10.f, p[0] / 20); }; }},
        {"delay", "Delay", {{"delay_ms", "Time", "ms", 1, 2000, 400}, {"feedback", "Feedback", "", 0, .95f, .35f}, {"mix", "Mix", "", 0, 1, .2f}},
            [](double sr, const auto&) { Delay d; d.prepare(sr); return [d = std::move(d)](float x, const auto& p) mutable { return d.processSample(x, p[0], p[1], p[2]); }; }},
        {"reverb", "Reverb", {{"room_size", "Room", "", 0, 1, .6f}, {"damping", "Damping", "", 0, .99f, .4f}, {"mix", "Mix", "", 0, 1, .15f}},
            [](double sr, const auto&) { Reverb r; r.prepare(sr); return [r = std::move(r)](float x, const auto& p) mutable { return r.processSample(x, p[0], p[1], p[2]); }; }},
        {"tremolo", "Tremolo", {{"rate_hz", "Rate", "Hz", .1f, 20, 4}, {"depth", "Depth", "", 0, 1, .5f}},
            [](double sr, const auto&) { Tremolo t; t.prepare(sr); return [t](float x, const auto& p) mutable { return t.processSample(x, p[0], p[1]); }; }},
        {"custom_drive", "Custom Drive", {{"drive", "Drive", "%", 0, 100, 50}, {"tone", "Tone", "%", 0, 100, 50}, {"crunch", "Crunch", "%", 0, 100, 50}, {"headroom", "Headroom", "%", 0, 100, 50}, {"grain", "Grain", "%", 0, 100, 25}, {"level_db", "Level", "dB", -24, 12, -6}, {"mix", "Mix", "", 0, 1, 1, false}, {"oversampling", "Oversampling", "0=1x 1=2x 2=4x", 0, 2, 1, false, true}},
            [](double sr, const auto& initial) { CustomDrive d; d.prepare(sr,1 << int(initial[7])); return CustomDriveLane{std::move(d)}; }}
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
void Chain::processStereo(float* left, float* right, std::size_t frames) noexcept { for (auto& e : effects) e->processStereo(left, right, frames); }
void Chain::move(std::size_t from, std::size_t to)
{
    if (from >= effects.size() || to >= effects.size()) throw std::out_of_range("Effect index");
    auto e = std::move(effects[from]); effects.erase(effects.begin() + from); effects.insert(effects.begin() + to, std::move(e));
}
Chain cloneConfiguration(const Chain& source)
{
    Chain copy;
    for (const auto& original : source.effects)
    {
        auto e = createEffect(original->type, original->id);
        e->name = original->name; e->bypass.store(original->bypass.load());
        e->impulseResponse = original->impulseResponse; e->impulseSampleRate = original->impulseSampleRate;
        e->impulseName = original->impulseName;
        for (const auto& p : original->parameters) e->parameter(p->info.id)->set(p->target.load());
        copy.effects.push_back(std::move(e));
    }
    return copy;
}

}
