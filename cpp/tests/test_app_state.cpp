#include "../app/AppState.h"
#include "TestSupport.h"
#include <iostream>
using namespace multi_effects;
int main(){
    const auto dir=juce::File::getCurrentWorkingDirectory().getNonexistentChildFile("alpha-settings-test","",false);dir.createDirectory();
    struct Cleanup{juce::File dir;~Cleanup(){dir.deleteRecursively();}}cleanup{dir};
    try{
        juce::PropertiesFile::Options options;options.millisecondsBeforeSaving=-1;options.storageFormat=juce::PropertiesFile::storeAsXML;
        const auto file=dir.getChildFile("test.settings");
        {juce::PropertiesFile properties(file,options);app::Preferences p;p.input=1;p.deviceXml="<DEVICESETUP audioDeviceRate=\"48000\" audioDeviceBufferSize=\"128\"/>";p.lastPreset="example.json";p.presetFolder=dir.getFullPathName();p.write(properties);require(properties.saveIfNeeded(),"settings save");}
        {juce::PropertiesFile properties(file,options);auto p=app::Preferences::read(properties);require(p.input==1&&p.lastPreset=="example.json"&&p.deviceXml.contains("48000"),"settings restore");properties.setValue("input",999);properties.setValue("deviceXml","broken xml");p=app::Preferences::read(properties);require(p.input==0&&p.deviceXml.isEmpty(),"invalid settings fallback");}
        auto chain=makeTestChain();for(auto& e:chain.effects)if(e->type=="cabinet"){e->impulseResponse={1,.2f,-.1f};e->impulseName="My cabinet.wav";}
        app::PresetSession session;session.dirty=true;const auto a=dir.getChildFile("A.json"),b=dir.getChildFile("B.json");session.save(chain,a,false);require(session.file==a&&!session.dirty,"save as state");
        session.dirty=true;requireRejected([&]{session.save(chain,a,false);},"unapproved overwrite");require(session.dirty&&session.file==a,"failed save lost state");
        session.save(chain,a,true);const auto original=a.loadFileAsString();session.save(chain,b,false);require(a.loadFileAsString()==original&&session.file==b,"save as modified old file");
        auto restored=app::loadPreset(b);for(auto& e:restored.effects)if(e->type=="cabinet")require(e->impulseName=="My cabinet.wav"&&e->impulseResponse==std::vector<float>({1,.2f,-.1f}),"embedded IR metadata");
        session.dirty=true;requireRejected([&]{session.save(chain,dir.getChildFile("absent/failed.json"),false);},"failed save accepted");require(session.file==b&&session.dirty,"failed save changed current preset");
        std::cout<<"Settings disk roundtrip/fallback; Save/Save As/overwrite/failure state; IR metadata PASS\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
