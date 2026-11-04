#pragma once
#include "PresetIO.h"
#include <juce_data_structures/juce_data_structures.h>
namespace multi_effects::app
{
struct Preferences
{
    int input=0;
    bool stereo=false;
    juce::String deviceXml,lastPreset,presetFolder;
    static Preferences read(const juce::PropertySet& properties)
    {
        Preferences p;p.stereo=properties.getBoolValue("stereoInput",false);
        p.input=juce::jlimit(-1,255,properties.getIntValue("input",0));
        if (properties.getIntValue("input",0)<-1 || properties.getIntValue("input",0)>255) p.input=0;
        p.deviceXml=properties.getValue("deviceXml");
        if (!juce::parseXML(p.deviceXml)) p.deviceXml.clear();
        p.lastPreset=properties.getValue("lastPreset");p.presetFolder=properties.getValue("presetFolder");return p;
    }
    void write(juce::PropertySet& properties) const
    {
        properties.setValue("stereoInput",stereo);properties.setValue("settingsVersion",1);properties.setValue("input",input);
        properties.setValue("deviceXml",deviceXml);properties.setValue("lastPreset",lastPreset);properties.setValue("presetFolder",presetFolder);
    }
};
struct PresetSession
{
    juce::File file;
    bool dirty=false;
    juce::String displayName;
    juce::String title() const {return (file==juce::File()?(displayName.isEmpty()?juce::String("Untitled"):displayName):file.getFileNameWithoutExtension())+(dirty?" *":"");}
    void loaded(const juce::File& path) {file=path;displayName.clear();dirty=false;}
    void save(const Chain& chain,const juce::File& destination,bool overwriteApproved)
    {
        if(destination.existsAsFile()&&!overwriteApproved)throw std::runtime_error("Overwrite requires confirmation");
        savePreset(chain,destination);loaded(destination);
    }
};
}
