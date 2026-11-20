// macOS integration test. Default: installed user AU. Optional executable path:
// load/register that candidate only in this process, leaving installation untouched.
#include <AudioToolbox/AudioToolbox.h>
#include "engine/Engine.h"
#include <array>
#include <cmath>
#include <iostream>
#include <set>
#include <stdexcept>
#include <dlfcn.h>

using namespace multi_effects;
static void check(bool ok,const char* what) { if(!ok) throw std::runtime_error(what); }
static void checked(OSStatus s,const char* what) {
    if(s!=noErr) throw std::runtime_error(std::string(what)+" OSStatus "+std::to_string(s));
}
static float signal(unsigned frame,unsigned lane) {
    return float(.27*std::sin(frame*(lane?.041:.057))+.07*std::sin(frame*.137));
}
static OSStatus feed(void*,AudioUnitRenderActionFlags*,const AudioTimeStamp* ts,UInt32,UInt32 n,AudioBufferList* b) {
    for(unsigned ch=0;ch<b->mNumberBuffers;++ch)
        for(unsigned i=0;i<n;++i) static_cast<float*>(b->mBuffers[ch].mData)[i]=signal(unsigned(ts->mSampleTime)+i,ch);
    return noErr;
}
struct Unit {
    AudioUnit au=nullptr;
    ~Unit(){if(au){AudioUnitUninitialize(au);AudioComponentInstanceDispose(au);}}
};
struct Result {std::vector<float> au,core;};
// Optional candidate binary: process-local registration, no installation or cache edits.
static AudioComponent candidateComponent=nullptr;
static Result render(const char* key,const char* name,float value,unsigned block) {
    AudioComponentDescription desc{'aufx','Mfx6','Mefx',0,0};
    Unit u;auto component=candidateComponent?candidateComponent:AudioComponentFindNext(nullptr,&desc);check(component,"AU not registered");
    checked(AudioComponentInstanceNew(component,&u.au),"instantiate");
    constexpr auto global=kAudioUnitScope_Global;
    CFPropertyListRef saved=nullptr;
    UInt32 size=sizeof(saved);
    checked(AudioUnitGetProperty(u.au,kAudioUnitProperty_ClassInfo,global,0,&saved,&size),"read class info");
    auto state=CFDictionaryCreateMutableCopy(nullptr,0,static_cast<CFDictionaryRef>(saved));
    CFRelease(saved);
    const char json[]=R"({"plugin_state":1,"bypass":false,"preset":{"version":3,"schema":"multi-effects","effects":[{"type":"custom_drive","id":"probe","parameters":{"grain":25}}]}})";
    auto bytes=CFDataCreate(nullptr,reinterpret_cast<const UInt8*>(json),sizeof(json)-1);
    CFDictionarySetValue(state,CFSTR("jucePluginState"),bytes);
    checked(AudioUnitSetProperty(u.au,kAudioUnitProperty_ClassInfo,global,0,&state,sizeof(state)),"restore chain");
    CFRelease(bytes);CFRelease(state);
    AudioStreamBasicDescription fmt{48000,kAudioFormatLinearPCM,kAudioFormatFlagsNativeFloatPacked|kAudioFormatFlagIsNonInterleaved,4,1,4,2,32,0};
    for(auto scope:{kAudioUnitScope_Input,kAudioUnitScope_Output})
        checked(AudioUnitSetProperty(u.au,kAudioUnitProperty_StreamFormat,scope,0,&fmt,sizeof(fmt)),"format");
    AURenderCallbackStruct cb{feed,nullptr};
    checked(AudioUnitSetProperty(u.au,kAudioUnitProperty_SetRenderCallback,kAudioUnitScope_Input,0,&cb,sizeof(cb)),"input");
    checked(AudioUnitInitialize(u.au),"initialize");
    checked(AudioUnitGetPropertyInfo(u.au,kAudioUnitProperty_ParameterList,global,0,&size,nullptr),"list size");
    std::vector<AudioUnitParameterID> ids(size/sizeof(AudioUnitParameterID));
    checked(AudioUnitGetProperty(u.au,kAudioUnitProperty_ParameterList,global,0,ids.data(),&size),"list");
    check(std::set<AudioUnitParameterID>(ids.begin(),ids.end()).size()==ids.size(),"duplicate AU IDs");
    const auto expected=std::string("Custom Drive / ")+name;
    AudioUnitParameterID id=0;unsigned matches=0;
    for(auto candidate:ids) {
        AudioUnitParameterInfo info{};size=sizeof(info);
        checked(AudioUnitGetProperty(u.au,kAudioUnitProperty_ParameterInfo,global,candidate,&info,&size),"parameter info");
        if(expected==info.name){id=candidate;++matches;check(info.minValue==0&&info.maxValue==1,"AU range");}
        if(info.flags&kAudioUnitParameterFlag_CFNameRelease)CFRelease(info.cfNameString);
    }
    check(matches==1,"missing or ambiguous AU parameter");
    auto core=createEffect("custom_drive","probe");core->prepare({48000,128,2});
    Result result;
    for(unsigned pos=0;pos<49152;) {
        if(pos==1024){
            checked(AudioUnitSetParameter(u.au,id,global,0,value/100,0),"live edit");
            core->parameter(key)->set(value);
        }
        unsigned n=std::min(block,(pos<1024?1024u:49152u)-pos);
        std::array<std::array<float,128>,2> native{},direct{};
        struct {UInt32 count;AudioBuffer buffers[2];} list{2,{{1,n*4,native[0].data()},{1,n*4,native[1].data()}}};
        AudioTimeStamp ts{};ts.mFlags=kAudioTimeStampSampleTimeValid;ts.mSampleTime=pos;
        AudioUnitRenderActionFlags flags=0;
        checked(AudioUnitRender(u.au,&flags,&ts,0,n,reinterpret_cast<AudioBufferList*>(&list)),"AU render");
        for(unsigned ch=0;ch<2;++ch)for(unsigned i=0;i<n;++i)direct[ch][i]=signal(pos+i,ch);
        core->processStereo(direct[0].data(),direct[1].data(),n);
        for(unsigned i=0;i<n;++i)for(unsigned ch=0;ch<2;++ch){
            check(std::abs(native[ch][i]-direct[ch][i])<1e-6f,"AU/core waveform mismatch");
            if(pos+i>=32768){result.au.push_back(native[ch][i]);result.core.push_back(direct[ch][i]);}
        }
        pos+=n;
    }
    float read=0;checked(AudioUnitGetParameter(u.au,id,global,0,&read),"AU readback");
    check(read==value/100,"host value overwritten");
    std::cout<<key<<" id="<<id<<" host="<<read;
    const std::string parameter=key;
    if(parameter=="grain"||parameter=="headroom") {
        const float applied=parameter=="grain"?core->appliedGrain():core->appliedHeadroom();
        check(std::abs(applied-value)<.01f,"DSP current");
        std::cout<<" DSP="<<applied;
    }
    std::cout<<'\n';
    return result;
}
static void difference(const std::vector<float>& a,const std::vector<float>& b) {
    double sum=0,max=0;check(a.size()==b.size()&&!a.empty(),"buffer sizes");
    for(size_t i=0;i<a.size();++i){double d=double(a[i])-b[i];sum+=d*d;max=std::max(max,std::abs(d));}
    check(max>.001,"identical or negligible output");std::cout<<"max="<<max<<" rms="<<std::sqrt(sum/a.size())<<" equal=false\n";
}
int main(int argc,char** argv){try{
    if(argc>1) {
        check(argc==2,"usage: test_installed_au_grain [candidate AU executable]");
        // Keep the library loaded until process exit: the registrar owns its factory.
        auto* library=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
        if(!library)throw std::runtime_error(dlerror());
        auto factory=reinterpret_cast<AudioComponentFactoryFunction>(dlsym(library,"Multi_EffectsAUFactory"));
        check(factory,"candidate AU factory missing");
        AudioComponentDescription desc{'aufx','Mfx6','Mefx',kAudioComponentFlag_SandboxSafe,0};
        candidateComponent=AudioComponentRegister(&desc,CFSTR("Multi Effects candidate probe"),0x800,factory);
        check(candidateComponent,"process-local AU registration failed");
        std::cout<<"Process-local candidate AU: "<<argv[1]<<'\n';
    }
    const auto low=render("grain","Grain",0,128),mid=render("grain","Grain",50,128),high=render("grain","Grain",100,128);
    check(render("grain","Grain",100,47).au==high.au,"AU block split");
    std::cout<<"Core ";difference(low.core,high.core);std::cout<<"AU ";difference(low.au,high.au);
    difference(mid.au,high.au);
    for(float h:{25.f,50.f,75.f})render("headroom","Headroom",h,128);
    const auto h0=render("headroom","Headroom",0,128),h100=render("headroom","Headroom",100,128);
    std::cout<<"Headroom AU ";difference(h0.au,h100.au);
    const auto bias0=render("bias","Bias",0,128),bias50=render("bias","Bias",50,128),bias100=render("bias","Bias",100,128);
    std::cout<<"Bias AU 0 to 50 ";difference(bias0.au,bias50.au);
    std::cout<<"Bias AU 50 to 100 ";difference(bias50.au,bias100.au);
    const auto sym0=render("symmetry","Symmetry",0,128),sym50=render("symmetry","Symmetry",50,128),sym100=render("symmetry","Symmetry",100,128);
    std::cout<<"Symmetry AU 0 to 50 ";difference(sym0.au,sym50.au);
    std::cout<<"Symmetry AU 50 to 100 ";difference(sym50.au,sym100.au);
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
