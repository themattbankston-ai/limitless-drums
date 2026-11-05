cmake_minimum_required(VERSION 3.24)

project(LimitlessDrums VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum macOS version")
endif()

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# --- Tracktion Engine (bundles the JUCE version it is tested against) -------------------
# Pinned to an exact commit so every build uses the same engine and JUCE.
set(TE_ADD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(tracktion
    GIT_REPOSITORY https://github.com/Tracktion/tracktion_engine.git
    GIT_TAG        a5e44996f5f08dfa6af5f5d02d900fb3cfa37422
    GIT_SUBMODULES modules/juce
    GIT_SHALLOW    FALSE)
FetchContent_MakeAvailable(tracktion)

# --- Catch2 for tests --------------------------------------------------------------------
option(LD_BUILD_TESTS "Build the test suite" ON)
option(LD_BUILD_APP "Build the app" ON)

if(LD_BUILD_TESTS)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.7.1
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(Catch2)
endif()

# --- Shared code: everything except the app shell, used by the app and the tests ----------
add_library(LDShared INTERFACE)
target_sources(LDShared INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Conform.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Session.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Analysis.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Naming.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Import.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Project.cpp)
target_include_directories(LDShared INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(LDShared INTERFACE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_PLUGINHOST_VST3=1
    JUCE_DISPLAY_SPLASH_SCREEN=0
    LD_VERSION_STRING="${PROJECT_VERSION}")
if(APPLE)
    target_compile_definitions(LDShared INTERFACE JUCE_PLUGINHOST_AU=1)
endif()
target_link_libraries(LDShared INTERFACE
    tracktion::tracktion_core
    tracktion::tracktion_engine
    tracktion::tracktion_graph
    juce::juce_audio_devices
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_gui_extra)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(LDShared INTERFACE atomic)
endif()

# --- The app ---------------------------------------------------------------------------------
if(LD_BUILD_APP)
    juce_add_gui_app(LimitlessDrums
        PRODUCT_NAME "Limitless Drums"
        COMPANY_NAME "Limitless Drums"
        BUNDLE_ID "com.limitlessdrums.app"
        ICON_BIG ${CMAKE_CURRENT_SOURCE_DIR}/assets/logo/app-icon.png
        MICROPHONE_PERMISSION_ENABLED TRUE
        MICROPHONE_PERMISSION_TEXT "Limitless Drums uses audio inputs only when you choose to record or monitor."
        NEEDS_CURL FALSE
        NEEDS_WEB_BROWSER FALSE
        PLUGINHOST_AU TRUE)

    juce_add_binary_data(LDAssets SOURCES
        assets/logo/mark-night.svg
        assets/logo/lockup-horizontal-night.svg
        assets/logo/lockup-horizontal-day.svg)

    target_sources(LimitlessDrums PRIVATE
        src/app/Main.cpp
        src/app/MainComponent.cpp
        src/app/SessionView.cpp)
    target_link_libraries(LimitlessDrums PRIVATE
        LDShared
        LDAssets
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)
endif()

# --- Tests -----------------------------------------------------------------------------------
if(LD_BUILD_TESTS)
    juce_add_console_app(LDTests PRODUCT_NAME "LDTests")
    target_sources(LDTests PRIVATE
        tests/TestMain.cpp
        tests/TestSignals.cpp
        tests/SpikeTests.cpp
        tests/NamingTests.cpp
        tests/ImportTests.cpp
        tests/ProjectTests.cpp)
    target_link_libraries(LDTests PRIVATE LDShared Catch2::Catch2 juce::juce_recommended_config_flags)
    enable_testing()
    add_test(NAME LDTests COMMAND LDTests --reporter console --durations yes)
endif()
