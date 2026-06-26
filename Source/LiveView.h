#pragma once

#include <JuceHeader.h>
#include "Models.h"
#include "AudioEngine.h"
#include "Text.h"

//==============================================================================
// Stripped-down live view: pick a setlist, fire songs one by one. Stub for now.
class LiveView  : public juce::Component
{
public:
    LiveView (Library& library, AudioEngine& engine);

    void refresh();
    void resized() override;

private:
    Library&     library;
    AudioEngine& engine;
    juce::Label  info { {}, "Vue live — à venir"_t };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LiveView)
};
