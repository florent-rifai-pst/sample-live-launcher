#pragma once

#include <JuceHeader.h>
#include "Models.h"
#include "Text.h"

//==============================================================================
// One editable brick of the click track: "Mesures A à B", time signature
// (numerator / denominator) and tempo. Edits write straight into the bound
// ClickSegment and notify via onChanged / onRemove.
class ClickSegmentRow  : public juce::Component
{
public:
    ClickSegmentRow();

    void bind (ClickSegment* seg);   // fill widgets from the segment
    void resized() override;
    void paint (juce::Graphics&) override;

    std::function<void()> onChanged;   // a field was edited (segment already updated)
    std::function<void()> onRemove;

private:
    void commit();                     // read widgets back into *segment

    ClickSegment* segment = nullptr;

    juce::Label      fromCaption { {}, "Mesures"_t };
    juce::TextEditor fromEd;
    juce::Label      toCaption   { {}, "\xc3\xa0"_t };   // "à"
    juce::TextEditor toEd;
    juce::Label      sigCaption  { {}, "Signature" };
    juce::ComboBox   numCombo;
    juce::Label      slashCaption { {}, "/" };
    juce::ComboBox   denCombo;
    juce::TextEditor bpmEd;
    juce::Label      bpmCaption  { {}, "BPM" };
    juce::TextButton removeButton { "X" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClickSegmentRow)
};

//==============================================================================
// The click track, always shown in first position of a song. Header has the
// enable toggle, output channels and gain; below it the list of tempo/signature
// bricks plus a button to add more. Bound to a song's ClickTrack; every edit
// fires onChange so the host can mark the library dirty.
class ClickTrackPanel  : public juce::Component
{
public:
    ClickTrackPanel();

    void setClickTrack (ClickTrack* click);   // nullptr = no song selected
    void setSoundBanks (const std::vector<ClickSoundBank>& banks);   // fill the bank picker
    void setExpanded (bool e);                // open/close the collapsible part
    int  preferredHeight() const;

    void resized() override;
    void paint (juce::Graphics&) override;

    std::function<void()> onChange;
    std::function<void()> onHeightChanged;   // rows added/removed -> host re-lays out

private:
    void rebuildRows();
    void addSegment();
    void notify();                            // segment/header edited
    void refreshBankSelection();              // sync combo to click->soundBankId

    void applyExpandedState();                // show/hide the collapsible part

    ClickTrack* click = nullptr;
    juce::StringArray bankIds;                // combo item id (index+2) -> bank id
    bool expanded = false;                    // collapsed by default

    juce::TextButton   disclosure;            // ▸ / ▾ collapse toggle
    juce::ToggleButton enableToggle { "Click" };
    juce::Label        chanCaption  { {}, "Sorties:" };
    juce::TextEditor   chanEditor;
    juce::Label        gainCaption  { {}, "Vol." };
    juce::Slider       gainSlider;
    juce::Label        bankCaption  { {}, "Son:" };
    juce::ComboBox     bankCombo;

    juce::OwnedArray<ClickSegmentRow> rows;
    juce::TextButton   addSegBtn { "+ Section"_t };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClickTrackPanel)
};
