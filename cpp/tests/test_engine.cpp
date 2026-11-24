#include "engine/Engine.h"
#include "effects/Cabinet.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <new>
static bool watch = false;
static std::size_t allocations = 0;
void* operator new(std::size_t n) { if(watch) ++allocations; if(auto* p=std::malloc(n?n:1)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { ::operator delete(p); }
int main()
{
    using namespace multi_effects;
    // Resampling an impulse response must preserve convolution gain, not
    // waveform amplitude. A smooth, interior kernel avoids boundary ambiguity.
    double referenceGain=0;
    for(double rate : {48000.,44100.,96000.}) {
        auto cab=createEffect("cabinet","rate-gain");
        cab->impulseSampleRate=48000;cab->impulseResponse.resize(1024);
        double sum=0;for(std::size_t i=0;i<1024;++i){double v=std::exp(-std::pow((double(i)-512)/32,2));cab->impulseResponse[i]=float(v);sum+=v;}
        for(auto& v:cab->impulseResponse)v/=float(sum);
        cab->prepare({rate,128});std::vector<float> data(4096,1.f);
        watch=true;cab->process(data.data(),data.size());watch=false;
        const double gain=data.back();if(rate==48000)referenceGain=gain;
        std::cout<<"Cabinet rate="<<rate<<" steady gain="<<gain<<'\n';
        if(std::abs(gain-referenceGain)>1e-4){std::cerr<<"IR resampling changed convolution gain\n";return 9;}
    }
    for (double rate : {44100.,48000.,96000.})
    {
        for (const auto& definition : registry())
        {
            auto a=createEffect(definition.type,"a"), b=createEffect(definition.type,"b");
            a->prepare({rate,512}); b->prepare({rate,512});
            std::array<float,8192> whole{}, split{};
            for(std::size_t i=0;i<whole.size();++i) whole[i]=split[i]=float(.1*std::sin(i*.07));
            watch=true;
            a->process(whole.data(),whole.size());
            for(std::size_t i=0;i<split.size();)
            {
                auto n=std::min<std::size_t>(1+i%257,split.size()-i); b->process(split.data()+i,n); i+=n;
            }
            watch=false;
            if(allocations || whole!=split) { std::cerr<<definition.type<<" partition/allocation failed\n"; return 1; }
            for(float x:whole) if(!std::isfinite(x)) return 2;
            b->reset();
            for(std::size_t i=0;i<split.size();++i) split[i]=float(.1*std::sin(i*.07));
            b->process(split.data(),split.size());
            if(whole!=split) { std::cerr<<"reset failed\n"; return 3; }
            b->bypass.store(true);
            for(auto& p:b->parameters) p->set(p->info.maximum);
            watch=true; b->process(split.data(),split.size()); watch=false;
            if(allocations) return 4;
            for(float x:split) if(!std::isfinite(x)) return 5;
        }
    }
    // Multiple convolution partitions, compared to direct FIR after fixed latency.
    std::vector<float> ir(513), input(2048), actual(2048);
    for(std::size_t i=0;i<ir.size();++i) ir[i]=float(std::exp(-double(i)/70)*.05);
    for(std::size_t i=0;i<input.size();++i) input[i]=float(std::sin(i*.13));
    Cabinet c; c.prepare(ir);
    watch=true; for(std::size_t i=0;i<input.size();++i) actual[i]=c.processSample(input[i]); watch=false;
    for(std::size_t i=0;i<input.size();++i)
    {
        double expected=0;
        if(i>=128) for(std::size_t k=0;k<ir.size() && k<=i-128;++k) expected+=double(ir[k])*input[i-128-k];
        if(std::abs(actual[i]-expected)>1e-5) { std::cerr<<"convolution failed\n"; return 6; }
    }
    if(allocations) return 7;
    Chain chain; chain.effects.push_back(createEffect("gain","g")); chain.effects.push_back(createEffect("delay","d")); chain.move(0,1);
    if(chain.effects[0]->id!="d") return 8;
    std::cout<<"Engine state, partition, allocation, convolution, registry tests passed\n";
}
