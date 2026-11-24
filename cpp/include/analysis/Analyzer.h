#pragma once
#include "AnalysisTap.h"
#include <complex>
#include <cmath>
#include <vector>

namespace multi_effects {
// Consumer-thread only. FFT/latency history never run in the audio callback.
class Analyzer {
public:
    static constexpr unsigned maximumFFT=4096,maximumLatency=65536;
    struct Snapshot {
        std::array<std::array<float,maximumFFT>,4> waveform{};
        std::array<std::array<float,maximumFFT/2+1>,4> spectrum{};
        double sampleRate=48000;
        std::uint64_t frameIndex=0;
        unsigned channels=0,latency=0,epoch=0,drops=0,fftSize=2048,validFrames=0;
        bool aligned=true;
    };
    Analyzer(): delay(2*maximumLatency,0) {clear();}
    void reset() {clear();epoch=session=channels=0;expected=0;}
    void setFFTSize(unsigned n) {if(n!=2048&&n!=4096)return;if(n!=fftSize){fftSize=n;clear();}}
    void accept(const AnalysisTap::Packet& p) {
        if(p.epoch!=epoch||p.session!=session||p.frameIndex!=expected||p.sampleRate!=rate||p.latency!=latency||p.channels!=channels) {
            clear();epoch=p.epoch;session=p.session;rate=p.sampleRate;latency=p.latency;channels=p.channels;
        }
        drops=p.drops;expected=p.frameIndex+p.frames;
        if(latency>=maximumLatency){valid=0;return;}
        for(unsigned i=0;i<p.frames;++i) {
            float in[2];
            for(unsigned c=0;c<2;++c) {
                auto& slot=delay[c*maximumLatency+delayIndex];
                in[c]=latency?slot:p.samples[c][i];slot=p.samples[c][i];
            }
            if(latency)delayIndex=(delayIndex+1)%latency;
            if(warmup<latency){++warmup;continue;}
            history[0][cursor]=in[0];history[1][cursor]=in[1];
            history[2][cursor]=p.samples[2][i];history[3][cursor]=p.samples[3][i];
            cursor=(cursor+1)%maximumFFT;valid=std::min(maximumFFT,valid+1);++sinceFFT;
        }
    }
    bool ready() const {return sinceFFT>=fftSize/2 && valid>=fftSize;}
    Snapshot snapshot() {
        Snapshot result;result.sampleRate=rate;result.channels=channels;result.latency=latency;result.epoch=epoch;result.drops=drops;
        result.frameIndex=expected;result.fftSize=fftSize;result.validFrames=valid;result.aligned=latency<maximumLatency;
        for(unsigned c=0;c<4;++c)for(unsigned i=0;i<valid;++i)
            result.waveform[c][maximumFFT-valid+i]=history[c][(cursor+maximumFFT-valid+i)%maximumFFT];
        if(ready()) {
            double sum=0;for(unsigned i=0;i<fftSize;++i){window[i]=float(.5-.5*std::cos(6.283185307179586*i/fftSize));sum+=window[i];}
            // Time-based smoothing (~100 ms), independent of sample rate / hop size.
            const float blend=haveSpectrum?float(std::exp(-double(sinceFFT)/(rate*.1))):0;
            for(unsigned c=0;c<4;++c) {
                for(unsigned i=0;i<fftSize;++i)work[i]={history[c][(cursor+maximumFFT-fftSize+i)%maximumFFT]*window[i],0};
                fft();
                for(unsigned k=0;k<=fftSize/2;++k){const float amplitude=float(std::abs(work[k])*(k==0||k==fftSize/2?1:2)/sum);smoothed[c][k]=blend*smoothed[c][k]+(1-blend)*amplitude;}
            }
            haveSpectrum=true;sinceFFT=0;
        }
        for(unsigned c=0;c<4;++c)for(unsigned k=0;k<=fftSize/2;++k)result.spectrum[c][k]=20*std::log10(std::max(1e-6f,smoothed[c][k]));
        return result;
    }
private:
    void clear() {for(auto& h:history)h.fill(0);for(auto& s:smoothed)s.fill(0);std::fill(delay.begin(),delay.end(),0);cursor=valid=sinceFFT=delayIndex=warmup=0;haveSpectrum=false;}
    void fft() {
        for(unsigned i=1,j=0;i<fftSize;++i){unsigned bit=fftSize>>1;for(;j&bit;bit>>=1)j^=bit;j^=bit;if(i<j)std::swap(work[i],work[j]);}
        for(unsigned len=2;len<=fftSize;len<<=1){const float angle=float(-6.283185307179586/len);const std::complex<float> root(std::cos(angle),std::sin(angle));
            for(unsigned i=0;i<fftSize;i+=len){std::complex<float>w(1,0);for(unsigned j=0;j<len/2;++j){auto u=work[i+j],v=work[i+j+len/2]*w;work[i+j]=u+v;work[i+j+len/2]=u-v;w*=root;}}}
    }
    std::array<std::array<float,maximumFFT>,4> history{};
    std::array<std::array<float,maximumFFT/2+1>,4> smoothed{};
    std::array<std::complex<float>,maximumFFT> work{};std::array<float,maximumFFT> window{};
    std::vector<float> delay;
    unsigned fftSize=2048,cursor=0,valid=0,sinceFFT=0,delayIndex=0,warmup=0,epoch=0,session=0,latency=0,channels=0,drops=0;
    double rate=48000;std::uint64_t expected=0;bool haveSpectrum=false;
};
}
