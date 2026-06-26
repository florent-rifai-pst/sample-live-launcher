#include "TrackRow.h"

//==============================================================================
std::vector<int> TrackRow::parseChannels (const juce::String& text)
{
    std::vector<int> result;
    auto tokens = juce::StringArray::fromTokens (text, " ,;-", "");
    for (auto& tok : tokens)
    {
        auto trimmed = tok.trim();
        if (trimmed.isEmpty())
            continue;
        const int oneBased = trimmed.getIntValue();
        if (oneBased >= 1)
            result.push_back (oneBased - 1);
    }
    return result;
}

juce::String TrackRow::channelsToText (const std::vector<int>& chans0Based)
{
    juce::StringArray parts;
    for (int c : chans0Based)
        parts.add (juce::String (c + 1));
    return parts.joinIntoString (",");
}

//==============================================================================
TrackRow::TrackRow (const juce::String& name, const std::vector<int>& outChannels0Based, float gain)
{
    addAndMakeVisible (nameLabel);
    nameLabel.setText (name, juce::dontSendNotification);
    nameLabel.setJustificationType (juce::Justification::centredLeft);

    addAndMakeVisible (chanCaption);
    chanCaption.setJustificationType (juce::Justification::centredRight);

    addAndMakeVisible (chanEditor);
    chanEditor.setText (channelsToText (outChannels0Based), false);
    chanEditor.setTooltip ("Sorties physiques, ex: 1,2 (facade) ou 3,4 (clic)");
    chanEditor.onReturnKey  = [this] { commitChannels(); };
    chanEditor.onFocusLost  = [this] { commitChannels(); };

    addAndMakeVisible (gainSlider);
    gainSlider.setRange (0.0, 1.5, 0.01);
    gainSlider.setValue (gain, juce::dontSendNotification);
    gainSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    gainSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 20);
    gainSlider.onValueChange = [this]
    {
        if (onGainChanged)
            onGainChanged (this, (float) gainSlider.getValue());
    };

    addAndMakeVisible (removeButton);
    removeButton.onClick = [this] { if (onRemove) onRemove (this); };
}

void TrackRow::commitChannels()
{
    if (onChannelsChanged)
        onChannelsChanged (this, parseChannels (chanEditor.getText()));
}

void TrackRow::paint (juce::Graphics& g)
{
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 4.0f);
}

void TrackRow::resized()
{
    auto r = getLocalBounds().reduced (6, 4);
    removeButton.setBounds (r.removeFromRight (32));
    r.removeFromRight (6);
    gainSlider.setBounds (r.removeFromRight (180));
    r.removeFromRight (6);
    chanEditor.setBounds (r.removeFromRight (90));
    chanCaption.setBounds (r.removeFromRight (60));
    nameLabel.setBounds (r);
}
