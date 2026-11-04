#include "AllocationTracker.h"
#include "TestSupport.h"
#include "engine/AudioIO.h"
#include "../app/Runtime.h"
#include <array>
#include <chrono>
#include <iostream>
#include <limits>
#include <thread>
using namespace multi_effects;
static Chain make(int mode, unsigned channels, double rate=48000) {
    auto c=makeTestChain();
    for(auto& e:c.effects)if(auto* p=e->parameter("oversampling"))p->set(float(mode));
    c.prepare({rate,128,channels});return c;
}
static void render(Chain& c,std::vector<float>& l,std::vector<float>& r,std::size_t block) {
    for(std::size_t i=0;i<l.size();) {
        auto n=std::min(block?block:1+(i*17)%513,l.size()-i);
        {allocation_tracker::ProcessScope scope;c.processStereo(l.data()+i,r.data()+i,n);} i+=n;
    }
}
static void effects() {
    for(double rate:{44100.,48000.,96000.})for(int mode=0;mode<3;++mode)for(const auto& def:registry()) {
        auto e=createEffect(def.type,"test"),m=createEffect(def.type,"mono");
        if(auto* p=e->parameter("oversampling"))p->set(float(mode));
        if(auto* p=m->parameter("oversampling"))p->set(float(mode));
        e->prepare({rate,128,2});m->prepare({rate,128});
        if(def.type=="delay"){e->parameter("delay_ms")->set(20);m->parameter("delay_ms")->set(20);e->reset();m->reset();}
        const bool spatial=e->stereoPolicy()==StereoPolicy::Decorrelated;
        std::vector<float> input(8192),l,r,reference;
        for(std::size_t i=0;i<input.size();++i)input[i]=testSignal(i,rate);
        reference=input;m->process(reference.data(),reference.size());l=input;r=input;
        {allocation_tracker::ProcessScope scope;e->processStereo(l.data(),r.data(),l.size());}
        require(l==reference,def.type+" mono left regression");
        if(!spatial)require(r==reference,def.type+" mono center/state isolation");
        else require(l!=r,def.type+" missing stereo field");
        require(e->latencySamples()==m->latencySamples(),"stereo added latency");
        e->reset();l=input;r.assign(input.size(),0);e->processStereo(l.data(),r.data(),l.size());
        for(float x:r)require(std::abs(x)<1e-8f,def.type+" L to R crosstalk");
        e->reset();r=input;l.assign(input.size(),0);e->processStereo(l.data(),r.data(),l.size());
        for(float x:l)require(std::abs(x)<1e-8f,def.type+" R to L crosstalk");
        if(!spatial)require(r==reference,def.type+" swap mismatch");
        e->reset();l=input;r=input;l[0]=std::numeric_limits<float>::quiet_NaN();r[1]=std::numeric_limits<float>::infinity();
        e->processStereo(l.data(),r.data(),l.size());
        for(std::size_t i=0;i<l.size();++i)require(std::isfinite(l[i])&&std::isfinite(r[i]),"nonfinite poisoned stereo state");
        require(e->invalidSampleCount==2,"invalid input diagnostics");
    }
}
static void matrix() {
    for(double rate:{44100.,48000.,96000.})for(int mode=0;mode<3;++mode) {
        auto c=make(mode,2,rate);for(auto& e:c.effects)if(auto* p=e->parameter("delay_ms"))p->set(20);c.reset();std::vector<float> a(4096),b(4096);
        for(std::size_t i=0;i<a.size();++i){a[i]=testSignal(i,rate);b[i]=testSignal(i+193,rate)*.7f;}
        auto expectedL=a,expectedR=b;render(c,expectedL,expectedR,128);
        for(std::size_t block:{0,64,128,256,512}) {
            c.reset();auto l=a,r=b;render(c,l,r,block);require(l==expectedL&&r==expectedR,"block/reset mismatch");
            c.prepare({rate,block?block:128,2});l=a;r=b;render(c,l,r,block);require(l==expectedL&&r==expectedR,"reprepare mismatch");
        }
    }
    auto c=make(0,2);for(double rate:{96000.,44100.,48000.}){c.prepare({rate,64,2});auto fresh=make(0,2,rate);std::vector<float> l(4096,.1f),r(4096,-.2f),a=l,b=r;render(c,l,r,64);render(fresh,a,b,64);require(l==a&&r==b,"rate transition state");}
}
static void cabinet() {
    auto c=createEffect("cabinet","cab");c->impulseResponse={1,.2f,-.3f};c->impulseSampleRate=48000;c->prepare({48000,128,2});
    std::vector<float> l(1024),r(1024);l[0]=1;r[13]=.5f;c->processStereo(l.data(),r.data(),l.size());
    for(std::size_t i=0;i<l.size();++i){float a=i>=128&&i<131?c->impulseResponse[i-128]:0;float b=i>=141&&i<144?.5f*c->impulseResponse[i-141]:0;require(std::abs(l[i]-a)<1e-6f&&std::abs(r[i]-b)<1e-6f,"cabinet L/R impulse");}
}
static void routing() {
    Chain c;AudioIO io;io.muted=false;io.prepare(c,{48000,128,2},{0,1});
    std::array<float,4096> l,r,a,b;l.fill(.4f);r.fill(-.8f);const float* in[]{l.data(),r.data()};float* out[]{a.data(),b.data()};
    {allocation_tracker::ProcessScope scope;io.process(c,{in,2,out,2,l.size()});}
    require(a==b&&a.back()>.199f,"mono guitar to stereo center");
    io.stereoInput=true;io.process(c,{in,2,out,2,l.size()});require(a.back()>.199f&&b.back()<-.399f,"stereo pair routing");
    auto levels=io.inputLevel.consume();require(levels.peak==.8f,"right input meter");
    io.prepare(c,{48000,64,2},{1});io.selectedInput=1;const float* rightOnly[]{r.data()};io.process(c,{rightOnly,1,out,2,l.size()});require(a.back()<-.399f&&b.back()==0,"missing right channel must be silent");
    io.process(c,{nullptr,0,nullptr,0,512});io.prepare(c,{44100,256,2},{});io.process(c,{nullptr,0,out,2,l.size()});require(a.back()==0&&b.back()==0,"zero channel recovery");
    io.prepare(c,{48000,128,2},{0,1});io.selectedInput=0;l.fill(0);r.fill(4);io.process(c,{in,2,out,2,l.size()});require(io.outputLevel.consume().clipped,"right output clip");
    io.muted=true;io.process(c,{in,2,out,2,l.size()});require(std::abs(b.back())<.001f,"stereo mute");
}
static void transitions() {
    for(const char* type:{"delay","reverb"}) {
        Chain a,b;a.effects.push_back(createEffect(type,"fx"));b=cloneConfiguration(a);
        a.prepare({48000,128,2});b.prepare({48000,512,2});
        std::vector<float> l(48000),r(48000);for(std::size_t i=0;i<l.size();++i){l[i]=testSignal(i,48000);r[i]=testSignal(i+123,48000);}
        auto x=l,y=r;
        for(std::size_t pos=0;pos<l.size();pos+=1000){
            auto* pa=a.effects[0]->parameter(type==std::string("delay")?"delay_ms":"room_size");
            auto* pb=b.effects[0]->parameter(pa->info.id);
            const float value=type==std::string("delay")?20+float(pos%231):float(pos%901)/901;
            pa->set(value);pb->set(value);
            {allocation_tracker::ProcessScope scope;a.processStereo(l.data()+pos,r.data()+pos,1000);}
            for(std::size_t j=0;j<1000;j+=64){allocation_tracker::ProcessScope scope;b.processStereo(x.data()+pos+j,y.data()+pos+j,std::min<std::size_t>(64,1000-j));}
        }
        require(l==x&&r==y,"spatial automation split equivalence");
        for(std::size_t i=0;i<l.size();++i)require(std::isfinite(l[i])&&std::isfinite(r[i]),"spatial transition stability");
    }
    app::Runtime runtime;runtime.chain=makeTestChain();require(runtime.prepare({48000,128,2},{0,1}),"stereo runtime prepare");
    auto candidate=cloneConfiguration(runtime.chain);for(auto& e:candidate.effects)if(e->type=="cabinet"){e->impulseResponse.assign(32768,0);e->impulseSampleRate=8000;}
    auto* first=runtime.chain.effects[0].get();require(!runtime.replace(std::move(candidate))&&runtime.chain.effects[0].get()==first&&runtime.ready,"stereo rollback");
    auto cab=createEffect("cabinet","long");cab->impulseResponse.assign(32768,0);cab->impulseResponse[0]=1;cab->impulseResponse.back()=.25f;cab->impulseSampleRate=48000;cab->prepare({48000,128,2});
    std::vector<float> l(33000),r(33000);l[0]=1;r[1]=-.5f;
    {allocation_tracker::ProcessScope scope;cab->processStereo(l.data(),r.data(),l.size());}
    require(std::abs(l[32895]-.25f)<1e-6f&&std::abs(r[32896]+.125f)<1e-6f,"long stereo IR last partition");
}
static void controls() {
    auto c=make(2,2);std::atomic<bool> done{false};
    std::thread writer([&]{while(!done){for(auto& e:c.effects){e->bypass.store(!e->bypass.load());for(auto& p:e->parameters)if(!p->info.requiresPrepare)p->set((p->info.minimum+p->info.maximum)*.5f);}}});
    std::array<float,128> l{},r{};for(int k=0;k<3000;++k){l.fill(.2f);r.fill(-.1f);allocation_tracker::ProcessScope scope;c.processStereo(l.data(),r.data(),128);}done=true;writer.join();
    for(float x:l)require(std::isfinite(x),"concurrent invalid L");for(float x:r)require(std::isfinite(x),"concurrent invalid R");
}
static void profile() {
    std::cout<<"channels,factor,mean_us,max_us,max_deadline_percent,latency_samples\n";
    for(unsigned channels:{1,2})for(int mode=0;mode<3;++mode){auto c=make(mode,channels);c.effects.pop_back();std::array<float,128> l{},r{};double sum=0,worst=0;
        for(int k=0;k<2100;++k){for(int i=0;i<128;++i){l[i]=testSignal(k*128+i,48000);r[i]=testSignal(k*128+i+59,48000);}
            auto start=std::chrono::steady_clock::now();if(channels==1)c.process(l.data(),128);else c.processStereo(l.data(),r.data(),128);
            double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();if(k>=100){sum+=us;worst=std::max(worst,us);}}
        std::size_t latency=0;for(auto& e:c.effects)latency+=e->latencySamples();std::cout<<channels<<','<<(1<<mode)<<','<<sum/2000<<','<<worst<<','<<worst/2666.6667*100<<','<<latency<<'\n';}
}
int main(int argc,char** argv) {try {
    if(argc>1&&std::string(argv[1])=="profile"){profile();return 0;}
    if(argc>1)controls();else{effects();matrix();cabinet();routing();transitions();}
    require(allocation_tracker::processAllocations==0&&allocation_tracker::processFrees==0,"process allocation/free");
    std::cout<<"Stereo state/matrix/mono/routing/allocation PASS\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
