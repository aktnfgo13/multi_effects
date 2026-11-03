#include "AllocationTracker.h"
#include "TestSupport.h"
#include "engine/AudioIO.h"
#include <array>
#include <chrono>
#include <iostream>
#include <limits>
#include <thread>
using namespace multi_effects;
static void run(MonoAudioIO& io, Chain& chain, AudioBlock block)
{
    { allocation_tracker::ProcessScope scope; io.process(chain, block); }
    require(allocation_tracker::processAllocations == 0 && allocation_tracker::processFrees == 0, "IO process allocation/free");
}
static void routingAndMeters()
{
    Chain chain; MonoAudioIO io;
    std::array<float, 8192> a{}, b{}, l{}, r{};
    const float* inputs[]{a.data(), b.data()}; float* outputs[]{l.data(), r.data()};
    a.fill(.25f); b.fill(-.5f); io.muted = false;
    io.prepare(chain, {48000, 128}, {0, 1});
    run(io, chain, {inputs, 2, outputs, 2, a.size()});
    require(l == r && l.back() > .12f, "input 0 / stereo");
    auto m = io.inputLevel.consume(); require(m.peak == .25f && !m.clipped && !m.invalid, "normal meter");
    io.selectedInput = 1;
    run(io, chain, {inputs, 2, outputs, 1, a.size()});
    require(l.back() < -.24f, "live input 1 / mono");
    io.selectedInput = 99; run(io, chain, {inputs, 2, outputs, 2, a.size()});
    require(l.back() == 0 && r.back() == 0, "invalid selection");
    io.selectedInput = 1; run(io, chain, {nullptr, 0, outputs, 2, a.size()});
    require(l.back() == 0, "zero inputs");
    io.prepare(chain, {44100, 64}, {3}); // Only physical input 4 is enabled: callback slot 0.
    require(io.selectedInput == 3, "channel-change fallback");
    run(io, chain, {inputs, 1, outputs, 2, a.size()}); require(l.back() > .12f, "packed mapping");
    io.prepare(chain, {96000, 512}, {});
    require(io.selectedInput == -1, "no active inputs");
    run(io, chain, {inputs, 2, outputs, 2, a.size()}); require(l.back() == 0, "empty map");
    io.prepare(chain, {48000, 128}, {0});
    a.fill(0); run(io, chain, {inputs, 1, outputs, 2, a.size()});
    m = io.inputLevel.consume(); require(m.peak == 0 && !m.clipped, "silence meter");
    a.fill(.999f); run(io, chain, {inputs, 1, outputs, 2, a.size()});
    m = io.inputLevel.consume(); require(m.peak == .999f && !m.clipped, "near clip");
    a.fill(1); run(io, chain, {inputs, 1, outputs, 2, a.size()});
    m = io.inputLevel.consume(); require(m.clipped, "input full-scale clip");
    a.fill(3); run(io, chain, {inputs, 1, outputs, 2, a.size()});
    m = io.outputLevel.consume(); require(m.peak == 1 && m.clipped && l.back() == 1, "pre-clamp output clip");
    io.inputLevel.consume();
    a.fill(0); a[0] = std::numeric_limits<float>::quiet_NaN(); a[1] = std::numeric_limits<float>::infinity();
    run(io, chain, {inputs, 1, outputs, 2, a.size()});
    m = io.inputLevel.consume(); require(m.invalid && m.peak == 0, "invalid input protection");
    for (auto x : l) require(std::isfinite(x), "nonfinite output");
    io.muted = true; a.fill(.5f); run(io, chain, {inputs, 1, outputs, 2, a.size()});
    require(std::abs(l.back()) < 1e-6f, "mute ramp");
    const float* missing[]{nullptr}; float* sparseOut[]{nullptr, r.data()};
    io.muted = false;
    run(io, chain, {missing, 1, sparseOut, 2, a.size()}); require(r.back() == 0, "null channel pointers");
    io.stop(); l.fill(1); run(io, chain, {inputs, 1, outputs, 2, a.size()}); require(l.back() == 0, "stopped silence");
    requireRejected([&]{io.prepare(chain, {48000, 0}, {0});}, "invalid spec");
    run(io, chain, {nullptr, 0, nullptr, 0, 0});
    // Device rate change can invalidate an otherwise legal source IR; remain silent,
    // then recover on a supported device rate without exposing partially prepared DSP.
    chain.effects.push_back(createEffect("cabinet", "cab"));
    chain.effects.back()->impulseResponse.assign(17000, 0);
    chain.effects.back()->impulseResponse[0] = 1;
    chain.effects.back()->impulseSampleRate = 44100;
    io.prepare(chain, {44100, 128}, {0});
    requireRejected([&]{io.prepare(chain, {96000, 128}, {0});}, "oversized resampled IR");
    l.fill(1); run(io, chain, {inputs, 1, outputs, 2, a.size()}); require(l.back() == 0, "failed prepare silence");
    io.prepare(chain, {44100, 128}, {0}); a.fill(.25f);
    run(io, chain, {inputs, 1, outputs, 2, a.size()}); require(l.back() > 0, "prepare failure recovery");
    std::cout << "routing, sparse mapping, device-change fallback, meters, clipping, mute PASS\n";
}
static Chain defaultChain()
{
    auto chain = makeTestChain(); chain.effects.pop_back(); return chain;
}
static void matrix()
{
    for (double rate : {44100., 48000., 96000.}) for (std::size_t block : {64, 128, 256, 512})
    {
        const auto count = std::size_t(rate) + 37;
        std::vector<float> input(count), expected(count), actual(count), right(count);
        for (std::size_t i=0;i<count;++i) input[i]=testSignal(i,rate);
        expected = input;
        auto reference = defaultChain(); reference.prepare({rate, block}); reference.process(expected.data(), count);
        float gain = 0;
        for(auto& x:expected) { gain += .002f*(.5f-gain); x=std::clamp(x*gain,-1.f,1.f); }
        auto chain = defaultChain(); MonoAudioIO io; io.muted=false;
        for (bool irregular : {false, true})
        {
            io.prepare(chain,{rate,block},{0,1});
            for (std::size_t start=0;start<count;)
            {
                const auto n=std::min(irregular ? 1+start%777 : block,count-start);
                const float* in[]{input.data()+start, nullptr}; float* out[]{actual.data()+start,right.data()+start};
                run(io,chain,{in,2,out,2,n}); start+=n;
            }
            require(actual==expected && right==expected,"Input 1 legacy/partition/reprepare regression");
            for(auto& e:chain.effects) require(e->invalidSampleCount==0,"DSP invalid diagnostic");
        }
        std::cout << "IO rate="<<rate<<" block="<<block<<" exact legacy/fixed/irregular/reprepare PASS\n";
    }
}
static void concurrent()
{
    auto chain=defaultChain(); MonoAudioIO io; io.prepare(chain,{48000,128},{0,1});
    std::atomic<bool> stop{false};
    std::thread gui([&]{int c=0; while(!stop.load()) {io.selectedInput=c++%3; io.muted=(c%2)==0; io.inputLevel.consume(); io.outputLevel.consume();}});
    std::array<float,128> a{},b{},l{},r{}; a.fill(.1f); b.fill(-.2f);
    const float* in[]{a.data(),b.data()}; float* out[]{l.data(),r.data()};
    try {for(int i=0;i<10000;++i) run(io,chain,{in,2,out,2,128});}
    catch(...) {stop=true;gui.join();throw;}
    stop=true;gui.join();
    std::cout << "concurrent routing/mute/meter 10000 blocks PASS\n";
}
static void timing()
{
    auto chain=defaultChain(); MonoAudioIO io; io.prepare(chain,{48000,128},{0});io.muted=false;
    std::array<float,128> a{},l{},r{};const float* in[]{a.data()};float* out[]{l.data(),r.data()};
    double sum=0, worst=0;
    for(int n=0;n<20000;++n)
    {
        for(std::size_t i=0;i<a.size();++i)a[i]=testSignal(std::size_t(n)*128+i,48000);
        const auto start=std::chrono::steady_clock::now();run(io,chain,{in,1,out,2,128});
        const double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();
        if(n>=100) {sum+=us;worst=std::max(worst,us);}
    }
    std::cout<<"48k/128 default-chain offline timing mean_us="<<sum/19900<<" max_us="<<worst<<" budget_us=2666.67 (observation, not device deadline guarantee)\n";
}
int main(int argc,char** argv)
{
    try {if(argc>1 && std::string(argv[1])=="controls")concurrent();else {routingAndMeters();matrix();timing();}}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
