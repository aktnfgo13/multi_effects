#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace multi_effects
{
// Linear-phase windowed-sinc interpolation/decimation, one nonlinear island.
// 64*factor+1 Blackman taps, cutoff 0.45/factor cycles/high-rate sample.
// Unity DC decimator; interpolation gain=factor. Total delay=64 base samples.
// Fixed capacity avoids allocation even during prepare; 1x is an exact bypass.
class Oversampler
{
public:
    void prepare(int factor)
    {
        if (factor != 1 && factor != 2 && factor != 4) throw std::invalid_argument("Oversampling factor");
        factor_ = factor; taps_ = 64 * factor + 1;
        double sum = 0;
        for (int i=0;i<taps_;++i)
        {
            const double d=i-(taps_-1)*.5, cutoff=.45/factor;
            const double sinc=d==0 ? 2*cutoff : std::sin(6.283185307179586*cutoff*d)/(3.141592653589793*d);
            const double w=.42-.5*std::cos(6.283185307179586*i/(taps_-1))+.08*std::cos(12.566370614359172*i/(taps_-1));
            coefficients_[i]=float(sinc*w); sum+=coefficients_[i];
        }
        for(int i=0;i<taps_;++i) coefficients_[i]=float(coefficients_[i]/sum);
        reset();
    }
    void reset() noexcept { input_.fill(0); output_.fill(0); inIndex_=outIndex_=0; }
    int latencySamples() const noexcept { return factor_==1 ? 0 : 64; }
    template<class F> float processSample(float x, F&& nonlinear) noexcept
    {
        if (!std::isfinite(x)) x=0;
        if (factor_==1) { const float y=nonlinear(x); return std::isfinite(y)?y:0; }
        input_[inIndex_]=x;
        float result=0;
        for(int phase=0;phase<factor_;++phase)
        {
            float up=0;
            for(int k=phase,j=0;k<taps_;k+=factor_,++j)
                up+=coefficients_[k]*input_[(inIndex_+65-j)%65];
            float y=nonlinear(up*factor_);
            output_[outIndex_]=std::isfinite(y)?y:0;
            // Pick phase zero consistently: combined symmetric FIR delay is integral.
            if(phase==0)
                for(int k=0;k<taps_;++k) result+=coefficients_[k]*output_[(outIndex_+257-k)%257];
            outIndex_=(outIndex_+1)%257;
        }
        inIndex_=(inIndex_+1)%65;
        return result;
    }
private:
    int factor_=1,taps_=65,inIndex_=0,outIndex_=0;
    std::array<float,257> coefficients_{},output_{};
    std::array<float,65> input_{};
};
class SampleDelay
{
public:
    void reset(int length) { if(length<0 || length>int(data_.size()))throw std::invalid_argument("Sample delay capacity"); length_=length; index_=0; data_.fill(0); }
    float process(float x) noexcept { if(!length_)return x; const float y=data_[index_];data_[index_]=x;index_=(index_+1)%length_;return y; }
private:
    std::array<float,128> data_{}; int length_=0,index_=0;
};
}
