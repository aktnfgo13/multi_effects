#pragma once
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <stdexcept>

namespace multi_effects {
// Digital reference, pending measurements of instrument/interface pairs.
// RMS dBFS uses 1.0 RMS as its reference (a full-scale sine is -3.01 dBFS).
struct InputReference {
    static constexpr float nominalRmsDb = -20.f;
    static constexpr float hotPeak = .7079458f; // -3 dBFS digital warning
    static constexpr float fullScale = 1.f;
    static constexpr float trimLimitDb = 12.f;
};

class InputSystem {
public:
    static_assert(std::atomic<float>::is_always_lock_free && std::atomic<bool>::is_always_lock_free);
    void setTrimDb(float db) noexcept {
        trimDb_.store(std::isfinite(db) ? std::clamp(db,-InputReference::trimLimitDb,InputReference::trimLimitDb) : 0.f,
                      std::memory_order_relaxed);
    }
    float trimDb() const noexcept { return trimDb_.load(std::memory_order_relaxed); }
    void prepare(double rate) {
        if (!std::isfinite(rate) || rate<8000 || rate>192000) throw std::invalid_argument("Unsupported input rate");
        rampFrames_=int(std::round(rate*.005));
        windowFrames_=int(std::round(rate*.02));
        reset();
    }
    void reset() noexcept {
        lastDb_=trimDb(); gain_=target_=gainFor(lastDb_); remaining_=0;
        frames_=0; sum_=0; peak_=0; rms.store(0); peak.store(0); invalid.store(false); clipped.store(false);
    }
    // Post-trim, 20 ms windows. Peak is the largest magnitude across lanes.
    // RMS averages both lanes' power. Flags latch until consumed by the UI.
    std::atomic<float> rms{0}, peak{0};
    std::atomic<bool> invalid{false}, clipped{false};
    void process(float* left,float* right,std::size_t count) noexcept {
        const float db=trimDb();
        if(db!=lastDb_) {
            lastDb_=db; target_=gainFor(db); remaining_=rampFrames_;
            step_=(target_-gain_)/float(remaining_);
        }
        bool bad=false,clip=false;
        for(std::size_t i=0;i<count;++i) {
            if(remaining_>0) gain_=--remaining_ ? gain_+step_ : target_;
            const auto lane=[&](float& sample) {
                sample*=gain_;
                if(!std::isfinite(sample)) {sample=0;bad=true;}
                peak_=std::max(peak_,std::abs(sample));
                clip|=std::abs(sample)>=InputReference::fullScale;
                return double(sample)*sample;
            };
            const double l=lane(left[i]);
            sum_+=right ? .5*(l+lane(right[i])) : l;
            if(++frames_==windowFrames_) {
                rms.store(float(std::sqrt(sum_/frames_)),std::memory_order_relaxed);
                peak.store(peak_,std::memory_order_relaxed);
                frames_=0;sum_=0;peak_=0;
            }
        }
        if(bad) invalid.store(true,std::memory_order_relaxed);
        if(clip) clipped.store(true,std::memory_order_relaxed);
    }
private:
    static float gainFor(float db) noexcept {return db==0 ? 1.f : std::pow(10.f,db*.05f);}
    std::atomic<float> trimDb_{0};
    float lastDb_=0,gain_=1,target_=1,step_=0,peak_=0;
    double sum_=0;
    int frames_=0,remaining_=0,rampFrames_=240,windowFrames_=960;
};
}
