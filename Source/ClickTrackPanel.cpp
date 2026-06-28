#include "ClickTrackPanel.h"
#include "TrackRow.h"   // reuse parseChannels / channelsToText

//==============================================================================
ClickSegmentRow::ClickSegmentRow()
{
    auto addCaption = [this] (juce::Label& l)
    {
        addAndMakeVisible (l);
        l.setJustificationType (juce::Justification::centred);
        l.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.7f));
    };

    addCaption (fromCaption);
    addCaption (toCaption);
    addCaption (sigCaption);
    addCaption (slashCaption);
    addCaption (bpmCaption);

    auto setupInt = [this] (juce::TextEditor& e)
    {
        addAndMakeVisible (e);
        e.setInputRestrictions (4, "0123456789");
        e.setJustification (juce::Justification::centred);
        e.onFocusLost = [this] { commit(); };
        e.onReturnKey = [this] { commit(); };
    };

    setupInt (fromEd);
    setupInt (toEd);

    addAndMakeVisible (numCombo);
    for (int n = 1; n <= 16; ++n)
        numCombo.addItem (juce::String (n), n);
    numCombo.onChange = [this] { commit(); };

    addAndMakeVisible (denCombo);
    for (int d : { 1, 2, 4, 8, 16 })
        denCombo.addItem (juce::String (d), d);
    denCombo.onChange = [this] { commit(); };

    addAndMakeVisible (bpmEd);
    bpmEd.setInputRestrictions (6, "0123456789.");
    bpmEd.setJustification (juce::Justification::centred);
    bpmEd.onFocusLost = [this] { commit(); };
    bpmEd.onReturnKey = [this] { commit(); };

    addAndMakeVisible (removeButton);
    removeButton.onClick = [this] { if (onRemove) onRemove(); };
}

void ClickSegmentRow::bind (ClickSegment* seg)
{
    segment = seg;
    if (segment == nullptr)
        return;

    fromEd.setText (juce::String (segment->barFrom), juce::dontSendNotification);
    toEd  .setText (juce::String (segment->barTo),   juce::dontSendNotification);
    numCombo.setSelectedId (juce::jlimit (1, 16, segment->numerator),   juce::dontSendNotification);
    denCombo.setSelectedId (segment->denominator,                       juce::dontSendNotification);
    bpmEd .setText (juce::String (segment->bpm, 1),  juce::dontSendNotification);
}

void ClickSegmentRow::commit()
{
    if (segment == nullptr)
        return;

    const int from = juce::jmax (1, fromEd.getText().getIntValue());
    int       to   = juce::jmax (1, toEd.getText().getIntValue());
    to = juce::jmax (to, from);                       // B can't be before A

    segment->barFrom     = from;
    segment->barTo       = to;
    segment->numerator   = juce::jmax (1, numCombo.getSelectedId());
    segment->denominator = juce::jmax (1, denCombo.getSelectedId());
    segment->bpm         = juce::jlimit (20.0, 400.0, bpmEd.getText().getDoubleValue());

    // reflect any clamping back into the fields
    fromEd.setText (juce::String (segment->barFrom), juce::dontSendNotification);
    toEd  .setText (juce::String (segment->barTo),   juce::dontSendNotification);
    bpmEd .setText (juce::String (segment->bpm, 1),  juce::dontSendNotification);

    if (onChanged)
        onChanged();
}

void ClickSegmentRow::paint (juce::Graphics& g)
{
    g.setColour (juce::Colours::white.withAlpha (0.04f));
    g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 4.0f);
}

void ClickSegmentRow::resized()
{
    auto r = getLocalBounds().reduced (6, 3);

    fromCaption.setBounds (r.removeFromLeft (56));
    fromEd     .setBounds (r.removeFromLeft (44));
    r.removeFromLeft (4);
    toCaption  .setBounds (r.removeFromLeft (18));
    toEd       .setBounds (r.removeFromLeft (44));
    r.removeFromLeft (12);

    sigCaption .setBounds (r.removeFromLeft (66));
    numCombo   .setBounds (r.removeFromLeft (54));
    slashCaption.setBounds (r.removeFromLeft (14));
    denCombo   .setBounds (r.removeFromLeft (54));
    r.removeFromLeft (12);

    bpmEd      .setBounds (r.removeFromLeft (56));
    bpmCaption .setBounds (r.removeFromLeft (40));

    removeButton.setBounds (r.removeFromRight (30));
}

//==============================================================================
ClickTrackPanel::ClickTrackPanel()
{
    addAndMakeVisible (enableToggle);
    enableToggle.onClick = [this]
    {
        if (click != nullptr)
        {
            click->enabled = enableToggle.getToggleState();
            notify();
        }
    };

    addAndMakeVisible (chanCaption);
    chanCaption.setJustificationType (juce::Justification::centredRight);

    addAndMakeVisible (chanEditor);
    chanEditor.setTooltip ("Sorties physiques, ex: 3 4"_t);
    chanEditor.onFocusLost = [this]
    {
        if (click != nullptr)
        {
            click->channels = TrackRow::parseChannels (chanEditor.getText());
            notify();
        }
    };
    chanEditor.onReturnKey = [this] { chanEditor.giveAwayKeyboardFocus(); };

    addAndMakeVisible (gainCaption);
    gainCaption.setJustificationType (juce::Justification::centredRight);

    addAndMakeVisible (gainSlider);
    gainSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    gainSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 20);
    gainSlider.setRange (0.0, 1.0, 0.01);
    gainSlider.onValueChange = [this]
    {
        if (click != nullptr)
        {
            click->gain = (float) gainSlider.getValue();
            notify();
        }
    };

    addAndMakeVisible (bankCaption);
    bankCaption.setJustificationType (juce::Justification::centredRight);

    addAndMakeVisible (bankCombo);
    bankCombo.onChange = [this]
    {
        if (click == nullptr)
            return;
        const int id = bankCombo.getSelectedId();
        click->soundBankId = id <= 1 ? juce::String()                 // 1 = default beep
                                     : bankIds[id - 2];
        notify();
    };

    addAndMakeVisible (addSegBtn);
    addSegBtn.onClick = [this] { addSegment(); };
}

void ClickTrackPanel::setSoundBanks (const std::vector<ClickSoundBank>& banks)
{
    bankIds.clear();
    bankCombo.clear (juce::dontSendNotification);
    bankCombo.addItem ("Son par d\xc3\xa9""faut"_t, 1);          // "Son par défaut"

    int id = 2;
    for (auto& b : banks)
    {
        bankIds.add (b.id);
        bankCombo.addItem (b.name, id++);
    }

    refreshBankSelection();
}

void ClickTrackPanel::refreshBankSelection()
{
    int id = 1;                                  // default
    if (click != nullptr && click->soundBankId.isNotEmpty())
    {
        const int idx = bankIds.indexOf (click->soundBankId);
        if (idx >= 0)
            id = idx + 2;
    }
    bankCombo.setSelectedId (id, juce::dontSendNotification);
}

void ClickTrackPanel::setClickTrack (ClickTrack* c)
{
    click = c;

    const bool has = click != nullptr;
    enableToggle.setEnabled (has);
    chanEditor.setEnabled (has);
    gainSlider.setEnabled (has);
    bankCombo.setEnabled (has);
    addSegBtn.setEnabled (has);

    if (has)
    {
        enableToggle.setToggleState (click->enabled, juce::dontSendNotification);
        chanEditor.setText (TrackRow::channelsToText (click->channels), juce::dontSendNotification);
        gainSlider.setValue (click->gain, juce::dontSendNotification);
    }
    else
    {
        chanEditor.clear();
        gainSlider.setValue (1.0, juce::dontSendNotification);
    }

    refreshBankSelection();
    rebuildRows();
}

void ClickTrackPanel::notify()
{
    if (onChange)
        onChange();
}

void ClickTrackPanel::addSegment()
{
    if (click == nullptr)
        return;

    ClickSegment s;
    if (! click->segments.empty())
    {
        // continue from the last brick: same signature/tempo, next 8 bars
        const auto& prev = click->segments.back();
        s = prev;
        s.barFrom = prev.barTo + 1;
        s.barTo   = prev.barTo + 8;
    }
    click->segments.push_back (s);

    rebuildRows();
    notify();
}

void ClickTrackPanel::rebuildRows()
{
    rows.clear();

    if (click != nullptr)
    {
        for (auto& seg : click->segments)
        {
            auto* row = new ClickSegmentRow();
            row->bind (&seg);
            row->onChanged = [this] { notify(); };
            row->onRemove  = [this, &seg]
            {
                if (click == nullptr)
                    return;
                auto& segs = click->segments;
                const auto idx = (int) (&seg - segs.data());
                if (juce::isPositiveAndBelow (idx, (int) segs.size()))
                {
                    segs.erase (segs.begin() + idx);
                    rebuildRows();
                    notify();
                }
            };
            rows.add (row);
            addAndMakeVisible (row);
        }
    }

    if (onHeightChanged)
        onHeightChanged();   // height changed -> let the host re-lay the track list
    resized();
}

int ClickTrackPanel::preferredHeight() const
{
    const int header = 28 + 4 + 26;                   // controls row + bank row
    const int body   = (int) rows.size() * 32 + 36;   // rows + add button
    return header + 6 + body + 6;
}

void ClickTrackPanel::paint (juce::Graphics& g)
{
    g.setColour (juce::Colours::cornflowerblue.withAlpha (0.10f));
    g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 5.0f);
    g.setColour (juce::Colours::cornflowerblue.withAlpha (0.45f));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 5.0f, 1.0f);
}

void ClickTrackPanel::resized()
{
    auto area = getLocalBounds().reduced (8, 6);

    auto header = area.removeFromTop (28);
    enableToggle.setBounds (header.removeFromLeft (80));
    header.removeFromLeft (10);
    chanCaption.setBounds (header.removeFromLeft (60));
    chanEditor .setBounds (header.removeFromLeft (90));
    header.removeFromLeft (16);
    gainCaption.setBounds (header.removeFromLeft (40));
    gainSlider .setBounds (header.removeFromLeft (180));

    area.removeFromTop (4);

    auto bankRow = area.removeFromTop (26);
    bankCaption.setBounds (bankRow.removeFromLeft (40));
    bankCombo  .setBounds (bankRow.removeFromLeft (220));

    area.removeFromTop (6);

    for (auto* row : rows)
    {
        row->setBounds (area.removeFromTop (30));
        area.removeFromTop (2);
    }

    addSegBtn.setBounds (area.removeFromTop (30).removeFromLeft (120));
}
