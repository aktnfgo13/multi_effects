#pragma once
#include "engine/AudioIO.h"
#include <mutex>

namespace multi_effects::app
{
// Control operations require stopped device callbacks. GUI reads/atomic updates
// share only this control mutex; process() never takes it. No GUI work in lifecycle.
class Runtime
{
public:
    Chain chain;
    MonoAudioIO io;
    std::mutex control;
    std::atomic<bool> ready{false};
    std::atomic<unsigned> generation{0};
    std::atomic<int> preferredInput{0};
    ProcessSpec spec{48000,128};
    std::string error; // Protected by control, formatted outside audio processing.
    std::vector<int> channels;

    bool prepare(ProcessSpec next, std::vector<int> active)
    {
        std::lock_guard<std::mutex> lock(control);
        ready=false;
        try
        {
            auto candidate=cloneConfiguration(chain);
            io.selectedInput=preferredInput.load();
            io.prepare(candidate,next,active);
            chain.effects.swap(candidate.effects); spec=next; channels=std::move(active);
            error.clear();ready=true;++generation;return true;
        }
        catch(const std::exception& e) {error=std::string("Prepare failed: ")+e.what();io.stop();return false;}
        catch(...) {error="Prepare failed: unknown error";io.stop();return false;}
    }
    // Validate before publishing: failure preserves all previous effect instances.
    bool replace(Chain candidate)
    {
        std::lock_guard<std::mutex> lock(control);
        try {candidate.prepare(spec);chain.effects.swap(candidate.effects);error.clear();++generation;return true;}
        catch(const std::exception& e){error=std::string("Configuration rejected: ")+e.what();return false;}
    }
    void stop() noexcept {ready=false;io.stop();}
    void process(AudioBlock block) noexcept {
        if(!ready.load(std::memory_order_relaxed)) {for(int c=0;block.outputs&&c<block.outputCount;++c)if(block.outputs[c])std::fill_n(block.outputs[c],block.frames,0.f);return;}
        io.process(chain,block);
    }
};
}
