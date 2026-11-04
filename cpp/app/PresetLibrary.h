#pragma once
#include "PresetIO.h"
#include <cstdint>
#include <cstring>

// UI / non-realtime services only. Factory presets are immutable recipes, never
// installed over user files. Custom IR audio remains embedded by PresetIO.
namespace multi_effects::app {
inline juce::File userPresetDirectory() {
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Application Support/Multi Effects/Presets");
}
inline juce::StringArray factoryNames() { return {"Clean", "Crunch", "High Gain", "Ambient", "Lead"}; }
inline Chain factoryPreset(int index) {
    if(index<0||index>=factoryNames().size())throw std::invalid_argument("Unknown factory preset");
    Chain c;
    for(const auto* type:{"noise_gate","compressor","gain","distortion","eq","amp_sim","cabinet","delay","reverb"})
        c.effects.push_back(createEffect(type,std::string(type)+"_1"));
    const auto set=[&](const char* type,const char* key,float value){for(auto& e:c.effects)if(e->type==type)e->parameter(key)->set(value);};
    const auto off=[&](const char* type){for(auto& e:c.effects)if(e->type==type)e->bypass=true;};
    // Conservative demonstration settings, not claims of a captured amp/cabinet.
    set("amp_sim","oversampling",1);set("distortion","oversampling",1);
    set("amp_sim","output_db",-12);off("distortion");
    if(index==0){set("amp_sim","gain_db",0);set("amp_sim","drive",.3f);off("delay");set("reverb","mix",.1f);}
    if(index==1){set("amp_sim","gain_db",9);set("amp_sim","drive",1.2f);off("delay");}
    if(index==2){set("amp_sim","gain_db",20);set("amp_sim","drive",2.5f);off("delay");set("reverb","mix",.08f);}
    if(index==3){set("amp_sim","gain_db",2);set("amp_sim","drive",.5f);set("delay","delay_ms",550);set("delay","mix",.3f);set("reverb","mix",.35f);}
    if(index==4){set("amp_sim","gain_db",16);set("amp_sim","drive",1.8f);set("delay","mix",.18f);set("delay","delay_ms",320);}
    return c;
}
struct PresetEntry {
    juce::String label;
    juce::File file;
    int factory=-1;
    Chain load() const {return factory>=0?factoryPreset(factory):loadPreset(file);}
};
inline std::vector<PresetEntry> presetEntries(const juce::File& extra={}) {
    std::vector<PresetEntry> entries;const auto names=factoryNames();
    for(int i=0;i<names.size();++i)entries.push_back({"Factory / "+names[i],{},i});
    const auto append=[&](const juce::File& folder){auto files=folder.findChildFiles(juce::File::findFiles,false,"*.json");files.sort();for(const auto& f:files)entries.push_back({"User / "+f.getFileNameWithoutExtension(),f,-1});};
    append(userPresetDirectory());if(extra!=juce::File()&&extra!=userPresetDirectory())append(extra);
    return entries;
}
// Exact configuration fingerprint for modified indicators, never on audio thread.
// Excludes display name; includes order, IDs, target values and embedded asset bytes.
inline std::uint64_t presetFingerprint(const Chain& c) {
    std::uint64_t h=14695981039346656037ull;
    const auto bytes=[&](const void* p,std::size_t n){auto* b=static_cast<const unsigned char*>(p);while(n--){h^=*b++;h*=1099511628211ull;}};
    const auto str=[&](const std::string& s){bytes(s.data(),s.size());const char end=0;bytes(&end,1);};
    for(const auto& e:c.effects){str(e->id);str(e->type);const bool bypass=e->bypass.load();bytes(&bypass,sizeof(bypass));
        for(const auto& p:e->parameters){str(p->info.id);const float v=p->target.load();bytes(&v,sizeof(v));}
        str(e->impulseName);bytes(&e->impulseSampleRate,sizeof(e->impulseSampleRate));bytes(e->impulseResponse.data(),e->impulseResponse.size()*sizeof(float));}
    return h;
}
}
