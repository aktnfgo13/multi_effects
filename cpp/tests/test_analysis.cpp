#include "AllocationTracker.h"
#include "TestSupport.h"
#include "analysis/Analyzer.h"
#include <thread>
#include <chrono>
#include <iostream>
using namespace multi_effects;
static void queueTest() {
    AnalysisTap tap;tap.prepare();tap.setEnabled(true);std::array<float,256>x{};AnalysisTap::Packet p;
    {allocation_tracker::ProcessScope scope;
        for(unsigned k=0;k<64;++k)require(tap.tryPush(x.data(),nullptr,x.data(),nullptr,256,48000,64,3),"capacity");
        require(!tap.tryPush(x.data(),nullptr,x.data(),nullptr,128,48000,64,3),"drop newest");}
    require(tap.dropCount()==128,"drop frames");
    for(unsigned k=0;k<64;++k){require(tap.tryRead(p),"read packet");require(p.frameIndex==k*256&&p.channels==1&&p.latency==64,"metadata");}
    require(!tap.tryRead(p),"empty");tap.tryPush(x.data(),nullptr,x.data(),nullptr,1,96000,0,4);tap.tryRead(p);require(p.frameIndex==64*256+128&&p.epoch==4&&p.sampleRate==96000,"gap/configuration");
    const auto old=p.session;tap.setEnabled(false);require(!tap.tryPush(x.data(),nullptr,x.data(),nullptr,1,48000,0,4),"off");tap.setEnabled(true);tap.tryPush(x.data(),nullptr,x.data(),nullptr,1,48000,0,4);tap.tryRead(p);require(p.session!=old,"resume epoch");
    std::atomic<bool> done{false};unsigned received=0;
    std::thread reader([&]{AnalysisTap::Packet item;std::uint64_t previous=0;while(true){if(!tap.tryRead(item)){if(done)break;std::this_thread::yield();continue;}require(item.frameIndex>previous,"queue order");previous=item.frameIndex;for(unsigned i=1;i<item.frames;++i)require(item.samples[0][i]==item.samples[0][0],"torn packet");++received;}});
    for(unsigned k=0;k<10000;++k){x.fill(float(k));allocation_tracker::ProcessScope scope;tap.tryPush(x.data(),nullptr,x.data(),nullptr,256,48000,0,5);}done=true;reader.join();require(received>0,"concurrent consumer");
}
static void fftTest() {
    for(unsigned size:{2048,4096})for(double rate:{44100.,48000.,96000.}) {
        Analyzer analyzer;analyzer.setFFTSize(size);AnalysisTap::Packet p;p.frames=256;p.sampleRate=rate;p.channels=2;p.latency=128;p.epoch=1;
        const double frequency=37*rate/size;
        for(unsigned start=0;start<12288;start+=256){p.frameIndex=start;for(unsigned i=0;i<256;++i){const auto n=start+i;p.samples[0][i]=float(.5*std::sin(6.283185307179586*frequency*n/rate));p.samples[1][i]=p.samples[0][i]*.5f;p.samples[2][i]=n>=128?float(.5*std::sin(6.283185307179586*frequency*(n-128)/rate)):0;p.samples[3][i]=p.samples[2][i]*.5f;}analyzer.accept(p);}
        auto s=analyzer.snapshot();require(s.validFrames==4096&&s.latency==128,"latency warmup");require(s.waveform[0]==s.waveform[2]&&s.waveform[1]==s.waveform[3],"sample aligned stereo waveform");
        unsigned peak=1;for(unsigned k=2;k<size/2;++k)if(s.spectrum[0][k]>s.spectrum[0][peak])peak=k;
        require(peak==37,"FFT peak frequency");require(std::abs(s.spectrum[0][37]+6.0206f)<.01,"Hann calibrated dBFS");require(std::abs(s.spectrum[1][37]+12.0412f)<.01,"stereo amplitude");
        p.epoch=2;p.frameIndex+=256;p.sampleRate=48000;analyzer.accept(p);require(analyzer.snapshot().validFrames<2048,"configuration invalidates history");
        p.frameIndex+=512;analyzer.accept(p);require(analyzer.snapshot().validFrames<2048,"dropped gap invalidates history");
        std::cout<<"FFT "<<size<<" rate "<<rate<<" bin "<<peak<<" amplitude "<<s.spectrum[0][37]<<" dBFS PASS\n";
    }
}
static Chain chain() {Chain c;c.effects.push_back(createEffect("custom_drive","od"));c.effects.push_back(createEffect("cabinet","cab"));return c;}
static void outputTest() {
    for(double rate:{44100.,48000.,96000.})for(unsigned channels:{1,2})for(unsigned block:{1,64,128,256,512,777}) {
        auto a=chain(),b=chain();AnalysisTap tap;tap.prepare();tap.setEnabled(true);b.analysis=&tap;a.prepare({rate,512,channels});b.prepare({rate,512,channels});
        std::vector<float> left(8192),right(8192);for(unsigned i=0;i<8192;++i){left[i]=testSignal(i,rate);right[i]=testSignal(i+123,rate);}
        auto l=left,r=right;
        {allocation_tracker::ProcessScope scope;if(channels==2)a.processStereo(left.data(),right.data(),8192);else a.process(left.data(),8192);}
        AnalysisTap::Packet p;
        for(unsigned pos=0;pos<8192;){unsigned n=std::min(block,8192-pos);{allocation_tracker::ProcessScope scope;if(channels==2)b.processStereo(l.data()+pos,r.data()+pos,n);else b.process(l.data()+pos,n);}while(tap.tryRead(p)){}pos+=n;}
        require(l==left&&r==right,"analysis changed DSP samples");
        b.reset();l.assign(8192,0);r=l;{allocation_tracker::ProcessScope scope;if(channels==2)b.processStereo(l.data(),r.data(),8192);else b.process(l.data(),8192);}for(float v:l)require(v==0,"reset silence");
    }
}
static void profile() {
    for(bool enabled:{false,true}) {
        auto c=chain();c.prepare({48000,128,2});AnalysisTap tap;tap.prepare();tap.setEnabled(enabled);c.analysis=&tap;
        std::atomic<bool> done{false};std::thread consumer([&]{Analyzer a;AnalysisTap::Packet p;while(!done){while(tap.tryRead(p))a.accept(p);if(a.ready())a.snapshot();std::this_thread::sleep_for(std::chrono::milliseconds(2));}});
        double sum=0,max=0;std::array<float,128>l{},r{};
        for(unsigned k=0;k<2100;++k){for(unsigned i=0;i<128;++i){l[i]=testSignal(k*128+i,48000);r[i]=l[i]*.7f;}auto start=std::chrono::steady_clock::now();{allocation_tracker::ProcessScope scope;c.processStereo(l.data(),r.data(),128);}double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();if(k>=100){sum+=us;max=std::max(max,us);}}
        done=true;consumer.join();std::cout<<"Analysis "<<enabled<<" stereo 48k/128 mean_us="<<sum/2000<<" max_us="<<max<<" drops="<<tap.dropCount()<<'\n';
    }
}
int main(){try {queueTest();fftTest();outputTest();profile();require(allocation_tracker::processAllocations==0&&allocation_tracker::processFrees==0,"audio allocation/free");std::cout<<"Analysis output exact, SPSC, reset/rate/block/stereo, FFT and allocation/free 0 PASS\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
