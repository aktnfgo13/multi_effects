cmake_minimum_required(VERSION 3.20)

project(
    MultiEffectsDSP
    VERSION 0.1.0
    LANGUAGES CXX
)


# =========================================================
# C++ Standard
# =========================================================

set(
    CMAKE_CXX_STANDARD 17
)

set(
    CMAKE_CXX_STANDARD_REQUIRED ON
)

set(
    CMAKE_CXX_EXTENSIONS OFF
)


# =========================================================
# DSP Library
# =========================================================

add_library(
    multi_effects_dsp

    src/effects/Gain.cpp
    src/effects/Distortion.cpp
    src/effects/Biquad.cpp
    src/effects/Filters.cpp
    src/effects/Compressor.cpp
    src/effects/NoiseGate.cpp
)


target_include_directories(
    multi_effects_dsp

    PUBLIC

    ${CMAKE_CURRENT_SOURCE_DIR}/include
)


# =========================================================
# MSVC Library Settings
# =========================================================

if(MSVC)

    target_compile_options(
        multi_effects_dsp
        PRIVATE
        /utf-8
    )

endif()


# =========================================================
# Gain CLI
# =========================================================

add_executable(
    gain_cli
    tools/gain_cli.cpp
)

target_link_libraries(
    gain_cli
    PRIVATE
    multi_effects_dsp
)


# =========================================================
# Distortion CLI
# =========================================================

add_executable(
    distortion_cli
    tools/distortion_cli.cpp
)

target_link_libraries(
    distortion_cli
    PRIVATE
    multi_effects_dsp
)


# =========================================================
# Filter CLI
# =========================================================

add_executable(
    filter_cli
    tools/filter_cli.cpp
)

target_link_libraries(
    filter_cli
    PRIVATE
    multi_effects_dsp
)


# =========================================================
# Compressor CLI
# =========================================================

add_executable(
    compressor_cli
    tools/compressor_cli.cpp
)

target_link_libraries(
    compressor_cli
    PRIVATE
    multi_effects_dsp
)


# =========================================================
# MSVC CLI Settings
# =========================================================

if(MSVC)

    target_compile_options(
        gain_cli
        PRIVATE
        /utf-8
    )

    target_compile_options(
        distortion_cli
        PRIVATE
        /utf-8
    )

    target_compile_options(
        filter_cli
        PRIVATE
        /utf-8
    )

    target_compile_options(
        compressor_cli
        PRIVATE
        /utf-8
    )

endif()


# =========================================================
# Testing
# =========================================================

enable_testing()

add_executable(noise_gate_cli tools/noise_gate_cli.cpp)
target_link_libraries(noise_gate_cli PRIVATE multi_effects_dsp)
add_executable(test_noise_gate_stream tests/test_noise_gate_stream.cpp)
target_link_libraries(test_noise_gate_stream PRIVATE multi_effects_dsp)
add_test(NAME NoiseGateStreamTest COMMAND test_noise_gate_stream)
if(MSVC)
    target_compile_options(noise_gate_cli PRIVATE /utf-8)
endif()


# ---------------------------------------------------------
# Gain Test
# ---------------------------------------------------------

add_executable(
    test_gain
    tests/test_gain.cpp
)

target_link_libraries(
    test_gain
    PRIVATE
    multi_effects_dsp
)


# ---------------------------------------------------------
# Distortion Test
# ---------------------------------------------------------

add_executable(
    test_distortion
    tests/test_distortion.cpp
)

target_link_libraries(
    test_distortion
    PRIVATE
    multi_effects_dsp
)


# ---------------------------------------------------------
# Biquad Test
# ---------------------------------------------------------

add_executable(
    test_biquad
    tests/test_biquad.cpp
)

target_link_libraries(
    test_biquad
    PRIVATE
    multi_effects_dsp
)


# ---------------------------------------------------------
# Filter Test
# ---------------------------------------------------------

add_executable(
    test_filters
    tests/test_filters.cpp
)

target_link_libraries(
    test_filters
    PRIVATE
    multi_effects_dsp
)


# ---------------------------------------------------------
# Compressor Test
# ---------------------------------------------------------

add_executable(
    test_compressor
    tests/test_compressor.cpp
)

target_link_libraries(
    test_compressor
    PRIVATE
    multi_effects_dsp
)


# ---------------------------------------------------------
# Noise Gate Test
# ---------------------------------------------------------

add_executable(
    test_noise_gate
    tests/test_noise_gate.cpp
)

target_link_libraries(
    test_noise_gate
    PRIVATE
    multi_effects_dsp
)


# =========================================================
# MSVC Test Settings
# =========================================================

if(MSVC)

    target_compile_options(
        test_gain
        PRIVATE
        /utf-8
    )

    target_compile_options(
        test_distortion
        PRIVATE
        /utf-8
    )

    target_compile_options(
        test_biquad
        PRIVATE
        /utf-8
    )

    target_compile_options(
        test_filters
        PRIVATE
        /utf-8
    )

    target_compile_options(
        test_compressor
        PRIVATE
        /utf-8
    )

    target_compile_options(
        test_noise_gate
        PRIVATE
        /utf-8
    )

endif()


# =========================================================
# Register Tests
# =========================================================

add_test(
    NAME GainTest
    COMMAND test_gain
)

add_test(
    NAME DistortionTest
    COMMAND test_distortion
)

add_test(
    NAME BiquadTest
    COMMAND test_biquad
)

add_test(
    NAME FilterTest
    COMMAND test_filters
)

add_test(
    NAME CompressorTest
    COMMAND test_compressor
)

add_test(
    NAME NoiseGateTest
    COMMAND test_noise_gate
)

add_executable(prototype_cli tools/prototype_cli.cpp)
target_link_libraries(prototype_cli PRIVATE multi_effects_dsp)

target_sources(multi_effects_dsp PRIVATE src/engine/Engine.cpp)

option(MULTI_EFFECTS_BUILD_PLUGIN "Build macOS AU and VST3" OFF)
option(MULTI_EFFECTS_BUILD_APP "Build JUCE standalone prototype" OFF)
if(MULTI_EFFECTS_BUILD_APP OR MULTI_EFFECTS_BUILD_PLUGIN)
    enable_language(C)
    add_subdirectory(app)
endif()

add_executable(test_engine tests/test_engine.cpp)
target_link_libraries(test_engine PRIVATE multi_effects_dsp)
add_test(NAME EngineTest COMMAND test_engine)
# Legacy assert-based tests must remain active in Release as well.
foreach(test_target test_gain test_distortion test_biquad test_filters test_compressor test_noise_gate)
    if(MSVC)
        target_compile_options(${test_target} PRIVATE /UNDEBUG)
    else()
        target_compile_options(${test_target} PRIVATE -UNDEBUG)
    endif()
endforeach()

find_package(Threads REQUIRED)
add_executable(test_stability tests/test_stability.cpp)
target_link_libraries(test_stability PRIVATE multi_effects_dsp Threads::Threads)
add_test(NAME BlockSampleRateMatrix COMMAND test_stability matrix)
add_test(NAME ChainStress COMMAND test_stability stress)
add_test(NAME ConcurrentParameterUpdates COMMAND test_stability controls)
add_test(NAME OfflineLongRun COMMAND test_stability long 600)
set_tests_properties(OfflineLongRun PROPERTIES TIMEOUT 600 LABELS "long")
# Preserve sanitizer allocator redzones; allocation accounting is tested separately
# by the normal Release executable, not by sanitizer builds.
if(CMAKE_CXX_FLAGS MATCHES "fsanitize")
    target_compile_definitions(test_stability PRIVATE MULTI_EFFECTS_TEST_SANITIZER=1)
endif()

add_executable(test_audio_io tests/test_audio_io.cpp)
target_link_libraries(test_audio_io PRIVATE multi_effects_dsp Threads::Threads)
add_test(NAME AudioIORoutingMeterMatrix COMMAND test_audio_io)
add_test(NAME AudioIOConcurrentControls COMMAND test_audio_io controls)
if(CMAKE_CXX_FLAGS MATCHES "fsanitize")
    target_compile_definitions(test_audio_io PRIVATE MULTI_EFFECTS_TEST_SANITIZER=1)
endif()

add_executable(nonlinear_benchmark tools/nonlinear_benchmark.cpp)
target_link_libraries(nonlinear_benchmark PRIVATE multi_effects_dsp)
add_executable(test_oversampling tests/test_oversampling.cpp)
target_link_libraries(test_oversampling PRIVATE multi_effects_dsp Threads::Threads)
add_test(NAME OversamplingQualityFoundation COMMAND test_oversampling)
if(CMAKE_CXX_FLAGS MATCHES "fsanitize")
    target_compile_definitions(test_oversampling PRIVATE MULTI_EFFECTS_TEST_SANITIZER=1)
endif()

add_test(NAME OversamplingLongRun COMMAND test_oversampling long)
add_test(NAME OversamplingConcurrentControls COMMAND test_oversampling controls)
set_tests_properties(OversamplingLongRun PROPERTIES TIMEOUT 600 LABELS "long")

add_executable(test_alpha_runtime tests/test_alpha_runtime.cpp)
target_link_libraries(test_alpha_runtime PRIVATE multi_effects_dsp Threads::Threads)
add_test(NAME AlphaTransitionsRollback COMMAND test_alpha_runtime)
add_test(NAME AlphaConcurrentControls COMMAND test_alpha_runtime controls)
if(CMAKE_CXX_FLAGS MATCHES "fsanitize")
    target_compile_definitions(test_alpha_runtime PRIVATE MULTI_EFFECTS_TEST_SANITIZER=1)
endif()

add_executable(test_stereo tests/test_stereo.cpp)
target_link_libraries(test_stereo PRIVATE multi_effects_dsp Threads::Threads)
add_test(NAME StereoCoreMatrix COMMAND test_stereo)
add_test(NAME StereoConcurrentControls COMMAND test_stereo controls)
if(CMAKE_CXX_FLAGS MATCHES "fsanitize")
    target_compile_definitions(test_stereo PRIVATE MULTI_EFFECTS_TEST_SANITIZER=1)
endif()

add_executable(custom_drive_benchmark tools/custom_drive_benchmark.cpp)
target_link_libraries(custom_drive_benchmark PRIVATE multi_effects_dsp)
add_executable(test_custom_drive tests/test_custom_drive.cpp)
target_link_libraries(test_custom_drive PRIVATE multi_effects_dsp Threads::Threads)
add_test(NAME CustomDriveFoundation COMMAND test_custom_drive)
add_test(NAME CustomDriveConcurrentControls COMMAND test_custom_drive controls)
if(CMAKE_CXX_FLAGS MATCHES "fsanitize")
    target_compile_definitions(test_custom_drive PRIVATE MULTI_EFFECTS_TEST_SANITIZER=1)
endif()

add_executable(test_analysis tests/test_analysis.cpp)
target_link_libraries(test_analysis PRIVATE multi_effects_dsp Threads::Threads)
add_test(NAME AnalysisTapSpectrum COMMAND test_analysis)
if(CMAKE_CXX_FLAGS MATCHES "fsanitize")
    target_compile_definitions(test_analysis PRIVATE MULTI_EFFECTS_TEST_SANITIZER=1)
endif()
