#pragma once
#include "Engine.h"
#include "InputSystem.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace multi_effects
{
// Non-owning planar device boundary. Mono selection or adjacent physical L/R pair.
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
class AudioIO
{
public:
    std::atomic<int> selectedInput{0}; // Physical device index, not packed callback index.
    std::atomic<bool> muted{true};
    std::atomic<bool> stereoInput{false}; // Selected physical input and its next channel.
    InputSystem input;
    LevelMeter inputLevel, outputLevel;
    // Lifecycle and channel-map mutation require a stopped callback.
    void prepare(Chain& chain, ProcessSpec spec, std::vector<int> activeInputs)
    {
        ready_ = false;
        if (!spec.maxBlockSize || !std::isfinite(spec.sampleRate) || spec.sampleRate < 8000 || spec.sampleRate > 192000)
            throw std::invalid_argument("Unsupported audio IO spec");
        scratch_.assign(spec.maxBlockSize, 0);
        stereo_ = spec.channels == 2;
        rightScratch_.assign(stereo_ ? spec.maxBlockSize : 0, 0);
        routeStereo_ = stereoInput.load();
        activeInputs_ = std::move(activeInputs);
        const int selected = selectedInput.load();
        if (std::find(activeInputs_.begin(), activeInputs_.end(), selected) == activeInputs_.end())
            selectedInput.store(activeInputs_.empty() ? -1 : activeInputs_.front());
        routeInput_ = selectedInput.load(); routeGain_ = 1; routeStep_ = float(1.0 / std::max(1.0, spec.sampleRate * .005));
        outputGain_ = 0;
        inputLevel.consume(); outputLevel.consume();
        input.prepare(spec.sampleRate);
        chain.prepare(spec);
        ready_ = true;
    }
    void stop() noexcept { ready_ = false; input.reset(); }
    void process(Chain& chain, AudioBlock block) noexcept
    {
        if (!ready_)
        {
            for (int c = 0; block.outputs && c < block.outputCount; ++c)
                if (block.outputs[c]) std::fill_n(block.outputs[c], block.frames, 0.f);
            return;
        }
        const int selected = selectedInput.load(std::memory_order_relaxed);
        const bool paired = stereo_ && stereoInput.load(std::memory_order_relaxed);
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
        const float* currentRight = routeStereo_ ? channel(routeInput_ + 1) : currentSource;
        const float* selectedRight = paired ? channel(selected + 1) : source;
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
                if (stereo_) {
                    float r = selectedRight ? selectedRight[start+i] : 0;
                    if (!std::isfinite(r)) { inInvalid = true; r = 0; }
                    inPeak = std::max(inPeak, std::abs(r)); inClip |= std::abs(r) >= 1.f;
                }
                if (routeInput_ != selected || routeStereo_ != paired)
                {
                    routeGain_ = std::max(0.f, routeGain_ - routeStep_);
                    if (routeGain_ == 0) { routeInput_ = selected; routeStereo_ = paired; currentSource = source; currentRight = selectedRight; }
                }
                else routeGain_ = std::min(1.f, routeGain_ + routeStep_);
                float routed = currentSource ? currentSource[start+i] : 0;
                if (!std::isfinite(routed)) { routed = 0; inInvalid = true; }
                scratch_[i] = routed * routeGain_;
                if (stereo_) {
                    float r = currentRight ? currentRight[start+i] : 0;
                    if (!std::isfinite(r)) { r = 0; inInvalid = true; }
                    rightScratch_[i] = r * routeGain_;
                }
            }
            input.process(scratch_.data(), stereo_ ? rightScratch_.data() : nullptr, n);
            if (stereo_) chain.processStereo(scratch_.data(), rightScratch_.data(), n);
            else chain.process(scratch_.data(), n);
            for (std::size_t i = 0; i < n; ++i)
            {
                const float target = muted.load(std::memory_order_relaxed) ? 0.f : .5f;
                outputGain_ += .002f * (target - outputGain_);
                float preClamp = scratch_[i] * outputGain_;
                if (!std::isfinite(preClamp)) { outInvalid = true; preClamp = 0; }
                outClip |= std::abs(preClamp) >= 1.f;
                const float y = std::clamp(preClamp, -1.f, 1.f);
                outPeak = std::max(outPeak, std::abs(y));
                float right = y;
                if (stereo_) {
                    float r = rightScratch_[i] * outputGain_;
                    if (!std::isfinite(r)) { r = 0; outInvalid = true; }
                    outClip |= std::abs(r) >= 1.f;
                    right = std::clamp(r, -1.f, 1.f);
                    outPeak = std::max(outPeak, std::abs(right));
                }
                for (int c = 0; block.outputs && c < block.outputCount; ++c)
                    if (block.outputs[c]) block.outputs[c][start + i] = block.outputCount == 1 ? .5f * (y + right) : (c % 2 ? right : y);
            }
            start += n;
        }
        inputLevel.publish(inPeak, inClip, inInvalid);
        outputLevel.publish(outPeak, outClip, outInvalid);
    }
private:
    std::vector<float> scratch_, rightScratch_;
    bool stereo_ = false, routeStereo_ = false;
    std::vector<int> activeInputs_;
    int routeInput_ = -1;
    float routeGain_ = 1, routeStep_ = 1;
    float outputGain_ = 0;
    bool ready_ = false;
};
using MonoAudioIO = AudioIO; // Source compatibility; ProcessSpec controls lane count.
static_assert(std::atomic<int>::is_always_lock_free && std::atomic<float>::is_always_lock_free
              && std::atomic<bool>::is_always_lock_free, "Audio IO requires lock-free atomics");
}
