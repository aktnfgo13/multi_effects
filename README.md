# 🎸 Multi-Effects DSP Project

기타용 소프트웨어 멀티이펙터를 직접 설계하고 구현하는 개인 프로젝트입니다.

단순히 이펙터의 소리를 흉내 내는 것을 넘어,

- Audio DSP 원리 학습
- Effect Block 기반 멀티이펙터 엔진 설계
- Amp / Cabinet 모델링
- Audio Analysis를 통한 DSP 검증
- Parameter / Preset 시스템
- C++ / JUCE 기반 실시간 오디오 처리
- Tone Matching
- Neural Amp Capture
- Embedded Hardware 이식

까지 단계적으로 구현하는 것을 목표로 합니다.

---

# 🎯 Project Goal

최종 목표는 다음과 같은 기타 멀티이펙터 시스템을 직접 구현하는 것입니다.

```text
Guitar Input
     ↓
Noise Gate
     ↓
Compressor
     ↓
Overdrive / Distortion
     ↓
Amp Simulation
     ↓
Cabinet IR
     ↓
Modulation
     ↓
Delay
     ↓
Reverb
     ↓
Audio Output
```

장기적으로는 다음 구조까지 확장할 예정입니다.

```text
Software DSP Engine
        ↓
C++ DSP Engine
        ↓
JUCE Realtime Audio
        ↓
VST3 / Standalone
        ↓
DSP Quality Improvement
        ↓
Tone Matching
        ↓
Neural Amp Capture
        ↓
Embedded Hardware
```

---

# 🚀 Current Milestone

## Python Multi-Effects Engine v0.1

현재 Python과 NumPy를 기반으로 각 DSP 알고리즘을 구현하고 있습니다.

WAV 파일을 이용해 이펙터의 동작을 직접 청취하고, FFT / THD / Spectrogram 등의 분석 도구를 통해 DSP 결과를 검증합니다.

현재 단계는 실제 제품 개발 이전의 **DSP Prototype 및 Multi-Effects Engine 설계 단계**입니다.

---

# ✅ Implemented Features

## Audio Engine

- [x] WAV Audio Input / Output
- [x] Mono / Stereo Processing
- [x] Effect Block Architecture
- [x] Effect Chain
- [x] Effect Bypass
- [x] Effect Order
- [x] Multiple Instances of Same Effect
- [x] Unique Effect Instance ID
- [x] Effect Type / Display Name 분리

---

# ⚙️ Effect Architecture

각 이펙터는 공통 `EffectBlock` 구조를 사용합니다.

```text
EffectBlock
├─ effect_id
├─ effect_type
├─ name
├─ processor
├─ parameters
├─ parameter_specs
├─ bypass
└─ process()
```

예를 들어 동일한 Delay DSP를 여러 번 사용할 수 있습니다.

```text
delay_1
├─ type : delay
├─ name : Slapback Delay
└─ delay_ms : 120

delay_2
├─ type : delay
├─ name : Ambient Delay
└─ delay_ms : 600
```

따라서 다음과 같은 Effect Chain 구성이 가능합니다.

```text
Compressor
     ↓
Overdrive
     ↓
Amp Sim
     ↓
Cabinet IR
     ↓
Delay 1
     ↓
Delay 2
     ↓
Reverb
```

---

# 🎚 Parameter System

각 이펙터의 파라미터를 공통 구조로 관리합니다.

```text
Parameter
├─ value
├─ default
├─ minimum
├─ maximum
├─ step
└─ unit
```

지원 기능:

- [x] Parameter Value
- [x] Default Value
- [x] Minimum / Maximum
- [x] Automatic Clamp
- [x] Parameter Reset
- [x] Step
- [x] Unit

예:

```text
Delay Time

Default : 400 ms
Minimum : 1 ms
Maximum : 2000 ms
Step    : 1 ms
```

잘못된 값이 입력될 경우 자동으로 허용 범위 안으로 제한됩니다.

```python
delay.set_parameter(
    "delay_ms",
    5000
)
```

설정 가능한 최대값이 `2000 ms`라면 실제 값은 다음처럼 제한됩니다.

```text
Requested : 5000 ms
Actual    : 2000 ms
```

---

# 🎛 DSP Effects

## Dynamics

- [x] Gain
- [x] Compressor
- [x] Noise Gate

---

## Drive

- [x] Hard Clipping
- [x] Soft Clipping
- [x] Basic Overdrive / Distortion

---

## EQ / Filter

- [x] Peaking EQ
- [x] High Pass Filter
- [x] Low Pass Filter
- [x] Low Shelf Filter
- [x] High Shelf Filter

---

## Modulation

- [x] Chorus
- [x] Flanger
- [x] Phaser
- [x] Tremolo
- [x] Vibrato

---

## Time Based

- [x] Delay
- [x] Algorithmic Reverb

---

## Cabinet

- [x] Cabinet IR Loader
- [x] FFT Convolution
- [x] IR Sample Rate Conversion

---

# 🌊 Modulation DSP

## Chorus

Chorus는 짧은 Delay의 시간을 LFO로 변화시켜 원음과 섞는 방식으로 구현했습니다.

```text
Input
 ├──────────────→ Dry
 │
 ↓
Modulated Delay
 │
 ↓
Fractional Delay
 │
 ↓
Interpolation
 │
 ↓
Wet
 │
 └──────────────→ Mix
```

사용 개념:

- LFO
- Fractional Delay
- Linear Interpolation
- Stereo Phase Offset

---

## Flanger

Flanger는 Chorus보다 훨씬 짧은 Delay를 사용하여 Comb Filtering을 발생시킵니다.

```text
Dry Signal
     +
Short Modulated Delay
     ↓
Comb Filtering
```

---

## Phaser

Phaser는 Delay가 아닌 여러 개의 All-Pass Filter를 이용해 위상을 변화시킵니다.

```text
Input
  ↓
All-pass
  ↓
All-pass
  ↓
All-pass
  ↓
All-pass
  ↓
Wet

Dry + Wet
    ↓
Phaser
```

LFO에 의해 All-Pass Filter의 주파수 특성이 움직이며 여러 개의 Notch가 이동하게 됩니다.

---

## Tremolo

Tremolo는 LFO를 이용해 오디오 신호의 Amplitude를 변화시킵니다.

```text
Input
  ↓
LFO Controlled Gain
  ↓
Output
```

---

## Vibrato

Vibrato는 Modulated Delay를 이용해 Pitch Modulation을 생성합니다.

```text
Input
  ↓
Modulated Delay
  ↓
Pitch Modulation
  ↓
Output
```

Chorus와 달리 Dry 신호를 섞지 않고 Modulated Delay 신호를 중심으로 사용합니다.

---

# 🔊 Amp Simulation

기본적인 Guitar Amp Simulation 구조를 구현했습니다.

```text
Input
 ↓
Low Cut
 ↓
Preamp Gain
 ↓
Preamp Saturation
 ↓
Bass / Mid / Treble
 ↓
Master
 ↓
Power Amp Saturation
 ↓
Presence
 ↓
High Cut
 ↓
Output
 ↓
Cabinet IR
```

현재 구현:

- [x] Preamp Gain
- [x] Multi-stage Saturation
- [x] Asymmetric Bias
- [x] Bass
- [x] Mid
- [x] Treble
- [x] Master
- [x] Power Amp Saturation
- [x] Presence
- [x] Output Level
- [x] Cabinet IR Integration

현재 Amp Simulation은 실제 진공관 회로를 완전히 재현한 모델이 아니라, Amp DSP 구조와 Nonlinear Processing을 학습하고 검증하기 위한 **Amp Sim v0.1**입니다.

---

# 🔥 Nonlinear Amp Modeling

Preamp에서는 비선형 Saturation을 이용합니다.

```text
Input
 ↓
Gain
 ↓
Nonlinear Function
 ↓
Harmonics
 ↓
Distortion
```

기본적으로 `tanh()` 기반의 Saturation을 사용하고 있습니다.

```python
output = np.tanh(
    input_signal * drive
)
```

Bias를 추가하면 비대칭적인 Saturation을 생성할 수 있습니다.

```text
Symmetric Saturation
→ Odd Harmonics 중심

Asymmetric Saturation
→ Even Harmonics 추가
```

실제 분석 결과에서도 Bias가 추가되었을 때 H2, H4 등의 Even Harmonic이 증가하는 것을 확인했습니다.

---

# 📊 Amp Simulation Analysis

Amp Sim의 동작을 청취만으로 판단하지 않고 Harmonic / THD 분석을 수행했습니다.

예시 결과:

```text
Clean-ish
THD ≈ 0.18 %

Preamp Drive
THD ≈ 7.68 %

Biased Preamp
THD ≈ 7.59 %

Power Amp Push
THD ≈ 20.80 %
```

Power Amp를 강하게 Push한 경우:

```text
H1 : Fundamental
H3 : Strong
H5 : Medium
H7 : Lower
H9 : Lower
```

형태의 Odd Harmonic 분포를 확인했습니다.

---

# 🔉 Cabinet IR

Cabinet IR은 실제 스피커 / 캐비넷 / 마이크의 응답을 Impulse Response를 이용해 적용합니다.

```text
Amp Output
     ↓
Impulse Response
     ↓
FFT Convolution
     ↓
Cabinet Tone
```

현재 지원:

- [x] WAV IR Load
- [x] Stereo IR → Mono 처리
- [x] DC Removal
- [x] Sample Rate Conversion
- [x] FFT Convolution
- [x] Peak Normalization

---

# 💾 Preset System

Effect Chain 설정을 JSON 형식으로 저장하고 복원할 수 있습니다.

현재 Preset Version은 `v2`입니다.

각 Effect는 다음 세 가지 식별 정보를 갖습니다.

```text
effect_id
→ 프로그램 내부에서 사용하는 고유 인스턴스 ID

effect_type
→ DSP 종류

name
→ 사용자에게 표시되는 이름
```

예:

```text
effect_id   = delay_1
effect_type = delay
name        = Slapback Delay
```

두 번째 Delay:

```text
effect_id   = delay_2
effect_type = delay
name        = Ambient Delay
```

---

# 📄 Preset JSON Example

```json
{
    "version": 2,
    "effects": [
        {
            "id": "delay_1",
            "type": "delay",
            "name": "Slapback Delay",
            "bypass": false,
            "parameters": {
                "delay_ms": 120.0,
                "feedback": 0.15,
                "mix": 0.1
            }
        },
        {
            "id": "delay_2",
            "type": "delay",
            "name": "Ambient Delay",
            "bypass": false,
            "parameters": {
                "delay_ms": 600.0,
                "feedback": 0.45,
                "mix": 0.35
            }
        }
    ]
}
```

지원 기능:

- [x] Preset Save
- [x] Preset Load
- [x] Effect Order Restore
- [x] Parameter Restore
- [x] Bypass Restore
- [x] Multiple Effect Instances
- [x] Unique Effect ID
- [x] Preset v1 Compatibility
- [x] Preset v2

---

# 📈 Audio Analysis

DSP를 청취만으로 평가하지 않고 수치와 그래프로 검증하기 위한 Audio Analyzer를 함께 개발하고 있습니다.

현재 구현:

- [x] Peak
- [x] RMS
- [x] Waveform
- [x] FFT
- [x] Harmonic Analysis
- [x] THD
- [x] STFT
- [x] Spectrogram
- [x] Envelope
- [x] Transient Analysis
- [x] EQ Frequency Response
- [x] Compressor Gain Reduction
- [x] Amp Harmonic Analysis

---

# 🔍 Harmonic Analysis

Test Sine Wave를 이용해 DSP가 어떤 Harmonic을 생성하는지 분석할 수 있습니다.

예:

```text
Input
1000 Hz Sine Wave

↓

Distortion / Amp

↓

1000 Hz
3000 Hz
5000 Hz
7000 Hz
...
```

대칭적인 Saturation에서는 Odd Harmonic이 강하게 발생하는 것을 확인할 수 있습니다.

---

# 🌈 Spectrogram

STFT 기반 Spectrogram을 이용해 주파수 성분이 시간에 따라 어떻게 변화하는지 확인합니다.

```text
Time
→

Frequency
↑
│
│    Harmonics
│  █ █ █ █
│ █ █ █ █
│
└──────────────
```

---

# 🧪 Testing

`pytest`를 이용해 Core Engine과 DSP 기능을 자동으로 검증합니다.

현재 테스트 범위:

- Parameter Validation
- Parameter Clamp
- Parameter Reset
- Parameter Specification
- Amp Parameter
- Amp Bypass
- Preset Save / Load
- Preset Restore
- Effect Order
- Effect Instance ID
- Duplicate Effect Instances
- Chorus
- Flanger
- Phaser
- Tremolo
- Vibrato
- Modulation Effect Factory

실행:

```bash
python -m pytest -v
```

개발 과정은 다음 흐름을 사용합니다.

```text
DSP 구현
   ↓
examples/ 청취 테스트
   ↓
pytest 자동 테스트
   ↓
Audio Analysis
   ↓
Git Commit
```

---

# 📁 Project Structure

```text
multi_effects/
│
├─ src/
│  │
│  ├─ core/
│  │  ├─ effect.py
│  │  ├─ effect_chain.py
│  │  ├─ effect_factory.py
│  │  ├─ parameter.py
│  │  └─ preset.py
│  │
│  ├─ effects/
│  │  ├─ amp_sim.py
│  │  ├─ cabinet_ir.py
│  │  ├─ chorus.py
│  │  ├─ compressor.py
│  │  ├─ delay.py
│  │  ├─ distortion.py
│  │  ├─ eq.py
│  │  ├─ filters.py
│  │  ├─ flanger.py
│  │  ├─ gain.py
│  │  ├─ noise_gate.py
│  │  ├─ phaser.py
│  │  ├─ reverb.py
│  │  ├─ tremolo.py
│  │  └─ vibrato.py
│  │
│  └─ analysis/
│
├─ tests/
│
├─ examples/
│
├─ audio/
│  ├─ input/
│  ├─ output/
│  └─ ir/
│
├─ presets/
│
├─ docs/
│
├─ pyproject.toml
├─ requirements.txt
├─ .gitignore
└─ README.md
```

---

# 🛠 Development Environment

현재 Python Prototype 개발 환경:

```text
Python 3.12
NumPy
SciPy
SoundFile
Matplotlib
pytest
```

프로젝트 설치:

```bash
pip install -e .
```

Windows에서 특정 Python을 사용할 경우:

```bash
"C:/Program Files/Python312/python.exe" -m pip install -e .
```

테스트:

```bash
python -m pytest -v
```

---

# 🗺 Development Roadmap

## Phase 1 — Python DSP Prototype

- [x] Audio I/O
- [x] Gain
- [x] Distortion
- [x] EQ / Filters
- [x] Compressor
- [x] Noise Gate
- [x] Delay
- [x] Reverb
- [x] Cabinet IR
- [x] Chorus
- [x] Flanger
- [x] Phaser
- [x] Tremolo
- [x] Vibrato

---

## Phase 2 — Multi-Effects Engine

- [x] Effect Block
- [x] Effect Chain
- [x] Effect Order
- [x] Bypass
- [x] Parameter System
- [x] Parameter Validation
- [x] Preset Save / Load
- [x] Unique Effect ID
- [x] Multiple Effect Instances
- [x] Preset v2
- [x] Automated Testing

---

## Phase 3 — Amp System

- [x] Basic Amp Simulation
- [x] Preamp Gain
- [x] Preamp Saturation
- [x] Asymmetric Bias
- [x] Tone Stack
- [x] Bass / Mid / Treble
- [x] Basic Power Amp Saturation
- [x] Master
- [x] Presence
- [x] Cabinet IR
- [ ] Advanced Amp Modeling
- [ ] Oversampling
- [ ] Anti-Aliasing
- [ ] Dynamic Nonlinear Modeling
- [ ] Circuit Based Modeling

---

## Phase 4 — C++ DSP Engine

- [ ] C++ Fundamentals
- [ ] DSP Code Porting
- [ ] Stateful DSP Processing
- [ ] Circular Buffer
- [ ] Realtime Parameter Handling
- [ ] Memory Optimization
- [ ] CPU Performance Optimization

---

## Phase 5 — JUCE Realtime Engine

- [ ] JUCE Project Setup
- [ ] Realtime Audio I/O
- [ ] Audio Buffer Processing
- [ ] Effect Chain Engine
- [ ] Parameter System
- [ ] Preset Manager
- [ ] Standalone Application
- [ ] VST3 Plugin
- [ ] GUI

---

## Phase 6 — DSP Quality Improvement

- [ ] Oversampling
- [ ] Anti-Aliasing
- [ ] Advanced Gain Staging
- [ ] Improved Interpolation
- [ ] Improved Reverb
- [ ] Feedback Modulation Effects
- [ ] Latency Measurement
- [ ] CPU Benchmark
- [ ] Reference Device Comparison

Reference candidates:

```text
Fractal Audio
Neural DSP Quad Cortex
IK Multimedia TONEX
Kemper
Line 6
BOSS
```

비교 예정 항목:

```text
Waveform
FFT
STFT
Harmonics
THD
Envelope
Transient
Phase
Dynamics
Frequency Response
Latency
```

---

# 🎯 Tone Matching

Reference 장비 또는 Plugin의 출력을 분석하고 현재 DSP의 파라미터를 자동 조정하는 시스템을 목표로 합니다.

```text
Reference Audio
      ↓
Audio Analyzer
      ↓
Target Feature Extraction
      ↓
Our DSP
      ↓
Difference Measurement
      ↓
Parameter Optimization
      ↓
Tone Matching
```

예정 기능:

- [ ] Similarity Metrics
- [ ] Frequency Response Matching
- [ ] EQ Matching
- [ ] Gain Matching
- [ ] Drive Matching
- [ ] Automatic Parameter Search
- [ ] Reference Tone Matching

---

# 🧠 Neural Amp Capture

장기적으로 Neural Amp Modeling 기반의 Capture 기능을 연구할 예정입니다.

```text
DI Signal
    ↓
Reference Amp / Pedal
    ↓
Wet Signal

DI + Wet Dataset
       ↓
Neural Model Training
       ↓
Amp / Pedal Capture
```

예정 작업:

- [ ] DI / Wet Dataset
- [ ] Neural Effect Modeling
- [ ] NAM Architecture Research
- [ ] NAM Inference
- [ ] Training Pipeline
- [ ] Capture Pipeline
- [ ] Realtime Neural Processing

---

# 🖥 Realtime Processing

현재 Python 단계에서는 NumPy 기반 Vector Processing을 사용합니다.

예:

```text
Entire WAV
   ↓
NumPy Processing
   ↓
Output WAV
```

향후 C++ / JUCE 단계에서는 실제 오디오 장비와 동일하게 Block 기반 실시간 처리를 사용합니다.

```text
Audio Interface
      ↓
Audio Block
      ↓
DSP Chain
      ↓
Audio Block
      ↓
Output
```

실시간 DSP에서는 다음 요소가 중요합니다.

```text
Circular Buffer
DSP State
Block Size
Latency
CPU Usage
Memory Allocation
Thread Safety
```

---

# 🔧 Hardware Roadmap

최종적으로 소프트웨어 DSP 엔진을 실제 Embedded Hardware로 이식하는 것이 목표입니다.

```text
Guitar
  ↓
Hi-Z Analog Input
  ↓
ADC / Audio Codec
  ↓
Embedded CPU / DSP
  ↓
C++ Multi-Effects Engine
  ↓
DAC
  ↓
Audio Output

       ↕
Footswitch
Encoder
Display
Preset Storage
```

예정 작업:

- [ ] Embedded Platform Selection
- [ ] Audio Codec Selection
- [ ] Realtime DSP Port
- [ ] Audio Input Circuit
- [ ] Audio Output Circuit
- [ ] Footswitch
- [ ] Encoder
- [ ] Display
- [ ] Preset Storage
- [ ] PCB
- [ ] Enclosure
- [ ] Power Supply Design

하드웨어는 C++ / JUCE 기반의 실시간 DSP 성능을 먼저 측정한 뒤 필요한 CPU 성능과 I/O 구조를 결정할 예정입니다.

---

# 📌 Current Development Direction

현재까지는 다음 단계가 완료되었습니다.

```text
Python DSP Prototype
        ✅

Multi-Effects Engine
        ✅

Parameter / Preset System
        ✅

Amp Sim v0.1
        ✅

Audio Analyzer
        ✅

Automated Tests
        ✅
```

다음 주요 단계는:

```text
Python Multi-Effects v0.1
        ↓
C++ DSP Port
        ↓
JUCE Realtime Engine
        ↓
VST3 / Standalone
        ↓
DSP Quality Improvement
        ↓
Tone Matching
        ↓
Neural Capture
        ↓
Embedded Hardware
```

입니다.

Python 단계에서는 Effect 종류를 무한정 추가하기보다 핵심 DSP 원리와 Multi-Effects Architecture를 검증하는 것을 목표로 합니다.

---

# 🎸 Long-Term Vision

최종적으로 다음 세 가지 방향을 결합하는 것이 목표입니다.

```text
High-Quality DSP
        +
Flexible Multi-Effects Workflow
        +
Neural Amp Capture
```

목표 방향은 다음과 같습니다.

```text
Software First
      ↓
Realtime DSP
      ↓
Professional Audio Quality
      ↓
Tone Matching
      ↓
Neural Capture
      ↓
Portable Hardware
```

소프트웨어에서 충분히 검증한 뒤 실제 기타 멀티이펙터 하드웨어까지 확장할 예정입니다.

---

# 📦 Version

Current Milestone:

```text
Python Multi-Effects Engine
v0.1.0
```

주요 구현:

```text
DSP Effects
Amp Sim
Cabinet IR
Effect Chain
Parameter System
Preset v2
Multiple Effect Instances
Audio Analysis
pytest Automation
```