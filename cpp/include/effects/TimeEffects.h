#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace multi_effects
{
class Delay
{
public:
    void prepare(double rate)
    {
        if (!std::isfinite(rate) || rate < 8000 || rate > 192000) throw std::invalid_argument("sample rate");
        rate_ = rate;
        buffer_.assign(static_cast<std::size_t>(rate * 2) + 1, 0);
        reset();
    }
    void reset() { std::fill(buffer_.begin(), buffer_.end(), 0); index_ = 0; currentDelay_ = targetDelay_ = 0; fadePosition_ = 0; }
    float processSample(float x, float ms = 400, float feedback = .35f, float mix = .3f)
    {
        if (buffer_.empty()) return x;
        const auto delay = std::clamp<std::size_t>(static_cast<std::size_t>(rate_ * std::clamp(ms, 1.f, 2000.f) / 1000), 1, buffer_.size() - 1);
        if (!currentDelay_) currentDelay_ = targetDelay_ = delay;
        const auto fadeLength = std::max<std::size_t>(1, std::size_t(rate_ * .01));
        // Freeze both taps during each 10 ms crossfade; coalesce newer requests.
        // A constant delay retains the exact Python/v0.3 integer-tap result.
        if (currentDelay_ == targetDelay_ && delay != currentDelay_) { targetDelay_ = delay; fadePosition_ = 0; }
        float delayed = buffer_[(index_ + buffer_.size() - currentDelay_) % buffer_.size()];
        if (targetDelay_ != currentDelay_)
        {
            const float next = buffer_[(index_ + buffer_.size() - targetDelay_) % buffer_.size()];
            const float blend = float(fadePosition_) / float(fadeLength);
            delayed += blend * (next - delayed);
            if (++fadePosition_ >= fadeLength) currentDelay_ = targetDelay_;
        }
        const float echo = delayed * std::clamp(feedback, 0.f, .95f);
        buffer_[index_] = x + echo;
        index_ = (index_ + 1) % buffer_.size();
        return x * (1 - mix) + echo * mix; // Python v0.1 wet semantics
    }
private:
    double rate_ = 44100;
    std::vector<float> buffer_;
    std::size_t index_ = 0, currentDelay_ = 0, targetDelay_ = 0, fadePosition_ = 0;
};

class Reverb
{
    struct Line
    {
        std::vector<float> data;
        std::size_t index = 0;
        float store = 0;
        void reset() { std::fill(data.begin(), data.end(), 0); index = 0; store = 0; }
        float read() const { return data[index]; }
        void write(float x) { data[index] = x; index = (index + 1) % data.size(); }
    };
public:
    void prepare(double rate)
    {
        if (!std::isfinite(rate) || rate < 8000 || rate > 192000) throw std::invalid_argument("sample rate");
        constexpr double times[] = {29.7, 37.1, 41.1, 43.7, 5.0, 1.7};
        for (std::size_t i = 0; i < lines_.size(); ++i)
            lines_[i].data.assign(std::max<std::size_t>(1, static_cast<std::size_t>(rate * times[i] / 1000)), 0);
        reset();
    }
    void reset() { for (auto& line : lines_) line.reset(); }
    float processSample(float x, float room = .6f, float damping = .4f, float mix = .3f)
    {
        if (lines_[0].data.empty()) return x;
        float wet = 0;
        for (int i = 0; i < 4; ++i)
        {
            auto& line = lines_[i];
            const float delayed = line.read();
            line.store = delayed * (1 - damping) + line.store * damping;
            line.write(x + line.store * (.2f + room * .72f));
            wet += delayed;
        }
        wet *= .25f;
        for (int i = 4; i < 6; ++i)
        {
            auto& line = lines_[i];
            const float delayed = line.read();
            line.write(wet + delayed * .5f);
            wet = delayed - wet;
        }
        return x * (1 - mix) + wet * mix;
    }
private:
    std::array<Line, 6> lines_;
};

class Tremolo
{
public:
    void prepare(double rate) { rate_ = rate; reset(); }
    void reset() { phase_ = 0; }
    float processSample(float x, float hz = 4, float depth = .5f)
    {
        const float y = x * (1 - depth * float((std::sin(phase_) + 1) * .5));
        phase_ += 6.283185307179586 * hz / rate_;
        if (phase_ >= 6.283185307179586) phase_ -= 6.283185307179586;
        return y;
    }
private:
    double rate_ = 44100, phase_ = 0;
};
}
