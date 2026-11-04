#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "engine/Engine.h"
namespace multi_effects::ui {
inline juce::String parameterText(const ParameterInfo& p,double value) {
    if(p.requiresPrepare)return juce::String(1<<juce::jlimit(0,2,int(std::lround(value))))+"x";
    const bool percent=p.unit.empty()&&p.minimum==0&&p.maximum<=1;
    const int decimals=percent||p.unit=="ms"?0:p.unit=="dB"||p.unit==":1"?1:2;
    return juce::String(percent?value*100:value,decimals)+(percent?" %":p.unit.empty()?"":" "+juce::String(p.unit));
}
inline juce::String effectText(const Effect& e,int row) {
    return juce::String(row+1)+". "+juce::String(e.type=="cabinet"?"Cabinet":e.name)+(e.bypass.load()?" [BYPASS]":" [ON]");
}
inline juce::String cabinetText(const Chain& c) {
    for(const auto& e:c.effects)if(e->type=="cabinet")return e->impulseResponse.empty()?"Cabinet: Synthetic default IR (not a capture)":"Cabinet: Embedded user IR / "+juce::String(e->impulseName);
    return "Cabinet: None";
}
class ParameterRow final : public juce::Component {
public:
    explicit ParameterRow(ParameterInfo metadata):info(std::move(metadata)) {
        addAndMakeVisible(label);addAndMakeVisible(slider);addAndMakeVisible(choice);
        label.setText(juce::String(info.name),juce::dontSendNotification);
        slider.setSliderStyle(juce::Slider::LinearHorizontal);slider.setTextBoxStyle(juce::Slider::TextBoxRight,false,105,24);
        slider.setRange(info.minimum,info.maximum);slider.setDoubleClickReturnValue(true,info.initial);
        slider.textFromValueFunction=[p=info](double v){return parameterText(p,v);};
        slider.valueFromTextFunction=[p=info](const juce::String& s){const auto v=s.getDoubleValue();return p.unit.empty()&&p.minimum==0&&p.maximum<=1?v/100:v;};
        choice.addItemList({"1x","2x","4x"},1);choice.setVisible(info.requiresPrepare);slider.setVisible(!info.requiresPrepare);
        const auto help="Range: "+parameterText(info,info.minimum)+" to "+parameterText(info,info.maximum)+" | Default: "+parameterText(info,info.initial);
        label.setTooltip(help);slider.setTooltip(help+" | Double-click to reset");choice.setTooltip(help+" | Changes rebuild DSP; stop transport first");
        slider.onValueChange=[this]{if(onChange)onChange(float(slider.getValue()));};
        choice.onChange=[this]{if(onChange)onChange(float(choice.getSelectedId()-1));};
        setValue(info.initial);
    }
    void setValue(float value){slider.setValue(value,juce::dontSendNotification);choice.setSelectedId(int(value)+1,juce::dontSendNotification);}
    float value() const {return info.requiresPrepare?float(choice.getSelectedId()-1):float(slider.getValue());}
    void resized() override {label.setBounds(0,0,getWidth(),22);slider.setBounds(0,24,getWidth(),30);choice.setBounds(0,24,150,28);}
    ParameterInfo info;juce::Slider slider;juce::ComboBox choice;juce::Label label;
    std::function<void(float)> onChange;
};
}
