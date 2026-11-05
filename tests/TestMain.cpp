#include <catch2/catch_session.hpp>
#include "TestSignals.h"
#include <iostream>


int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    int result = 0;
    {
        tracktion::Engine engine { "LimitlessDrumsTests", std::make_unique<ld::SyncUIBehaviour>(), nullptr };
        ldtest::enginePtr() = &engine;
        result = Catch::Session().run (argc, argv);
        ldtest::enginePtr() = nullptr;
    }
    return result;
}
