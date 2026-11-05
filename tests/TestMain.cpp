#include <catch2/catch_session.hpp>
#include "TestSignals.h"

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    int result = 0;
    {
        tracktion::Engine engine { "LimitlessDrumsTests" };
        ldtest::enginePtr() = &engine;
        result = Catch::Session().run (argc, argv);
        ldtest::enginePtr() = nullptr;
    }
    return result;
}
