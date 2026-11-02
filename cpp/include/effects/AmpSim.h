#pragma once
#include "effects/Filters.h"
#include <cmath>
#include <algorithm>

namespace multi_effects
{
class AmpSim
{
public:
    void prepare(float rate, float bass = 0, float mid = 0, float treble = 0,
                 float presence = 0, float lowCut = 70, float highCut = 9000)
    {
        low_.prepare(rate, std::min(lowCut, rate * .45f));
        bass_.setLowShelf(rate, std::min(180.f, rate * .45f), bass);
        mid_.setPeaking(rate, std::min(800.f, rate * .45f), mid, .8f);
        treble_.setHighShelf(rate, std::min(3500.f, rate * .45f), treble);
        presence_.setHighShelf(rate, std::min(4000.f, rate * .45f), presence);
        high_.setLowPass(rate, std::min(highCut, rate * .45f));
    }
    void reset() { low_.reset(); bass_.reset(); mid_.reset(); treble_.reset(); presence_.reset(); high_.reset(); }
    float processSample(float x, float gainDb = 12, float drive = 1.5f, float bias = .05f,
                        float masterDb = -3, float powerDrive = 1.2f, float outputDb = -6)
    {
        x = low_.processSample(x) * std::pow(10.f, gainDb / 20);
        x = saturate(x, drive, bias);
        x = saturate(x * 1.3f, drive * .8f, -bias * .5f);
        x = treble_.processSample(mid_.processSample(bass_.processSample(x)));
        x = std::tanh(x * std::pow(10.f, masterDb / 20) * powerDrive);
        x = presence_.processSample(x) * std::pow(10.f, outputDb / 20);
        return high_.processSample(x);
    }
private:
    static float saturate(float x, float drive, float bias) { return std::tanh((x + bias) * drive) - std::tanh(bias * drive); }
    // Low cutoff at high sample rates is sensitive to coefficient rounding.
    // Keep this recursive section in double; the public audio path remains float.
    struct LowCut
    {
        double b0=1, b1=0, b2=0, a1=0, a2=0, x1=0, x2=0, y1=0, y2=0;
        void prepare(double sr, double hz)
        {
            const double w = 6.283185307179586 * hz / sr;
            const double c = std::cos(w), alpha = std::sin(w) / (2 * .707);
            b0 = (1+c)/2/(1+alpha); b1 = -(1+c)/(1+alpha); b2 = b0;
            a1 = -2*c/(1+alpha); a2 = (1-alpha)/(1+alpha); reset();
        }
        void reset() { x1=x2=y1=y2=0; }
        float processSample(float x)
        {
            const double y = b0*x+b1*x1+b2*x2-a1*y1-a2*y2;
            x2=x1; x1=x; y2=y1; y1=y; return float(y);
        }
    } low_;
    Filter bass_, mid_, treble_, presence_, high_;
};
}
