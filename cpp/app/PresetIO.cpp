#include "PresetIO.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace multi_effects::app
{
namespace
{
bool isNumber(const juce::var& value)
{
    return value.isDouble() || value.isInt() || value.isInt64();
}
float readFiniteSample(const juce::var& value)
{
    if (!isNumber(value) || !std::isfinite(double(value)) || !std::isfinite(float(double(value))))
        throw std::runtime_error("Expected a finite numeric sample/parameter");
    return float(double(value));
}
} // namespace
juce::var serializePreset(const Chain& chain)
{
    auto root = std::make_unique<juce::DynamicObject>(); root->setProperty("version", 3); root->setProperty("schema", "multi-effects");
    juce::Array<juce::var> effects;
    for (const auto& e : chain.effects)
    {
        auto item = std::make_unique<juce::DynamicObject>();
        item->setProperty("id", juce::String(e->id)); item->setProperty("type", juce::String(e->type)); item->setProperty("name", juce::String(e->name)); item->setProperty("bypass", e->bypass.load());
        auto params = std::make_unique<juce::DynamicObject>();
        for (const auto& p : e->parameters) params->setProperty(juce::Identifier(juce::String(p->info.id)), double(p->target.load()));
        if (!e->impulseResponse.empty())
        {
            juce::Array<juce::var> ir; for (float x : e->impulseResponse) ir.add(double(x));
            item->setProperty("ir", ir); item->setProperty("ir_sample_rate", e->impulseSampleRate);
        }
        item->setProperty("parameters", juce::var(params.release())); effects.add(juce::var(item.release()));
    }
    root->setProperty("effects", effects); return juce::var(root.release());
}
void savePreset(const Chain& chain, const juce::File& file)
{
    if (!file.replaceWithText(juce::JSON::toString(serializePreset(chain))))
        throw std::runtime_error("Cannot save preset");
}
Chain loadPreset(const juce::File& file)
{
    juce::var json;
    const auto parsed = juce::JSON::parse(file.loadFileAsString(), json);
    if (parsed.failed()) throw std::runtime_error(parsed.getErrorMessage().toStdString());
    if (!(json["version"].isInt() || json["version"].isInt64())) throw std::runtime_error("Invalid preset version");
    const int version = int(json["version"]);
    if (version == 3 && json["schema"].toString() != "multi-effects") throw std::runtime_error("Unknown preset schema");
    if ((version != 2 && version != 3) || !json["effects"].isArray()) throw std::runtime_error("Expected preset version 2 or 3");
    Chain replacement; std::set<std::string> ids;
    for (const auto& value : *json["effects"].getArray())
    {
        if (replacement.effects.size() >= 24) throw std::runtime_error("Maximum 24 effects");
        if (!value["id"].isString() || !value["type"].isString()) throw std::runtime_error("Invalid effect identity");
        if (value.hasProperty("bypass") && !value["bypass"].isBool()) throw std::runtime_error("Invalid bypass value");
        if (value.hasProperty("parameters") && !value["parameters"].isObject()) throw std::runtime_error("Invalid parameters object");
        if (value.hasProperty("ir") && !value["ir"].isArray()) throw std::runtime_error("Invalid IR array");
        const auto id = value["id"].toString().toStdString();
        if (id.empty() || !ids.insert(id).second) throw std::runtime_error("Invalid/duplicate effect ID");
        auto e = createEffect(value["type"].toString().toStdString(), id);
        if (value["name"].isString() && value["name"].toString().isNotEmpty()) e->name = value["name"].toString().toStdString();
        e->bypass.store(bool(value["bypass"]));
        if (auto* params = value["parameters"].getDynamicObject())
            for (const auto& prop : params->getProperties())
            {
                auto* p = e->parameter(prop.name.toString().toStdString());
                if (!p) throw std::runtime_error("Unsupported parameter: " + prop.name.toString().toStdString());
                p->set(readFiniteSample(prop.value));
            }
        if (auto* ir = value["ir"].getArray())
        {
            if (e->type != "cabinet" || ir->isEmpty() || ir->size() > 32768) throw std::runtime_error("Invalid embedded IR");
            if (!isNumber(value["ir_sample_rate"])) throw std::runtime_error("Invalid IR sample rate");
            e->impulseSampleRate = double(value["ir_sample_rate"]);
            if (!std::isfinite(e->impulseSampleRate) || e->impulseSampleRate < 8000 || e->impulseSampleRate > 192000)
                throw std::runtime_error("Invalid IR sample rate");
            for (const auto& x : *ir) e->impulseResponse.push_back(readFiniteSample(x));
        }
        replacement.effects.push_back(std::move(e));
    }
    return replacement;
}
ImpulseAsset loadImpulse(const juce::File& file)
{
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (!reader || reader->lengthInSamples < 1 || reader->lengthInSamples > 32768) throw std::runtime_error("IR must have 1..32768 samples");
    if (reader->numChannels < 1 || reader->numChannels > 2 || !std::isfinite(reader->sampleRate)
        || reader->sampleRate < 8000 || reader->sampleRate > 192000) throw std::runtime_error("Unsupported IR channels/sample rate");
    juce::AudioBuffer<float> buffer(int(reader->numChannels), int(reader->lengthInSamples));
    if (!reader->read(&buffer, 0, buffer.getNumSamples(), 0, true, true)) throw std::runtime_error("IR read failed");
    std::vector<float> ir(std::size_t(buffer.getNumSamples()), 0);
    double mean = 0;
    for (int i=0;i<buffer.getNumSamples();++i) { for(int c=0;c<buffer.getNumChannels();++c) ir[std::size_t(i)] += buffer.getSample(c,i)/float(buffer.getNumChannels()); mean += ir[std::size_t(i)]; }
    mean /= double(ir.size()); float peak = 0;
    for(auto& x:ir) { x -= float(mean); if(!std::isfinite(x)) throw std::runtime_error("Non-finite IR"); peak=std::max(peak,std::abs(x)); }
    if(peak <= 0) throw std::runtime_error("IR has no signal after DC removal");
    for(auto& x:ir) x /= peak;
    return {std::move(ir), reader->sampleRate};
}
}
