#include "PluginProcessor.h"
namespace multi_effects::plugin {
class Editor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit Editor(Processor& processor):AudioProcessorEditor(processor),owner(processor),controls(processor) {
        addAndMakeVisible(controls);for(auto* l:{&chain,&meters,&status})addAndMakeVisible(*l);
        for(auto* b:{&load,&save,&ir})addAndMakeVisible(*b);
        load.onClick=[this]{choose(false);};save.onClick=[this]{choose(true);};
        ir.onClick=[this]{
            chooser=std::make_unique<juce::FileChooser>("Load cabinet WAV IR",juce::File(),"*.wav");
            chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[safe=juce::Component::SafePointer<Editor>(this)](const auto& fc){
                if(!safe||fc.getResult()==juce::File())return;
                try {auto asset=app::loadImpulse(fc.getResult());auto c=safe->owner.snapshot();bool found=false;for(auto& e:c.effects)if(e->type=="cabinet"){e->impulseResponse=asset.samples;e->impulseSampleRate=asset.sampleRate;e->impulseName=fc.getResult().getFileName().toStdString();found=true;break;}
                    if(!found)throw std::runtime_error("Preset has no Cabinet");safe->owner.install(std::move(c),{});safe->localError.clear();
                }catch(const std::exception& e){safe->localError=juce::String("IR error: ")+e.what();}
            });
        };
        setSize(800,650);startTimerHz(10);
    }
    void resized() override {
        chain.setBounds(10,5,getWidth()-20,55);load.setBounds(10,65,140,28);save.setBounds(160,65,140,28);ir.setBounds(310,65,140,28);
        controls.setBounds(10,100,getWidth()-20,getHeight()-165);meters.setBounds(10,getHeight()-60,getWidth()-20,25);status.setBounds(10,getHeight()-32,getWidth()-20,25);
    }
private:
    void choose(bool writing) {
        chooser=std::make_unique<juce::FileChooser>(writing?"Save standalone-compatible preset":"Load preset",juce::File(),"*.json");
        chooser->launchAsync(juce::FileBrowserComponent::canSelectFiles|(writing?juce::FileBrowserComponent::saveMode:juce::FileBrowserComponent::openMode),
            [safe=juce::Component::SafePointer<Editor>(this),writing](const auto& fc){if(!safe||fc.getResult()==juce::File())return;
                const auto file=writing?fc.getResult().withFileExtension("json"):fc.getResult();
                if(writing&&file.existsAsFile()) {
                    juce::AlertWindow::showAsync(juce::MessageBoxOptions().withTitle("Replace preset?").withMessage(file.getFullPathName()).withButton("Replace").withButton("Cancel"),[safe,file](int r){if(safe&&r==1)safe->saveFile(file);});return;
                }
                if(writing){safe->saveFile(file);return;}
                try{safe->owner.install(app::loadPreset(file),file.getFileName());safe->localError.clear();}catch(const std::exception& e){safe->localError=juce::String("Preset error: ")+e.what();}
            });
    }
    void saveFile(const juce::File& file) {try{auto c=owner.snapshot();app::savePreset(c,file);localError="Saved: "+file.getFileName();}catch(const std::exception& e){localError=juce::String("Save error: ")+e.what();}}
    void timerCallback() override {
        chain.setText(owner.chainText(),juce::dontSendNotification);const auto in=owner.inputLevel.consume(),out=owner.outputLevel.consume();
        const auto db=[](float v){return v>0?juce::String(20*std::log10(v),1):juce::String("-inf");};
        meters.setText("Input "+db(in.peak)+" dBFS"+(in.clipped?" CLIP":"")+" | Output "+db(out.peak)+" dBFS"+(out.clipped?" CLIP":"")+" | Latency "+juce::String(owner.getLatencySamples())+" samples",juce::dontSendNotification);
        status.setText(localError.isEmpty()?owner.statusText():localError,juce::dontSendNotification);
    }
    Processor& owner;juce::GenericAudioProcessorEditor controls;
    juce::Label chain,meters,status;juce::TextButton load{"Load preset"},save{"Save preset..."},ir{"Load cabinet IR"};
    juce::String localError;std::unique_ptr<juce::FileChooser> chooser;
};
juce::AudioProcessorEditor* Processor::createEditor(){return new Editor(*this);}
}
