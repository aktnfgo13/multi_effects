#pragma once
#include "effects/Oversampler.h"
#include "effects/GrainFeedback.h"
namespace multi_effects {
// Provisional analytical drive, not a model of a commercial pedal. Single-channel
// fixed state; Engine copies a freshly prepared instance for independent stereo.
class CustomDrive {
public:
    struct Controls { float drive=50,tone=50,crunch=50,headroom=50,grain=25,level=-6,mix=1; };
    void prepare(double rate,int factor) {
        if(!std::isfinite(rate)||rate<8000||rate>192000)throw std::invalid_argument("CustomDrive sample rate");
        os.prepare(factor);sr=rate;highRate=rate*factor;rampSamples=std::max(1,int(std::round(rate*.020)));
        preSplit.prepare(rate,1400);postSplit.prepare(rate,1800);postLow.prepare(rate,std::min(10000.,rate*.42));
        dc.prepare(rate,12);inputLow.prepare(rate,70);
        memoryA=coefficient(highRate,.008);textureA=coefficient(highRate,.025);
        interA=float(std::exp(-6.283185307179586*std::min(11000.,rate*.42)/highRate));
        feedback.prepare(highRate);cached=false;reset();
    }
    void reset() noexcept {
        mixInitialized=false; remaining=0;
        os.reset();dry.reset(os.latencySamples());inputLow.reset();preSplit.reset();postSplit.reset();postLow.reset();dc.reset();
        feedback.reset();envelope=memory=texture=interState=0;
    }
    // Last validated controls used by update() on this audio lane.
    float appliedGrain() const noexcept { return cached ? last.grain : -1.f; }
    float appliedHeadroom() const noexcept { return cached ? last.headroom : -1.f; }
    float appliedMix() const noexcept { return mixInitialized ? mix : -1.f; }
    int latencySamples() const noexcept {return os.latencySamples();}
    float processSample(float input,const Controls& controls) noexcept {
        update(controls);
        // Abnormal overload guard only (>36 dBFS); no normal-range limiter/normalizer.
        const float x=std::isfinite(input)?std::clamp(input,-64.f,64.f):0.f;
        const float aligned=dry.process(std::isfinite(input) ? input : 0.f);
        float wet=x-inputLow.process(x);
        const float low=preSplit.process(wet);wet=low+preEmphasis*(wet-low);
        wet=os.processSample(wet*driveGain,[this](float v){return island(v);});
        const float bass=postSplit.process(wet);wet=postLow.process(bass+postTilt*(wet-bass));
        wet-=dc.process(wet);wet*=outputGain;
        // Preserve the approved full-wet floating-point operation, including its guard.
        const float referenceDry=std::clamp(aligned,-64.f,64.f);
        if (mix == 1.f) return referenceDry + 1.f*(wet-referenceDry);
        if (mix == 0.f) return aligned;
        return aligned+mix*(wet-aligned);
    }
private:
    struct LowPass {
        double state=0,a=0;
        void prepare(double rate,double hz){a=std::exp(-6.283185307179586*hz/rate);state=0;}
        void reset(){state=0;}
        float process(float x){state=x+a*(state-x);return float(state);}
    } inputLow,preSplit,postSplit,postLow,dc;
    static float coefficient(double rate,double seconds){return float(std::exp(-1/(rate*seconds)));}
    // Odd smooth knees with unity small-signal slope and unit rails.
    // Crunch blends rounded p=2 and firm p=8, plus serial-stage contribution.
    // Reciprocal evaluation above unity avoids high-power overflow.
    static float curve(float x,float hardness) noexcept {
        const float squared=x*x;
        const float soft=x/std::sqrt(1+squared);
        const float q=std::abs(x)>1 ? 1/std::abs(x) : std::abs(x);
        const float q2=q*q,q4=q2*q2;
        const float denominator=std::sqrt(std::sqrt(std::sqrt(1+q4*q4)));
        const float firm=(std::abs(x)>1 ? std::copysign(1.f,x) : x)/denominator;
        return soft+hardness*(firm-soft);
    }
    float island(float v) noexcept {
        const float detector=std::min(std::abs(v),8.f);
        const float a=detector>envelope?attack:release;
        envelope=detector+a*(envelope-detector);
        // Compress stage input rather than lowering the output rail: avoids
        // static foldback (louder playing producing a lower saturation ceiling).
        const float conditioned=v/(1+sag*envelope);
        const float knee=baseHeadroom*(3.f-2.f*hardness);
        const float sensed=conditioned/baseHeadroom;
        const float shaped=conditioned+baseHeadroom*(feedback.process(sensed)-sensed);
        const float stageA=knee*curve(shaped/knee,hardness);
        interState=stageA+interA*(interState-stageA);
        memory=interState+memoryA*(memory-interState);
        // Legacy operating point shared by all Grain settings.
        constexpr float grain=.25f;
        const float bias=grain*(.9f+.18f*envelope/(1+envelope)+.4f*memory/(1+std::abs(memory)));
        const float rail=secondThreshold*(1-.45f*.25f);
        const float excitation=stageBGain*(1+1.05f*hardness);
        const float stageB=rail*(curve(interState*excitation/rail+bias,hardness)-curve(bias,hardness));
        const float rectified=stageB*stageB/(1+stageB*stageB);
        texture=rectified+textureA*(texture-rectified);
        const float contribution=1-(1-.85f*hardness)*(1-.98f*.25f);
        const float interaction=stageB+.65f*grain*(rectified-texture);
        return .7f*(interState+contribution*(interaction-interState));
    }
    // Provisional fixed feed-forward curve; never observes signal RMS/peak.
    static float driveCompensationDb(float normalizedDrive) noexcept {
        return 10.f-14.f*(1-std::exp(-4.f*normalizedDrive));
    }
    void update(const Controls& c) noexcept {
        const auto finite=[](float v,float fallback,float lo,float hi){return std::isfinite(v)?std::clamp(v,lo,hi):fallback;};
        const Controls next{finite(c.drive,50,0,100),finite(c.tone,50,0,100),finite(c.crunch,50,0,100),finite(c.headroom,50,0,100),finite(c.grain,25,0,100),finite(c.level,-6,-24,12),finite(c.mix,1,0,1)};
        if(!cached||next.drive!=last.drive||next.tone!=last.tone||next.headroom!=last.headroom||next.level!=last.level||next.grain!=last.grain||next.crunch!=last.crunch){
            const float d=next.drive*.01f,t=(next.tone-50)/50;
            driveGain=std::pow(10.f,(-6+42*d)/20);stageBGain=1+.6f*d;
            inputLow.a=std::exp(-6.283185307179586*(70+80*d)/sr);
            // Drive-dependent HF excitation trim, separate from centered Tone shelves.
            preEmphasis=std::pow(10.f,(4*t-10*d*d)/20);postTilt=std::pow(10.f,12*t/20);
            // Preserve the approved Crunch damping at the reference Grain=25 setting.
            interA=float(std::exp(-6.283185307179586*std::min(11000.-3500.*.25-3500.*next.crunch*.01,sr*.42)/highRate));
            feedback.configure(next.grain*.01f);
            // Both clipping rails move together; Crunch owns the knee shape.
            // The middle position retains the reference operating point.
            const float openness=next.headroom*.01f;
            const float railScale=std::exp2(1.3f*(openness-.5f));
            baseHeadroom=.93f*railScale;
            secondThreshold=1.25f*railScale;
            sag=.4f*(1.f-openness);
            attack=coefficient(highRate,.0022+.004*openness);
            release=coefficient(highRate,.10-.05*openness);
            // Fixed level trim; it does not track the signal envelope.
            outputGain=std::pow(10.f,(next.level+driveCompensationDb(d))/20)*std::pow(railScale,-(.55f+.29f*next.crunch*.01f));
            // Static compensation for feedback insertion loss. The hard knee
            // already reduces level variation, so it needs less compensation.
            const float g=next.grain*.01f;
            const float fine=std::max(0.f,1-4*g);
            const float coarse=std::max(0.f,(g-.25f)/.75f);
            const float kneeTrim=1-.68f*next.crunch*.01f;
            outputGain*=1+kneeTrim*(.16f*fine+.57f*coarse*coarse);
        }
        hardness=next.crunch*.01f;
        if (!mixInitialized) {
            mix=next.mix;
            mixInitialized=true;
        } else if (next.mix != last.mix) {
            remaining=rampSamples;
            step=(next.mix-mix)/rampSamples;
        }
        if (remaining > 0) mix=--remaining == 0 ? next.mix : mix+step;
        last=next;cached=true;
    }
    int rampSamples=1,remaining=0;
    float step=0;
    bool mixInitialized=false;
    Oversampler os;SampleDelay dry;Controls last;bool cached=false;
    double sr=48000,highRate=48000;
    float driveGain=1,stageBGain=1,preEmphasis=1,postTilt=1,baseHeadroom=1,secondThreshold=1,sag=0,outputGain=1,hardness=.5f,mix=1;
    float envelope=0,memory=0,texture=0,interState=0;
    float attack=0,release=0,memoryA=0,textureA=0,interA=0;
    GrainFeedback feedback;
};
}
