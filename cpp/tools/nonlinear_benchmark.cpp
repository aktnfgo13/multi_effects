#include "engine/Engine.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
using namespace multi_effects;
static Chain make(const std::string& type, int mode, double rate)
{
    Chain c;
    const std::vector<std::string> names = type=="chain" ? std::vector<std::string>{"noise_gate","compressor","gain","distortion","eq","amp_sim","cabinet","delay","reverb"} : std::vector<std::string>{type};
    for(auto& name:names) {auto e=createEffect(name=="cabinet_long"?"cabinet":name,name);if(name=="cabinet_long"){e->impulseResponse.assign(32768,.0001f);e->impulseResponse[0]=1;e->impulseSampleRate=48000;}if(auto* p=e->parameter("oversampling"))p->set(float(mode));c.effects.push_back(std::move(e));}
    c.prepare({rate,128});return c;
}
int main(int argc,char** argv)
{
    try {
        if(argc==2 && std::string(argv[1])=="profile") {
            for(auto type:{"distortion","amp_sim","cabinet","cabinet_long","chain"})for(int mode=0;mode<3;++mode){
                auto c=make(type,mode,48000);std::vector<float> b(128);double sum=0,worst=0;std::size_t latency=0;
                for(auto& e:c.effects)latency+=e->latencySamples();
                for(int k=0;k<2100;++k){for(int i=0;i<128;++i)b[i]=float(.3*std::sin((k*128+i)*.037));
                    auto t=std::chrono::steady_clock::now();c.process(b.data(),128);
                    const double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-t).count();
                    if(k>=100){sum+=us;worst=std::max(worst,us);}}
                std::cout<<type<<","<<(1<<mode)<<","<<sum/2000<<","<<worst<<","<<worst/2666.6667*100<<","<<latency<<"\n";
            }return 0;
        }
        if(argc!=7)throw std::runtime_error("type mode rate input.f32 output.f32 drive_db");
        auto c=make(argv[1],std::stoi(argv[2]),std::stod(argv[3]));auto& e=*c.effects[0];
        if(auto* p=e.parameter("drive_db"))p->set(std::stof(argv[6]));
        if(auto* p=e.parameter("gain_db"))p->set(std::stof(argv[6]));
        if(auto* p=e.parameter("mix"))p->set(1);
        c.reset();std::ifstream in(argv[4],std::ios::binary|std::ios::ate);if(!in)throw std::runtime_error("input");
        const auto bytes=in.tellg();if(bytes<0 || std::streamoff(bytes)%sizeof(float)!=0)throw std::runtime_error("Input must contain complete float32 samples");std::vector<float> data(std::size_t(bytes)/sizeof(float));in.seekg(0);in.read(reinterpret_cast<char*>(data.data()),bytes);
        for(std::size_t i=0;i<data.size();i+=128)c.process(data.data()+i,std::min<std::size_t>(128,data.size()-i));
        std::ofstream out(argv[5],std::ios::binary);out.write(reinterpret_cast<const char*>(data.data()),std::streamsize(data.size()*sizeof(float)));if(!out)throw std::runtime_error("output");
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
