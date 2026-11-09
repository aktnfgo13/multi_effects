#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace multi_effects {
// Normalized antiparallel piecewise-linear shunts in a resistive return.
// Solve |u| = v + sum(g[i] * max(v-t[i], 0)) in its active affine segment.
// The RC retains signed return charge. Below conduction there is no signal EQ.
class GrainFeedback {
public:
    void prepare(double rate) noexcept {
        highRate=rate;
        senseStep=float(-std::expm1(-6.283185307179586*2400/rate));
        leak=float(std::exp(-1/(.006*rate)));
        reset();
    }
    void reset() noexcept { charge=sense=0; }
    void configure(float position) noexcept {
        const float knob=position;
        position*=.35f+.65f*position; // Delay the dominant path takeover until the upper half.
        const float spread=1-position;
        slope={.65f*spread,5.8f-4.6f*spread,2.35f*spread};
        // Closer knees overlap at Fine; the dominant Coarse path fires later and harder.
        threshold={.10f,.12f+.075f*position,.145f+.16f*position};
        float sum=0,offset=0;
        for (int i=0;i<3;++i) {
            onset[i]=threshold[i]*(1+sum)-offset;
            sum+=slope[i];offset+=slope[i]*threshold[i];
            reciprocal[i]=1/(1+sum);intercept[i]=offset;
        }
        chargeStep=float(-std::expm1(-1/(highRate*(.000135+.0034*position*position))));
        // Grain 25 anchors the approved A/B transfer for existing presets.
        const float upper=std::max(0.f,(knob-.25f)/.75f);
        depth=knob<.25f ? .92f*(1-4*knob) : .65f*upper*(1+.55f*(1-upper));
    }
    struct Node { float magnitude, current; };
    Node solve(float input) const noexcept {
        const float sensed=sense+senseStep*(input-sense);
        const float u=sensed-charge,magnitude=std::abs(u);
        int path=magnitude>onset[2]?2:(magnitude>onset[1]?1:(magnitude>onset[0]?0:-1));
        if (path<0) return {magnitude,0};
        const float v=(magnitude+intercept[path])*reciprocal[path];
        return {v,std::copysign(magnitude-v,u)};
    }
    float process(float input) noexcept {
        const float current=solve(input).current;
        sense+=senseStep*(input-sense);
        charge=leak*charge+chargeStep*current;
        return input-depth*current;
    }
    // Offline probe reads a snapshot before advancing the same cell.
    std::array<float,3> currents(float input) const noexcept {
        const float v=solve(input).magnitude;
        return {slope[0]*std::max(0.f,v-threshold[0]),slope[1]*std::max(0.f,v-threshold[1]),
                slope[2]*std::max(0.f,v-threshold[2])};
    }
private:
    double highRate=48000;
    float sense=0,senseStep=0,charge=0,leak=0,chargeStep=0,depth=0;
    std::array<float,3> slope{},threshold{},onset{},intercept{},reciprocal{};
};
}
