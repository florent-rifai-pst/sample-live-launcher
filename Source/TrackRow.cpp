#include "TrackRow.h"

//==============================================================================
// Small pop-up content (shown in a CallOutBox) listing the available physical
// outputs as clickable toggles. Selection changes are reported live.
namespace
{
    class OutputSelector  : public juce::Component
    {
    public:
        OutputSelector (int numOutputs, const std::vector<int>& selected0Based,
                        std::function<void (std::vector<int>)> onChange)
            : onChanged (std::move (onChange))
        {
            for (int c = 0; c < numOutputs; ++c)
            {
                auto* b = buttons.add (new juce::TextButton (juce::String (c + 1)));
                b->setClickingTogglesState (true);
                b->setToggleState (std::find (selected0Based.begin(), selected0Based.end(), c)
                                       != selected0Based.end(),
                                   juce::dontSendNotification);
                b->setColour (juce::TextButton::buttonOnColourId, juce::Colours::cornflowerblue);
                b->onClick = [this] { fire(); };
                addAndMakeVisible (b);
            }

            const int cols = juce::jmin (numOutputs, 4);
            const int rows = (numOutputs + cols - 1) / juce::jmax (1, cols);
            setSize (juce::jmax (1, cols) * (cellW + gap) + gap,
                     juce::jmax (1, rows) * (cellH + gap) + gap);
        }

        void resized() override
        {
            const int cols = juce::jmin (buttons.size(), 4);
            for (int i = 0; i < buttons.size(); ++i)
            {
                const int r = i / juce::jmax (1, cols);
                const int c = i % juce::jmax (1, cols);
                buttons[i]->setBounds (gap + c * (cellW + gap), gap + r * (cellH + gap), cellW, cellH);
            }
        }

    private:
        void fire()
        {
            std::vector<int> sel;
            for (int i = 0; i < buttons.size(); ++i)
                if (buttons[i]->getToggleState())
                    sel.push_back (i);
            if (onChanged)
                onChanged (sel);
        }

        static constexpr int cellW = 52, cellH = 30, gap = 6;
        juce::OwnedArray<juce::TextButton> buttons;
        std::function<void (std::vector<int>)> onChanged;
    };
}

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
TrackRow::TrackRow (const juce::String& name, const std::vector<int>& outChannels0Based,
                    float gain, bool mute, bool solo)
{
    addAndMakeVisible (gripLabel);
    gripLabel.setJustificationType (juce::Justification::centred);
    gripLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.5f));
    gripLabel.setTooltip ("Glisser pour réordonner"_t);
    gripLabel.setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    gripLabel.addMouseListener (this, false);   // forward grip drags to TrackRow::mouseDrag

    addAndMakeVisible (nameLabel);
    nameLabel.setText (name, juce::dontSendNotification);
    nameLabel.setJustificationType (juce::Justification::centredLeft);

    channels = outChannels0Based;
    addAndMakeVisible (chanButton);
    chanButton.setTooltip ("Choisir les sorties physiques"_t);
    chanButton.onClick = [this] { openOutputSelector(); };
    updateChanButtonText();

    addAndMakeVisible (muteButton);
    muteButton.setClickingTogglesState (true);
    muteButton.setToggleState (mute, juce::dontSendNotification);
    muteButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::firebrick);
    muteButton.setTooltip ("Muet"_t);
    muteButton.onClick = [this]
    {
        if (onMuteChanged)
            onMuteChanged (this, muteButton.getToggleState());
    };

    addAndMakeVisible (soloButton);
    soloButton.setClickingTogglesState (true);
    soloButton.setToggleState (solo, juce::dontSendNotification);
    soloButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::goldenrod);
    soloButton.setTooltip ("Solo");
    soloButton.onClick = [this]
    {
        if (onSoloChanged)
            onSoloChanged (this, soloButton.getToggleState());
    };

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

void TrackRow::updateChanButtonText()
{
    const juce::String list = channelsToText (channels);
    chanButton.setButtonText ("Sorties: "_t + (list.isEmpty() ? juce::String ("\xe2\x80\x94") : list));
}

void TrackRow::openOutputSelector()
{
    auto content = std::make_unique<OutputSelector> (numOutputs, channels,
        [this] (std::vector<int> sel)
        {
            channels = std::move (sel);
            updateChanButtonText();
            if (onChannelsChanged)
                onChannelsChanged (this, channels);
        });

    juce::CallOutBox::launchAsynchronously (std::move (content),
                                            chanButton.getScreenBounds(),
                                            nullptr);
}

//==============================================================================
void TrackRow::mouseDrag (const juce::MouseEvent& e)
{
    // Only the grip starts a drag.
    if (e.eventComponent != &gripLabel)
        return;

    if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
        if (! container->isDragAndDropActive())
            container->startDragging (index, this);
}

bool TrackRow::isInterestedInDragSource (const SourceDetails& d)
{
    return dynamic_cast<TrackRow*> (d.sourceComponent.get()) != nullptr;
}

void TrackRow::itemDragEnter (const SourceDetails&)
{
    dragOver = true;
    repaint();
}

void TrackRow::itemDragExit (const SourceDetails&)
{
    dragOver = false;
    repaint();
}

void TrackRow::itemDropped (const SourceDetails& d)
{
    dragOver = false;
    repaint();

    const int from = (int) d.description;
    if (onReorder && from != index)
        onReorder (from, index);
}

void TrackRow::paint (juce::Graphics& g)
{
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 4.0f);

    if (dragOver)
    {
        g.setColour (juce::Colours::cornflowerblue);
        g.drawLine (0.0f, 1.0f, (float) getWidth(), 1.0f, 2.0f);
    }

    // Real-time level meter under the name.
    if (! meterBounds.isEmpty())
    {
        auto bg = meterBounds.toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (bg, 2.0f);

        if (level > 0.001f)
        {
            auto fill = bg.withWidth (bg.getWidth() * level);
            const juce::Colour c = level < 0.7f ? juce::Colours::limegreen
                                 : level < 0.9f ? juce::Colours::gold
                                                : juce::Colours::orangered;
            g.setColour (c);
            g.fillRoundedRectangle (fill, 2.0f);
        }
    }
}

void TrackRow::resized()
{
    auto r = getLocalBounds().reduced (6, 4);
    gripLabel.setBounds (r.removeFromLeft (22));
    r.removeFromLeft (4);
    removeButton.setBounds (r.removeFromRight (32));
    r.removeFromRight (6);
    gainSlider.setBounds (r.removeFromRight (180));
    r.removeFromRight (6);
    soloButton.setBounds (r.removeFromRight (30));
    r.removeFromRight (3);
    muteButton.setBounds (r.removeFromRight (30));
    r.removeFromRight (8);
    chanButton.setBounds (r.removeFromRight (130));

    // Name block: label on top, real-time level meter underneath.
    meterBounds = r.removeFromBottom (6).reduced (0, 1);
    r.removeFromBottom (2);
    nameLabel.setBounds (r);
}

void TrackRow::setLevel (float linearPeak)
{
    const float target = juce::jlimit (0.0f, 1.0f, linearPeak);
    // Fast attack, slower release so the bar reads naturally at the UI refresh rate.
    level = target >= level ? target : level + (target - level) * 0.35f;
    repaint (meterBounds);
}
