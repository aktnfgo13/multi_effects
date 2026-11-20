#pragma once
#include "effects/Oversampler.h"
#include "effects/GrainFeedback.h"
namespace multi_effects {
// Provisional analytical drive, not a model of a commercial pedal. Single-channel
// fixed state; Engine copies a freshly prepared instance for independent stereo.
class CustomDrive {
public:
    struct Controls { float drive=50,tone=50,crunch=50,headroom=50,grain=25,level=-6,mix=1,tight=50,midCharacter=50,bias=50,symmetry=50; };
    void prepare(double rate,int factor) {
        if(!std::isfinite(rate)||rate<8000||rate>192000)throw std::invalid_argument("CustomDrive sample rate");
        os.prepare(factor);sr=rate;highRate=rate*factor;rampSamples=std::max(1,int(std::round(rate*.020)));
        preSplit.prepare(rate,1400);postSplit.prepare(rate,1800);postLow.prepare(rate,std::min(10000.,rate*.42));
        tightBand.prepare(rate,220);dc.prepare(rate,12);inputLow.prepare(rate,70);
        memoryA=coefficient(highRate,.008);textureA=coefficient(highRate,.025);
        pickAttack=coefficient(highRate,.0002);pickRelease=coefficient(highRate,.012);
        interA=float(std::exp(-6.283185307179586*std::min(11000.,rate*.42)/highRate));
        midPre.prepare(rate);midPost.prepare(rate);feedback.prepare(highRate);cached=false;reset();
    }
    void reset() noexcept {
        midPre.reset();midPost.reset();mixInitialized=false; remaining=0;
        os.reset();dry.reset(os.latencySamples());inputLow.reset();tightBand.reset();preSplit.reset();postSplit.reset();postLow.reset();dc.reset();
        feedback.reset();envelope=memory=texture=interState=pickEnvelope=0;
    }
    // Last validated controls used by update() on this audio lane.
    float appliedGrain() const noexcept { return cached ? last.grain : -1.f; }
    float appliedHeadroom() const noexcept { return cached ? last.headroom : -1.f; }
    float appliedMix() const noexcept { return mixInitialized ? mix : -1.f; }
    float appliedTight() const noexcept { return cached ? last.tight : -1.f; }
    float appliedMidCharacter() const noexcept { return cached ? last.midCharacter : -1.f; }
    int latencySamples() const noexcept {return os.latencySamples();}
    float processSample(float input,const Controls& controls) noexcept {
        update(controls);
        // Abnormal overload guard only (>36 dBFS); no normal-range limiter/normalizer.
        const float x=std::isfinite(input)?std::clamp(input,-64.f,64.f):0.f;
        const float aligned=dry.process(std::isfinite(input) ? input : 0.f);
        float wet=x-inputLow.process(x);
        const float low=preSplit.process(wet);wet=low+preEmphasis*(wet-low);
        // Fixed 220 Hz low shelf before both the saturation and sag detector.
        const float excitationLF = tightBand.process(wet);
        wet += tightDelta * excitationLF;
        wet=midPre.process(wet,midCutPre,midFocusPre);
        wet=os.processSample(wet*driveGain,[this](float v){return island(v);});
        wet=midPost.process(wet,midCutPost,midFocusPost);
        const float bass=postSplit.process(wet);wet=postLow.process(bass+postTilt*(wet-bass));
        wet-=dc.process(wet);wet*=outputGain;
        // Preserve the approved full-wet floating-point operation, including its guard.
        const float referenceDry=std::clamp(aligned,-64.f,64.f);
        if (mix == 1.f) return referenceDry + 1.f*(wet-referenceDry);
        if (mix == 0.f) return aligned;
        return aligned+mix*(wet-aligned);
    }
private:
    // Unity-peak bandpass from a trapezoidal state-variable filter.
    struct MidBand {
        double a1=0,a2=0,a3=0,damping=1,s1=0,s2=0;
        void prepare(double rate,double center,double q) {
            const double g=std::tan(3.141592653589793*center/rate);
            damping=1/q;a1=1/(1+g*(g+damping));a2=g*a1;a3=g*a2;
        }
        double process(double x) noexcept {
            const double v3=x-s2,v1=a1*s1+a2*v3,v2=s2+a2*s1+a3*v3;
            s1=2*v1-s1;s2=2*v2-s2;
            return damping*v1;
        }
    };
    struct MidSection {
        MidBand scoop,focus;
        void prepare(double rate) { scoop.prepare(rate,700,.85);focus.prepare(rate,1050,1.15); }
        void reset() { scoop.s1=scoop.s2=focus.s1=focus.s2=0; }
        float process(float x,float cut,float lift) noexcept {
            const double low=scoop.process(x),high=focus.process(x);
            if (cut==0 && lift==0) return x;
            return float(x+cut*low+lift*high);
        }
    } midPre,midPost;
    float midCutPre=0,midFocusPre=0,midCutPost=0,midFocusPost=0;
    struct LowPass {
        double state=0,a=0;
        void prepare(double rate,double hz){a=std::exp(-6.283185307179586*hz/rate);state=0;}
        void reset(){state=0;}
        float process(float x){state=x+a*(state-x);return float(state);}
    } tightBand,inputLow,preSplit,postSplit,postLow,dc;
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
        // A fast detector isolates pick onset from the existing slow sag envelope.
        // Only low Headroom adds transient loading; H50..100 and Grain's legacy
        // envelope remain unchanged. Rail-relative, bounded loading avoids a
        // Drive-dependent absolute trigger and cannot invert the signal gain.
        const float pickA=detector>pickEnvelope?pickAttack:pickRelease;
        pickEnvelope=detector+pickA*(pickEnvelope-detector);
        const float onset=std::max(0.f,pickEnvelope-envelope)/baseHeadroom;
        const float loading=pickLoading*onset/(1.f+onset);
        const float conditioned=v/(1+sag*envelope+loading);
        const float knee=baseHeadroom*(3.f-2.f*hardness);
        const float sensed=conditioned/baseHeadroom;
        const float shaped=conditioned+baseHeadroom*(feedback.process(sensed)-sensed);
        const float stageA=knee*curve(shaped/knee,hardness);
        interState=stageA+interA*(interState-stageA);
        memory=interState+memoryA*(memory-interState);
        // Legacy operating point shared by all Grain settings.
        constexpr float grain=.25f;
        const float bias=grain*(.9f+.18f*envelope/(1+envelope)+.4f*memory/(1+std::abs(memory)))+biasOffset;
        // Scale both the input knee and the output rail by polarity to keep
        // the small-signal slope continuous at zero.
        const float rail=secondThreshold*(1-.45f*.25f)
            *(1+(interState<0 ? -railSkew : railSkew));
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
        const Controls next{finite(c.drive,50,0,100),finite(c.tone,50,0,100),finite(c.crunch,50,0,100),finite(c.headroom,50,0,100),finite(c.grain,25,0,100),finite(c.level,-6,-24,12),finite(c.mix,1,0,1),finite(c.tight,50,0,100),finite(c.midCharacter,50,0,100),finite(c.bias,50,0,100),finite(c.symmetry,50,0,100)};
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
            const float closed=std::max(0.f,1.f-2.f*openness);
            pickLoading=2.f*closed*closed;
            attack=coefficient(highRate,.0022+.004*openness);
            release=coefficient(highRate,.10-.05*openness);
            // Below the clipping region, full makeup gain reverses pick level.
            // Restore the approved trim progressively between Drive 40 and 60.
            const float ramp=std::clamp((next.drive-40.f)/20.f,0.f,1.f);
            const float trimWeight=ramp*ramp;
            outputGain=std::pow(10.f,(next.level+driveCompensationDb(d))/20)*std::pow(railScale,-trimWeight*(.55f+.29f*next.crunch*.01f));
            // Static compensation for feedback insertion loss. The hard knee
            // already reduces level variation, so it needs less compensation.
            const float g=next.grain*.01f;
            const float fine=std::max(0.f,1-4*g);
            const float coarse=std::max(0.f,(g-.25f)/.75f);
            const float kneeTrim=1-.68f*next.crunch*.01f;
            outputGain*=1+kneeTrim*(.16f*fine+.57f*coarse*coarse);
        }
        if (!cached || next.tight != last.tight) {
            const float gainRoot = 1.f + (50.f - next.tight) * .01f;
            tightDelta = gainRoot * gainRoot - 1.f;
        }
        if (!cached || next.midCharacter!=last.midCharacter) {
            const float position=(next.midCharacter-50)*.02f;
            const float scoop=std::min(position,0.f),focus=std::max(position,0.f);
            midCutPre=.18f*scoop;midCutPost=.27f*scoop;
            midFocusPre=.2f*focus;midFocusPost=.3f*focus;
        }
        hardness=next.crunch*.01f;
        biasOffset=(next.bias-50.f)*.006f;
        railSkew=(next.symmetry-50.f)*.0048f;
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
    float step=0, tightDelta=0;
    float biasOffset=0,railSkew=0;
    bool mixInitialized=false;
    Oversampler os;SampleDelay dry;Controls last;bool cached=false;
    double sr=48000,highRate=48000;
    float driveGain=1,stageBGain=1,preEmphasis=1,postTilt=1,baseHeadroom=1,secondThreshold=1,sag=0,outputGain=1,hardness=.5f,mix=1;
    float envelope=0,memory=0,texture=0,interState=0;
    float pickEnvelope=0,pickAttack=0,pickRelease=0,pickLoading=0;
    float attack=0,release=0,memoryA=0,textureA=0,interA=0;
    GrainFeedback feedback;
};
}
