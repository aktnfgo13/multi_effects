#pragma once
#include "Engine.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace multi_effects
{
// Non-owning planar device boundary; the current policy selects one physical input
// and replicates mono DSP to outputs. A future stereo processor can retain this view.
struct AudioBlock
{
    const float* const* inputs;
    int inputCount;
    float* const* outputs;
    int outputCount;
    std::size_t frames;
};
struct LevelSnapshot { float peak; bool clipped; bool invalid; };
class LevelMeter
{
public:
    void publish(float peak, bool clipped, bool invalid) noexcept
    {
        // One producer. Exchange on the UI cannot lose a peak in this CAS loop.
        float old = peak_.load(std::memory_order_relaxed);
        while (old < peak && !peak_.compare_exchange_weak(old, peak, std::memory_order_relaxed)) {}
        if (clipped) clipped_.store(true, std::memory_order_relaxed);
        if (invalid) invalid_.store(true, std::memory_order_relaxed);
    }
    LevelSnapshot consume() noexcept
    {
        return {peak_.exchange(0), clipped_.exchange(false), invalid_.exchange(false)};
    }
private:
    std::atomic<float> peak_{0};
    std::atomic<bool> clipped_{false}, invalid_{false};
};
class MonoAudioIO
{
public:
    std::atomic<int> selectedInput{0}; // Physical device index, not packed callback index.
    std::atomic<bool> muted{true};
    LevelMeter inputLevel, outputLevel;
    // Lifecycle and channel-map mutation require a stopped callback.
    void prepare(Chain& chain, ProcessSpec spec, std::vector<int> activeInputs)
    {
        ready_ = false;
        if (!spec.maxBlockSize || !std::isfinite(spec.sampleRate) || spec.sampleRate < 8000 || spec.sampleRate > 192000)
            throw std::invalid_argument("Unsupported audio IO spec");
        scratch_.assign(spec.maxBlockSize, 0);
        activeInputs_ = std::move(activeInputs);
        const int selected = selectedInput.load();
        if (std::find(activeInputs_.begin(), activeInputs_.end(), selected) == activeInputs_.end())
            selectedInput.store(activeInputs_.empty() ? -1 : activeInputs_.front());
        routeInput_ = selectedInput.load(); routeGain_ = 1; routeStep_ = float(1.0 / std::max(1.0, spec.sampleRate * .005));
        outputGain_ = 0;
        inputLevel.consume(); outputLevel.consume();
        chain.prepare(spec);
        ready_ = true;
    }
    void stop() noexcept { ready_ = false; }
    void process(Chain& chain, AudioBlock block) noexcept
    {
        if (!ready_)
        {
            for (int c = 0; block.outputs && c < block.outputCount; ++c)
                if (block.outputs[c]) std::fill_n(block.outputs[c], block.frames, 0.f);
            return;
        }
        const int selected = selectedInput.load(std::memory_order_relaxed);
        int packed = -1;
        for (std::size_t c = 0; c < activeInputs_.size(); ++c)
            if (activeInputs_[c] == selected) { packed = int(c); break; }
        const float* source = block.inputs && packed >= 0 && packed < block.inputCount ? block.inputs[packed] : nullptr;
        const auto channel = [&](int physical) -> const float* {
            for (std::size_t c=0;c<activeInputs_.size();++c)
                if(activeInputs_[c]==physical && block.inputs && int(c)<block.inputCount) return block.inputs[c];
            return nullptr;
        };
        const float* currentSource = channel(routeInput_);
        float inPeak = 0, outPeak = 0;
        bool inClip = false, outClip = false, inInvalid = false, outInvalid = false;
        for (std::size_t start = 0; start < block.frames;)
        {
            const auto n = std::min(block.frames - start, scratch_.size());
            for (std::size_t i = 0; i < n; ++i)
            {
                float x = source ? source[start + i] : 0;
                if (!std::isfinite(x)) { inInvalid = true; x = 0; }
                inPeak = std::max(inPeak, std::abs(x)); inClip |= std::abs(x) >= 1.f;
                if (routeInput_ != selected)
                {
                    routeGain_ = std::max(0.f, routeGain_ - routeStep_);
                    if (routeGain_ == 0) { routeInput_ = selected; currentSource = source; }
                }
                else routeGain_ = std::min(1.f, routeGain_ + routeStep_);
                float routed = currentSource ? currentSource[start+i] : 0;
                if (!std::isfinite(routed)) { routed = 0; inInvalid = true; }
                scratch_[i] = routed * routeGain_;
            }
            chain.process(scratch_.data(), n);
            for (std::size_t i = 0; i < n; ++i)
            {
                const float target = muted.load(std::memory_order_relaxed) ? 0.f : .5f;
                outputGain_ += .002f * (target - outputGain_);
                float preClamp = scratch_[i] * outputGain_;
                if (!std::isfinite(preClamp)) { outInvalid = true; preClamp = 0; }
                outClip |= std::abs(preClamp) >= 1.f;
                const float y = std::clamp(preClamp, -1.f, 1.f);
                outPeak = std::max(outPeak, std::abs(y));
                for (int c = 0; block.outputs && c < block.outputCount; ++c)
                    if (block.outputs[c]) block.outputs[c][start + i] = y;
            }
            start += n;
        }
        inputLevel.publish(inPeak, inClip, inInvalid);
        outputLevel.publish(outPeak, outClip, outInvalid);
    }
private:
    std::vector<float> scratch_;
    std::vector<int> activeInputs_;
    int routeInput_ = -1;
    float routeGain_ = 1, routeStep_ = 1;
    float outputGain_ = 0;
    bool ready_ = false;
};
static_assert(std::atomic<int>::is_always_lock_free && std::atomic<float>::is_always_lock_free
              && std::atomic<bool>::is_always_lock_free, "Audio IO requires lock-free atomics");
}
