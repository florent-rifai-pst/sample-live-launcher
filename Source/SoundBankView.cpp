#include "SoundBankView.h"
#include "Text.h"

//==============================================================================
SoundBankView::SoundBankView (Library& lib) : library (lib)
{
    addAndMakeVisible (title);
    title.setFont (juce::Font (juce::FontOptions (16.0f, juce::Font::bold)));

    addAndMakeVisible (list);
    list.setModel (this);
    list.setRowHeight (26);

    addAndMakeVisible (addBtn);
    addBtn.onClick = [this] { addBank(); };

    addAndMakeVisible (delBtn);
    delBtn.onClick = [this] { deleteBank(); };

    addAndMakeVisible (nameCaption);
    nameCaption.setJustificationType (juce::Justification::centredRight);

    addAndMakeVisible (nameEditor);
    nameEditor.onTextChange = [this]
    {
        if (auto* b = current())
        {
            b->name = nameEditor.getText();
            list.repaintRow (selected);
            if (onChange) onChange();
        }
    };

    auto setupFileRow = [this] (juce::Label& cap, juce::Label& file, juce::TextButton& btn, bool accent)
    {
        addAndMakeVisible (cap);
        cap.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (file);
        file.setColour (juce::Label::backgroundColourId, juce::Colours::black.withAlpha (0.25f));
        file.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.85f));
        addAndMakeVisible (btn);
        btn.onClick = [this, accent] { chooseFile (accent); };
    };
    setupFileRow (accentCaption, accentFile, accentBtn, true);
    setupFileRow (normalCaption, normalFile, normalBtn, false);

    selectBank (-1);
}

//==============================================================================
ClickSoundBank* SoundBankView::current()
{
    if (juce::isPositiveAndBelow (selected, (int) library.clickBanks.size()))
        return &library.clickBanks[(size_t) selected];
    return nullptr;
}

void SoundBankView::refresh()
{
    list.updateContent();
    list.repaint();
    selectBank (selected);
}

//==============================================================================
int SoundBankView::getNumRows()
{
    return (int) library.clickBanks.size();
}

void SoundBankView::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool rowSelected)
{
    if (! juce::isPositiveAndBelow (row, (int) library.clickBanks.size()))
        return;

    if (rowSelected)
    {
        g.setColour (juce::Colours::cornflowerblue.withAlpha (0.4f));
        g.fillRect (0, 0, w, h);
    }

    g.setColour (juce::Colours::white);
    g.drawText (library.clickBanks[(size_t) row].name, 8, 0, w - 12, h,
                juce::Justification::centredLeft);
}

void SoundBankView::selectedRowsChanged (int lastRow)
{
    selectBank (lastRow);
}

//==============================================================================
void SoundBankView::addBank()
{
    ClickSoundBank b;
    b.name = "Banque " + juce::String ((int) library.clickBanks.size() + 1);
    library.clickBanks.push_back (b);

    if (onChange) onChange();
    list.updateContent();
    list.selectRow ((int) library.clickBanks.size() - 1);
}

void SoundBankView::deleteBank()
{
    if (current() != nullptr)
    {
        library.clickBanks.erase (library.clickBanks.begin() + selected);
        selected = -1;
        if (onChange) onChange();
        list.updateContent();
        selectBank (-1);
    }
}

void SoundBankView::selectBank (int index)
{
    selected = index;

    if (auto* b = current())
    {
        nameEditor.setText (b->name, juce::dontSendNotification);
    }
    else
    {
        nameEditor.clear();
    }

    const bool has = current() != nullptr;
    nameEditor.setEnabled (has);
    accentBtn.setEnabled (has);
    normalBtn.setEnabled (has);

    updateFileLabels();
}

void SoundBankView::updateFileLabels()
{
    auto* b = current();
    auto shortName = [] (const juce::String& path)
    {
        return path.isEmpty() ? juce::String ("(aucun)") : juce::File (path).getFileName();
    };
    accentFile.setText (b ? shortName (b->accentFile) : juce::String(), juce::dontSendNotification);
    normalFile.setText (b ? shortName (b->normalFile) : juce::String(), juce::dontSendNotification);
}

void SoundBankView::chooseFile (bool accent)
{
    if (current() == nullptr)
        return;

    chooser = std::make_unique<juce::FileChooser> ("Choisir un son de click"_t,
                                                   juce::File{},
                                                   "*.wav;*.mp3;*.aiff;*.flac");

    const auto flags = juce::FileBrowserComponent::openMode
                     | juce::FileBrowserComponent::canSelectFiles;

    chooser->launchAsync (flags, [this, accent] (const juce::FileChooser& fc)
    {
        auto result = fc.getResult();
        if (result == juce::File{})
            return;

        if (auto* b = current())
        {
            (accent ? b->accentFile : b->normalFile) = result.getFullPathName();
            updateFileLabels();
            if (onChange) onChange();
        }
    });
}

//==============================================================================
void SoundBankView::resized()
{
    auto area = getLocalBounds().reduced (10);

    title.setBounds (area.removeFromTop (24));
    area.removeFromTop (8);

    auto left = area.removeFromLeft (220);
    auto leftButtons = left.removeFromBottom (32);
    addBtn.setBounds (leftButtons.removeFromLeft (105));
    leftButtons.removeFromLeft (6);
    delBtn.setBounds (leftButtons);
    left.removeFromBottom (6);
    list.setBounds (left);

    area.removeFromLeft (12);
    auto right = area;

    auto row = [&right] (int h) { auto r = right.removeFromTop (h); right.removeFromTop (8); return r; };

    auto nameRow = row (26);
    nameCaption.setBounds (nameRow.removeFromLeft (90));
    nameEditor.setBounds (nameRow.removeFromLeft (260));

    auto accentRow = row (26);
    accentCaption.setBounds (accentRow.removeFromLeft (90));
    accentBtn.setBounds (accentRow.removeFromLeft (100));
    accentRow.removeFromLeft (8);
    accentFile.setBounds (accentRow.removeFromLeft (260));

    auto normalRow = row (26);
    normalCaption.setBounds (normalRow.removeFromLeft (90));
    normalBtn.setBounds (normalRow.removeFromLeft (100));
    normalRow.removeFromLeft (8);
    normalFile.setBounds (normalRow.removeFromLeft (260));
}
