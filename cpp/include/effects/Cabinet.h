#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <stdexcept>
#include <vector>

namespace multi_effects
{
// Uniform partitioned overlap-add. Exactly 128 samples latency, independent of host blocks.
// IR preparation and all allocation are outside processSample().
class Cabinet
{
public:
    static constexpr std::size_t partition = 128, fftSize = 256;
    using Complex = std::complex<float>;
    using Spectrum = std::array<Complex, fftSize>;
    void prepare(const std::vector<float>& ir)
    {
        if (ir.empty() || ir.size() > 32768) throw std::invalid_argument("IR must contain 1..32768 samples");
        for (float x : ir) if (!std::isfinite(x)) throw std::invalid_argument("Non-finite IR");
        kernels_.assign((ir.size() + partition - 1) / partition, {});
        history_.assign(kernels_.size(), {});
        for (std::size_t p = 0; p < kernels_.size(); ++p)
        {
            for (std::size_t j = 0; j < partition && p * partition + j < ir.size(); ++j)
                kernels_[p][j] = ir[p * partition + j];
            fft(kernels_[p], false);
        }
        reset();
    }
    void reset()
    {
        for (auto& x : history_) x.fill({});
        input_.fill(0); output_.fill(0); overlap_.fill(0);
        cursor_ = head_ = 0;
    }
    float processSample(float x)
    {
        if (kernels_.empty()) return x;
        const float y = output_[cursor_];
        input_[cursor_++] = x;
        if (cursor_ == partition)
        {
            auto& spectrum = history_[head_];
            spectrum.fill({});
            for (std::size_t i = 0; i < partition; ++i) spectrum[i] = input_[i];
            fft(spectrum, false);
            Spectrum sum{};
            for (std::size_t p = 0; p < kernels_.size(); ++p)
            {
                const auto& past = history_[(head_ + kernels_.size() - p) % kernels_.size()];
                for (std::size_t k = 0; k < fftSize; ++k) sum[k] += past[k] * kernels_[p][k];
            }
            fft(sum, true);
            for (std::size_t i = 0; i < partition; ++i)
            {
                output_[i] = sum[i].real() + overlap_[i];
                overlap_[i] = sum[i + partition].real();
            }
            cursor_ = 0;
            head_ = (head_ + 1) % kernels_.size();
        }
        return y;
    }
private:
    static void fft(Spectrum& a, bool inverse)
    {
        for (std::size_t i = 1, j = 0; i < fftSize; ++i)
        {
            std::size_t bit = fftSize >> 1;
            for (; j & bit; bit >>= 1) j ^= bit;
            j ^= bit;
            if (i < j) std::swap(a[i], a[j]);
        }
        for (std::size_t len = 2; len <= fftSize; len <<= 1)
        {
            const float angle = (inverse ? 1.f : -1.f) * 6.283185307179586f / float(len);
            const Complex root(std::cos(angle), std::sin(angle));
            for (std::size_t i = 0; i < fftSize; i += len)
            {
                Complex w(1, 0);
                for (std::size_t j = 0; j < len / 2; ++j)
                {
                    const auto u = a[i + j], v = a[i + j + len / 2] * w;
                    a[i + j] = u + v; a[i + j + len / 2] = u - v; w *= root;
                }
            }
        }
        if (inverse) for (auto& x : a) x /= float(fftSize);
    }
    std::vector<Spectrum> kernels_, history_;
    std::array<float, partition> input_{}, output_{}, overlap_{};
    std::size_t cursor_ = 0, head_ = 0;
};
}
