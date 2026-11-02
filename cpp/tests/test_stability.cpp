#include "AllocationTracker.h"
#include "TestSupport.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <set>
#include <thread>
#if defined(__APPLE__)
#include <mach/mach.h>
#endif
using namespace multi_effects;

static void process(Chain& chain, float* data, std::size_t count)
{
    { allocation_tracker::ProcessScope scope; chain.process(data, count); }
    for (std::size_t i = 0; i < count; ++i) require(std::isfinite(data[i]), "NaN/Inf output");
    for (const auto& e : chain.effects) require(e->invalidSampleCount == 0, "non-finite sample masked by engine");
    require(allocation_tracker::processAllocations.load() == 0, "allocation during process");
    require(allocation_tracker::processFrees.load() == 0, "deallocation during process");
}
static void matrix()
{
    for (double rate : {44100., 48000., 96000.})
    {
        std::vector<float> expected(std::size_t(rate) + 37);
        auto reference = makeTestChain(); reference.prepare({rate, 512});
        for (std::size_t i = 0; i < expected.size(); ++i) expected[i] = testSignal(i, rate);
        process(reference, expected.data(), expected.size());
        for (std::size_t block : {64, 128, 256, 512})
        {
            auto candidate = makeTestChain(); candidate.prepare({rate, block});
            std::vector<float> actual(expected.size());
            for (std::size_t i = 0; i < actual.size(); ++i) actual[i] = testSignal(i, rate);
            candidate.process(nullptr, 0);
            for (std::size_t i = 0; i < actual.size(); i += block) process(candidate, actual.data() + i, std::min(block, actual.size() - i));
            require(actual == expected, "fixed block partition mismatch");
            // Reset and re-prepare the same object, including changed rate and max block size.
            candidate.prepare({rate == 96000 ? 44100. : 96000., 64});
            candidate.prepare({rate, block});
            for (std::size_t i = 0; i < actual.size(); ++i) actual[i] = testSignal(i, rate);
            for (std::size_t i = 0; i < actual.size();)
            {
                const auto n = std::min<std::size_t>(1 + i % 509, actual.size() - i);
                process(candidate, actual.data() + i, n); i += n;
            }
            require(actual == expected, "variable partition/reprepare mismatch");
            std::cout << "rate=" << rate << " block=" << block << " fixed/variable/reprepare exact PASS\n";
        }
    }
}
static void stress()
{
    auto chain = makeTestChain();
    std::array<float, 512> audio{};
    for (int cycle = 0; cycle < 1000; ++cycle)
    {
        // Mirrors GUI's stopped-callback ownership contract, not unsupported concurrent edits.
        const auto& definition = registry()[std::size_t(cycle) % registry().size()];
        chain.effects.push_back(createEffect(definition.type, "added_" + std::to_string(cycle)));
        auto* added = chain.effects.back().get();
        const std::size_t to = std::size_t(cycle) % chain.effects.size();
        chain.move(chain.effects.size() - 1, to);
        require(chain.effects[to].get() == added, "move changed instance identity");
        std::set<std::string> ids;
        for (auto& e : chain.effects)
        {
            require(ids.insert(e->id).second, "duplicate instance ID");
            e->bypass.store(cycle % 2);
            for (auto& p : e->parameters) p->set(cycle % 2 ? p->info.maximum : p->info.initial);
        }
        const double rate = cycle % 3 == 0 ? 44100. : cycle % 3 == 1 ? 48000. : 96000.;
        chain.prepare({rate, 512});
        for (std::size_t i = 0; i < audio.size(); ++i) audio[i] = testSignal(i + cycle * audio.size(), rate);
        process(chain, audio.data(), audio.size());
        chain.effects.erase(chain.effects.begin() + to);
        require(chain.effects.size() == 10, "remove count");
        chain.prepare({rate, 64}); process(chain, audio.data(), 64);
    }
    requireRejected([&]{ chain.move(1000, 0); }, "invalid move not rejected");
    chain.effects.clear(); process(chain, audio.data(), audio.size());
    const auto bytes = allocation_tracker::liveBytes.load();
    const auto blocks = allocation_tracker::liveBlocks.load();
    for (int i = 0; i < 50; ++i)
    {
        { auto temporary = makeTestChain(); temporary.prepare({48000, 128}); process(temporary, audio.data(), 128); }
        require(allocation_tracker::liveBytes.load() == bytes && allocation_tracker::liveBlocks.load() == blocks, "destroyed chain retained C++ allocations");
    }
    std::cout << "1000 add/remove/reorder/prepare cycles + bypass/parameter changes; 50 complete lifetimes PASS\n";
}
static void controls()
{
    auto chain = makeTestChain(); chain.prepare({48000, 64});
    std::atomic<bool> stop{false};
    std::thread writer([&] {
        std::size_t n = 0;
        while (!stop.load())
        {
            for (auto& e : chain.effects) { e->bypass.store(n % 2); for (auto& p : e->parameters) p->set(n % 2 ? p->info.initial : p->info.maximum); }
            ++n;
        }
    });
    try
    {
        std::array<float, 64> data{};
        for (int block = 0; block < 10000; ++block) { data.fill(.1f); process(chain, data.data(), data.size()); }
    }
    catch (...) { stop.store(true); writer.join(); throw; }
    stop.store(true); writer.join();
    std::cout << "10000 blocks with concurrent atomic parameter/bypass writer PASS\n";
}
static std::size_t residentBytes()
{
#if defined(__APPLE__)
    mach_task_basic_info_data_t info{}; mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS) return info.resident_size;
#endif
    return 0;
}
static void longRun(int seconds)
{
    for (double rate : {44100., 48000., 96000.})
    {
        auto chain = makeTestChain(); chain.prepare({rate, 512});
        std::array<float, 512> data{};
        // Warm up math/runtime pages before measuring resident memory.
        data.fill(.1f); process(chain, data.data(), data.size());
        const auto baseline = allocation_tracker::liveBytes.load();
        const auto baselineBlocks = allocation_tracker::liveBlocks.load();
        const auto rssStart = residentBytes(); auto rssMax = rssStart;
        const auto total = std::size_t(rate * seconds);
        double energy = 0; float peak = 0;
        std::size_t blocks = 0;
        for (std::size_t position = 0; position < total;)
        {
            const std::size_t sizes[] = {64, 128, 256, 512};
            const auto n = std::min(sizes[blocks % 4], total - position);
            for (std::size_t i = 0; i < n; ++i) data[i] = testSignal(position + i, rate);
            if (blocks % 4096 == 0)
            {
                for (auto& e : chain.effects) e->bypass.store((blocks / 4096) % 2);
                chain.effects[7]->parameter("mix")->set((blocks / 4096) % 2 ? .4f : .2f);
            }
            process(chain, data.data(), n);
            for (std::size_t i = 0; i < n; ++i) { energy += double(data[i]) * data[i]; peak = std::max(peak, std::abs(data[i])); }
            require(allocation_tracker::liveBytes.load() == baseline && allocation_tracker::liveBlocks.load() == baselineBlocks, "live C++ memory growth");
            if (blocks % 10000 == 0) rssMax = std::max(rssMax, residentBytes());
            position += n; ++blocks;
        }
        require(energy > .01 && peak < 16, "unexpected silence/unbounded output");
        require(rssMax <= rssStart + 32 * 1024 * 1024, "resident memory grew over 32 MiB after warmup");
        std::cout << "long-run rate=" << rate << " seconds=" << seconds << " samples=" << total
                  << " blocks=" << blocks << " peak=" << peak << " live_byte_delta=0 allocations=0 frees=0 rss_growth="
                  << (rssMax - rssStart) << " PASS" << std::endl;
    }
}
int main(int argc, char** argv)
{
    try
    {
        const std::string mode = argc > 1 ? argv[1] : "matrix";
        if (mode == "matrix") matrix();
        else if (mode == "stress") stress();
        else if (mode == "controls") controls();
        else if (mode == "long") longRun(argc > 2 ? std::stoi(argv[2]) : 600);
        else throw std::runtime_error("unknown mode");
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
