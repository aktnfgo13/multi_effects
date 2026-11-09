#include "PluginProcessor.h"
namespace multi_effects::plugin {
namespace {
Effect* first(Chain& c,const std::string& type) { for(auto& e:c.effects)if(e->type==type)return e.get();return nullptr; }
}
juce::AudioProcessorValueTreeState::ParameterLayout Processor::layout() {
    juce::AudioProcessorValueTreeState::ParameterLayout result;
    result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"plugin.bypass",1},"Plugin bypass",false));
    for(const auto& d:registry()) {
        const auto prefix=juce::String(d.type)+".";
        result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{prefix+"bypass",1},juce::String(d.name)+" bypass",false));
        for(const auto& p:d.parameters) {
            const auto id=juce::ParameterID{prefix+juce::String(p.id),1};
            const auto name=juce::String(d.name)+" / "+juce::String(p.name);
            if(p.requiresPrepare)result.add(std::make_unique<juce::AudioParameterChoice>(id,name,juce::StringArray{"1x","2x","4x"},int(p.initial)));
            else result.add(std::make_unique<juce::AudioParameterFloat>(id,name,juce::NormalisableRange<float>(p.minimum,p.maximum),p.initial,juce::AudioParameterFloatAttributes().withLabel(juce::String(p.unit))));
        }
    }
    result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"input.trim",2},"Input Trim",
        juce::NormalisableRange<float>(-InputReference::trimLimitDb,InputReference::trimLimitDb),0.f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));
    return result;
}
Processor::Processor():AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)),parameters(*this,nullptr,"HostParameters",layout()) {
    hostTrim=parameters.getRawParameterValue("input.trim");
    globalBypass=parameters.getRawParameterValue("plugin.bypass");
    for(const auto& d:registry()) {
        const auto prefix=juce::String(d.type)+".";
        mappings.push_back({d.type,"",prefix+"bypass",parameters.getRawParameterValue(prefix+"bypass"),true,false});
        for(const auto& p:d.parameters){const auto id=prefix+juce::String(p.id);mappings.push_back({d.type,p.id,id,parameters.getRawParameterValue(id),false,p.requiresPrepare});if(p.requiresPrepare)parameters.addParameterListener(id,this);}
    }
    for(const auto* type:{"noise_gate","compressor","gain","distortion","eq","amp_sim","cabinet","delay","reverb"})model.effects.push_back(createEffect(type,std::string(type)+"_1"));
    {auto initial=cloneConfiguration(model);applyTargets(initial);cleanFingerprint=app::presetFingerprint(initial);}
    worker=std::thread([this]{runWorker();});
}
Processor::~Processor() {
    quitting=true;worker.join();cancelPendingUpdate();
    for(const auto& m:mappings)if(m.staged)parameters.removeParameterListener(m.hostId,this);
    delete pending.exchange(nullptr);delete retired.exchange(nullptr);delete active;
}
bool Processor::isBusesLayoutSupported(const BusesLayout& l) const {
    return l.getMainOutputChannelSet()==juce::AudioChannelSet::stereo() && (l.getMainInputChannelSet()==juce::AudioChannelSet::mono()||l.getMainInputChannelSet()==juce::AudioChannelSet::stereo());
}
void Processor::applyTargets(Chain& c) {
    for(const auto& m:mappings)if(auto* e=first(c,m.type)){if(m.bypass)e->bypass=m.target->load();else if(auto* p=e->parameter(m.key))p->set(m.target->load());}
}
std::unique_ptr<Processor::RenderState> Processor::build(unsigned ticket) {
    auto r=std::make_unique<RenderState>();r->chain=cloneConfiguration(model);applyTargets(r->chain);r->chain.prepare(spec);
    r->hostDrive=first(r->chain,"custom_drive");
    for(const auto& m:mappings){auto* e=first(r->chain,m.type);r->bindings.push_back({e&&!m.bypass?e->parameter(m.key):nullptr,e});}
    for(const auto& e:r->chain.effects)r->latency+=e->latencySamples();
    if(r->latency>r->historyL.size())throw std::runtime_error("Unsupported plugin latency");
    r->dryL.resize(spec.maxBlockSize);r->dryR.resize(spec.maxBlockSize);r->coefficient=float(std::exp(-1.0/(spec.sampleRate*.02)));r->wet=globalBypass->load()>=.5f?0.f:1.f;r->revision=ticket;return r;
}
void Processor::prepareToPlay(double rate,int block) {
    std::lock_guard<std::mutex> lock(control);prepared=false;delete pending.exchange(nullptr);delete retired.exchange(nullptr);delete active;active=nullptr;
    spec={rate,std::size_t(std::max(1,block)),2};
    try {input.setTrimDb(hostTrim->load());input.prepare(rate);auto r=build(revision.load());active=r.release();builtRevision=active->revision;prepared=true;error.clear();activeLatency=int(active->latency);}
    catch(const std::exception& e){error=juce::String("Prepare failed: ")+e.what();activeLatency=0;}
    refreshLatency();
}
void Processor::releaseResources() {
    input.reset();
    grainReadback.store(-1.f,std::memory_order_relaxed);
    mixReadback.store(-1.f,std::memory_order_relaxed);
    headroomReadback.store(-1.f,std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(control);prepared=false;delete pending.exchange(nullptr);delete retired.exchange(nullptr);delete active;active=nullptr;
}
void Processor::serviceConfiguration() {
    std::lock_guard<std::mutex> lock(control);delete retired.exchange(nullptr);
    const unsigned ticket=revision.load();if(!prepared||installing.load()||ticket==builtRevision)return;
    try {auto next=build(ticket);if(ticket!=revision.load())return;delete pending.exchange(next.release());builtRevision=ticket;error.clear();}
    catch(const std::exception& e){error=juce::String("Configuration rejected: ")+e.what();builtRevision=ticket;}
}
void Processor::runWorker() {
    while(!quitting){serviceConfiguration();if(activeLatency.load()!=reportedLatency.load())triggerAsyncUpdate();std::this_thread::sleep_for(std::chrono::milliseconds(10));}
}
void Processor::refreshLatency(){const int value=activeLatency.load();setLatencySamples(value);reportedLatency=value;}
juce::AudioProcessorParameter* Processor::getBypassParameter() const {return parameters.getParameter("plugin.bypass");}
void Processor::processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer& midi){midi.clear();render(b,false);}
void Processor::processBlockBypassed(juce::AudioBuffer<float>& b,juce::MidiBuffer& midi){midi.clear();render(b,true);}
void Processor::render(juce::AudioBuffer<float>& b,bool forcedBypass) noexcept {
    juce::ScopedNoDenormals noDenormals;
    if(!retired.load())if(auto* next=pending.exchange(nullptr)){auto* old=active;active=next;retired.store(old);activeLatency=int(active->latency);appliedRevision=active->revision;}
    if(!active||b.getNumChannels()<2){b.clear();return;}
    if(getTotalNumInputChannels()==1)b.copyFrom(1,0,b,0,0,b.getNumSamples());
    auto& r=*active;
    for(std::size_t i=0;i<mappings.size();++i){const auto& m=mappings[i];auto binding=r.bindings[i];if(m.bypass&&binding.effect)binding.effect->bypass=m.target->load();else if(!m.staged&&binding.parameter)binding.parameter->set(m.target->load());}
    input.setTrimDb(hostTrim->load(std::memory_order_relaxed));
    float inPeak=0,outPeak=0;bool invalidIn=false,invalidOut=false;
    for(int pos=0;pos<b.getNumSamples();) {
        const int n=std::min<int>(int(r.dryL.size()),b.getNumSamples()-pos);auto* l=b.getWritePointer(0,pos);auto* right=b.getWritePointer(1,pos);
        for(int i=0;i<n;++i){if(!std::isfinite(l[i])){l[i]=0;invalidIn=true;}if(!std::isfinite(right[i])){right[i]=0;invalidIn=true;}
            inPeak=std::max({inPeak,std::abs(l[i]),std::abs(right[i])});r.dryL[std::size_t(i)]=r.latency?r.historyL[r.index]:l[i];r.dryR[std::size_t(i)]=r.latency?r.historyR[r.index]:right[i];
            if(r.latency){r.historyL[r.index]=l[i];r.historyR[r.index]=right[i];r.index=(r.index+1)%r.latency;}}
        input.process(l,right,std::size_t(n));
        r.chain.processStereo(l,right,std::size_t(n));
        for(int i=0;i<n;++i){const float target=forcedBypass||globalBypass->load()>=.5f?0.f:1.f;r.wet=target+r.coefficient*(r.wet-target);
            l[i]=r.dryL[std::size_t(i)]+r.wet*(l[i]-r.dryL[std::size_t(i)]);right[i]=r.dryR[std::size_t(i)]+r.wet*(right[i]-r.dryR[std::size_t(i)]);
            if(!std::isfinite(l[i])){l[i]=0;invalidOut=true;}if(!std::isfinite(right[i])){right[i]=0;invalidOut=true;}outPeak=std::max({outPeak,std::abs(l[i]),std::abs(right[i])});}pos+=n;
    }
    grainReadback.store(r.hostDrive?r.hostDrive->appliedGrain():-1.f,std::memory_order_relaxed);
    mixReadback.store(r.hostDrive?r.hostDrive->appliedMix():-1.f,std::memory_order_relaxed);
    headroomReadback.store(r.hostDrive?r.hostDrive->appliedHeadroom():-1.f,std::memory_order_relaxed);
    inputLevel.publish(inPeak,inPeak>=1,invalidIn);outputLevel.publish(outPeak,outPeak>=1,invalidOut);
}
Chain Processor::snapshot(){std::lock_guard<std::mutex> lock(control);auto c=cloneConfiguration(model);applyTargets(c);return c;}
bool Processor::install(Chain candidate,const juce::String& name) {
    std::unique_lock<std::mutex> lock(control);
    if(installing.load()){error="Configuration update already in progress";return false;}
    try {if(prepared)candidate.prepare(spec);}catch(const std::exception& e){error=juce::String("Preset rejected: ")+e.what();return false;}
    installing=true;lock.unlock();
    // Host notifications may reenter non-realtime state getters; do not hold control.
    // Only host-facing controls are updated; extra instances retain their preset values.
    for(const auto& m:mappings)if(auto* e=first(candidate,m.type)) {
        const float v=m.bypass?float(e->bypass.load()):e->parameter(m.key)->target.load();
        auto* p=parameters.getParameter(m.hostId);p->setValueNotifyingHost(p->convertTo0to1(v));
    }
    lock.lock();model.effects.swap(candidate.effects);if(name.isNotEmpty()){presetName=name;{auto initial=cloneConfiguration(model);applyTargets(initial);cleanFingerprint=app::presetFingerprint(initial);}restoredModified=false;}uiGeneration.fetch_add(1);revision.fetch_add(1);installing=false;error.clear();return true;
}
void Processor::getStateInformation(juce::MemoryBlock& dest) {
    std::lock_guard<std::mutex> lock(control);auto c=cloneConfiguration(model);applyTargets(c);auto root=std::make_unique<juce::DynamicObject>();root->setProperty("plugin_state",1);root->setProperty("preset",app::serializePreset(c));root->setProperty("bypass",globalBypass->load()>=.5f);
    root->setProperty("input_trim_db",hostTrim->load());
    root->setProperty("preset_name",presetName);
    root->setProperty("preset_modified",restoredModified||app::presetFingerprint(c)!=cleanFingerprint);
    const auto text=juce::JSON::toString(juce::var(root.release()));dest.replaceAll(text.toRawUTF8(),text.getNumBytesAsUTF8());
}
void Processor::setStateInformation(const void* data,int bytes) {
    try {if(!data||bytes<1||bytes>64*1024*1024)throw std::runtime_error("Invalid state size");juce::var root;
        if(juce::JSON::parse(juce::String::fromUTF8(static_cast<const char*>(data),bytes),root).failed())throw std::runtime_error("Invalid state JSON");
        if(int(root["plugin_state"])!=1||!root["bypass"].isBool())throw std::runtime_error("Unsupported plugin state");
        const auto storedTrim=root.getProperty("input_trim_db",0.f);
        if (!(storedTrim.isDouble() || storedTrim.isInt() || storedTrim.isInt64()) ||
            !std::isfinite(double(storedTrim)) || std::abs(double(storedTrim))>InputReference::trimLimitDb)
            throw std::runtime_error("Invalid input trim");
        auto c=app::deserializePreset(root["preset"]);if(!install(std::move(c),root["preset_name"].toString()))return;
        auto* trim=parameters.getParameter("input.trim");
        trim->setValueNotifyingHost(trim->convertTo0to1(float(storedTrim)));
        parameters.getParameter("plugin.bypass")->setValueNotifyingHost(bool(root["bypass"])?1.f:0.f);
        {std::lock_guard<std::mutex> lock(control);restoredModified=bool(root.getProperty("preset_modified",false));}
    }catch(const std::exception& e){std::lock_guard<std::mutex> lock(control);error=juce::String("State rejected: ")+e.what();}
}
bool Processor::presetModified(){std::lock_guard<std::mutex> lock(control);auto c=cloneConfiguration(model);applyTargets(c);return restoredModified||app::presetFingerprint(c)!=cleanFingerprint;}
juce::String Processor::presetTitle(){std::lock_guard<std::mutex> lock(control);return presetName;}
void Processor::markPresetSaved(const juce::String& name,std::uint64_t fingerprint){std::lock_guard<std::mutex> lock(control);presetName=name;cleanFingerprint=fingerprint;restoredModified=false;}
juce::String Processor::statusText(){std::lock_guard<std::mutex> lock(control);return error.isNotEmpty()?error:juce::String("Preset: ")+presetName;}
juce::String Processor::chainText(){std::lock_guard<std::mutex> lock(control);juce::String text;for(const auto& e:model.effects){if(text.isNotEmpty())text+=" > ";text+=juce::String(e->name);}return text;}
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new multi_effects::plugin::Processor();}
