if(NOT APPLE)
    message(FATAL_ERROR "v0.6 plugin targets are currently macOS-only")
endif()
juce_add_plugin(MultiEffectsPlugin
    VERSION 0.6.0 COMPANY_NAME "Multi Effects"
    BUNDLE_ID "com.multieffects.plugin"
    PLUGIN_MANUFACTURER_CODE Mefx PLUGIN_CODE Mfx6
    PRODUCT_NAME "Multi Effects" FORMATS AU VST3
    IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(MultiEffectsPlugin PRIVATE PluginProcessor.cpp PluginEditor.cpp ../app/PresetIO.cpp)
target_compile_definitions(MultiEffectsPlugin PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(MultiEffectsPlugin PRIVATE multi_effects_dsp juce::juce_audio_utils juce::juce_gui_extra)
add_executable(test_plugin ../tests/test_plugin.cpp)
target_link_libraries(test_plugin PRIVATE MultiEffectsPlugin multi_effects_dsp juce::juce_audio_utils juce::juce_gui_extra)
add_test(NAME PluginLifecycleState COMMAND test_plugin)
add_test(NAME PluginConcurrentControls COMMAND test_plugin controls)
if(CMAKE_CXX_FLAGS MATCHES "fsanitize")
    target_compile_definitions(test_plugin PRIVATE MULTI_EFFECTS_TEST_SANITIZER=1)
endif()
