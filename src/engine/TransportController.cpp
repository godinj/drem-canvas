#include "TransportController.h"

namespace dc
{

TransportController::TransportController()
{
}

void TransportController::play()
{
    playing.store (true);
}

void TransportController::stop()
{
    playing.store (false);
}

void TransportController::togglePlayStop()
{
    if (playing.load())
        stop();
    else
        play();
}

void TransportController::advancePosition (int numSamples)
{
    if (playing.load())
    {
        auto newPos = positionInSamples.load() + static_cast<int64_t> (numSamples);

        if (loopEnabled.load())
        {
            auto loopStart = loopStartInSamples.load();
            auto loopEnd = loopEndInSamples.load();

            if (loopEnd > loopStart && newPos >= loopEnd)
                newPos = loopStart + (newPos - loopEnd);
        }

        positionInSamples.store (newPos);
    }
}

double TransportController::getPositionInSeconds() const
{
    double sr = sampleRate.load();

    if (sr <= 0.0)
        return 0.0;

    return static_cast<double> (positionInSamples.load()) / sr;
}

juce::String TransportController::getTimeString() const
{
    double totalSeconds = getPositionInSeconds();

    int minutes      = static_cast<int> (totalSeconds) / 60;
    int seconds      = static_cast<int> (totalSeconds) % 60;
    int milliseconds = static_cast<int> ((totalSeconds - std::floor (totalSeconds)) * 1000.0);

    return juce::String::formatted ("%02d:%02d.%03d", minutes, seconds, milliseconds);
}

} // namespace dc
