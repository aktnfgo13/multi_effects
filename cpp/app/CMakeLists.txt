include(${CMAKE_CURRENT_LIST_DIR}/../cmake/Product.cmake)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/generated)
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 29396c22c93392d6738e021b83196283d6e4d850
    GIT_PROGRESS TRUE)
FetchContent_MakeAvailable(JUCE)
if(MULTI_EFFECTS_BUILD_APP)
juce_add_gui_app(MultiEffectsStandalone
    VERSION ${MULTI_EFFECTS_PRODUCT_VERSION}
    PRODUCT_NAME "${MULTI_EFFECTS_PRODUCT_NAME}"
    COMPANY_NAME "Multi Effects"
    BUNDLE_ID "com.multieffects.prototype"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Use your audio input to process guitar through the effect chain.")
target_sources(MultiEffectsStandalone PRIVATE Main.cpp)
target_compile_definitions(MultiEffectsStandalone PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:MultiEffectsStandalone,JUCE_PRODUCT_NAME>" JUCE_APPLICATION_VERSION_STRING="${MULTI_EFFECTS_PRODUCT_VERSION}")
target_link_libraries(MultiEffectsStandalone PRIVATE multi_effects_dsp juce::juce_audio_utils juce::juce_gui_extra)

# Shared application IO: tests exercise the same implementation as the GUI.
target_sources(MultiEffectsStandalone PRIVATE PresetIO.cpp)
endif()
add_executable(test_preset_ir ../tests/test_preset_ir.cpp PresetIO.cpp)
target_compile_definitions(test_preset_ir PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(test_preset_ir PRIVATE multi_effects_dsp juce::juce_audio_formats)
add_test(NAME PresetIRRoundtrip COMMAND test_preset_ir)

add_executable(test_app_state ../tests/test_app_state.cpp PresetIO.cpp)
target_compile_definitions(test_app_state PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(test_app_state PRIVATE multi_effects_dsp juce::juce_audio_formats juce::juce_data_structures)
add_test(NAME AlphaSettingsPresetUX COMMAND test_app_state)

if(MULTI_EFFECTS_BUILD_PLUGIN)
    add_subdirectory(../plugin ${CMAKE_CURRENT_BINARY_DIR}/plugin)
endif()
