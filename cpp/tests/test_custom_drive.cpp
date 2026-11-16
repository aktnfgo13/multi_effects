#include "AllocationTracker.h"
#include "TestSupport.h"
#include "effects/CustomDrive.h"
#include <iostream>
#include <thread>
using namespace multi_effects;
static void checkMixRamp() {
    for (int factor : {1, 2, 4}) {
        CustomDrive lane;
        lane.prepare(48000, factor);
        CustomDrive::Controls controls;
        for (float start : {0.f, 1.f}) {
            lane.reset();
            controls.mix = start;
            lane.processSample(.25f, controls);
            require(lane.appliedMix() == start, "reset starts at target");
            controls.mix = 1.f - start;
            for (int i = 1; i <= 960; ++i) {
                allocation_tracker::ProcessScope scope;
                lane.processSample(.25f, controls);
                const float expected = start + (controls.mix - start) * i / 960.f;
                require(std::abs(lane.appliedMix() - expected) < 2e-5f, "20 ms Mix ramp");
            }
            require(lane.appliedMix() == controls.mix, "exact Mix endpoint");
        }
    }
}
int main(int argc,char**){try{
    checkMixRamp();
    if(argc>1){auto e=createEffect("custom_drive","threaded");e->prepare({48000,128,2});std::atomic<bool> done{false};
        std::thread writer([&]{for(int n=0;n<2000;++n){e->parameter("drive")->set(float(n%101));e->parameter("grain")->set(float((n*3)%101));e->parameter("headroom")->set(float((n*7)%101));}done=true;});
        std::array<float,128> l{},r{};for(int n=0;n<4000||!done;++n){l.fill(.1f);r.fill(-.2f);allocation_tracker::ProcessScope scope;e->processStereo(l.data(),r.data(),128);}writer.join();
        require(allocation_tracker::processAllocations==0&&allocation_tracker::processFrees==0,"concurrent allocation");return 0;}

    for(double rate:{44100.,48000.,96000.})for(int mode:{0,1,2}){
        const auto make=[&]{auto e=createEffect("custom_drive","drive");e->parameter("oversampling")->set(float(mode));e->parameter("drive")->set(100);e->parameter("grain")->set(100);e->prepare({rate,128,2});return e;};
        auto whole=make(),split=make();require(whole->latencySamples()==std::size_t(mode?64:0),"latency");
        std::vector<float> l(8192),r(8192);for(std::size_t i=0;i<l.size();++i)l[i]=testSignal(i,rate);auto a=l,b=r;
        {allocation_tracker::ProcessScope scope;whole->processStereo(l.data(),r.data(),l.size());}
        std::size_t offset=0,j=0;const std::array<std::size_t,7> blocks{64,128,256,512,1,7,1001};
        while(offset<a.size()){auto n=std::min(blocks[j++%blocks.size()],a.size()-offset);allocation_tracker::ProcessScope scope;split->processStereo(a.data()+offset,b.data()+offset,n);offset+=n;}
        require(l==a&&r==b,"block partition equivalence");for(float v:r)require(v==0,"stereo crosstalk");for(float v:l)require(std::isfinite(v),"nonfinite output");
        whole->reset();a.assign(a.size(),0);b=a;for(std::size_t i=0;i<a.size();++i)a[i]=testSignal(i,rate);whole->processStereo(a.data(),b.data(),a.size());require(a==l,"reset reproducibility");
        auto dry=make();dry->parameter("mix")->set(0);dry->reset();a.assign(a.size(),0);b=a;a[0]=.5f;dry->processStereo(a.data(),b.data(),a.size());require(a[mode?64:0]==.5f,"dry alignment");
        a[0]=std::numeric_limits<float>::quiet_NaN();a[1]=std::numeric_limits<float>::infinity();a[2]=std::numeric_limits<float>::max();whole->processStereo(a.data(),b.data(),a.size());for(float v:a)require(std::isfinite(v),"overload recovery");
        for(int i=0;i<250;++i){whole->parameter("tight")->set(float(i%101));whole->parameter("drive")->set(float(i%101));whole->parameter("headroom")->set(float((i*7)%101));whole->parameter("crunch")->set(float((i*3)%101));a.assign(512,.5f);b.assign(512,-.2f);allocation_tracker::ProcessScope scope;whole->processStereo(a.data(),b.data(),512);for(float v:a)require(std::isfinite(v),"dynamic stability");}
    }
    // Tuning guards at the hardware baseline only. Every macro corner must remain
    // finite and settle to silence, including the brightest/hardest combination.
    for(int mode:{1,2,4})for(float headroom:{0.f,100.f}) {
        CustomDrive effect;effect.prepare(48000,mode);
        CustomDrive::Controls controls;controls.drive=controls.tone=controls.crunch=controls.grain=100;controls.headroom=headroom;
        float tail=0;
        {allocation_tracker::ProcessScope scope;
            for(int i=0;i<96000;++i){float y=effect.processSample(i<24000?float(.5*std::sin(i*.07)):0.f,controls);require(std::isfinite(y),"extreme macro finite");if(i>95000)tail=std::max(tail,std::abs(y));}}
        require(tail<1e-5f,"extreme macro settles to silence");
    }
    // Default input-level response may saturate but must not fold backwards.
    double previous=0;
    for(float amplitude:{.001f,.004f,.016f,.032f,.063f,.126f,.251f,.501f,1.f}) {
        CustomDrive effect;effect.prepare(48000,4);CustomDrive::Controls controls;double energy=0;
        for(int i=0;i<96000;++i){float y=effect.processSample(amplitude*float(std::sin(i*6.283185307179586*440/48000)),controls);if(i>=48000)energy+=y*y;}
        require(energy>=previous*.9801,"input-level foldback");previous=energy;
    }
    // Headroom meaning is tested at the output, independently of static gain:
    const auto harmonicRatio=[](float headroom,float amplitude) {
        CustomDrive effect;effect.prepare(48000,2);CustomDrive::Controls controls;
        controls.drive=70;controls.headroom=headroom;
        double sum=0,energy=0,sine=0,cosine=0;
        {allocation_tracker::ProcessScope scope;
            for(int i=0;i<96000;++i){const double angle=6.283185307179586*440*i/48000;
                const float y=effect.processSample(amplitude*float(std::sin(angle)),controls);
                if(i>=48000){sum+=y;energy+=double(y)*y;sine+=y*std::sin(angle);cosine+=y*std::cos(angle);}}}
        const double fundamental=2*(sine*sine+cosine*cosine)/48000;
        return std::sqrt(std::max(0.,energy-sum*sum/48000-fundamental)/fundamental);
    };
    double previousDistortion=harmonicRatio(0,.0630957f);
    for(float headroom:{25.f,50.f,75.f,100.f}){
        const double weak=harmonicRatio(headroom,.0630957f);
        require(weak<previousDistortion,"weak-input distortion falls as Headroom opens");
        previousDistortion=weak;
    }
    require(harmonicRatio(100,.501187f)>previousDistortion*2,"weak/strong onset separation");
    // Captured from the pre-change executable with this exact integer ramp input.
    struct Frozen {float crunch,headroom;std::array<float,8> tail;};
    const Frozen frozen[]{
        {0,50,{.0761441812f,.0773836598f,.0785990283f,.0797906965f,.0809590742f,.0821045637f,.0832275972f,.0843285173f}},
        {25,50,{.0847222731f,.0861065835f,.0874615237f,.08878766f,.0900854692f,.0913554803f,.0925981849f,.0938140899f}},
        {50,50,{.100448415f,.102145143f,.103800662f,.105415612f,.106990457f,.108525723f,.110021941f,.111479647f}},
        {75,50,{.125463799f,.127608418f,.129683524f,.131689548f,.133626923f,.135496214f,.137298092f,.139033422f}},
        {100,50,{.161286488f,.163826346f,.166236028f,.168517262f,.170672119f,.17270349f,.174614474f,.17640841f}}
    };
    for(const auto& reference:frozen){
        auto effect=createEffect("custom_drive","frozen");effect->parameter("crunch")->set(reference.crunch);
        effect->parameter("headroom")->set(reference.headroom);effect->prepare({48000,128,1});
        std::vector<float> data(12000);for(std::size_t i=0;i<data.size();++i)data[i]=float(int(i%251)-125)/512;
        {allocation_tracker::ProcessScope scope;effect->process(data.data(),data.size());}
        for(std::size_t i=0;i<8;++i)require(std::abs(data[data.size()-8+i]-reference.tail[i])<2e-6f,"frozen Crunch / centered Headroom reference");
    }
    {auto warm=createEffect("custom_drive","warm");}
    const auto before=allocation_tracker::liveBytes.load();
    {auto measured=createEffect("custom_drive","memory");measured->prepare({48000,128,2});const auto prepared=allocation_tracker::liveBytes.load();
        for(int i=0;i<20;++i)measured->prepare({i%2?48000.:96000.,128,2});require(allocation_tracker::liveBytes.load()==prepared,"prepare memory growth");
        std::cout<<"Prepared stereo effect tracked heap bytes: "<<prepared-before<<'\n';}
    require(allocation_tracker::liveBytes.load()==before,"effect lifetime leak");
    CustomDrive d;requireRejected([&]{d.prepare(0,2);},"invalid rate");d.prepare(48000,4);CustomDrive::Controls p;p.grain=std::numeric_limits<float>::quiet_NaN();require(std::isfinite(d.processSample(.1f,p)),"invalid control");
    require(allocation_tracker::processAllocations==0&&allocation_tracker::processFrees==0,"processing allocation/free");
    std::cout<<"Custom Drive rate/block/reset/stereo/dry latency/overload/automation/allocation PASS; lane bytes "<<sizeof(CustomDrive)<<'\n';
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
