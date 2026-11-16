#include "AllocationTracker.h"
#include "TestSupport.h"
#include "../plugin/PluginProcessor.h"
#include "../ui/AlphaUI.h"
#include "../app/Runtime.h"
#include <iostream>
#include <chrono>
using namespace multi_effects;
using plugin::Processor;
static void set(Processor& p,const char* id,float value){auto* parameter=p.parameters.getParameter(id);require(parameter!=nullptr,"missing host parameter");parameter->setValueNotifyingHost(parameter->convertTo0to1(value));}
static void pump(Processor& p,juce::AudioBuffer<float>& b){juce::MidiBuffer midi;allocation_tracker::ProcessScope scope;p.processBlock(b,midi);}
static void layout(Processor& p,int inputs){auto l=p.getBusesLayout();l.inputBuses.set(0,inputs==1?juce::AudioChannelSet::mono():juce::AudioChannelSet::stereo());l.outputBuses.set(0,juce::AudioChannelSet::stereo());require(p.setBusesLayout(l),"host layout rejected");}
static void inputTrim() {
    Processor p; Chain empty;
    require(p.install(std::move(empty),"Input test"),"empty input chain");
    layout(p,1);p.prepareToPlay(48000,128);
    juce::AudioBuffer<float> b(2,960);
    auto fill=[&]{for(int i=0;i<960;++i){b.setSample(0,i,.25f);b.setSample(1,i,-.9f);}};
    fill();pump(p,b);
    require(b.getSample(0,959)==.25f && b.getSample(1,959)==.25f,"mono routing unity");
    set(p,"input.trim",6.f);fill();pump(p,b);
    const float expected=.25f*std::pow(10.f,.3f);
    require(std::abs(b.getSample(0,959)-expected)<1e-6f && b.getSample(1,959)==b.getSample(0,959),"host trim");
    fill();pump(p,b);require(std::abs(p.input.rms.load()-expected)<1e-6f,"post-trim RMS");
    juce::MemoryBlock data;p.getStateInformation(data);
    Processor restored;restored.setStateInformation(data.getData(),int(data.getSize()));
    require(std::abs(restored.parameters.getRawParameterValue("input.trim")->load()-6)<1e-5f,"trim restore");
    auto state=juce::JSON::parse(juce::String::fromUTF8(static_cast<const char*>(data.getData()),int(data.getSize())));
    state.getDynamicObject()->removeProperty("input_trim_db");auto legacy=juce::JSON::toString(state);
    restored.setStateInformation(legacy.toRawUTF8(),int(legacy.getNumBytesAsUTF8()));
    require(restored.parameters.getRawParameterValue("input.trim")->load()==0,"legacy state unity");
    set(p,"plugin.bypass",1);for(int i=0;i<40;++i){fill();pump(p,b);}
    require(b.getSample(0,959)==.25f,"raw global bypass");
}
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
static void customDriveState(){
    Processor p;auto chain=p.snapshot();chain.effects.insert(chain.effects.begin(),createEffect("custom_drive","custom-instance"));
    require(p.install(std::move(chain),"Custom Drive test"),"custom install");set(p,"custom_drive.drive",75);set(p,"custom_drive.grain",66);set(p,"custom_drive.oversampling",2);
    p.prepareToPlay(48000,128);require(p.getLatencySamples()==192,"custom chain latency");
    auto editor=std::unique_ptr<juce::AudioProcessorEditor>(p.createEditor());int controls=0;for(auto* child:editor->getChildren())if(auto* row=dynamic_cast<ui::ParameterRow*>(child)){++controls;require(row->getRight()<=editor->getWidth()&&row->getBottom()<478,"custom panel bounds");}
    require(controls==9,"custom selected controls");
    juce::MemoryBlock state;p.getStateInformation(state);Processor restored;restored.setStateInformation(state.getData(),int(state.getSize()));
    auto c=restored.snapshot();require(c.effects.front()->id=="custom-instance"&&c.effects.front()->parameter("grain")->target==66,"custom state IDs/parameter restore");
    restored.prepareToPlay(48000,128);
    juce::AudioBuffer<float> probe(2,128);probe.clear();pump(restored,probe);
    require(restored.grainReadback.load()==66,"restored DSP grain");
    for(float value:{0.f,50.f,100.f}) {
        set(restored,"custom_drive.grain",value);
        set(restored,"custom_drive.tight",value);
        for(int n=0;n<375;++n){probe.clear();pump(restored,probe);}
        require(std::abs(restored.grainReadback.load()-value)<.01f,"restored live DSP grain");
        require(std::abs(restored.tightReadback.load()-value)<.01f,"restored live DSP Tight");
        std::cout<<"Plugin rendered Grain "<<value<<" -> "<<restored.grainReadback.load()<<'\n';
    }
    for(float h:{0.f,100.f}) {
        set(restored,"custom_drive.headroom",h);
        for(int n=0;n<375;++n){probe.clear();pump(restored,probe);}
        require(std::abs(restored.headroomReadback.load()-h)<.01f,"restored live DSP headroom");
    }
    auto preset=app::deserializePreset(app::serializePreset(c));require(preset.effects.front()->type=="custom_drive","custom preset schema");
    juce::AudioBuffer<float> b(2,128);for(int k=0;k<1000;++k){set(p,"custom_drive.drive",float(k%101));b.clear();pump(p,b);}
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
// Same live Runtime objects used by Standalone: never reset/rebuild on edits.
static void customDriveRealtime() {
    for(int mode:{0,1,2})for(const char* key:{"drive","tone","crunch","headroom","grain"}) {
        app::Runtime live,held;
        const auto setup=[&](app::Runtime& r){r.chain.effects.push_back(createEffect("custom_drive","live-drive"));
            r.chain.effects.front()->parameter(key)->set(0);r.chain.effects.front()->parameter("oversampling")->set(float(mode));
            r.io.stereoInput=true;require(r.prepare({48000,128,2},{0,1}),"runtime prepare");r.io.muted=false;};
        setup(live);setup(held);
        auto* parameter=live.chain.effects.front()->parameter(key);
        ui::ParameterRow row(parameter->info);row.setValue(0);
        // Standalone captures stable instance + parameter IDs, resolving the live chain.
        row.onChange=[&live,id=std::string("live-drive"),param=std::string(key)](float value){
            std::lock_guard<std::mutex> lock(live.control);
            for(auto& e:live.chain.effects)if(e->id==id)e->parameter(param)->set(value);
        };
        std::array<float,128> inL{},inR{},outL{},outR{},refL{},refR{};
        const float* inputs[]{inL.data(),inR.data()};float* outputs[]{outL.data(),outR.data()};float* reference[]{refL.data(),refR.data()};
        double deltaL=0,deltaR=0,energyL=0,energyR=0;
        for(int block=0;block<1125;++block){
            if(block==375)row.slider.setValue(100,juce::sendNotificationSync);
            for(int i=0;i<128;++i){double t=(block*128+i)/48000.;inL[i]=float(.24*std::sin(6.283185307179586*440*t)+.06*std::sin(6.283185307179586*2300*t));inR[i]=float(.17*std::sin(6.283185307179586*337*t));}
            {allocation_tracker::ProcessScope scope;live.process({inputs,2,outputs,2,128});held.process({inputs,2,reference,2,128});}
            for(int i=0;i<128;++i){
                require(std::isfinite(outL[i])&&std::isfinite(outR[i]),"live output nonfinite");
                if(block<375)require(outL[i]==refL[i]&&outR[i]==refR[i],"baseline mismatch");
                if(block>=1000){deltaL+=std::pow(outL[i]-refL[i],2);deltaR+=std::pow(outR[i]-refR[i],2);energyL+=refL[i]*refL[i];energyR+=refR[i]*refR[i];}
            }
        }
        require(parameter->target==100,"GUI target not delivered");
        const double relativeL=std::sqrt(deltaL/energyL),relativeR=std::sqrt(deltaR/energyR);
        require(relativeL>.01&&relativeR>.01,"live macro failed to change both output lanes");
        std::cout<<"Standalone "<<key<<" "<<(1<<mode)<<"x waveform delta/ref RMS L="<<relativeL<<" R="<<relativeR<<'\n';
    }
    // Actual PluginEditor bindings, not a stand-in for host/GUI propagation.
    for(const char* key:{"drive","tone","crunch","headroom","grain"}) {
        Processor live,held;
        const auto setup=[&](Processor& p){Chain c;c.effects.push_back(createEffect("custom_drive","plugin-drive"));c.effects.front()->parameter(key)->set(0);require(p.install(std::move(c),"Propagation"),"plugin install");p.prepareToPlay(48000,128);};
        setup(live);setup(held);auto editor=std::unique_ptr<juce::AudioProcessorEditor>(live.createEditor());
        ui::ParameterRow* control=nullptr;for(auto* child:editor->getChildren())if(auto* row=dynamic_cast<ui::ParameterRow*>(child))if(row->info.id==key)control=row;
        require(control!=nullptr,"missing editor macro");juce::AudioBuffer<float> a(2,128),b(2,128);double delta=0,energy=0;
        for(int block=0;block<1125;++block){
            if(block==375)control->slider.setValue(100,juce::sendNotificationSync);
            for(int i=0;i<128;++i){float x=float(.3*std::sin(6.283185307179586*440*(block*128+i)/48000.));a.setSample(0,i,x);b.setSample(0,i,x);a.setSample(1,i,0);b.setSample(1,i,0);}
            pump(live,a);pump(held,b);
            for(int i=0;i<128;++i){require(a.getSample(1,i)==0,"live stereo leakage");if(block>=1000){delta+=std::pow(a.getSample(0,i)-b.getSample(0,i),2);energy+=std::pow(b.getSample(0,i),2);}}
        }
        require(std::sqrt(delta/energy)>.01,"plugin editor failed to change output");
        std::cout<<"Plugin "<<key<<" waveform delta/ref RMS="<<std::sqrt(delta/energy)<<'\n';
    }
}
int main(int argc,char** argv){juce::ScopedJuceInitialiser_GUI init;try{if(argc>1&&std::string(argv[1])=="profile"){profile();return 0;}if(argc>1&&std::string(argv[1])=="input")inputTrim();else if(argc>1&&std::string(argv[1])=="drive-propagation")customDriveRealtime();else if(argc>1&&std::string(argv[1])=="grain-state")customDriveState();else if(argc>1&&std::string(argv[1])=="alpha")alphaUX();else if(argc>1)concurrent();else{matrix();state();automation();customDriveState();}require(allocation_tracker::processAllocations==0&&allocation_tracker::processFrees==0,"plugin process allocated/freed");std::cout<<"Plugin lifecycle/state/stereo/automation/allocation PASS\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
