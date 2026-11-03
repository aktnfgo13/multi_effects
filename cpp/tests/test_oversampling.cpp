#include "AllocationTracker.h"
#include "TestSupport.h"
#include "effects/Oversampler.h"
#include "effects/Cabinet.h"
#include <iostream>
#include <limits>
#include <thread>
using namespace multi_effects;
int main(int argc,char** argv){try{
    if(argc>1){
        const bool concurrent=std::string(argv[1])=="controls";
        for(double rate:{44100.,48000.,96000.}){
            auto chain=makeTestChain();for(auto& e:chain.effects)if(auto* p=e->parameter("oversampling"))p->set(2);
            chain.prepare({rate,128});std::atomic<bool> stop{false};std::thread writer;
            if(concurrent)writer=std::thread([&]{int n=0;while(!stop){for(auto& e:chain.effects){e->bypass=n%2;for(auto& p:e->parameters)p->set(n%2?p->info.maximum:p->info.initial);}++n;}});
            std::array<float,512> data{};const auto baseline=allocation_tracker::liveBytes.load();
            const auto total=std::size_t(concurrent?128*1000:rate*60);
            bool valid=true;
            for(std::size_t pos=0;pos<total;){const std::size_t sizes[]{64,128,256,512};auto n=std::min(sizes[(pos/64)%4],total-pos);
                for(std::size_t i=0;i<n;++i)data[i]=testSignal(pos+i,rate);
                {allocation_tracker::ProcessScope scope;chain.process(data.data(),n);}
                for(std::size_t i=0;i<n;++i)valid &= std::isfinite(data[i]);
                if(!concurrent)valid &= allocation_tracker::liveBytes.load()==baseline;
                pos+=n;
            }
            if(concurrent){stop=true;writer.join();}
            require(valid,"4x full chain nonfinite/memory growth");for(auto& e:chain.effects)require(e->invalidSampleCount==0,"4x hidden invalid");
            require(allocation_tracker::processAllocations==0&&allocation_tracker::processFrees==0,"4x process alloc/free");
            std::cout<<"4x full chain rate="<<rate<<" samples="<<total<<" controls="<<concurrent<<" PASS\n";
        }return 0;
    }

    Oversampler invalid;requireRejected([&]{invalid.prepare(3);},"unsupported factor");
    SampleDelay invalidDelay;requireRejected([&]{invalidDelay.reset(129);},"delay capacity");
    for(int factor:{1,2,4}) {
        Oversampler os;os.prepare(factor);std::vector<float> impulse(1024);
        for(int i=0;i<1024;++i){allocation_tracker::ProcessScope scope;impulse[i]=os.processSample(i==0?1.f:0.f,[](float x){return x;});}
        const auto peak=std::max_element(impulse.begin(),impulse.end());require(std::distance(impulse.begin(),peak)==os.latencySamples(),"impulse latency");
        double sum=0;for(auto x:impulse)sum+=x;require(std::abs(sum-1)<1e-4,"DC gain");
        os.reset();for(int i=0;i<1024;++i)require(os.processSample(i==0?1.f:0.f,[](float x){return x;})==impulse[i],"reset deterministic");
        os.reset();for(int i=0;i<512;++i)require(os.processSample(0,[](float x){return x;})==0,"silence");
        require(std::isfinite(os.processSample(std::numeric_limits<float>::infinity(),[](float x){return x;})),"nonfinite");
        require(std::isfinite(os.processSample(1,[](float){return std::numeric_limits<float>::quiet_NaN();})),"nonfinite nonlinear result");
        if(factor==1)require(os.processSample(.123f,[](float x){return x;})==.123f,"exact 1x");
    }
    for(double rate:{44100.,48000.,96000.})for(int mode:{0,1,2})for(auto type:{"amp_sim","distortion"})for(std::size_t block:{64,128,256,512}){
        auto e=createEffect(type,"test");e->parameter("oversampling")->set(float(mode));
        for(auto& p:e->parameters)if(!p->info.requiresPrepare)p->set(p->info.maximum);
        e->prepare({rate,block});std::vector<float> original(4099),a,b;
        for(std::size_t i=0;i<original.size();++i)original[i]=testSignal(i,rate);a=original;b=original;
        {allocation_tracker::ProcessScope scope;e->process(a.data(),a.size());}
        e->reset();for(std::size_t i=0;i<b.size();){auto n=std::min(block,b.size()-i);{allocation_tracker::ProcessScope scope;e->process(b.data()+i,n);}i+=n;}require(a==b,"block split");
        e->prepare({rate,block});b=original;for(std::size_t i=0;i<b.size();){auto n=std::min(1+i%701,b.size()-i);e->process(b.data()+i,n);i+=n;}require(a==b,"irregular prepare");
        require(e->invalidSampleCount==0,"high drive invalid");for(auto v:a)require(std::isfinite(v),"finite");
        // Discrete target is staged until stopped reset, never rebuilt by processing.
        e->bypass=true;e->reset();b=original;e->process(b.data(),b.size());
        for(std::size_t i=0;i<b.size();++i)require(b[i]==(i>=e->latencySamples()?original[i-e->latencySamples()]:0),"bypass latency alignment");
        e->parameter("oversampling")->set(float((mode+1)%3));auto latency=e->latencySamples();e->process(b.data(),b.size());require(e->latencySamples()==latency,"unsafe mode change");
    }
    // Maximum IR with sparse known taps, validates every output including late partitions.
    std::vector<float> ir(32768);ir[0]=.5f;ir[127]=-.2f;ir[16000]=.1f;ir.back()=.3f;Cabinet cab;cab.prepare(ir);
    for(std::size_t i=0;i<ir.size()+256;++i){float y;{allocation_tracker::ProcessScope scope;y=cab.processSample(i==0?1.f:0.f);}float expected=i>=128&&i-128<ir.size()?ir[i-128]:0;require(std::abs(y-expected)<1e-6f,"long IR impulse");}
    require(allocation_tracker::processAllocations==0&&allocation_tracker::processFrees==0,"allocation/free");
    std::cout<<"Oversampling identity/filter/latency/reset/nonfinite + nonlinear 72 rate/mode/block cases + long IR PASS\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
