#include "engine/Engine.h"
#include "engine/InputSystem.h"
#include "effects/CustomDrive.h"
#include <chrono>
#include <fstream>
#include <iostream>
using namespace multi_effects;
int main(int argc,char** argv){try{
    if(argc==4 && std::string(argv[1])=="conduction") {
        GrainFeedback cell;
        cell.prepare(48000);cell.configure(std::stof(argv[2])*.01f);
        std::ifstream input(argv[3],std::ios::binary);
        std::array<double,3> total{};
        std::array<size_t,3> active{};
        size_t samples=0;
        float x;
        while(input.read(reinterpret_cast<char*>(&x),sizeof(x))) {
            const auto currents=cell.currents(x);
            for(size_t i=0;i<3;++i) {
                total[i]+=currents[i];
                active[i]+=currents[i]>0;
            }
            cell.process(x);
            ++samples;
        }
        if(!samples || !input.eof())throw std::runtime_error("Invalid cell probe");
        for(size_t i=0;i<3;++i)std::cout<<double(active[i])/samples<<' '<<total[i]/samples<<'\n';
        return 0;
    }
    if((argc>=2&&argc<=4)&&std::string(argv[1])=="profile"){
        const float grain=argc>=3?std::stof(argv[2]):25.f;
        const float headroom=argc==4?std::stof(argv[3]):50.f;
        if(!std::isfinite(headroom)||headroom<0||headroom>100)throw std::runtime_error("Profile Headroom must be 0..100");
        if(!std::isfinite(grain)||grain<0||grain>100)throw std::runtime_error("Profile Grain must be 0..100");
        std::cout<<"factor,mean_us,max_us,max_deadline_percent,latency_samples,lane_state_bytes,grain\n";
        for(int mode=0;mode<3;++mode){auto e=createEffect("custom_drive","bench");e->parameter("grain")->set(grain);e->parameter("headroom")->set(headroom);e->parameter("oversampling")->set(float(mode));e->prepare({48000,128,2});std::array<float,128> l{},r{};double sum=0,worst=0;
            for(int k=0;k<2100;++k){for(int i=0;i<128;++i){l[i]=.3f*std::sin(float(k*128+i)*.037f);r[i]=l[i]*.8f;}auto t=std::chrono::steady_clock::now();e->processStereo(l.data(),r.data(),128);double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-t).count();if(k>=100){sum+=us;worst=std::max(worst,us);}}
            std::cout<<(1<<mode)<<','<<sum/2000<<','<<worst<<','<<worst/2666.6667*100<<','<<e->latencySamples()<<','<<sizeof(CustomDrive)<<','<<grain<<'\n';}return 0;
    }
    if(argc<6)throw std::runtime_error("type mode rate input.f32 output.f32 [parameter=value ...]");
    auto e=createEffect(argv[1],"benchmark");if(auto* p=e->parameter("oversampling"))p->set(float(std::stoi(argv[2])));
    InputSystem input;bool useInput=false;
    for(int i=6;i<argc;++i){std::string a=argv[i];auto equals=a.find('=');if(equals==std::string::npos)throw std::runtime_error("Expected parameter=value");if(a.substr(0,equals)=="input_trim_db"){input.setTrimDb(std::stof(a.substr(equals+1)));useInput=true;continue;}auto* p=e->parameter(a.substr(0,equals));if(!p)throw std::runtime_error("Unknown parameter");p->set(std::stof(a.substr(equals+1)));}
    e->prepare({std::stod(argv[3]),128,1});std::ifstream in(argv[4],std::ios::binary|std::ios::ate);if(!in)throw std::runtime_error("Input file");auto bytes=in.tellg();if(bytes<0||std::streamoff(bytes)%4)throw std::runtime_error("Incomplete float32 samples");std::vector<float> data(std::size_t(bytes)/4);in.seekg(0);in.read(reinterpret_cast<char*>(data.data()),bytes);
    if(useInput) {
        input.prepare(std::stod(argv[3]));
        double power=0;float peak=0;
        for(std::size_t i=0;i<data.size();i+=128) {
            const auto count=std::min<std::size_t>(128,data.size()-i);
            input.process(data.data()+i,nullptr,count);
        }
        for(float sample:data){peak=std::max(peak,std::abs(sample));power+=double(sample)*sample;}
        std::cout.precision(10);
        std::cout<<peak<<" "<<std::sqrt(power/std::max<std::size_t>(1,data.size()))<<" "<<InputReference::nominalRmsDb<<"\n";
    }
    for(std::size_t i=0;i<data.size();i+=128)e->process(data.data()+i,std::min<std::size_t>(128,data.size()-i));
    std::ofstream out(argv[5],std::ios::binary);out.write(reinterpret_cast<const char*>(data.data()),std::streamsize(bytes));if(!in||!out)throw std::runtime_error("File IO failed");
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
