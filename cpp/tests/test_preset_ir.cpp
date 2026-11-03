#include "../app/PresetIO.h"
#include "TestSupport.h"
#include <algorithm>
#include <iostream>
using namespace multi_effects;

static void compare(const Chain& a, const Chain& b)
{
    require(a.effects.size() == b.effects.size(), "effect count");
    for (std::size_t i = 0; i < a.effects.size(); ++i)
    {
        const auto& x = *a.effects[i]; const auto& y = *b.effects[i];
        require(&x != &y, "reload must create independent instances");
        require(x.id == y.id && x.type == y.type && x.name == y.name, "identity/order/name");
        require(x.bypass.load() == y.bypass.load(), "bypass");
        require(x.parameters.size() == y.parameters.size(), "parameter count");
        for (std::size_t p = 0; p < x.parameters.size(); ++p)
            require(x.parameters[p]->info.id == y.parameters[p]->info.id
                    && x.parameters[p]->target.load() == y.parameters[p]->target.load(), "parameter exact restoration");
        require(x.impulseResponse == y.impulseResponse && x.impulseSampleRate == y.impulseSampleRate, "IR samples/source rate");
    }
}
static void writeWav(const juce::File& file, bool silent = false, int count = 1025)
{
    juce::WavAudioFormat format;
    auto stream = file.createOutputStream();
    require(stream != nullptr, "create WAV stream");
    std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.get(), 48000, 2, 24, {}, 0));
    require(writer != nullptr, "create WAV writer"); stream.release();
    juce::AudioBuffer<float> audio(2, count); audio.clear();
    if (!silent) for (int i = 0; i < count; ++i)
    {
        audio.setSample(0, i, float(.6 * std::exp(-i / 90.) * std::cos(i * .13) + .01));
        audio.setSample(1, i, float(.4 * std::exp(-i / 120.) * std::sin(i * .11) - .02));
    }
    require(writer->writeFromAudioSampleBuffer(audio, 0, count), "write WAV");
}
int main()
{
    const auto directory = juce::File::getCurrentWorkingDirectory()
        .getNonexistentChildFile("multi-effects-regression", "", false);
    const auto created = directory.createDirectory();
    if (created.failed()) { std::cerr << directory.getFullPathName() << ": " << created.getErrorMessage() << "\n"; return 1; }
    struct Cleanup { juce::File file; ~Cleanup() { file.deleteRecursively(); } } cleanup{directory};
    try
    {
        const auto wav = directory.getChildFile("ir.wav"); writeWav(wav);
        const auto asset = app::loadImpulse(wav);
        require(asset.samples.size() == 1025 && asset.sampleRate == 48000, "WAV metadata");
        float peak = 0; double mean = 0;
        for (auto x : asset.samples) { require(std::isfinite(x), "finite IR"); peak = std::max(peak, std::abs(x)); mean += x; }
        require(peak == 1 && std::abs(mean / asset.samples.size()) < 1e-7, "IR DC removal/normalization");
        auto source = makeTestChain();
        source.effects.push_back(createEffect("delay", "delay_2")); // Same type, independent instance.
        for (std::size_t i = 0; i < source.effects.size(); ++i)
        {
            auto& e = *source.effects[i]; e.name = "Custom " + std::to_string(i); e.bypass.store(i % 3 == 1);
            for (auto& p : e.parameters) p->set(p->info.minimum + (.13f + .037f * float(i)) * (p->info.maximum - p->info.minimum));
            if (e.type == "cabinet") { e.impulseResponse = asset.samples; e.impulseSampleRate = asset.sampleRate; }
        }
        source.move(2, 8);
        const auto preset = directory.getChildFile("preset.json"); app::savePreset(source, preset);
        require(wav.deleteFile(), "remove source WAV to test self-contained preset");
        auto restored = app::loadPreset(preset); compare(source, restored);
        for (const double rate : {44100., 48000., 96000.}) for (const std::size_t block : {64, 128, 256, 512})
        {
            source.prepare({rate, block}); restored.prepare({rate, block});
            std::vector<float> a(16387), b(a.size());
            for (std::size_t i = 0; i < a.size(); ++i) a[i] = b[i] = testSignal(i, rate);
            source.process(a.data(), a.size());
            for (std::size_t i = 0; i < b.size(); i += block) restored.process(b.data() + i, std::min(block, b.size() - i));
            require(a == b, "restored processing/partition must match exactly");
        }
        // Default synthetic cabinet and empty chain must also survive round trips.
        auto defaults = makeTestChain(); app::savePreset(defaults, preset); compare(defaults, app::loadPreset(preset));
        Chain empty; app::savePreset(empty, preset); require(app::loadPreset(preset).effects.empty(), "empty chain");
        // Repeated complete disk roundtrips detect cumulative float/IR conversion drift.
        for (int i = 0; i < 25; ++i) { app::savePreset(restored, preset); restored = app::loadPreset(preset); compare(source, restored); }
        const auto valid = juce::JSON::toString(app::serializePreset(source));
        auto reject = [&](juce::var value, const char* label) { require(preset.replaceWithText(juce::JSON::toString(value)), "write invalid fixture"); requireRejected([&]{ app::loadPreset(preset); }, label); };
        auto json = juce::JSON::parse(valid); json.getDynamicObject()->setProperty("version", 999); reject(json, "unknown version accepted");
        json = juce::JSON::parse(valid); auto* effects = json["effects"].getArray(); effects->add((*effects)[0]); reject(json, "duplicate ID accepted");
        json = juce::JSON::parse(valid);
        for (auto& e : *json["effects"].getArray()) if (e["type"].toString() == "cabinet") { e["ir"].getArray()->set(0, "not a number"); }
        reject(json, "nonnumeric IR accepted");
        json = juce::JSON::parse(valid);
        for (auto& e : *json["effects"].getArray()) if (e["type"].toString() == "cabinet") e.getDynamicObject()->setProperty("ir_sample_rate", 0);
        reject(json, "zero IR rate accepted");
        json = juce::JSON::parse(valid);
        (*json["effects"].getArray())[0].getDynamicObject()->setProperty("parameters", "invalid");
        reject(json, "nonnumeric parameters object accepted");
        json = juce::JSON::parse(valid);
        for (auto& e : *json["effects"].getArray()) if (e["type"].toString() == "cabinet") e["ir"].getArray()->set(0, 1e300);
        reject(json, "float-overflowing IR accepted");
        json = juce::JSON::parse(valid);
        for (auto& e : *json["effects"].getArray()) if (e["type"].toString() == "cabinet") e.getDynamicObject()->setProperty("ir", "wrong type");
        reject(json, "non-array IR accepted");
        // Loading invalid data cannot mutate the live chain: commit only a successfully parsed candidate.
        auto live = makeTestChain();
        const auto before = juce::JSON::toString(app::serializePreset(live));
        requireRejected([&]{ auto candidate = app::loadPreset(preset); live.effects.swap(candidate.effects); }, "invalid preset installed");
        require(before == juce::JSON::toString(app::serializePreset(live)), "failed load changed live chain");
        // Supported Python v2 shape remains accepted without requiring the v3 schema key.
        json = juce::JSON::parse(valid); json.getDynamicObject()->setProperty("version", 2);
        json.getDynamicObject()->removeProperty("schema");
        require(preset.replaceWithText(juce::JSON::toString(json)), "write v2 fixture");
        compare(source, app::loadPreset(preset));
        for(int mode:{0,1,2}) {
            auto modes=makeTestChain();for(auto& e:modes.effects)if(auto* p=e->parameter("oversampling"))p->set(float(mode));
            app::savePreset(modes,directory.getChildFile("modes.json"));
            auto loaded=app::loadPreset(directory.getChildFile("modes.json"));compare(modes,loaded);
            loaded.prepare({48000,128});
            for(auto& e:loaded.effects)if(e->parameter("oversampling"))require(e->latencySamples()==(mode==0?0:e->type=="amp_sim"?128:64),"mode/latency restoration");
        }
        // Old v2/v3 presets omit the new discrete mode: retain legacy 1x sound.
        for(int version:{2,3}) {
            json=juce::JSON::parse(valid);json.getDynamicObject()->setProperty("version",version);
            for(auto& node:*json["effects"].getArray())node["parameters"].getDynamicObject()->removeProperty("oversampling");
            require(preset.replaceWithText(juce::JSON::toString(json)),"legacy preset write");
            auto old=app::loadPreset(preset);
            for(auto& e:old.effects)if(auto* p=e->parameter("oversampling"))require(p->target==0,"legacy 1x default");
        }
        require(preset.replaceWithText("{broken"), "write malformed JSON"); requireRejected([&]{ app::loadPreset(preset); }, "malformed JSON accepted");
        writeWav(wav, true); requireRejected([&]{ app::loadImpulse(wav); }, "silent IR accepted");
        const auto longWav = directory.getChildFile("long.wav"); writeWav(longWav, false, 32769);
        requireRejected([&]{ app::loadImpulse(longWav); }, "oversize IR accepted");
        requireRejected([&]{ app::loadImpulse(directory.getChildFile("missing.wav")); }, "missing IR accepted");
        auto large = createEffect("cabinet", "large");
        large->impulseResponse.assign(32768, .001f); large->impulseSampleRate = 44100;
        requireRejected([&]{ large->prepare({96000, 64}); }, "oversize resampled IR accepted");
        std::cout << "Preset/IR: exact instances/order/bypass/parameters/embedded IR; 25 disk roundtrips; 12 rate/block cases; invalid inputs PASS\n";
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
