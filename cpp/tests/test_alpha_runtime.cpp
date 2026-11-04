#include "AllocationTracker.h"
#include "TestSupport.h"
#include "../app/Runtime.h"
#include "effects/TimeEffects.h"
#include <array>
#include <iostream>
#include <thread>
using namespace multi_effects;
static void transitions()
{
    for(double sr:{44100.,48000.,96000.}) {
        Chain chain;MonoAudioIO io;io.muted=false;io.prepare(chain,{sr,128},{0,1});
        std::array<float,8192> a,b,out; a.fill(.8f);b.fill(-.8f);const float* in[]{a.data(),b.data()};float* outputs[]{out.data()};
        io.process(chain,{in,2,outputs,1,out.size()});const float previous=out.back();io.selectedInput=1;
        {allocation_tracker::ProcessScope scope;io.process(chain,{in,2,outputs,1,out.size()});}
        float maxStep=std::abs(out[0]-previous);for(std::size_t i=1;i<out.size();++i)maxStep=std::max(maxStep,std::abs(out[i]-out[i-1]));
        require(maxStep<.003f,"input switch discontinuity");require(out.back()<-.39f,"input switch did not settle");
        std::cout<<"routing rate="<<sr<<" max_step="<<maxStep<<" (old hard switch ~0.8) PASS\n";
    }
}
static void delay()
{
    for(double sr:{44100.,48000.,96000.}) {
        Delay a,b;a.prepare(sr);b.prepare(sr);std::vector<float> x(std::size_t(sr),0.f),expected(x.size()),actual(x.size());
        for(std::size_t i=0;i<x.size();++i)x[i]=float(.5*std::sin(6.283185307179586*100*i/sr));
        // 20 -> 25ms flips a 100Hz tap phase. Compare jump near the transition.
        const auto change=std::size_t(sr/2)+std::size_t(sr/400);
        for(std::size_t i=0;i<x.size();++i){allocation_tracker::ProcessScope scope;expected[i]=a.processSample(x[i],i<change?20.f:25.f,.8f,1);}
        float maxStep=0;for(std::size_t i=change;i<change+std::size_t(sr*.015);++i)maxStep=std::max(maxStep,std::abs(expected[i]-expected[i-1]));
        std::vector<float> legacyBuffer(std::size_t(sr*2)+1,0.f);std::size_t legacyIndex=0;float legacyPrevious=0,legacyMax=0;
        for(std::size_t i=0;i<x.size();++i){auto tap=std::size_t(sr*(i<change?20.:25.)/1000);float y=legacyBuffer[(legacyIndex+legacyBuffer.size()-tap)%legacyBuffer.size()]*.8f;legacyBuffer[legacyIndex]=x[i]+y;legacyIndex=(legacyIndex+1)%legacyBuffer.size();if(i>=change&&i<change+std::size_t(sr*.015))legacyMax=std::max(legacyMax,std::abs(y-legacyPrevious));legacyPrevious=y;}
        require(maxStep<.09f && maxStep<legacyMax*.2f,"delay tap transition click");
        for(std::size_t block:{64,128,256,512}){
            b.reset();for(std::size_t pos=0;pos<x.size();){auto n=std::min(block,x.size()-pos);for(std::size_t j=0;j<n;++j){auto i=pos+j;actual[i]=b.processSample(x[i],i<change?20.f:25.f,.8f,1);}pos+=n;}require(actual==expected,"delay reset/block equivalence");
        }
        b.reset();for(std::size_t i=0;i<x.size();++i)actual[i]=b.processSample(x[i],1+float(i%1999),.95f,1);
        for(float y:actual)require(std::isfinite(y)&&std::abs(y)<10,"delay changing feedback stability");
        std::cout<<"delay rate="<<sr<<" transition max_step="<<maxStep<<" legacy_max_step="<<legacyMax<<" PASS\n";
    }
}
static void rollback()
{
    app::Runtime runtime;runtime.chain=makeTestChain();runtime.io.muted=false;
    require(runtime.prepare({44100,128},{0,1}),"valid prepare");
    auto* original=runtime.chain.effects.front().get();auto candidate=cloneConfiguration(runtime.chain);
    for(auto& e:candidate.effects)if(e->type=="cabinet"){e->impulseResponse.assign(32768,.01f);e->impulseSampleRate=8000;}
    require(!runtime.replace(std::move(candidate)),"invalid edit accepted");require(runtime.ready&&runtime.chain.effects.front().get()==original,"failed edit destroyed valid state");require(runtime.error.find("32768")!=std::string::npos,"readable IR error missing");
    require(!runtime.prepare({1,128},{0}),"invalid device rate accepted");require(runtime.chain.effects.front().get()==original,"device error lost old configuration");
    std::array<float,512> input{},output{};input.fill(.1f);output.fill(1);const float* in[]{input.data()};float* out[]{output.data()};
    runtime.process({in,1,out,1,512});require(output[0]==0,"failed device output not silent");
    require(runtime.prepare({44100,128},{0,1}),"restore prior valid device");
    for(double rate:{44100.,48000.,96000.})for(std::size_t block:{64,128,256,512}){
        runtime.stop();runtime.preferredInput=1;require(runtime.prepare({rate,block},{0}),"reconnect mono");require(runtime.io.selectedInput==0,"missing selected channel fallback");
        {allocation_tracker::ProcessScope scope;runtime.process({in,1,out,1,512});runtime.process({nullptr,0,nullptr,0,0});}
        runtime.stop();require(runtime.prepare({rate,block},{}),"zero-input device prepare");
        {allocation_tracker::ProcessScope scope;runtime.process({nullptr,0,out,1,512});}
        runtime.stop();require(runtime.prepare({rate,block},{0,1}),"input 2 reconnect");require(runtime.io.selectedInput==1,"preferred input restoration");
    }
    std::cout<<"transaction preservation/errors/reconnect 12 rate/block cases PASS\n";
}
static void controls()
{
    app::Runtime runtime;runtime.chain=makeTestChain();runtime.prepare({48000,128},{0,1});std::atomic<bool> stop{false};
    std::thread ui([&]{unsigned n=0;while(!stop){std::lock_guard<std::mutex> lock(runtime.control);runtime.io.selectedInput=n%2;runtime.io.muted=n%3==0;for(auto& e:runtime.chain.effects)for(auto& p:e->parameters)p->set(n%2?p->info.maximum:p->info.initial);runtime.io.inputLevel.consume();++n;}});
    std::array<float,128>a{},b{},out{};a.fill(.1f);b.fill(-.2f);const float* inputs[]{a.data(),b.data()};float* outputs[]{out.data()};
    for(int i=0;i<10000;++i){allocation_tracker::ProcessScope scope;runtime.process({inputs,2,outputs,1,128});}stop=true;ui.join();
    std::cout<<"Runtime control mutex never taken by processing; concurrent targets/routing/meter PASS\n";
}
int main(int argc,char**){try{if(argc>1)controls();else{transitions();delay();rollback();}require(allocation_tracker::processAllocations==0&&allocation_tracker::processFrees==0,"processing allocated/freed");}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
