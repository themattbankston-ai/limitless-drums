cmake_minimum_required(VERSION 3.24)

project(LimitlessDrums VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(APPLE)
    # Visage enables Objective-C++ in its own directory; it must be on here too, or targets
    # outside that directory can't build their .mm files.
    enable_language(OBJC OBJCXX)
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
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Analysis.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Conform.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Session.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Naming.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Import.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Project.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hl/Wav.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hl/Detect.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hl/Fft.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hl/Align.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hl/Resolve.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hl/Tune.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hl/Levels.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hl/FxCore.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hl/FxInserts.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hl/FxSat.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hl/Clean.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hl/Shape.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hl/Song.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hl/Session.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hl/Prep.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hl/Rules.cpp)
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
    # Visage: the GPU interface (Metal on macOS, Vulkan on Linux). Pinned to an exact commit.
    # freetype is declared first from its GitHub mirror so every machine can fetch it.
    set(VISAGE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(VISAGE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(VISAGE_WAYLAND OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(freetype
        GIT_REPOSITORY https://github.com/freetype/freetype.git
        GIT_TAG        VER-2-14-1
        GIT_SHALLOW    TRUE
        EXCLUDE_FROM_ALL)
    FetchContent_Declare(visage
        GIT_REPOSITORY https://github.com/VitalAudio/visage.git
        GIT_TAG        238d1b542002e03ad42accd076f938304e67971d
        GIT_SHALLOW    FALSE)
    FetchContent_MakeAvailable(visage)

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
        assets/ui/rack.png
        assets/ui/rack_align.png
        assets/ui/rack_tune.png
        assets/ui/rack_mixstep.png
        assets/ui/rack_mix2.png
        assets/ui/parts/module_top.png
        assets/ui/parts/module_mid.png
        assets/ui/parts/module_bot.png
        assets/ui/parts/knob_mono.png
        assets/ui/parts/knob_stereo.png
        assets/ui/parts/cap_small.png
        assets/ui/parts/cap_big.png
        assets/ui/parts/bigfader.png
        assets/ui/parts/title.png
        assets/ui/parts/strip_mono.png
        assets/ui/parts/strip_stereo.png
        assets/ui/parts/strip_bus.png
        assets/ui/parts/divider.png
        assets/ui/plugins/skull_cracks.jpg
        assets/ui/plugins/skull_eyes.jpg
        assets/ui/plugins/skull_plate.jpg
        assets/ui/plugins/vise_plate.jpg
        assets/ui/plugins/horizon_plate.jpg
        assets/ui/plugins/horizon_lever.png
        assets/ui/plugins/prism_plate.jpg
        assets/ui/plugins/ember_plate.jpg
        assets/ui/plugins/ember_hot.jpg
        assets/ui/plugins/port_plate.jpg
        assets/ui/plugins/port_open.jpg
        assets/ui/plugins/port_closed.jpg
        assets/ui/plugins/port_gate.png
        assets/ui/plugins/port_arch.png
        assets/ui/plugins/vise_beacon.jpg
        assets/ui/plugins/vise_ram.png
        assets/ui/plugins/skull_saw_0.jpg
        assets/ui/plugins/skull_saw_1.jpg
        assets/ui/plugins/skull_saw_2.jpg
        assets/ui/plugins/skull_saw_3.jpg
        assets/ui/plugins/skull_saw_4.jpg
        assets/ui/plugins/skull_saw_5.jpg
        assets/ui/plugins/skull_saw_6.jpg
        assets/ui/plugins/skull_saw_7.jpg
        assets/ui/plugins/skull_sawb_0.jpg
        assets/ui/plugins/skull_sawb_1.jpg
        assets/ui/plugins/skull_sawb_2.jpg
        assets/ui/plugins/skull_sawb_3.jpg
        assets/ui/plugins/skull_sawb_4.jpg
        assets/ui/plugins/skull_sawb_5.jpg
        assets/ui/plugins/skull_sawb_6.jpg
        assets/ui/plugins/skull_sawb_7.jpg
        assets/ui/plugins/skull_skull.png
        assets/fonts/BlackOpsOne-Regular.ttf
        assets/fonts/Cinzel-Bold.ttf
        assets/fonts/BebasNeue-Regular.ttf
        assets/fonts/Oswald-SemiBold.ttf
        assets/irlib.json
        assets/libref.json
        assets/logo/lockup-horizontal-night.svg
        assets/logo/lockup-horizontal-day.svg
        assets/fonts/InterDisplay-SemiBold.otf
        assets/fonts/InterDisplay-Bold.otf
        assets/fonts/Inter-Medium.otf
        assets/fonts/DejaVuSansMono.ttf
        assets/fonts/DejaVuSansMono-Bold.ttf
        assets/ui/header-plate.png)

    target_sources(LimitlessDrums PRIVATE
        src/app/Main.cpp
        src/app/MainComponent.cpp
        src/app/SessionModel.cpp
        src/app/Engine.cpp
        src/app/Audition.cpp
        src/app/ExtPlugins.cpp
        src/app/Library.cpp
        src/app/ui/VisageHost.cpp
        src/app/ui/Studio.cpp
        src/app/ui/Wave.cpp
        src/app/ui/NewSession.cpp)
    target_link_libraries(LimitlessDrums PRIVATE
        LDShared
        LDAssets
        visage
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
        tests/ProjectTests.cpp
        tests/ParityTests.cpp
        tests/ExportParityTests.cpp
        tests/PrepParityTests.cpp
        tests/AuditionTests.cpp
        tests/RulesTests.cpp
        tests/AlignOpsTests.cpp
    tests/VerbSendTests.cpp
    tests/StandardTests.cpp
    tests/CompMeterTests.cpp
    tests/MemoryProbe.cpp
    tests/SongTests.cpp
    tests/AuditFixTests.cpp
    tests/ExtPluginTests.cpp
        tests/TuneAdviceTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/NewSessionTests.cpp
        tests/WaveTests.cpp
        src/app/ui/Wave.cpp
        src/app/Audition.cpp
        src/app/ExtPlugins.cpp
        src/app/Library.cpp
        src/app/Engine.cpp)
    target_compile_definitions(LDTests PRIVATE LD_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(LDTests PRIVATE LDShared Catch2::Catch2 juce::juce_recommended_config_flags)
    enable_testing()
    add_test(NAME LDTests COMMAND LDTests --reporter console --durations yes)
endif()
