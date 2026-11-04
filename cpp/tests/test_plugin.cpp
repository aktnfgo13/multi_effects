#include "AllocationTracker.h"
#include "TestSupport.h"
#include "../plugin/PluginProcessor.h"
#include "../ui/AlphaUI.h"
#include <iostream>
#include <chrono>
using namespace multi_effects;
using plugin::Processor;
static void set(Processor& p,const char* id,float value){auto* parameter=p.parameters.getParameter(id);require(parameter!=nullptr,"missing host parameter");parameter->setValueNotifyingHost(parameter->convertTo0to1(value));}
static void pump(Processor& p,juce::AudioBuffer<float>& b){juce::MidiBuffer midi;allocation_tracker::ProcessScope scope;p.processBlock(b,midi);}
static void layout(Processor& p,int inputs){auto l=p.getBusesLayout();l.inputBuses.set(0,inputs==1?juce::AudioChannelSet::mono():juce::AudioChannelSet::stereo());l.outputBuses.set(0,juce::AudioChannelSet::stereo());require(p.setBusesLayout(l),"host layout rejected");}
static void matrix(){
    for(double rate:{44100.,48000.,96000.})for(int inputs:{1,2})for(int mode:{0,1,2}) {
        Processor p;layout(p,inputs);set(p,"distortion.oversampling",float(mode));set(p,"amp_sim.oversampling",float(mode));p.prepareToPlay(rate,128);
        auto reference=p.snapshot();reference.prepare({rate,128,2});
        const auto latency=mode?320:128;require(p.getLatencySamples()==latency,"reported latency");
        for(int size:{64,128,256,512,7,1001}) {
            juce::AudioBuffer<float> b(2,size);std::vector<float> l(std::size_t(size),0.f),r=l;
            for(int i=0;i<size;++i){l[std::size_t(i)]=testSignal(std::size_t(i),rate);r[std::size_t(i)]=inputs==1?l[std::size_t(i)]:testSignal(std::size_t(i+123),rate);b.setSample(0,i,l[std::size_t(i)]);b.setSample(1,i,r[std::size_t(i)]);}
            reference.processStereo(l.data(),r.data(),std::size_t(size));pump(p,b);
            for(int i=0;i<size;++i)require(std::abs(b.getSample(0,i)-l[std::size_t(i)])<1e-6f&&std::abs(b.getSample(1,i)-r[std::size_t(i)])<1e-6f,"adapter/core mismatch");
        }
        p.releaseResources();p.prepareToPlay(rate,512);juce::AudioBuffer<float> b(2,128);b.clear();b.setSample(0,0,std::numeric_limits<float>::quiet_NaN());b.setSample(1,1,std::numeric_limits<float>::infinity());pump(p,b);for(int c=0;c<2;++c)for(int i=0;i<128;++i)require(std::isfinite(b.getSample(c,i)),"invalid host sample");
    }
}
static void state(){
    Processor p;auto c=p.snapshot();c.move(0,2);for(auto& e:c.effects){e->bypass=true;if(e->type=="cabinet"){e->impulseResponse={1,.2f,-.1f};e->impulseSampleRate=44100;e->impulseName="embedded only.wav";}}
    require(p.install(std::move(c),"Saved preset"),"install failed");set(p,"amp_sim.oversampling",2);set(p,"gain.gain_db",3);set(p,"plugin.bypass",1);
    juce::MemoryBlock data;p.getStateInformation(data);Processor restored;restored.setStateInformation(data.getData(),int(data.getSize()));
    auto a=p.snapshot(),b=restored.snapshot();require(juce::JSON::toString(app::serializePreset(a))==juce::JSON::toString(app::serializePreset(b)),"state order/IR/parameters roundtrip");require(restored.parameters.getRawParameterValue("plugin.bypass")->load()==1,"global bypass restore");
    restored.setStateInformation("bad",3);auto after=restored.snapshot();require(juce::JSON::toString(app::serializePreset(after))==juce::JSON::toString(app::serializePreset(b)),"corrupt state replaced valid state");
    restored.prepareToPlay(48000,128);juce::AudioBuffer<float> buffer(2,512);buffer.clear();buffer.setSample(0,0,.5f);buffer.setSample(1,0,-.25f);pump(restored,buffer);
    require(std::abs(buffer.getSample(0,256)-.5f)<1e-6f&&std::abs(buffer.getSample(1,256)+.25f)<1e-6f,"latency-aligned global bypass");
    set(restored,"distortion.oversampling",2);restored.serviceConfiguration();buffer.clear();pump(restored,buffer);restored.refreshLatency();require(restored.getLatencySamples()==320,"deferred latency update");
}
static void automation(){
    Processor p;Chain c;c.effects.push_back(createEffect("gain","legacy_id"));require(p.install(std::move(c),"gain"),"gain preset");set(p,"gain.gain_db",6);p.prepareToPlay(48000,64);
    juce::AudioBuffer<float> b(2,512);for(int i=0;i<512;++i){b.setSample(0,i,.1f);b.setSample(1,i,-.2f);}pump(p,b);
    require(std::abs(b.getSample(0,511)-.1f*std::pow(10.f,.3f))<1e-6f,"stable host ID did not map legacy effect ID");
    juce::MidiBuffer midi;for(int k=0;k<100;++k){for(int i=0;i<512;++i){b.setSample(0,i,.1f);b.setSample(1,i,-.2f);}allocation_tracker::ProcessScope scope;p.processBlockBypassed(b,midi);}
    require(std::abs(b.getSample(0,511)-.1f)<1e-6f&&std::abs(b.getSample(1,511)+.2f)<1e-6f,"host bypass corrupted channels");
    auto json=app::serializePreset(p.snapshot());json.getDynamicObject()->setProperty("version",2);json.getDynamicObject()->removeProperty("schema");require(p.install(app::deserializePreset(json),"v2"),"v2 plugin preset");
    p.releaseResources();b.clear();pump(p,b);require(b.getMagnitude(0,512)==0,"release resources output");
}
static void concurrent(){Processor p;p.prepareToPlay(48000,128);std::atomic<bool> done{false};std::thread writer([&]{for(int i=0;i<100;++i){set(p,"gain.gain_db",float(i%12));set(p,"distortion.oversampling",float(i%3));juce::MemoryBlock data;p.getStateInformation(data);}done=true;});juce::AudioBuffer<float> b(2,128);for(int k=0;k<3000||!done;++k){b.clear();pump(p,b);}writer.join();}
static void alphaUX(){
    require(app::userPresetDirectory().getFullPathName().endsWith("Library/Application Support/Multi Effects/Presets"),"user data location");
    for(int i=0;i<5;++i){auto c=app::factoryPreset(i);auto restored=app::deserializePreset(app::serializePreset(c));
        require(app::presetFingerprint(c)==app::presetFingerprint(restored),"factory roundtrip");
        for(const auto& e:c.effects){require(e->impulseResponse.empty(),"factory contains unlicensed IR");for(const auto& p:e->parameters)require(p->target>=p->info.minimum&&p->target<=p->info.maximum,"factory parameter range");}
        c.prepare({48000,128,2});std::array<float,128> l{},r{};l[0]=.1f;r[0]=-.1f;c.processStereo(l.data(),r.data(),128);for(float v:l)require(std::isfinite(v),"factory processing");}
    Processor p;require(!p.presetModified(),"initial modified");
    auto editor=std::unique_ptr<juce::AudioProcessorEditor>(p.createEditor());
    juce::ListBox* list=nullptr;for(auto* child:editor->getChildren())if(auto* box=dynamic_cast<juce::ListBox*>(child))list=box;
    require(list&&list->getModel()->getNumRows()==9,"chain always available");
    auto controls=[&]{std::vector<ui::ParameterRow*> result;for(auto* child:editor->getChildren())if(auto* row=dynamic_cast<ui::ParameterRow*>(child))result.push_back(row);return result;};
    for(int index:{0,2,5,8,2}){list->selectRow(index);auto rows=controls();require(rows.size()==p.snapshot().effects[std::size_t(index)]->parameters.size(),"selected parameter panel size");}
    auto rows=controls();require(rows.size()==1&&rows[0]->info.id=="gain_db","selected gain binding");
    rows[0]->slider.setValue(3,juce::sendNotificationSync);require(p.parameters.getRawParameterValue("gain.gain_db")->load()==3&&p.presetModified(),"UI to host parameter");
    set(p,"gain.gain_db",-4);require(rows[0]->value()==p.parameters.getRawParameterValue("gain.gain_db")->load(),"host automation to UI");
    list->selectRow(5);rows=controls();require(rows.back()->info.requiresPrepare,"oversampling choice");rows.back()->choice.setSelectedId(3,juce::sendNotificationSync);require(p.parameters.getRawParameterValue("amp_sim.oversampling")->load()==2,"choice to stable host ID");
    require(ui::parameterText(rows.back()->info,2)=="4x","oversampling formatting");
    auto snapshot=p.snapshot();p.markPresetSaved("User test",app::presetFingerprint(snapshot));require(!p.presetModified(),"saved baseline");
    set(p,"gain.gain_db",1);juce::MemoryBlock data;p.getStateInformation(data);Processor restored;restored.setStateInformation(data.getData(),int(data.getSize()));require(restored.presetModified()&&restored.presetTitle()=="User test","modified state restore");
    auto root=juce::JSON::parse(juce::String::fromUTF8(static_cast<const char*>(data.getData()),int(data.getSize())));root.getDynamicObject()->removeProperty("preset_modified");const auto old=juce::JSON::toString(root);restored.setStateInformation(old.toRawUTF8(),int(old.getNumBytesAsUTF8()));require(!restored.presetModified(),"v06 state compatibility");
    editor.reset();auto duplicate=p.snapshot();duplicate.effects.push_back(createEffect("gain","duplicate"));require(p.install(std::move(duplicate),"Duplicates"),"duplicate install");editor.reset(p.createEditor());list=nullptr;for(auto* child:editor->getChildren())if(auto* box=dynamic_cast<juce::ListBox*>(child))list=box;
    require(list!=nullptr,"duplicate list");list->selectRow(9);rows=controls();require(rows.size()==1&&!rows[0]->isEnabled(),"duplicate must not edit first bank");
    std::cout<<"Alpha factory presets/shared controls/selection/automation/state PASS\n";
}
static void profile(){
    std::cout<<"factor,dsp_mean_us,dsp_max_us,adapter_mean_us,adapter_max_us,max_deadline_percent\n";
    for(int mode:{0,1,2}){Processor p;set(p,"distortion.oversampling",float(mode));set(p,"amp_sim.oversampling",float(mode));p.prepareToPlay(48000,128);auto c=p.snapshot();c.prepare({48000,128,2});juce::AudioBuffer<float> b(2,128);std::array<float,128> l{},r{};double ds=0,dm=0,as=0,am=0;
        for(int k=0;k<2100;++k){for(int i=0;i<128;++i){l[std::size_t(i)]=testSignal(std::size_t(k*128+i),48000);r[std::size_t(i)]=testSignal(std::size_t(k*128+i+53),48000);b.setSample(0,i,l[std::size_t(i)]);b.setSample(1,i,r[std::size_t(i)]);}
            auto start=std::chrono::steady_clock::now();c.processStereo(l.data(),r.data(),128);double d=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();start=std::chrono::steady_clock::now();pump(p,b);double a=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();if(k>=100){ds+=d;as+=a;dm=std::max(dm,d);am=std::max(am,a);}}
        std::cout<<(1<<mode)<<','<<ds/2000<<','<<dm<<','<<as/2000<<','<<am<<','<<am/2666.6667*100<<'\n';}
}
int main(int argc,char** argv){juce::ScopedJuceInitialiser_GUI init;try{if(argc>1&&std::string(argv[1])=="profile"){profile();return 0;}if(argc>1&&std::string(argv[1])=="alpha")alphaUX();else if(argc>1)concurrent();else{matrix();state();automation();}require(allocation_tracker::processAllocations==0&&allocation_tracker::processFrees==0,"plugin process allocated/freed");std::cout<<"Plugin lifecycle/state/stereo/automation/allocation PASS\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
