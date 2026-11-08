cmake_minimum_required(VERSION 3.22)
project(LDTestGain VERSION 1.0.0)
# point JUCE_DIR at the JUCE inside the Tracktion Engine checkout CMake fetched (build/_deps/tracktion-src/modules/juce)
add_subdirectory(${JUCE_DIR} juce)
juce_add_plugin(LDTestGain
    COMPANY_NAME "LD Test" PLUGIN_MANUFACTURER_CODE Ldts PLUGIN_CODE Ldtg
    FORMATS VST3 PRODUCT_NAME "LD Test Gain" IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE)
target_sources(LDTestGain PRIVATE Source/Plugin.cpp)
target_compile_definitions(LDTestGain PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(LDTestGain PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
