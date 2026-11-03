# 청취 테스트 이후 코드 리뷰와 우선순위

기준: `4fc064f` (2026-11-02). 시작 시 working tree clean.
이번 변경은 이 문서뿐이다. DSP/GUI/빌드 설정 변경, commit/push 없음.
검토 방법: 프로젝트 소스·테스트·설정 전체 목록, 처리 경로 검색, 고정 JUCE
8.0.12 소스의 AudioDeviceManager/File 구현 확인. 기존 테스트 결과를 참고했으며
이번 리뷰에서 전체 suite를 재실행하거나 실제 장치를 재설정하지 않았다.
심각도는 결함/위험의 영향이며, 아래 P0–P6 개발 단계와는 별도다.

## 실제 문제와 근거

### R1 — 높음: 준비 실패가 설명 없는 무음으로 나타남

`cpp/app/Main.cpp:140–154`: audioDeviceAboutToStart가 모든 예외를 삼키고
ready=false만 설정한다. callback은 출력을 0으로 만들지만 UI에는 원인을 전달하지
않는다. audioDeviceError도 앱에서 override하지 않는다.
재현 가능한 조건: 44.1 kHz에서 허용되는 32768-sample IR을 사용한 뒤 장치를
96 kHz로 변경하면 Engine.cpp:48–49의 resampled length 제한을 초과한다.
효과 추가/IR 교체 중 prepare 실패도 이전 정상 상태로 자동 rollback하지 않는다.
P0: 오류 전달, 준비 실패 시 복구/rollback, rate 변경과 긴 IR 조합을 우선 검증.
현재 정상 입력의 음색 문제가 아니라 운영·복구 문제다.

### R2 — 높음: 장치 패널 Test 음은 앱 Mute/최종 clamp를 우회함

앱 Mute는 Main.cpp:163–165의 자체 출력에만 적용된다. 고정 JUCE 소스
`juce_audio_devices/audio_io/juce_AudioDeviceManager.cpp:1134–1148`는 앱 callback
이후 testSound를 출력에 더하고, 재생 종료 때 testSound.reset()으로 해제한다.
따라서 Mute=on이어도 장치 패널 Test 버튼은 소리를 낼 수 있고, 앱 최종 clamp도
그 합산에는 적용되지 않는다. 버튼은 리뷰 중 누르지 않았다.
P0: 테스트 전 반드시 인지하고 낮은 하드웨어 볼륨 사용. 이후 global mute의
의미/테스트음 경로를 명확히 정리. 현재 GUI 변경은 하지 않음.

### R3 — 중간: Engine reset은 allocation 없는 state clear가 아님

Engine.cpp:39–74는 processor를 재생성한다. Delay/Reverb 버퍼와 Cabinet FFT/IR
resampling을 다시 만들고 std::function을 교체/해제한다. raw Delay/Reverb/Cabinet
reset은 기존 버퍼를 지우지만 공통 Effect::reset은 더 무겁다.
현재 앱은 callback을 제거한 상태 또는 시작 단계에서만 호출하므로 확인된
process 내부 allocation 버그는 아니다. 향후 host reset에서 그대로 호출하면 위험.
Preset load는 교체 전에 prepare하고 callback 재등록 시 다시 prepare한다.
P1: prepare(자원 준비) / reset(상태 초기화) 계약 분리와 중복 준비 제거.

### R4 — 중간: Delay 시간 smoothing이 click 제거를 보장하지 않음

TimeEffects.h:24는 smoothed ms를 정수 sample index로 잘라 읽는다. 시간 변경 중
읽기 위치가 한 sample씩 불연속 이동한다. GUI 값이 부드럽게 움직여도 zipper/click,
pitch 변화가 남을 수 있다. 20 ms one-pole은 20 ms에 목표에 완전히 도달하는 ramp가
아니다. 기존 reference 동작이므로 이번에는 바꾸지 않았다.
P2: 녹음으로 확인한 뒤 fractional delay 또는 dual-tap crossfade를 별도 품질 개선.

### R5 — 중간: 현재 검증은 device wrapper 전체의 realtime 무잠금 검증이 아님

자체 process 경로에는 명시적 new/malloc/resize/IO/mutex/로그가 없다. 그러나 고정
JUCE AudioDeviceManager.cpp:1083–1100에는 audioCallbackLock과 tempBuffer.setSize가
있다. 용량이 충분하면 재할당을 피하지만 첫 callback/큰 block 변화에서는 재할당
가능성이 있다. 등록·제거와 일부 장치 동작도 같은 잠금을 사용한다.
이것은 JUCE adapter 경로의 특성이며 Core의 allocation 검사 통과와 모순되지 않는다.
P1: 장치 재시작/큰 block/최장 IR/최대 chain에서 callback 최대 소요시간·dropout 관측.
JUCE 자체를 성급히 교체하거나 내부 lock을 임의 제거하지 말 것.

### R6 — 중간: preset compatibility와 실패 범위에 제한

PresetIO.cpp는 v2/3만 수용한다. 저장소 `presets/amp_test.json`, `test_preset.json`은
실제로 v1이므로 앱에서 로드되지 않는다. v2도 registry에 없는 type/parameter는
거부한다. 현재 Amp는 gain_db/drive/output_db만 노출하므로 Python tone-stack 등
전체 parameter를 저장한 v2도 그대로 이식되지 않는다.
version은 int64를 허용한 뒤 int로 좁힌다(PresetIO.cpp:51–52). 향후 엄격한 범위
검증 필요. 파일은 크기/깊이 제한 없이 통째로 JSON parse한 다음 24 effects 및
32768 IR samples 제한을 적용하므로 큰/깊은 입력은 UI 지연·메모리 부담을 줄 수 있다.
잘못된 numeric IR, duplicate ID 등은 이미 검사한다. 지원하지 않는 데이터를 조용히
무시하지 않는 점은 좋다.
P1: 명시적 v1/v2 migration 또는 지원 subset 안내, 파일 크기/구조 제한,
버전·parameter·알고리즘 버전 관리 정책. 새 파라미터는 default로 추가 가능하지만
parameter rename/removal과 알고리즘 교체는 migration 없이는 음색 재현을 보장 못 함.

### R7 — 낮음/중간: 외부 장치·개발 환경 재현성 부채

`.vscode/settings.json`은 Git 추적 중이며 사용자 절대 macOS 경로가 들어 있다.
Core-only CMake는 JUCE를 가져오지 않지만 CLI/테스트/Threads를 항상 구성한다.
Windows comparison 경로 탐색은 Gain/Gate와 나머지 도구가 서로 다르고,
compressor_cli.cpp의 std::stof는 <string> 직접 include 없이 간접 include에 의존한다.
CI 설정이 없고 pyproject의 느슨한 dependency와 requirements의 버전 고정은 별개다.
pytest는 dependency 파일에 포함되지 않는다.
P1: portable workspace 경로, 공통 검증 실행 경로, MSVC CI, dev dependency 재현성.

## Architecture 및 lifetime 평가

- Core(`cpp/include`, `cpp/src`)는 표준 C++17만 사용. JUCE·filesystem·OS API 없음.
  mach API는 테스트 RSS 측정에만 있다. File/JSON은 app 계층에 분리되어 있다.
- unique_ptr Effect 소유권, 객체별 state, registry 생성 구조는 적절하다. move는
  unique_ptr만 옮기므로 effect 주소는 유지된다. GUI pointer 캡처는 현재 message
  thread에서 컨트롤 재생성과 함께 사용되고, callback 제거 후 구조 편집한다.
- JUCE removeAudioCallback 구현은 audioCallbackLock 아래 목록에서 제거하고
  진행 callback과 동기화한다. 현재 경로에서 확인된 use-after-free는 없다.
  다만 공개 effects/parameters/metadata는 임의 외부 수정이 가능하고, instance ID
  유일성·24개 제한은 Core가 아니라 GUI/parser에서 보장한다. 이후 다른 adapter가
  생기면 같은 규칙을 누락할 수 있다. 실패하는 edit의 강한 예외 보장은 없다.
- prepare/reset을 처리 중 호출하지 않는다는 계약이 핵심. ready/scratch/processor는
  atomics가 아니지만 현재 JUCE lifecycle 직렬화 및 callback 제거 계약에 의존한다.
  이 계약을 무시하는 새 thread 경로가 생기면 data race 가능. atomic으로 무조건
  바꾸는 대신 ownership을 문서화/캡슐화해야 한다.
- ProcessSpec에 channels가 없고 Effect는 단일 float buffer만 받는다. Main은 첫
  활성 입력만 읽고 모든 출력에 복제한다. stereo는 단순 interleaving이 아니라
  채널별 상태 + linked detector + planar buffer/latency 정책이 필요하다.
  Python compressor(mean abs)와 gate(max abs)의 linked stereo 의미도 보존 여부 결정.
  구조가 영구적으로 막지는 않지만 plugin 전 interface 확장이 필요하다.

## Parameter / realtime 처리 상세

- target/bypass는 lock-free static_assert가 있는 atomics. GUI는 target만 쓰고
  smoothing state는 오디오 thread 소유. 기존 TSan은 이 경로를 검사했다.
- 여러 parameter를 동시에 변경하면 하나의 원자적 snapshot으로 적용되지는 않는다.
  현재 GUI 단일 control에는 문제 없으나 preset automation/host event에는 별도 계약 필요.
- metadata는 GUI/preset stable ID를 공유하나 scaling/step/개별 smoothing time/enum은
  없다. DSP 클래스 기본값과 registry 기본값도 일부 중복된다.
- process 중 문자열 생성·logging은 없으나 sample마다 `type == "cabinet"` 문자열
  비교, parameter atomic load, std::function dispatch가 있다. 비교 자체가 heap
  allocation은 아니다. profiling 후 bool/capability 및 block snapshot으로 개선 가능.
- Compressor/Gate setter, gain pow, Amp pow/tanh, EQ 계수 갱신 등 sample별 연산이
  존재한다. correctness를 유지하며 변경된 parameter만 갱신하는 것이 후속 후보.
- bypass는 dry/wet crossfade이며 내부 DSP는 계속 처리한다. 상태/tail은 유지되지만
  bypass해도 CPU는 줄지 않는다. Cabinet dry에도 128 samples 지연을 유지한다.
- outputGain smoothing은 sample-rate와 무관한 .002 상수이며 restart 시 초기화되지
  않는다. 재시작 후 첫 sample이 이미 열린 출력 gain으로 처리될 수 있어 click 검증 필요.
- 비정상 sample guard는 출력만 0으로 바꾼다. IIR/feedback 내부 상태를 복구하지는
  않으므로 fault injection 후 지속 무음/회복 검증은 아직 필요하다.

## Cabinet / Amp / Delay / Reverb

Cabinet은 **이미 uniform partitioned overlap-add**다. direct convolution을 나중에
partitioned로 바꿔야 하는 단계가 아니다. 128 입력을 모아 FFT 256으로 처리하고 다음
128 구간에 내보내므로 지연은 128 samples(44.1k: 2.90ms, 48k: 2.67ms, 96k: 1.33ms).

P=ceil(IR length/128). kernel+history는 2×P×256 complex<float>로 일반적인 이 빌드에서
약 4096P bytes: 기본 1024 IR은 약 32KiB, 최대 32768 IR은 약 1MiB. 원본 IR·resampled
임시 버퍼·중복 prepare·이전 chain은 추가다. complex multiply-accumulate는 128 samples당
256P개로 길이에 선형 증가하며, 최대 IR은 기본보다 이 부분이 32배다. 모든 연산이
partition 경계에 몰리므로 평균 CPU보다 worst callback이 중요하다. 24 cabinets라면
spectra만 약 24MiB, 최대 3072 samples의 직렬 latency와 큰 CPU 부담이다.
Core의 공통 latency/tail query가 없어 현재 Cabinet 특별 취급은 Engine에 남아 있다.
P1 측정 후에만 FFT 최적화/non-uniform partition/backend 교체를 검토할 것.

IR은 preset에 float 값을 JSON 숫자로 내장한다. source rate까지 복원되지만 파일명,
content hash, 라이선스/출처는 보관하지 않는다. 동일 IR을 여러 node가 쓰면 중복 저장.
24×32768=786432개 숫자로 수~수십 MB가 될 수 있다(표현 길이에 따라 변동).
Preset parse/build/IR FFT는 UI thread이므로 긴 chain에서 UI 정지 및 준비 지연 가능.
JUCE replaceWithText는 temporary file 교체를 사용하므로 단순 직접 덮어쓰기라고
평가하면 안 된다. 다만 disk-full/권한 실패 실제 검증은 없다.

Amp는 HPF → biased tanh 2단 → shelf/peak tone stack → master/tanh power → presence
→ output gain → LPF 구조다. 회로 동역학/sag/model inference는 없다. low-cut만 double
재귀 상태를 사용한다. registry creator 내부를 교체하면 기존 세 stable control과
GUI/chain은 유지할 수 있다. 그러나 같은 preset에서 다른 소리가 나므로 algorithm
version이나 별도 effect type 도입은 필요하다. GUI 자동 생성은 유지 가능하지만
고정 레이아웃은 parameter 수 증가에 한계가 있다.

Delay/Reverb는 객체 소유 vector를 prepare에서 확보한다. 정상 registry 범위에서
Delay feedback<=.95, Reverb feedback=.2+.72×room<=.92, damping<=.99로 안정적인
기본 범위다. raw DSP public API 전체가 범위를 검증하는 것은 아니므로 adapter 외
직접 사용에는 precondition이 필요하다. Delay의 첫 echo가 feedback에 비례하는
reference 의미와 Reverb의 단순 comb/all-pass 식은 현재 보존 대상이다. 개선을
버그 수정으로 위장하지 말고 별도 음질 버전으로 측정·검증해야 한다.

## Build / hygiene / 테스트 공백

- CMake C++17, MSVC /utf-8, app 선택 옵션, JUCE commit 고정은 양호하다.
  macOS Debug/Release 및 Core-only 빌드 기록이 있다. Windows 실행은 미확인.
  plugin 추가 시 Core 재사용은 가능하나 현재 MainComponent 안에 device/control/
  preset 교체 정책이 집중되어 있으므로 host adapter로 분리해야 한다.
  JUCE add_plugin, bus/parameter automation, latency/tail, host state와 callback
  계약 검증이 선행되어야 한다. 설치/export/배포서명/notarization은 아직 없다.
- build, .venv, .DS_Store, pycache는 ignore되고 추적된 build/audio WAV는 없다.
  docs PNG는 분석 결과로 의도적 추적. .vscode는 위 절대 경로 부채가 있다.
  WAV ignore는 지정 디렉터리 바로 아래 소문자 .wav에 한정된다. 다른 확장자,
  중첩 asset, build 경로 외 generated file은 추가 관리 규칙이 필요하다.
- 기존 CTest 13개/pytest 42개/수치 비교/ASan·UBSan·TSan은 좋은 기반이나, GUI
  callback 전체를 자동 검증하는 것은 아니다. 최장 IR+다중 cabinet+64 samples의
  worst deadline, hotplug/rate 실패 복구, mute/test sound, 수동 GUI raw pointer
  lifecycle, disk-full/거대 JSON, clipping 위치·실제 입력 gain staging이 빠져 있다.
- long-run은 기본 합성 IR/10-node chain, 지원 controls를 대상으로 한다. test RSS
  함수는 측정 실패 또는 비macOS에서 0을 반환하므로 '0 증가'와 '미측정'을 구별하는
  표기가 필요하다. tracker는 C++ new/delete만 감시한다. sample finite 결과가 음질,
  모든 내부 state의 안정성, 실제 audio deadline을 보증하지 않는다.
- Python은 reference/research/analysis로 유지. filters/eq 재귀 처리 중복,
  modulation fractional-delay 중복은 당장 정리할 우선순위가 낮다.

## P0–P6 로드맵

| 단계/문제 | 왜 중요한가 | 예상 변경 영역 | 난이도 | 시점 |
|---|---|---|---|---|
| P0: prepare 실패 무음/장치 오류 은폐(R1) | 원인 없이 연주 불가, 실패 복구 없음 | App lifecycle, 오류 전달, candidate 준비/rollback, 장치 테스트 | 중 | 기타 테스트 직후 최우선 |
| P0: Mute와 Test 음 경로 불일치(R2) | mute를 믿고 볼륨을 올렸을 때 예상치 못한 소리 | JUCE adapter/Test 경로 정책, 운영 안내 | 중 | 테스트 전 인지, 직후 정책 수정 |
| P0: 실제 clipping/dropout/hotplug 재현 | 오프라인 통과로 실기 문제를 판정할 수 없음 | 장치 로그/측정, 해당 재현 원인만 수정 | 중 | 실제 발생한 것부터 즉시 |
| P1: reset 할당/중복 prepare/비원자적 편집 | host·장치 재시작 신뢰성 | Effect lifecycle, prepared state ownership, 실패 복구 | 중~상 | v0.1 안정화 때 |
| P1: preset migration/입력 한계/asset 출처 | 기존 preset 실패, 큰 파일 UI 정지, 음색 재현 | PresetIO, schema별 migration, IR hash/metadata | 중 | v0.1 안정화 때 |
| P1: validation 공백/Windows/환경 경로 | 재현 가능한 회귀 및 이식성 | device tests, CMake/CI, comparison paths, .vscode | 중 | v0.1 안정화 때 |
| P1: stereo 및 latency/tail 계약 결정 | plugin 단계에서 API 재작성 방지 | ProcessSpec, buffer/channel adapter, Effect capabilities | 중~상 | 설계는 안정화 때, 구현은 필요 범위만 |
| P2: delay-time click, gain staging, bypass level | 연주 중 품질과 조작성 | Delay transition, output protection, parameter별 smoothing | 중 | 실제 녹음과 측정 확보 후 |
| P2: IR resampling/normalization 및 Reverb 평가 | 톤/레벨/공간감 보존과 품질 | offline resampler, cabinet level, reverb 분석 | 중 | A/B 기준 확보 후 |
| P3: VST3/AU host 계약 부재 | DAW에서 자동화·복원·latency가 정확해야 함 | JUCE plugin target, host adapter, bus layout, state/automation | 상 | Standalone 안정화 후 |
| P4: aliasing/단순 amp 모델/긴 IR CPU | 고품질 비선형 처리와 제한된 deadline | 측정 기반 oversampling, amp backend, FFT/backend 교체 | 상 | aliasing/CPU 증거와 회귀 기준 이후 |
| P5: 자동 matching/capture 없음 | 연구와 제품 inference를 분리해야 함 | Python optimizer/data, model interface, versioned parameters | 상 | canonical Core와 dataset 관리 안정화 후 |
| P6: heap/std::function/OS adapter 예산 미확정 | embedded CPU/RAM·실시간 제약 | 메모리 예산, fixed-capacity 선택, HAL, cross-build | 상 | 실제 부하 측정과 플랫폼 선정 이후 |

## 실제 기타 테스트 후 바로 할 일과 성숙도

DI/출력 샘플, interface 모델·driver, sample rate/block, input gain/output level,
사용 IR 길이·source rate, preset, 정확한 knob/장치 전환 순서를 함께 보관한다.
한 번에 하나씩 바꿔 clipping/dropout/click/latency를 분리하고 재현 fixture로 만든다.
R1/R2를 먼저 정리하고, 증거 없는 DSP 음색 변경은 보류한다.

현재 분류는 **Prototype**. 실행 가능한 전체 chain과 자동 검증 기반은 있지만
실제 장치 장시간 운용, 오류 복구, 폭넓은 preset/host/플랫폼 계약과 음질 검증이
미완료다. 실제 기타 테스트 및 P0/P1 완료 후 제한된 Alpha로 평가하는 것이 적절하다.
Beta/Product로 볼 근거는 아직 없다.
