#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "engine/Engine.h"
#include "PresetIO.h"
using namespace multi_effects;

class MainComponent final : public juce::Component, private juce::AudioIODeviceCallback,
                            private juce::ListBoxModel, private juce::Timer
{
public:
    MainComponent() : list("Effects", this), devices(manager, 0, 2, 0, 2, false, false, true, false)
    {
        for (const auto& type : {"noise_gate", "compressor", "gain", "distortion", "eq", "amp_sim", "cabinet", "delay", "reverb"})
            chain.effects.push_back(createEffect(type, std::string(type) + "_1"));
        addAndMakeVisible(list); addAndMakeVisible(devices);
        for (auto* b : {&up, &down, &remove, &add, &save, &load, &irLoad}) addAndMakeVisible(*b);
        addAndMakeVisible(mute); addAndMakeVisible(types); addAndMakeVisible(title); addAndMakeVisible(status); addAndMakeVisible(bypass);
        int n = 1; for (const auto& d : registry()) types.addItem(juce::String(d.name), n++);
        types.setSelectedId(1);
        title.setText("Multi Effects | Mono guitar -> stereo output", juce::dontSendNotification);
        mute.setToggleState(true, juce::dontSendNotification);
        mute.onClick = [this] { muted.store(mute.getToggleState()); };
        bypass.onClick = [this] { if (auto* e = selected()) e->bypass.store(bypass.getToggleState()); };
        up.onClick = [this] { move(-1); }; down.onClick = [this] { move(1); };
        remove.onClick = [this] { const int row = list.getSelectedRow(); if (row < 0) return; edit([&] { chain.effects.erase(chain.effects.begin() + row); }); refresh(); };
        add.onClick = [this] { if (chain.effects.size() >= 24) return; edit([&] { const auto& d = registry().at(std::size_t(types.getSelectedId()-1)); chain.effects.push_back(createEffect(d.type, juce::Uuid().toString().toStdString())); }); refresh(); };
        irLoad.onClick = [this] { chooseIR(); };
        save.onClick = [this] { choosePreset(false); }; load.onClick = [this] { choosePreset(true); };
        setSize(1050, 760);
        const auto error = manager.initialise(1, 2, nullptr, true);
        status.setText(error.isEmpty() ? "Output muted - select input/output, use headphones, then unmute." : error, juce::dontSendNotification);
        manager.addAudioCallback(this);
        refresh(); startTimerHz(10);
    }
    ~MainComponent() override { stopTimer(); manager.removeAudioCallback(this); manager.closeAudioDevice(); }
    void resized() override
    {
        title.setBounds(15, 10, 850, 30); mute.setBounds(880, 10, 150, 30);
        list.setBounds(15, 50, 265, 240);
        up.setBounds(15, 300, 80, 28); down.setBounds(100, 300, 80, 28); remove.setBounds(185, 300, 95, 28);
        types.setBounds(15, 340, 180, 28); add.setBounds(200, 340, 80, 28);
        bypass.setBounds(305, 50, 200, 30); irLoad.setBounds(800, 50, 210, 30);
        for (std::size_t i = 0; i < sliders.size(); ++i)
        {
            labels[i]->setBounds(305, 95 + int(i)*75, 220, 24);
            sliders[i]->setBounds(305, 120 + int(i)*75, 710, 32);
        }
        save.setBounds(15, 380, 125, 30); load.setBounds(150, 380, 130, 30);
        devices.setBounds(15, 430, 1015, 275);
        status.setBounds(15, 715, 1015, 35);
    }
private:
    Effect* selected() { const int r = list.getSelectedRow(); return r >= 0 && r < int(chain.effects.size()) ? chain.effects[std::size_t(r)].get() : nullptr; }
    void edit(const std::function<void()>& fn)
    {
        manager.removeAudioCallback(this); // Waits for in-flight callbacks, never called by audio thread.
        try { fn(); }
        catch (const std::exception& e) { status.setText(e.what(), juce::dontSendNotification); }
        manager.addAudioCallback(this);
    }
    void move(int delta)
    {
        const int from = list.getSelectedRow(), to = from + delta;
        if (from < 0 || to < 0 || to >= int(chain.effects.size())) return;
        edit([&] { chain.move(std::size_t(from), std::size_t(to)); });
        list.updateContent(); list.selectRow(to); selectedRowsChanged(to);
    }
    void refresh() { list.updateContent(); list.selectRow(chain.effects.empty() ? -1 : 0); selectedRowsChanged(list.getSelectedRow()); }
    int getNumRows() override { return int(chain.effects.size()); }
    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool sel) override
    {
        if (sel) g.fillAll(juce::Colours::darkslateblue);
        g.setColour(juce::Colours::white);
        if (row < int(chain.effects.size())) g.drawText(juce::String(row+1) + ". " + juce::String(chain.effects[std::size_t(row)]->name), 8, 0, width-10, height, juce::Justification::centredLeft);
    }
    void selectedRowsChanged(int) override
    {
        sliders.clear(); labels.clear();
        auto* effect = selected(); bypass.setEnabled(effect != nullptr); irLoad.setEnabled(effect && effect->type == "cabinet");
        if (effect)
        {
            bypass.setToggleState(effect->bypass.load(), juce::dontSendNotification);
            for (auto& p : effect->parameters)
            {
                auto label = std::make_unique<juce::Label>();
                label->setText(juce::String(p->info.name + " (" + p->info.unit + ")"), juce::dontSendNotification);
                addAndMakeVisible(*label); labels.push_back(std::move(label));
                auto slider = std::make_unique<juce::Slider>();
                slider->setRange(p->info.minimum, p->info.maximum);
                slider->setNumDecimalPlacesToDisplay(2);
                slider->setValue(p->target.load(), juce::dontSendNotification);
                slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 100, 25);
                slider->onValueChange = [parameter = p.get(), control = slider.get()] { parameter->set(float(control->getValue())); };
                addAndMakeVisible(*slider); sliders.push_back(std::move(slider));
            }
        }
        resized();
    }
    void loadPreset(const juce::File& file)
    {
        auto replacement = multi_effects::app::loadPreset(file);
        if (auto* device = manager.getCurrentAudioDevice()) replacement.prepare({device->getCurrentSampleRate(), std::size_t(std::max(1, device->getCurrentBufferSizeSamples()))});
        edit([&] { chain.effects.swap(replacement.effects); }); refresh();
    }
    void chooseIR()
    {
        auto* effect = selected(); if (!effect || effect->type != "cabinet") return;
        const auto id = effect->id;
        chooser = std::make_unique<juce::FileChooser>("Load cabinet WAV IR (up to 32768 samples)", juce::File(), "*.wav");
        chooser->launchAsync(juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::openMode,
            [safe = juce::Component::SafePointer<MainComponent>(this), id](const juce::FileChooser& fc)
            {
                if (!safe || fc.getResult() == juce::File()) return;
                try
                {
                    auto asset = multi_effects::app::loadImpulse(fc.getResult());
                    if(auto* device=safe->manager.getCurrentAudioDevice()) if(asset.samples.size()*device->getCurrentSampleRate()/asset.sampleRate > 32768) throw std::runtime_error("Resampled IR too long");
                    safe->edit([&] { for(auto& e:safe->chain.effects) if(e->id==id) { e->impulseResponse=asset.samples; e->impulseSampleRate=asset.sampleRate; } });
                    safe->status.setText("IR loaded: " + fc.getResult().getFileName(), juce::dontSendNotification);
                }
                catch(const std::exception& e) { safe->status.setText(e.what(), juce::dontSendNotification); }
            });
    }
    void choosePreset(bool loading)
    {
        chooser = std::make_unique<juce::FileChooser>(loading ? "Load preset" : "Save preset", juce::File(), "*.json");
        chooser->launchAsync(juce::FileBrowserComponent::canSelectFiles | (loading ? juce::FileBrowserComponent::openMode : juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting),
            [safe = juce::Component::SafePointer<MainComponent>(this), loading](const juce::FileChooser& fc)
            {
                if (!safe || fc.getResult() == juce::File()) return;
                try
                {
                    if (loading) safe->loadPreset(fc.getResult());
                    else multi_effects::app::savePreset(safe->chain, fc.getResult().withFileExtension("json"));
                    safe->status.setText("Preset " + juce::String(loading ? "loaded" : "saved"), juce::dontSendNotification);
                }
                catch (const std::exception& e) { safe->status.setText(e.what(), juce::dontSendNotification); }
            });
    }
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override
    {
        ready = false;
        try
        {
            const auto block = std::size_t(std::max(1, device->getCurrentBufferSizeSamples()));
            scratch.assign(block, 0); chain.prepare({device->getCurrentSampleRate(), block}); ready = true;
        }
        catch (...) { ready = false; }
    }
    void audioDeviceStopped() override { ready = false; }
    void audioDeviceIOCallbackWithContext(const float* const* input, int inputs, float* const* output, int outputs, int count, const juce::AudioIODeviceCallbackContext&) override
    {
        juce::ScopedNoDenormals noDenormals;
        if (!ready || scratch.empty()) { for (int c=0;c<outputs;++c) if(output[c]) juce::FloatVectorOperations::clear(output[c], count); return; }
        float peak = 0;
        for (int start = 0; start < count;)
        {
            const int n = std::min(count - start, int(scratch.size()));
            for (int i=0;i<n;++i) scratch[std::size_t(i)] = inputs > 0 && input[0] ? input[0][start+i] : 0;
            chain.process(scratch.data(), std::size_t(n));
            for (int i=0;i<n;++i)
            {
                const float target = muted.load(std::memory_order_relaxed) ? 0.f : .5f;
                outputGain += .002f * (target - outputGain);
                const float y = std::clamp(scratch[std::size_t(i)] * outputGain, -1.f, 1.f);
                peak = std::max(peak, std::abs(y));
                for (int c=0;c<outputs;++c) if(output[c]) output[c][start+i] = y;
            }
            start += n;
        }
        meter.store(peak, std::memory_order_relaxed);
        callbackCount.fetch_add(1, std::memory_order_relaxed);
    }
    void timerCallback() override
    {
        mute.setButtonText("Mute | " + juce::String(meter.load(), 3));
        title.setText("Multi Effects | Mono -> stereo | blocks " + juce::String(callbackCount.load())
                      + " | CPU " + juce::String(manager.getCpuUsage() * 100, 1) + "%", juce::dontSendNotification);
    }
    Chain chain;
    juce::AudioDeviceManager manager;
    juce::ListBox list;
    juce::AudioDeviceSelectorComponent devices;
    juce::TextButton up{"Up"}, down{"Down"}, remove{"Remove"}, add{"Add"}, save{"Save preset"}, load{"Load preset"}, irLoad{"Load cabinet IR"};
    juce::ToggleButton mute{"Mute"}, bypass{"Bypass effect"};
    juce::ComboBox types;
    juce::Label title, status;
    std::vector<std::unique_ptr<juce::Slider>> sliders;
    std::vector<std::unique_ptr<juce::Label>> labels;
    std::unique_ptr<juce::FileChooser> chooser;
    std::vector<float> scratch;
    std::atomic<bool> muted{true};
    std::atomic<float> meter{0};
    std::atomic<int> callbackCount{0};
    bool ready = false;
    float outputGain = 0;
};
class AppWindow final : public juce::DocumentWindow
{
public:
    AppWindow(const juce::String& name) : DocumentWindow(name, juce::Colours::darkgrey, allButtons) {}
    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
};
class Application final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "Multi Effects Prototype"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    void initialise(const juce::String&) override
    {
        window = std::make_unique<AppWindow>(getApplicationName());
        window->setUsingNativeTitleBar(true); window->setContentOwned(new MainComponent(), true); window->centreWithSize(1050, 760); window->setVisible(true);
    }
    void shutdown() override { window.reset(); }
    void systemRequestedQuit() override { quit(); }
private:
    std::unique_ptr<juce::DocumentWindow> window;
};
START_JUCE_APPLICATION(Application)
