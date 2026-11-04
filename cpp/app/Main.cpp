#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "Runtime.h"
#include "AppState.h"
#include "PresetLibrary.h"
#include "../ui/AlphaUI.h"
#include "ProductInfo.h"
using namespace multi_effects;

class MainComponent final : public juce::Component, private juce::AudioIODeviceCallback,
                            private juce::ListBoxModel, private juce::Timer
{
public:
    MainComponent() : list("Effects",this), devices(manager,0,256,0,256,false,false,true,false)
    {
        juce::PropertiesFile::Options options;
        options.applicationName="MultiEffectsStandalone";options.filenameSuffix="settings";
        options.folderName="Multi Effects";options.osxLibrarySubFolder="Application Support";
        options.millisecondsBeforeSaving=1000;
        settings.setStorageParameters(options);
        preferences=multi_effects::app::Preferences::read(*settings.getUserSettings());
        runtime.preferredInput=preferences.input;runtime.io.stereoInput=preferences.stereo;
        lastGoodXml=preferences.deviceXml;
        presetFolder=juce::File(preferences.presetFolder);
        if(!presetFolder.isDirectory()) presetFolder=multi_effects::app::userPresetDirectory();
        for(const auto& type:{"noise_gate","compressor","gain","distortion","eq","amp_sim","cabinet","delay","reverb"})
            runtime.chain.effects.push_back(createEffect(type,std::string(type)+"_1"));
        if(preferences.lastPreset.isNotEmpty())
        {
            try {auto file=juce::File(preferences.lastPreset);auto candidate=multi_effects::app::loadPreset(file);candidate.prepare(runtime.spec);runtime.chain.effects.swap(candidate.effects);session.loaded(file);}
            catch(const std::exception& e){notice("Warning: Last preset unavailable: "+juce::String(e.what()),1);}
        }
        addAndMakeVisible(list);addAndMakeVisible(devices);
        for(auto* b:{&up,&down,&remove,&add,&save,&saveAs,&load,&loadSelected,&irLoad,&reconnect})addAndMakeVisible(*b);
        for(auto* label:{&title,&status,&levels,&audioStatus,&latencyStatus,&presetTitle,&irStatus,&deviceWarning})addAndMakeVisible(*label);
        for(auto* combo:{&types,&inputSelector,&presetList})addAndMakeVisible(*combo);
        addAndMakeVisible(stereoInput);addAndMakeVisible(mute);addAndMakeVisible(bypass);
        int n=1;for(const auto& d:registry())types.addItem(juce::String(d.name),n++);types.setSelectedId(1);
        mute.setToggleState(true,juce::dontSendNotification);
        stereoInput.setTooltip("Use selected physical input as L and the next input as R; missing R is silent.");
        stereoInput.setToggleState(preferences.stereo,juce::dontSendNotification);
        stereoInput.onClick=[this]{
            runtime.io.stereoInput=stereoInput.getToggleState();
            inputSelector.onChange();persist();
        };
        mute.onClick=[this]{runtime.io.muted=mute.getToggleState();};
        bypass.onClick=[this]{std::lock_guard<std::mutex> lock(runtime.control);if(auto* e=selectedUnsafe()){e->bypass=bypass.getToggleState();session.dirty=true;list.repaint();}};
        up.onClick=[this]{move(-1);};down.onClick=[this]{move(1);};
        remove.onClick=[this]{
            const auto id=selectedId;
            juce::AlertWindow::showAsync(juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::QuestionIcon)
                .withTitle("Remove effect?").withMessage("Remove the selected effect from this preset?").withButton("Remove").withButton("Cancel"),
                [safe=juce::Component::SafePointer<MainComponent>(this),id](int result){if(safe&&result==1){safe->edit([&](Chain& c){c.effects.erase(std::remove_if(c.effects.begin(),c.effects.end(),[&](const auto& e){return e->id==id;}),c.effects.end());});safe->refresh();}});
        };
        add.onClick=[this]{const auto type=registry().at(std::size_t(types.getSelectedId()-1)).type;edit([&](Chain& c){if(c.effects.size()>=24)throw std::runtime_error("Maximum 24 effects");c.effects.push_back(createEffect(type,juce::Uuid().toString().toStdString()));});refresh();};
        irLoad.onClick=[this]{chooseIR();};
        save.onClick=[this]{if(session.file==juce::File())chooseSaveAs();else saveTo(session.file,true);};
        saveAs.onClick=[this]{chooseSaveAs();};
        load.onClick=[this]{confirmDiscard([this]{chooseLoad();});};
        loadSelected.onClick=[this]{const int i=presetList.getSelectedId()-1;if(i>=0&&i<int(presetFiles.size())){const auto entry=presetFiles[std::size_t(i)];confirmDiscard([this,entry]{if(entry.factory<0){loadPreset(entry.file);return;}try{if(install(entry.load())){session.loaded({});session.displayName=entry.label;refresh();notice("Info: Factory preset loaded; Save creates a user copy.",0);}}catch(const std::exception& e){notice(e.what(),2);}});}};
        presetList.onChange=[this]{loadSelected.setEnabled(presetList.getSelectedId()>0);};
        reconnect.onClick=[this]{rollbackAttempted=false;reconnectDevice();};
        inputSelector.onChange=[this]{
            const int channel=inputSelector.getSelectedId()-1;auto* device=manager.getCurrentAudioDevice();
            if(!device||channel<0||channel>=device->getInputChannelNames().size())return;
            const int right=runtime.io.stereoInput?channel+1:channel;
            if(!device->getActiveInputChannels()[channel] || (right<device->getInputChannelNames().size()&&!device->getActiveInputChannels()[right])){
                auto setup=manager.getAudioDeviceSetup();setup.useDefaultInputChannels=false;setup.inputChannels.setBit(channel);if(right<device->getInputChannelNames().size())setup.inputChannels.setBit(right);
                const auto error=manager.setAudioDeviceSetup(setup,true);if(error.isNotEmpty()){notice("Error: Input configuration: "+error,2);return;}
            }
            runtime.preferredInput=channel;runtime.io.selectedInput=channel;persist();
        };
        deviceWarning.setText("Audio device | Test tone bypasses DSP Mute, meters and output clamp",juce::dontSendNotification);
        deviceWarning.setColour(juce::Label::textColourId,juce::Colours::orange);
        status.setMinimumHorizontalScale(.8f);
        setSize(1100,850);refresh();refreshPresets();
        // Persist a crash/hang recovery marker BEFORE entering a potentially blocking
        // native device API. A later launch can still open the UI without reopening it.
        if(settings.getUserSettings()->getBoolValue("audioInitialising",false))
            notice("Warning: Previous audio start did not finish. Choose a device or use Reconnect audio.",1);
        else {
            markAudioInitialising(true);
            auto xml=juce::parseXML(lastGoodXml);
            juce::AudioDeviceManager::AudioDeviceSetup preferred;preferred.sampleRate=48000;preferred.bufferSize=128;
            auto error=manager.initialise(2,2,xml.get(),true,{},xml?nullptr:&preferred);
            if(error.isNotEmpty())error=manager.initialise(2,2,nullptr,true);
            markAudioInitialising(false);
            if(error.isNotEmpty())notice("Error: Audio device unavailable: "+error,2);
            else if(status.getText().isEmpty())notice("Info: DSP output starts muted. Select input, then unmute.",0);
        }
        manager.addAudioCallback(this);startTimerHz(10);
    }
    ~MainComponent() override
    {
        stopTimer();persist();manager.removeAudioCallback(this);manager.closeAudioDevice();settings.saveIfNeeded();
    }
    void requestQuit(){confirmDiscard([]{juce::JUCEApplication::getInstance()->quit();});}
    void resized() override
    {
        const int w=std::max(1050,getWidth()),h=std::max(820,getHeight());
        title.setBounds(15,7,w-370,28);reconnect.setBounds(w-350,7,150,28);mute.setBounds(w-190,7,175,28);
        presetTitle.setBounds(15,42,210,28);presetList.setBounds(235,42,w-755,28);
        loadSelected.setBounds(w-510,42,115,28);save.setBounds(w-385,42,70,28);saveAs.setBounds(w-305,42,95,28);load.setBounds(w-200,42,185,28);
        list.setBounds(15,82,265,220);up.setBounds(15,312,80,28);down.setBounds(100,312,80,28);remove.setBounds(185,312,95,28);
        types.setBounds(15,350,180,28);add.setBounds(200,350,80,28);
        bypass.setBounds(305,80,240,28);irLoad.setBounds(w-225,80,210,28);
        for(std::size_t i=0;i<rows.size();++i)rows[i]->setBounds(305,112+int(i)*60,w-320,56);
        irStatus.setBounds(305,352,w-320,28);
        deviceWarning.setBounds(15,382,w-30,24);devices.setBounds(15,410,w-30,h-545);
        inputSelector.setBounds(15,h-125,265,28);stereoInput.setBounds(290,h-125,170,28);audioStatus.setBounds(465,h-125,w-480,28);
        latencyStatus.setBounds(15,h-94,w-30,25);levels.setBounds(15,h-66,w-30,25);status.setBounds(15,h-36,w-30,30);
    }
private:
    void notice(const juce::String& text,int severity)
    {
        status.setText(text,juce::dontSendNotification);
        status.setColour(juce::Label::textColourId,severity==2?juce::Colours::salmon:severity==1?juce::Colours::orange:juce::Colours::lightgrey);
    }
    Effect* selectedUnsafe(){for(auto& e:runtime.chain.effects)if(e->id==selectedId)return e.get();return nullptr;}
    void confirmDiscard(std::function<void()> action)
    {
        if(!session.dirty){action();return;}
        juce::AlertWindow::showAsync(juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::WarningIcon)
            .withTitle("Unsaved preset changes").withMessage("Save first to keep changes. Discard them and continue?").withButton("Discard").withButton("Cancel"),
            [safe=juce::Component::SafePointer<MainComponent>(this),action=std::move(action)](int result){if(safe&&result==1)action();});
    }
    bool install(Chain candidate)
    {
        // Preflight independent DSP while the existing callback continues untouched.
        ProcessSpec spec;{std::lock_guard<std::mutex> lock(runtime.control);spec=runtime.spec;}
        try{candidate.prepare(spec);}catch(const std::exception& e){notice("Error: Configuration rejected; previous preset retained: "+juce::String(e.what()),2);return false;}
        manager.removeAudioCallback(this);
        const bool ok=runtime.replace(std::move(candidate));juce::String error;
        {std::lock_guard<std::mutex> lock(runtime.control);error=runtime.error;}
        manager.addAudioCallback(this);
        if(!ok)notice("Error: "+error,2);
        return ok;
    }
    bool edit(const std::function<void(Chain&)>& fn)
    {
        try{
            Chain candidate;{std::lock_guard<std::mutex> lock(runtime.control);candidate=cloneConfiguration(runtime.chain);}
            fn(candidate);if(!install(std::move(candidate)))return false;session.dirty=true;return true;
        }catch(const std::exception& e){notice("Error: Edit failed; previous preset retained: "+juce::String(e.what()),2);return false;}
    }
    void move(int delta)
    {
        const int from=list.getSelectedRow(),to=from+delta;
        if(from<0||to<0||to>=getNumRows())return;
        if(edit([&](Chain& c){c.move(std::size_t(from),std::size_t(to));})){list.updateContent();list.selectRow(to);selectedRowsChanged(to);}
    }
    void refresh(){const int row=list.getSelectedRow();list.updateContent();const int count=getNumRows();list.selectRow(count?juce::jlimit(0,count-1,row):-1);selectedRowsChanged(list.getSelectedRow());}
    int getNumRows() override {std::lock_guard<std::mutex> lock(runtime.control);return int(runtime.chain.effects.size());}
    void paintListBoxItem(int row,juce::Graphics& g,int width,int height,bool sel) override
    {
        if(sel)g.fillAll(juce::Colours::darkslateblue);std::lock_guard<std::mutex> lock(runtime.control);
        if(row>=0&&row<int(runtime.chain.effects.size())){auto& e=*runtime.chain.effects[std::size_t(row)];g.setColour(e.bypass?juce::Colours::grey:juce::Colours::white);g.drawText(multi_effects::ui::effectText(e,row),8,0,width-10,height,juce::Justification::centredLeft);}
    }
    void selectedRowsChanged(int row) override
    {
        struct Control { ParameterInfo info; float value; };
        std::vector<Control> controls;bool exists=false,isCabinet=false,isBypassed=false;int count=0;juce::String irText;
        {
            std::lock_guard<std::mutex> lock(runtime.control);
            irText=multi_effects::ui::cabinetText(runtime.chain);
            count=int(runtime.chain.effects.size());selectedId=row>=0&&row<count?runtime.chain.effects[std::size_t(row)]->id:"";
            if(auto* effect=selectedUnsafe()){
                exists=true;isCabinet=effect->type=="cabinet";isBypassed=effect->bypass.load();
                if(isCabinet)irText=effect->impulseResponse.empty()?"IR: Synthetic default (not a cabinet capture)":"IR: Custom / embedded | "+juce::String(effect->impulseName.empty()?"unnamed":effect->impulseName);
                for(auto& p:effect->parameters)controls.push_back({p->info,p->target.load()});
            }
        }
        // Do not hold the lifecycle/control mutex while entering GUI/device widgets.
        rows.clear();bypass.setEnabled(exists);irLoad.setEnabled(isCabinet);remove.setEnabled(exists);
        up.setEnabled(row>0);down.setEnabled(row>=0&&row+1<count);add.setEnabled(count<24);
        irStatus.setText(irText,juce::dontSendNotification);bypass.setToggleState(isBypassed,juce::dontSendNotification);
        for(auto& p:controls){
            auto control=std::make_unique<multi_effects::ui::ParameterRow>(p.info);control->setValue(p.value);
            const auto effectId=selectedId,paramId=p.info.id;const bool prepare=p.info.requiresPrepare;
            control->onChange=[this,effectId,paramId,prepare](float value){
                if(prepare){if(!edit([&](Chain& c){for(auto& e:c.effects)if(e->id==effectId)e->parameter(paramId)->set(value);})){
                    // The control may currently be dispatching a notification; refresh later.
                    juce::MessageManager::callAsync([safe=juce::Component::SafePointer<MainComponent>(this)]{if(safe)safe->refresh();});}}
                else {std::lock_guard<std::mutex> lock(runtime.control);for(auto& e:runtime.chain.effects)if(e->id==effectId){e->parameter(paramId)->set(value);session.dirty=true;}}
            };addAndMakeVisible(*control);rows.push_back(std::move(control));
        }
        resized();
    }
    void loadPreset(const juce::File& file)
    {
        try{auto candidate=multi_effects::app::loadPreset(file);if(!install(std::move(candidate)))return;session.loaded(file);presetFolder=file.getParentDirectory();refresh();refreshPresets();persist();notice("Info: Preset loaded: "+file.getFileName(),0);}
        catch(const std::exception& e){notice("Error: Preset load failed; previous preset retained: "+juce::String(e.what()),2);}
    }
    void refreshPresets()
    {
        presetFiles=multi_effects::app::presetEntries(presetFolder);presetList.clear(juce::dontSendNotification);
        for(std::size_t i=0;i<presetFiles.size();++i){const auto& entry=presetFiles[i];presetList.addItem(entry.label,int(i+1));if(entry.file!=juce::File()&&entry.file==session.file)presetList.setSelectedId(int(i+1),juce::dontSendNotification);}
        presetList.setTextWhenNothingSelected("Factory / User presets");loadSelected.setEnabled(presetList.getSelectedId()>0);
    }
    void saveTo(const juce::File& file,bool approved)
    {
        try{Chain snapshot;{std::lock_guard<std::mutex> lock(runtime.control);snapshot=cloneConfiguration(runtime.chain);}session.save(snapshot,file,approved);presetFolder=file.getParentDirectory();refreshPresets();persist();notice("Info: Preset saved: "+file.getFileName(),0);}
        catch(const std::exception& e){notice("Error: Preset save failed: "+juce::String(e.what()),2);}
    }
    void chooseSaveAs()
    {
        if(session.file==juce::File())presetFolder=multi_effects::app::userPresetDirectory();
        if(!presetFolder.isDirectory()){auto result=presetFolder.createDirectory();if(result.failed()){notice("Error: "+result.getErrorMessage(),2);return;}}
        chooser=std::make_unique<juce::FileChooser>("Save preset as",presetFolder.getChildFile(session.file==juce::File()?"Untitled.json":session.file.getFileName()),"*.json");
        chooser->launchAsync(juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::saveMode,
            [safe=juce::Component::SafePointer<MainComponent>(this)](const juce::FileChooser& fc){if(!safe||fc.getResult()==juce::File())return;const auto file=fc.getResult().withFileExtension("json");
                if(file.existsAsFile())juce::AlertWindow::showAsync(juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::WarningIcon).withTitle("Replace preset?").withMessage(file.getFullPathName()).withButton("Replace").withButton("Cancel"),[safe,file](int r){if(safe&&r==1)safe->saveTo(file,true);});
                else safe->saveTo(file,false);
            });
    }
    void chooseLoad()
    {
        chooser=std::make_unique<juce::FileChooser>("Open preset",presetFolder,"*.json");
        chooser->launchAsync(juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::openMode,[safe=juce::Component::SafePointer<MainComponent>(this)](const juce::FileChooser& fc){if(safe&&fc.getResult()!=juce::File())safe->loadPreset(fc.getResult());});
    }
    void chooseIR()
    {
        const auto id=selectedId;chooser=std::make_unique<juce::FileChooser>("Load cabinet WAV IR (up to 32768 samples)",juce::File(),"*.wav");
        chooser->launchAsync(juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::openMode,[safe=juce::Component::SafePointer<MainComponent>(this),id](const juce::FileChooser& fc){
            if(!safe||fc.getResult()==juce::File())return;
            try{auto asset=multi_effects::app::loadImpulse(fc.getResult());if(safe->edit([&](Chain& c){for(auto& e:c.effects)if(e->id==id){e->impulseResponse=asset.samples;e->impulseSampleRate=asset.sampleRate;e->impulseName=fc.getResult().getFileName().toStdString();}})){safe->refresh();safe->notice("Info: IR loaded and embedded: "+fc.getResult().getFileName(),0);}}
            catch(const std::exception& e){safe->notice("Error: Invalid IR; previous IR retained: "+juce::String(e.what()),2);}
        });
    }
    void persist()
    {
        preferences.stereo=runtime.io.stereoInput.load();preferences.input=runtime.preferredInput.load();preferences.lastPreset=session.file==juce::File()?juce::String():session.file.getFullPathName();preferences.presetFolder=presetFolder.getFullPathName();preferences.deviceXml=lastGoodXml;preferences.write(*settings.getUserSettings());
    }
    void markAudioInitialising(bool active)
    {
        settings.getUserSettings()->setValue("audioInitialising",active);
        if(!settings.saveIfNeeded())notice("Warning: App settings could not be saved.",1);
    }
    void reconnectDevice()
    {
        runtime.io.muted=true;mute.setToggleState(true,juce::dontSendNotification);
        markAudioInitialising(true);auto xml=juce::parseXML(lastGoodXml);manager.closeAudioDevice();const auto error=manager.initialise(2,2,xml.get(),true);markAudioInitialising(false);
        if(error.isNotEmpty())notice("Error: Audio device unavailable: "+error,2);
        else notice("Info: Audio device reinitialised; DSP output muted.",0);
    }
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override
    {
        std::vector<int> active;const auto mask=device->getActiveInputChannels();
        for(int c=0;c<device->getInputChannelNames().size();++c)if(mask[c])active.push_back(c);
        if(!runtime.prepare({device->getCurrentSampleRate(),std::size_t(std::max(1,device->getCurrentBufferSizeSamples())),2},std::move(active)))rollbackRequested=true;
    }
    void audioDeviceStopped() override {runtime.stop();}
    void audioDeviceError(const juce::String& message) override
    {
        std::lock_guard<std::mutex> lock(runtime.control);runtime.error="Audio device error: "+message.toStdString();runtime.ready=false;
    }
    void audioDeviceIOCallbackWithContext(const float* const* in,int inputs,float* const* out,int outputs,int count,const juce::AudioIODeviceCallbackContext&) override
    {
        juce::ScopedNoDenormals noDenormals;runtime.process({in,inputs,out,outputs,std::size_t(std::max(0,count))});callbackCount.fetch_add(1,std::memory_order_relaxed);
    }
    void timerCallback() override
    {
        juce::String error;std::size_t latency=0;juce::String modes;
        {std::lock_guard<std::mutex> lock(runtime.control);error=runtime.error;for(auto& e:runtime.chain.effects){latency+=e->latencySamples();if(auto* p=e->parameter("oversampling"))modes+=juce::String(e->name)+" "+juce::String(1<<int(p->target.load()))+"x  ";}}
        if(error.isNotEmpty()&&error!=lastError){notice("Error: "+error,2);lastError=error;}
        // Let CoreAudio finish its asynchronous rate-change notifications before reopening.
        if(rollbackRequested.exchange(false)&&!rollbackAttempted)rollbackTicks=5;
        if(rollbackTicks>0&&--rollbackTicks==0&&!runtime.ready&&!rollbackAttempted&&lastGoodXml.isNotEmpty()){
            rollbackAttempted=true;reconnectDevice();notice(runtime.ready?"Warning: Previous audio configuration requested after: "+error:"Error: Recovery failed: "+error,runtime.ready?1:2);
        }
        const auto generation=runtime.generation.load();if(generation!=shownGeneration){shownGeneration=generation;refresh();}
        juce::StringArray names;auto* device=manager.getCurrentAudioDevice();if(device)names=device->getInputChannelNames();
        if(names!=inputNames){inputNames=names;inputSelector.clear(juce::dontSendNotification);for(int c=0;c<names.size();++c)inputSelector.addItem("Input "+juce::String(c+1)+" | "+names[c],c+1);}
        inputSelector.setEnabled(!names.isEmpty());inputSelector.setTextWhenNothingSelected("No active input channel");
        const int selected=runtime.io.selectedInput.load();inputSelector.setSelectedId(selected>=0&&selected<names.size()?selected+1:0,juce::dontSendNotification);
        const auto blocks=callbackCount.load();if(blocks==lastBlocks)++inactiveTicks;else inactiveTicks=0;lastBlocks=blocks;
        const bool active=runtime.ready&&inactiveTicks<20;
        if(!active&&wasActive){runtime.io.muted=true;if(error.isEmpty())notice("Warning: Audio inactive / disconnected. Reconnect to restore a device.",1);}
        if(active&&!wasActive&&status.getText().startsWith("Warning: Audio inactive"))notice("Info: Audio callback resumed; check DSP Mute before playing.",0);
        wasActive=active;
        mute.setToggleState(runtime.io.muted.load(),juce::dontSendNotification);
        const double rate=device?device->getCurrentSampleRate():0;
        audioStatus.setText((device?juce::String(rate,0)+" Hz | "+juce::String(device->getCurrentBufferSizeSamples())+" samples":"No audio device")+(runtime.io.muted?" | DSP MUTED":" | DSP OUTPUT ON")+(active?" | Callback active":" | Callback inactive"),juce::dontSendNotification);
        latencyStatus.setText("DSP latency: "+juce::String(int(latency))+" samples / "+juce::String(rate>0?1000.0*latency/rate:0,2)+" ms | "+modes,juce::dontSendNotification);
        const auto in=runtime.io.inputLevel.consume(),out=runtime.io.outputLevel.consume();inputClipTicks=in.clipped?20:std::max(0,inputClipTicks-1);outputClipTicks=out.clipped?20:std::max(0,outputClipTicks-1);
        const auto level=[](float p){return p>0?juce::String(20*std::log10(p),1)+" dBFS":juce::String("-inf dBFS");};
        levels.setText("Input "+level(in.peak)+(inputClipTicks?" [CLIP]":"")+"    Output "+level(out.peak)+(outputClipTicks?" [CLIP]":"")+(in.invalid||out.invalid?" [INVALID SAMPLE]":""),juce::dontSendNotification);
        title.setText(juce::String(multi_effects::product::name)+" "+multi_effects::product::version+" "+multi_effects::product::stage+" | CPU "+juce::String(manager.getCpuUsage()*100,1)+"% | blocks "+juce::String(blocks),juce::dontSendNotification);
        presetTitle.setText("Preset: "+session.title(),juce::dontSendNotification);
        if(++settingsTicks>=10){settingsTicks=0;if(active){settings.getUserSettings()->setValue("audioInitialising",false);if(auto xml=manager.createStateXml())lastGoodXml=xml->toString();rollbackAttempted=false;lastError.clear();}persist();}
    }
    multi_effects::app::Runtime runtime;
    juce::AudioDeviceManager manager;
    juce::ListBox list;juce::AudioDeviceSelectorComponent devices;juce::TooltipWindow tooltips{this,700};
    juce::ApplicationProperties settings;multi_effects::app::Preferences preferences;multi_effects::app::PresetSession session;
    juce::File presetFolder;std::vector<multi_effects::app::PresetEntry> presetFiles;
    juce::TextButton up{"Up"},down{"Down"},remove{"Remove..."},add{"Add"},save{"Save"},saveAs{"Save As..."},load{"Open preset..."},loadSelected{"Load selected"},irLoad{"Load cabinet IR..."},reconnect{"Reconnect audio"};
    juce::ToggleButton stereoInput{"Stereo input pair"};
    juce::ToggleButton mute{"Mute DSP"},bypass{"Bypass selected effect"};
    juce::ComboBox types,inputSelector,presetList;juce::StringArray inputNames;
    juce::Label title,status,levels,audioStatus,latencyStatus,presetTitle,irStatus,deviceWarning;
    int inputClipTicks=0,outputClipTicks=0,inactiveTicks=0,settingsTicks=0;
    int rollbackTicks=0;
    unsigned lastBlocks=0,shownGeneration=0;bool wasActive=false,rollbackAttempted=false;
    juce::String lastGoodXml,lastError;std::string selectedId;
    std::vector<std::unique_ptr<multi_effects::ui::ParameterRow>> rows;
    std::unique_ptr<juce::FileChooser> chooser;
    std::atomic<bool> rollbackRequested{false};std::atomic<unsigned> callbackCount{0};
};
class AppWindow final:public juce::DocumentWindow
{
public:
    AppWindow(const juce::String& name):DocumentWindow(name,juce::Colours::darkgrey,allButtons){}
    void closeButtonPressed() override {juce::JUCEApplication::getInstance()->systemRequestedQuit();}
};
class Application final:public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override{return multi_effects::product::name;}
    const juce::String getApplicationVersion() override{return multi_effects::product::version;}
    void initialise(const juce::String&) override
    {
        window=std::make_unique<AppWindow>(getApplicationName());window->setUsingNativeTitleBar(true);
        content=new MainComponent();window->setContentOwned(content,true);window->setResizable(true,false);window->setResizeLimits(1050,820,1800,1400);window->centreWithSize(1100,850);window->setVisible(true);
    }
    void shutdown() override{window.reset();content=nullptr;}
    void systemRequestedQuit() override{if(content)content->requestQuit();else quit();}
private:
    std::unique_ptr<AppWindow> window;MainComponent* content=nullptr;
};
START_JUCE_APPLICATION(Application)
