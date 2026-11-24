#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <algorithm>

namespace multi_effects {
// One audio producer, one analysis consumer. prepare/destruction require both stopped.
class AnalysisTap {
public:
    static constexpr unsigned blockSize=256, capacity=64;
    struct Packet {
        std::array<std::array<float,blockSize>,4> samples{}; // input L/R, output L/R
        double sampleRate=48000;
        std::uint64_t frameIndex=0;
        unsigned frames=0,channels=0,latency=0,epoch=0,session=0,drops=0;
    };
    static_assert(std::atomic<unsigned>::is_always_lock_free && std::atomic<bool>::is_always_lock_free);
    void prepare() { slots=std::make_unique<Packet[]>(capacity);write=0;read=0;dropped=0;frame=0; }
    void setEnabled(bool value) noexcept { if(value&&!enabled.load())session.fetch_add(1);enabled.store(value,std::memory_order_release); }
    bool isEnabled() const noexcept {return enabled.load(std::memory_order_acquire);}
    unsigned currentSession() const noexcept {return session.load(std::memory_order_relaxed);}
    unsigned dropCount() const noexcept {return dropped.load(std::memory_order_relaxed);}
    bool tryPush(const float* inL,const float* inR,const float* outL,const float* outR,
                 unsigned n,double rate,unsigned latency,unsigned epoch) noexcept {
        if(!isEnabled() || !slots)return false;
        const auto position=frame;frame+=n;
        const unsigned w=write.load(std::memory_order_relaxed);
        if(n>blockSize || w-read.load(std::memory_order_acquire)>=capacity){dropped.fetch_add(n,std::memory_order_relaxed);return false;}
        auto& p=slots[w%capacity];p.frames=n;p.channels=inR?2:1;p.sampleRate=rate;
        p.frameIndex=position;p.latency=latency;p.epoch=epoch;p.session=session.load(std::memory_order_relaxed);p.drops=dropCount();
        for(unsigned i=0;i<n;++i){p.samples[0][i]=inL[i];p.samples[1][i]=inR?inR[i]:inL[i];p.samples[2][i]=outL[i];p.samples[3][i]=outR?outR[i]:outL[i];}
        write.store(w+1,std::memory_order_release);return true;
    }
    bool tryRead(Packet& out) noexcept {
        const unsigned r=read.load(std::memory_order_relaxed);
        if(!slots||r==write.load(std::memory_order_acquire))return false;
        out=slots[r%capacity];read.store(r+1,std::memory_order_release);return true;
    }
private:
    std::unique_ptr<Packet[]> slots;
    alignas(64) std::atomic<unsigned> write{0};
    alignas(64) std::atomic<unsigned> read{0};
    std::atomic<unsigned> dropped{0},session{0};
    std::atomic<bool> enabled{false};
    std::uint64_t frame=0; // producer only; includes dropped frames
};
}
