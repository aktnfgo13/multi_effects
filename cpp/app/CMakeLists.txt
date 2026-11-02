include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 29396c22c93392d6738e021b83196283d6e4d850
    GIT_PROGRESS TRUE)
FetchContent_MakeAvailable(JUCE)
juce_add_gui_app(MultiEffectsStandalone
    PRODUCT_NAME "Multi Effects Prototype"
    COMPANY_NAME "Multi Effects"
    BUNDLE_ID "com.multieffects.prototype"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Use your audio input to process guitar through the effect chain.")
target_sources(MultiEffectsStandalone PRIVATE Main.cpp)
target_compile_definitions(MultiEffectsStandalone PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:MultiEffectsStandalone,JUCE_PRODUCT_NAME>" JUCE_APPLICATION_VERSION_STRING="0.1.0")
target_link_libraries(MultiEffectsStandalone PRIVATE multi_effects_dsp juce::juce_audio_utils juce::juce_gui_extra)
