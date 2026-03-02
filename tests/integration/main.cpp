// tests/integration/main.cpp
// Custom main for integration tests — initialises JUCE before Catch2 runs.
#include <JuceHeader.h>
#include <catch2/catch_session.hpp>

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI init;
    return Catch::Session().run (argc, argv);
}
