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
)


target_include_directories(
    multi_effects_dsp

    PUBLIC

    ${CMAKE_CURRENT_SOURCE_DIR}/include
)


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

endif()


# =========================================================
# Testing
# =========================================================

enable_testing()


add_executable(
    test_gain

    tests/test_gain.cpp
)


target_link_libraries(
    test_gain

    PRIVATE

    multi_effects_dsp
)


add_executable(
    test_distortion

    tests/test_distortion.cpp
)


target_link_libraries(
    test_distortion

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