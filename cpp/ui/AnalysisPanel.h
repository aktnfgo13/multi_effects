#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "analysis/Analyzer.h"
#include <thread>
#include <mutex>
#include <chrono>

namespace multi_effects::ui {
// Shared by Standalone and plugin editors. Mutex protects worker/UI snapshots only.
class AnalysisPanel final : public juce::Component, private juce::Timer {
public:
    explicit AnalysisPanel(AnalysisTap& source):tap(source) {
        for(auto* b:{&enable,&pause})addAndMakeVisible(*b);
        for(auto* c:{&view,&signals,&lanes,&fft,&zoom,&amplitude})addAndMakeVisible(*c);
        view.addItem("Waveform",1);view.addItem("Spectrum",2);view.setSelectedId(1);
        signals.addItem("Input + Output",1);signals.addItem("Input",2);signals.addItem("Output",3);signals.setSelectedId(1);
        lanes.addItem("L + R",1);lanes.addItem("Left",2);lanes.addItem("Right",3);lanes.setSelectedId(1);
        fft.addItem("FFT 2048",1);fft.addItem("FFT 4096",2);fft.setSelectedId(1);
        zoom.addItem("Time 1x",1);zoom.addItem("Time 2x",2);zoom.addItem("Time 4x",3);zoom.setSelectedId(1);
        amplitude.addItem("Amp +/-1",1);amplitude.addItem("Amp +/-0.5",2);amplitude.addItem("Amp +/-2",3);amplitude.setSelectedId(1);
        enable.onClick=pause.onClick=[this]{updateCapture();repaint();};
        fft.onChange=[this]{fftSize.store(fft.getSelectedId()==2?4096:2048);repaint();};
        for(auto* c:{&view,&signals,&lanes,&zoom,&amplitude})c->onChange=[this]{repaint();};
        worker=std::thread([this]{run();});startTimerHz(30);
    }
    ~AnalysisPanel() override {stopTimer();tap.setEnabled(false);quit=true;worker.join();}
    void visibilityChanged() override {updateCapture();}
    void resized() override {
        int x=8;enable.setBounds(x,4,82,26);x+=84;pause.setBounds(x,4,70,26);x+=72;
        for(auto* c:{&view,&signals,&lanes,&fft,&zoom,&amplitude}){const int w=c==&signals?130:100;c->setBounds(x,4,w-4,26);x+=w;}
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff171c24));
        const auto area=juce::Rectangle<float>(55,53,float(getWidth()-75),float(getHeight()-100));
        const bool spectrum=view.getSelectedId()==2;
        const float limit=amplitude.getSelectedId()==2?.5f:amplitude.getSelectedId()==3?2.f:1.f;
        const unsigned count=std::max(1u,std::min(frame.validFrames?frame.validFrames:Analyzer::maximumFFT,Analyzer::maximumFFT>>(zoom.getSelectedId()-1)));
        const float top=spectrum?6.f:limit,bottom=spectrum?-120.f:-limit;
        const auto y=[&](float v){return area.getBottom()-(v-bottom)/(top-bottom)*area.getHeight();};
        g.setFont(11.f);
        for(int i=0;i<=4;++i){float v=bottom+(top-bottom)*i/4;g.setColour(juce::Colours::darkgrey);g.drawHorizontalLine(int(y(v)),area.getX(),area.getRight());g.setColour(juce::Colours::lightgrey);g.drawText(juce::String(v,spectrum?0:2),0,int(y(v))-7,50,14,juce::Justification::right);}
        const float maxHz=float(std::min(20000.,frame.sampleRate*.5));
        const auto fx=[&](float hz){return area.getX()+std::log(hz/20.f)/std::log(maxHz/20.f)*area.getWidth();};
        if(spectrum){for(float hz:{20.f,100.f,1000.f,10000.f,maxHz}){g.setColour(juce::Colours::darkgrey);g.drawVerticalLine(int(fx(hz)),area.getY(),area.getBottom());g.setColour(juce::Colours::lightgrey);g.drawText(hz>=1000?juce::String(hz/1000,1)+"k":juce::String(hz,0),int(fx(hz))-18,int(area.getBottom())+3,40,16,juce::Justification::centred);}}
        else {g.setColour(juce::Colours::lightgrey);g.drawText("-"+juce::String(1000.*count/frame.sampleRate,1)+" ms",int(area.getX()),int(area.getBottom())+3,100,16,juce::Justification::left);g.drawText("0 ms",int(area.getRight())-60,int(area.getBottom())+3,60,16,juce::Justification::right);}
        const juce::Colour colors[]{juce::Colours::cyan,juce::Colours::cornflowerblue,juce::Colours::orange,juce::Colours::salmon};
        if(enable.getToggleState()&&frame.validFrames) {
            juce::Graphics::ScopedSaveState state(g);g.reduceClipRegion(area.toNearestInt());
            for(unsigned c=0;c<4;++c){if(signals.getSelectedId()==2&&c>=2)continue;if(signals.getSelectedId()==3&&c<2)continue;if(frame.channels==1&&c%2)continue;if(lanes.getSelectedId()==2&&c%2)continue;if(lanes.getSelectedId()==3&&!(c%2)&&frame.channels==2)continue;
                g.setColour(colors[c]);
                if(spectrum){juce::Path path;bool first=true;for(unsigned k=1;k<=frame.fftSize/2;++k){float hz=float(k*frame.sampleRate/frame.fftSize);if(hz<20||hz>maxHz)continue;const float px=fx(hz),py=y(frame.spectrum[c][k]);if(first){path.startNewSubPath(px,py);first=false;}else path.lineTo(px,py);}g.strokePath(path,juce::PathStrokeType(1.3f));}
                else {const unsigned pixels=unsigned(area.getWidth());for(unsigned px=0;px<pixels;++px){const unsigned begin=Analyzer::maximumFFT-count+px*count/pixels,end=std::min(Analyzer::maximumFFT,Analyzer::maximumFFT-count+(px+1)*count/pixels+1);float lo=frame.waveform[c][begin],hi=lo;for(unsigned j=begin+1;j<end;++j){lo=std::min(lo,frame.waveform[c][j]);hi=std::max(hi,frame.waveform[c][j]);}g.drawVerticalLine(int(area.getX()+px),y(hi),std::max(y(lo),y(hi)+1));}}
            }
        }
        g.setColour(juce::Colours::lightgrey);
        juce::String info=!enable.getToggleState()?"Analysis off":pause.getToggleState()?"Paused":frame.validFrames?"Live":"Waiting for audio";
        info+=" | Chain pre-output trim/clamp | "+juce::String(frame.channels)+" ch | "+juce::String(frame.sampleRate,0)+" Hz | aligned "+juce::String(frame.latency)+" samples | dropped "+juce::String(tap.dropCount())+" frames";
        g.drawText(info,10,getHeight()-22,getWidth()-20,18,juce::Justification::left);
        g.setColour(juce::Colours::cyan);g.drawText("Input L/R",58,32,100,18,juce::Justification::left);g.setColour(juce::Colours::orange);g.drawText("Output L/R",165,32,100,18,juce::Justification::left);
        g.setColour(juce::Colours::lightgrey);g.drawText(spectrum?"dBFS amplitude | Hann | 100 ms smoothing":"Amplitude | actual samples | latency-aligned",280,32,getWidth()-290,18,juce::Justification::left);
    }
private:
    void updateCapture() {
        const bool active=isShowing()&&enable.getToggleState()&&!pause.getToggleState();
        if(active&&!tap.isEnabled()){std::lock_guard<std::mutex> lock(snapshotMutex);latest=Analyzer::Snapshot{};frame=latest;}
        tap.setEnabled(active);
    }
    void run() {
        Analyzer analyzer;AnalysisTap::Packet packet;unsigned session=0;
        while(!quit.load()) {
            const auto current=tap.currentSession();if(current!=session){analyzer.reset();session=current;}
            analyzer.setFFTSize(fftSize.load());
            for(unsigned i=0;i<AnalysisTap::capacity&&tap.tryRead(packet);++i)if(tap.isEnabled()&&packet.session==session)analyzer.accept(packet);
            if(tap.isEnabled()&&analyzer.ready()){auto next=analyzer.snapshot();std::lock_guard<std::mutex> lock(snapshotMutex);latest=std::move(next);}
            std::this_thread::sleep_for(std::chrono::milliseconds(tap.isEnabled()?8:40));
        }
    }
    void timerCallback() override {if(!enable.getToggleState()||!isShowing()||pause.getToggleState())return;if(!pause.getToggleState()){std::unique_lock<std::mutex> lock(snapshotMutex,std::try_to_lock);if(lock.owns_lock())frame=latest;}repaint();}
    AnalysisTap& tap;
    juce::ToggleButton enable{"Analysis"},pause{"Pause"};
    juce::ComboBox view,signals,lanes,fft,zoom,amplitude;
    std::atomic<bool> quit{false};std::atomic<unsigned> fftSize{2048};
    std::mutex snapshotMutex;Analyzer::Snapshot latest,frame;std::thread worker;
};
}
