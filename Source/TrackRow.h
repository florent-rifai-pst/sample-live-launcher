#pragma once

#include <JuceHeader.h>

//==============================================================================
// One editable row in a song's track list: name, output-channel field, gain,
// remove. Channels are shown 1-based to the user and converted to the engine's
// 0-based indices via the callbacks.
class TrackRow  : public juce::Component
{
public:
    TrackRow (const juce::String& name, const std::vector<int>& outChannels0Based, float gain);

    void resized() override;
    void paint (juce::Graphics&) override;

    std::function<void (TrackRow*)>                    onRemove;
    std::function<void (TrackRow*, std::vector<int>)>  onChannelsChanged;
    std::function<void (TrackRow*, float)>             onGainChanged;

    static std::vector<int> parseChannels (const juce::String& text);
    static juce::String      channelsToText (const std::vector<int>& chans0Based);

private:
    void commitChannels();

    juce::Label      nameLabel;
    juce::Label      chanCaption  { {}, "Sorties:" };
    juce::TextEditor chanEditor;
    juce::Slider     gainSlider;
    juce::TextButton removeButton { "X" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackRow)
};
