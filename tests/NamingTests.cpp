#include <catch2/catch_test_macros.hpp>
#include "core/Naming.h"

using ld::classifyMic;

static juce::StringArray namesInOrder (const juce::StringArray& files)
{
    std::vector<ld::MicRole> roles;
    ld::orderSession (files, roles);
    juce::StringArray out;
    for (auto& r : roles) out.add (r.displayName);
    return out;
}

TEST_CASE ("Naming: sample-session files (Matt's SPLIT format)", "[naming]")
{
    CHECK (classifyMic ("SNARE_CENTER_TOP_V06_RR06.wav").displayName == "Snare Top");
    CHECK (classifyMic ("SNARE_CENTER_BOTTOM_RR06_V06.wav").displayName == "Snare Bottom");
    CHECK (classifyMic ("SNARE_CENTER_OH_V06_RR06.wav").displayName == "Overheads");
    CHECK (classifyMic ("SNARE_CENTER_ROOM-MONO_RR06_V06.wav").displayName == "Room Mono");
    CHECK (classifyMic ("SNARE_WIRESOFF_MONO-ROOM_V03_RR06.wav").displayName == "Room Mono");
    CHECK (classifyMic ("SNARE_CENTER_ROOM_CLOSE_V06_RR06.wav").displayName == "Room Close");
    CHECK (classifyMic ("SNARE_RIMSHOT_ROOM-FAR_V01_RR06.wav").displayName == "Room Far");
    CHECK (classifyMic ("SNARE_SIDESTICK_ROOM-CRUSH_V06_RR06.wav").displayName == "Room Crush");
}

TEST_CASE ("Naming: sample session sorts top, bottom, OH, mono, close, far, crush", "[naming]")
{
    juce::StringArray files { "SNARE_CENTER_ROOM-FAR_RR06_V06.wav", "SNARE_CENTER_OH_RR06_V06.wav",
                              "SNARE_CENTER_ROOM-CRUSH_RR06_V06.wav", "SNARE_CENTER_TOP_RR06_V06.wav",
                              "SNARE_CENTER_ROOM-MONO_RR06_V06.wav", "SNARE_CENTER_BOTTOM_RR06_V06.wav",
                              "SNARE_CENTER_ROOM-CLOSE_RR06_V06.wav" };
    CHECK (namesInOrder (files) == juce::StringArray { "Snare Top", "Snare Bottom", "Overheads", "Room Mono",
                                                       "Room Close", "Room Far", "Room Crush" });
}

TEST_CASE ("Naming: Underoath session", "[naming]")
{
    juce::StringArray files { "08 OH.wav", "01 Kick1.wav", "05 Tom1.wav", "10 Hall.wav", "03 Snare1.wav",
                              "09 Room.wav", "02 Kick2.wav", "07 Tom3.wav", "04 Snare2.wav", "06 Tom2.wav" };
    CHECK (namesInOrder (files) == juce::StringArray { "Kick 1", "Kick 2", "Snare 1", "Snare 2", "Tom 1", "Tom 2",
                                                       "Tom 3", "Overheads", "Room", "Hall" });
}

TEST_CASE ("Naming: Saosin session (prints go last)", "[naming]")
{
    juce::StringArray files { "01 Reverse Swell.wav", "02 Q Drum Print.wav", "03 BB Kick Print.wav", "04 Mega Room Print.wav",
                              "05 Snare Top.wav", "06 Snare Bottom.wav", "07 Rack.wav", "08 Floor.wav", "09 OH Hat.wav",
                              "10 OH Ride.wav", "11 Room Violet.wav", "12 Crash L.wav", "13 Crash R.wav", "14 Hat.wav",
                              "15 Ride.wav", "16 Ceiling.wav" };
    CHECK (namesInOrder (files) == juce::StringArray { "Snare Top", "Snare Bottom", "Hi-Hat", "Tom", "Floor Tom", "Ride",
                                                       "Crash L", "Crash R", "Overheads Hat", "Overheads Ride",
                                                       "Room Violet", "Ceiling", "Reverse Swell", "Q Drum Print",
                                                       "BB Kick Print", "Mega Room Print" });
}

TEST_CASE ("Naming: common DAW spellings", "[naming]")
{
    CHECK (classifyMic ("Kick In.wav").displayName == "Kick In");
    CHECK (classifyMic ("KICK OUT.wav").displayName == "Kick Out");
    CHECK (classifyMic ("Kick Sub NS10.wav").displayName == "Kick Sub");
    CHECK (classifyMic ("SN BTM.wav").displayName == "Snare Bottom");
    CHECK (classifyMic ("Snr Top_01.wav").displayName == "Snare Top");
    CHECK (classifyMic ("HH.wav").displayName == "Hi-Hat");
    CHECK (classifyMic ("FT 2.wav").displayName == "Floor Tom 2");
    CHECK (classifyMic ("OH L.wav").displayName == "Overheads L");
    CHECK (classifyMic ("Overheads R.wav").displayName == "Overheads R");
    CHECK (classifyMic ("RM Far L.wav").displayName == "Room Far L");
    CHECK (classifyMic ("Room Close R.wav").displayName == "Room Close R");
    CHECK (classifyMic ("Trash.wav").displayName == "Room Crush");
    CHECK (classifyMic ("Beta 91 Kick In.wav").displayName == "Kick In");
}
