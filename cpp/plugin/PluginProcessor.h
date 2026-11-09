#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "../app/PresetIO.h"
#include "../app/PresetLibrary.h"
#include "ProductInfo.h"
#include "engine/AudioIO.h"
#include <mutex>
#include <thread>

namespace multi_effects::plugin {
class Processor final : public juce::AudioProcessor, private juce::AudioProcessorValueTreeState::Listener,
                        private juce::AsyncUpdater {
public:
    Processor();
    ~Processor() override;
    const juce::String getName() const override { return product::name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 300; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Current preset"; }
    void changeProgramName(int,const juce::String&) override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void prepareToPlay(double,int) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
    juce::AudioProcessorParameter* getBypassParameter() const override;
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*,int) override;
    // Non-realtime services, shared by editor and lifecycle tests.
    Chain snapshot();
    bool install(Chain,const juce::String& presetName);
    juce::String statusText();
    juce::String chainText();
    juce::String presetTitle();
    bool presetModified();
    void markPresetSaved(const juce::String&, std::uint64_t fingerprint);
    std::atomic<unsigned> uiGeneration{0};
    void serviceConfiguration();
    void refreshLatency();
    juce::AudioProcessorValueTreeState parameters;
    std::atomic<float> grainReadback{-1.f}; // Last rendered host-controlled instance.
    std::atomic<float> mixReadback{-1.f};
    std::atomic<float> headroomReadback{-1.f}; // Last rendered host-controlled instance.
    InputSystem input;
    LevelMeter inputLevel,outputLevel;
    std::atomic<unsigned> appliedRevision{0};
private:
    struct Mapping { std::string type,key; juce::String hostId; std::atomic<float>* target=nullptr; bool bypass=false,staged=false; };
    struct Binding { Parameter* parameter=nullptr; Effect* effect=nullptr; };
    struct RenderState {
        Chain chain;
        Effect* hostDrive=nullptr;
        std::vector<Binding> bindings;
        std::vector<float> dryL,dryR;
        std::array<float,3072> historyL{},historyR{}; // 24 effects * maximum 128 frames.
        std::size_t index=0,latency=0;
        float wet=1,coefficient=0;
        unsigned revision=0;
    };
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    void applyTargets(Chain&);
    std::unique_ptr<RenderState> build(unsigned);
    void render(juce::AudioBuffer<float>&,bool) noexcept;
    void parameterChanged(const juce::String&,float) override { revision.fetch_add(1); }
    void handleAsyncUpdate() override { refreshLatency(); }
    void runWorker();
    std::vector<Mapping> mappings;
    std::atomic<float>* hostTrim=nullptr;
    std::atomic<float>* globalBypass=nullptr;
    std::mutex control; // Never acquired in processBlock.
    Chain model;
    ProcessSpec spec{48000,128,2};
    juce::String error,presetName="Default";
    bool prepared=false;
    std::uint64_t cleanFingerprint=0;
    bool restoredModified=false;
    unsigned builtRevision=0;
    std::atomic<unsigned> revision{1};
    std::atomic<bool> quitting{false};
    std::thread worker;
    // Single audio consumer; exchange transfers ownership. Destruction is worker-only.
    std::atomic<RenderState*> pending{nullptr},retired{nullptr};
    RenderState* active=nullptr; // Audio-owned; lifecycle accesses only while stopped.
    std::atomic<int> activeLatency{0},reportedLatency{0};
    std::atomic<bool> installing{false};
};
}
