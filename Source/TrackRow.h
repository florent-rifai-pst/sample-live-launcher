#pragma once

#include <JuceHeader.h>
#include "Text.h"

//==============================================================================
// One editable row in a song's track list: name, output-channel field, gain,
// remove. Channels are shown 1-based to the user and converted to the engine's
// 0-based indices via the callbacks.
class TrackRow  : public juce::Component,
                  public juce::DragAndDropTarget
{
public:
    TrackRow (const juce::String& name, const std::vector<int>& outChannels0Based,
              const std::vector<float>& outGains, float gain, bool mute, bool solo);

    void resized() override;
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;

    void setIndex (int i)  { index = i; }
    void setLevel (float linearPeak);   // feed the real-time meter (0..~1)
    void setNumOutputs (int n) { numOutputs = juce::jmax (1, n); }   // for the picker
    int  getPreferredHeight() const;    // grows when the per-output panel is open

    std::function<void (TrackRow*)>                    onRemove;
    std::function<void (TrackRow*, std::vector<int>)>  onChannelsChanged;
    std::function<void (TrackRow*, std::vector<float>)> onChannelGainsChanged;
    std::function<void (TrackRow*, float)>             onGainChanged;
    std::function<void (TrackRow*, bool)>              onMuteChanged;
    std::function<void (TrackRow*, bool)>              onSoloChanged;
    std::function<void (int from, int to)>             onReorder;    // drag'n'drop reorder
    std::function<void()>                              onLayoutChanged;   // height changed

    //== DragAndDropTarget ====================================================
    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override;
    void itemDragExit  (const SourceDetails&) override;
    void itemDropped   (const SourceDetails&) override;

    static std::vector<int> parseChannels (const juce::String& text);
    static juce::String      channelsToText (const std::vector<int>& chans0Based);

private:
    void openOutputSelector();
    void updateChanButtonText();
    void rebuildOutputGainRows();   // one slider per assigned output
    void toggleExpanded();

    static constexpr int headerH = 40;
    static constexpr int subRowH = 26;

    int   index      = 0;
    bool  dragOver   = false;
    bool  expanded   = false;       // per-output volume panel open
    int   numOutputs = 2;           // available physical outputs (for the picker)
    float level      = 0.0f;        // smoothed meter value, 0..1
    std::vector<int>   channels;    // current outputs (0-based)
    std::vector<float> channelGains;// per-output gain, aligned with channels
    juce::Rectangle<int> meterBounds;

    juce::Label      gripLabel    { {}, juce::String::fromUTF8 ("\xe2\x89\xa1") };  // drag handle
    juce::Label      nameLabel;
    juce::TextButton chanButton;    // opens the output picker
    juce::TextButton muteButton   { "M" };
    juce::TextButton soloButton   { "S" };
    juce::Slider     gainSlider;
    juce::TextButton removeButton { "X" };

    juce::OwnedArray<juce::Label>  outGainLabels;   // per-output volume rows
    juce::OwnedArray<juce::Slider> outGainSliders;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackRow)
};
