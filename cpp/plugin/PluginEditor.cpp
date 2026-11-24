#include "PluginProcessor.h"
#include "../ui/AlphaUI.h"
#include "../ui/AnalysisPanel.h"
namespace multi_effects::plugin {
class Editor final : public juce::AudioProcessorEditor, private juce::Timer, private juce::ListBoxModel {
public:
    explicit Editor(Processor& processor):AudioProcessorEditor(processor),owner(processor),list("Effect Chain",this),analysisPanel(owner.analysis) {
        addChildComponent(analysisPanel);
        for(auto* l:{&title,&preset,&meters,&status,&irStatus,&selection})addAndMakeVisible(*l);
        for(auto* b:{&load,&save,&saveAs,&ir,&loadSelected})addAndMakeVisible(*b);
        addAndMakeVisible(list);addAndMakeVisible(browser);addAndMakeVisible(bypass);addAndMakeVisible(globalBypass);addAndMakeVisible(visualization);
        visualization.onClick=[this]{
            const bool showing=visualization.getToggleState();
            list.setVisible(!showing);selection.setVisible(!showing);bypass.setVisible(!showing);
            for(auto& row:rows)row->setVisible(!showing);
            irStatus.setVisible(!showing);ir.setVisible(!showing);
            analysisPanel.setVisible(showing);
            if(showing)analysisPanel.toFront(false);
        };
        addAndMakeVisible(trim);addAndMakeVisible(trimLabel);
        trimLabel.setText("Input Trim",juce::dontSendNotification);
        trimAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(owner.parameters,"input.trim",trim);
        globalAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(owner.parameters,"plugin.bypass",globalBypass);
        load.onClick=[this]{confirmDiscard([this]{choose(false);});};saveAs.onClick=[this]{choose(true);};
        save.onClick=[this]{if(currentFile==juce::File())choose(true);else saveFile(currentFile);};
        loadSelected.onClick=[this]{const int i=browser.getSelectedId()-1;if(i<0||i>=int(entries.size()))return;const auto entry=entries[std::size_t(i)];confirmDiscard([this,entry]{try{if(!owner.install(entry.load(),entry.label))throw std::runtime_error(owner.statusText().toStdString());refreshChain();currentFile=entry.file;localError.clear();}catch(const std::exception& e){localError=juce::String("Preset error: ")+e.what();}});};
        ir.onClick=[this]{chooseIR();};
        title.setText(product::displayTitle,juce::dontSendNotification);
        setSize(900,620);refreshBrowser();refreshChain();startTimerHz(4);
    }
    ~Editor() override {stopTimer();clearBindings();}
    void resized() override {
        analysisPanel.setBounds(12,112,getWidth()-24,394);
        title.setBounds(12,8,600,28);visualization.setBounds(610,8,105,28);globalBypass.setBounds(720,8,165,28);
        preset.setBounds(12,40,485,26);trimLabel.setBounds(500,40,85,26);trim.setBounds(585,40,300,26);browser.setBounds(12,72,285,28);loadSelected.setBounds(305,72,85,28);
        load.setBounds(400,72,105,28);save.setBounds(515,72,80,28);saveAs.setBounds(605,72,105,28);
        list.setBounds(12,112,270,355);selection.setBounds(300,110,580,28);bypass.setBounds(300,142,260,26);
        const int rowsPerColumn = rows.size() > 8 && rows.size() <= 10 ? 5 : 4;
        const int columns = std::max(1, int((rows.size()+rowsPerColumn-1)/rowsPerColumn));
        const int width = 580 / columns;
        for(std::size_t i=0;i<rows.size();++i){rows[i]->setBounds(300+int(i/rowsPerColumn)*width,174+int(i%rowsPerColumn)*60,width-10,56);}
        irStatus.setBounds(12,478,675,28);ir.setBounds(700,478,185,28);
        meters.setBounds(12,508,875,42);status.setBounds(12,550,875,55);
    }
private:
    void clearBindings(){attachments.clear();bypassAttachment.reset();rows.clear();}
    int getNumRows() override {return int(view.effects.size());}
    void paintListBoxItem(int row,juce::Graphics& g,int w,int h,bool selected) override {
        if(row<0||row>=getNumRows())return;if(selected)g.fillAll(juce::Colours::darkslateblue);
        g.setColour(view.effects[std::size_t(row)]->bypass?juce::Colours::grey:juce::Colours::white);
        g.drawText(ui::effectText(*view.effects[std::size_t(row)],row),8,0,w-12,h,juce::Justification::centredLeft);
    }
    void selectedRowsChanged(int row) override {
        clearBindings();if(row<0||row>=getNumRows()){selection.setText("Select an effect",juce::dontSendNotification);bypass.setEnabled(false);return;}
        auto& e=*view.effects[std::size_t(row)];selectedId=e.id;
        bool first=true;for(int i=0;i<row;++i)if(view.effects[std::size_t(i)]->type==e.type)first=false;
        selection.setText(juce::String(e.name)+(first?"":" / preset-only duplicate"),juce::dontSendNotification);
        bypass.setEnabled(first);bypass.setToggleState(e.bypass.load(),juce::dontSendNotification);
        if(first)bypassAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(owner.parameters,juce::String(e.type)+".bypass",bypass);
        for(const auto& p:e.parameters){auto control=std::make_unique<ui::ParameterRow>(p->info);control->setValue(p->target.load());control->setEnabled(first);
            if(first){auto* parameter=owner.parameters.getParameter(juce::String(e.type)+"."+juce::String(p->info.id));
                auto binding=std::make_unique<juce::ParameterAttachment>(*parameter,[rowControl=control.get()](float v){rowControl->setValue(v);},nullptr);
                auto* attached=binding.get();binding->sendInitialUpdate();
                // A whole gesture per edit works for slider, typed text, reset and choice.
                control->onChange=[attached](float v){attached->setValueAsCompleteGesture(v);};attachments.push_back(std::move(binding));}
            addAndMakeVisible(*control);control->setVisible(!visualization.getToggleState());rows.push_back(std::move(control));}
        resized();
    }
    void refreshChain() {
        shownGeneration=owner.uiGeneration.load();view=owner.snapshot();list.updateContent();int index=0;
        for(std::size_t i=0;i<view.effects.size();++i)if(view.effects[i]->id==selectedId)index=int(i);
        list.selectRow(view.effects.empty()?-1:index);selectedRowsChanged(list.getSelectedRow());
        irStatus.setText(ui::cabinetText(view),juce::dontSendNotification);
        ir.setEnabled(std::any_of(view.effects.begin(),view.effects.end(),[](const auto& e){return e->type=="cabinet";}));
    }
    void refreshBrowser(){entries=app::presetEntries(currentFile.getParentDirectory());browser.clear(juce::dontSendNotification);for(std::size_t i=0;i<entries.size();++i)browser.addItem(entries[i].label,int(i+1));browser.setTextWhenNothingSelected("Factory / User presets");}
    void confirmDiscard(std::function<void()> action){if(!owner.presetModified()){action();return;}
        juce::AlertWindow::showAsync(juce::MessageBoxOptions().withTitle("Unsaved preset changes").withMessage("Save first to keep changes. Discard and load another preset?").withButton("Discard").withButton("Cancel"),[safe=juce::Component::SafePointer<Editor>(this),action=std::move(action)](int r){if(safe&&r==1)action();});}
    void chooseIR(){
        chooser=std::make_unique<juce::FileChooser>("Load cabinet WAV IR",juce::File(),"*.wav");
        chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[safe=juce::Component::SafePointer<Editor>(this)](const auto& fc){
            if(!safe||fc.getResult()==juce::File())return;
            try {auto asset=app::loadImpulse(fc.getResult());auto c=safe->owner.snapshot();bool found=false;for(auto& e:c.effects)if(e->type=="cabinet"){e->impulseResponse=asset.samples;e->impulseSampleRate=asset.sampleRate;e->impulseName=fc.getResult().getFileName().toStdString();found=true;break;}
                if(!found)throw std::runtime_error("Preset has no Cabinet");if(!safe->owner.install(std::move(c),{}))throw std::runtime_error(safe->owner.statusText().toStdString());safe->refreshChain();safe->localError.clear();
            }catch(const std::exception& e){safe->localError=juce::String("IR error: ")+e.what();}
        });
    }
    void choose(bool writing) {
        const auto directory=currentFile==juce::File()?app::userPresetDirectory():currentFile.getParentDirectory();
        if(writing){const auto result=directory.createDirectory();if(result.failed()){localError=result.getErrorMessage();return;}}
        chooser=std::make_unique<juce::FileChooser>(writing?"Save user preset as":"Load preset",writing?directory.getChildFile("Untitled.json"):directory,"*.json");
        chooser->launchAsync(juce::FileBrowserComponent::canSelectFiles|(writing?juce::FileBrowserComponent::saveMode:juce::FileBrowserComponent::openMode),
            [safe=juce::Component::SafePointer<Editor>(this),writing](const auto& fc){if(!safe||fc.getResult()==juce::File())return;
                const auto file=writing?fc.getResult().withFileExtension("json"):fc.getResult();
                if(writing&&file.existsAsFile()) {
                    juce::AlertWindow::showAsync(juce::MessageBoxOptions().withTitle("Replace preset?").withMessage(file.getFullPathName()).withButton("Replace").withButton("Cancel"),[safe,file](int r){if(safe&&r==1)safe->saveFile(file);});return;}
                if(writing){safe->saveFile(file);return;}
                try{if(!safe->owner.install(app::loadPreset(file),file.getFileNameWithoutExtension()))throw std::runtime_error(safe->owner.statusText().toStdString());safe->refreshChain();safe->currentFile=file;safe->refreshBrowser();safe->localError.clear();}catch(const std::exception& e){safe->localError=juce::String("Preset error: ")+e.what();}
            });
    }
    void saveFile(const juce::File& file) {try{auto c=owner.snapshot();app::savePreset(c,file);owner.markPresetSaved(file.getFileNameWithoutExtension(),app::presetFingerprint(c));currentFile=file;refreshBrowser();localError="Saved: "+file.getFileName();}catch(const std::exception& e){localError=juce::String("Save error: ")+e.what();}}
    void timerCallback() override {
        if(shownGeneration!=owner.uiGeneration.load()){currentFile=juce::File();refreshChain();}
        // Targets are UI snapshots only; attachments deliver host automation to controls.
        for(auto& e:view.effects){for(const auto& m:view.effects){if(m->type==e->type){if(m.get()==e.get())e->bypass=owner.parameters.getRawParameterValue(juce::String(e->type)+".bypass")->load()>=.5f;break;}}}
        list.repaint();preset.setText("Preset: "+owner.presetTitle()+(owner.presetModified()?" * (modified)":""),juce::dontSendNotification);
        const auto in=owner.inputLevel.consume(),out=owner.outputLevel.consume();
        inClip=in.clipped?8:std::max(0,inClip-1);outClip=out.clipped?8:std::max(0,outClip-1);
        const auto db=[](float v){return v>0?juce::String(20*std::log10(v),1):juce::String("-inf");};
        const float grain=owner.grainReadback.load(std::memory_order_relaxed);
        const float mid=owner.midReadback.load(std::memory_order_relaxed);
        const float tight=owner.tightReadback.load(std::memory_order_relaxed);
        const float mix=owner.mixReadback.load(std::memory_order_relaxed);
        const float headroom=owner.headroomReadback.load(std::memory_order_relaxed);
        const auto grainStatus=grain<0 ? juce::String("no rendered Custom Drive") : juce::String(grain,2);
        meters.setText("Input "+db(in.peak)+" dBFS"+(inClip?" CLIP":"")+(in.invalid?" INVALID":"")+" | Output "+db(out.peak)+" dBFS"+(outClip?" CLIP":"")+" | Latency "+juce::String(owner.getLatencySamples())+" samples | Grain DSP: "+grainStatus+" | Headroom DSP: "+(headroom<0?juce::String("—"):juce::String(headroom,2))+" | Tight DSP: "+(tight<0?juce::String("—"):juce::String(tight,1))+" | Mid Character DSP: "+(mid<0?juce::String("—"):juce::String(mid,1))+" | Mix DSP: "+(mix<0?juce::String("—"):juce::String(mix*100,1)+"%"),juce::dontSendNotification);
        juce::String modes;for(const auto* type:{"distortion","amp_sim","custom_drive"})if(std::any_of(view.effects.begin(),view.effects.end(),[type](const auto& e){return e->type==type;}))modes+=juce::String(type)+" "+juce::String(1<<int(owner.parameters.getRawParameterValue(juce::String(type)+".oversampling")->load()))+"x  ";
        status.setText((globalBypass.getToggleState()?"BYPASS | ":"DSP enabled (host controls playback) | ")+modes+" | "+ui::inputReading(owner.input)+"\n"+(localError.isEmpty()?owner.statusText():localError+" | "+owner.statusText()),juce::dontSendNotification);
    }
    Processor& owner;Chain view;juce::ListBox list;juce::TooltipWindow tooltips{this,700};
    ui::AnalysisPanel analysisPanel;
    ui::InputTrimSlider trim;juce::Label trimLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> trimAttachment;
    juce::Label title,preset,meters,status,irStatus,selection;juce::ComboBox browser;
    juce::TextButton load{"Open..."},save{"Save"},saveAs{"Save As..."},loadSelected{"Load"},ir{"Load user IR..."};
    juce::ToggleButton bypass{"Bypass selected effect"},globalBypass{"Plugin bypass"},visualization{"Visualize"};
    std::vector<std::unique_ptr<ui::ParameterRow>> rows;
    std::vector<std::unique_ptr<juce::ParameterAttachment>> attachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment,globalAttachment;
    std::vector<app::PresetEntry> entries;juce::File currentFile;juce::String localError;std::string selectedId;
    unsigned shownGeneration=0;int inClip=0,outClip=0;std::unique_ptr<juce::FileChooser> chooser;
};
juce::AudioProcessorEditor* Processor::createEditor(){return new Editor(*this);}
}
