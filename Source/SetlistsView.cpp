#include "SetlistsView.h"

//==============================================================================
SetlistsView::SetlistsView (Library& lib, AudioEngine& eng)
    : library (lib), engine (eng)
{
    // Left: setlist list -------------------------------------------------------
    setlistModel.getCount = [this] { return (int) library.setlists.size(); };
    setlistModel.getText  = [this] (int row)
    {
        return juce::isPositiveAndBelow (row, (int) library.setlists.size())
                   ? library.setlists[(size_t) row].name : juce::String();
    };
    setlistModel.onSelect = [this] (int row) { selectSetlist (row); };

    addAndMakeVisible (setlistList);
    setlistList.setModel (&setlistModel);
    setlistList.setRowHeight (28);

    addAndMakeVisible (newSetlistBtn);
    newSetlistBtn.onClick = [this] { newSetlist(); };

    addAndMakeVisible (delSetlistBtn);
    delSetlistBtn.onClick = [this] { deleteSetlist(); };

    // Right: editor ------------------------------------------------------------
    addAndMakeVisible (editorTitle);
    editorTitle.setFont (juce::Font (juce::FontOptions (18.0f, juce::Font::bold)));

    addAndMakeVisible (nameCaption);
    nameCaption.setJustificationType (juce::Justification::centredRight);

    addAndMakeVisible (nameEditor);
    nameEditor.onTextChange = [this]
    {
        if (auto* s = currentSetlist())
        {
            s->name = nameEditor.getText();
            setlistList.repaintRow (selected);
            notifyChanged();
        }
    };

    // Songs currently in the setlist
    inListModel.getCount = [this]
    {
        auto* s = currentSetlist();
        return s != nullptr ? s->songIds.size() : 0;
    };
    inListModel.getText = [this] (int row) -> juce::String
    {
        auto* s = currentSetlist();
        if (s != nullptr && juce::isPositiveAndBelow (row, s->songIds.size()))
            return juce::String (row + 1) + ". " + songNameFor (s->songIds[row]);
        return {};
    };
    inListModel.onSelect = [this] (int row) { inListSel = row; };

    addAndMakeVisible (inListCaption);
    addAndMakeVisible (inListList);
    inListList.setModel (&inListModel);
    inListList.setRowHeight (28);

    addAndMakeVisible (upBtn);
    upBtn.onClick = [this] { moveSelectedSong (-1); };
    addAndMakeVisible (downBtn);
    downBtn.onClick = [this] { moveSelectedSong (1); };
    addAndMakeVisible (removeBtn);
    removeBtn.onClick = [this] { removeSelectedSong(); };

    // Library of songs to add from
    libModel.getCount = [this] { return (int) library.songs.size(); };
    libModel.getText  = [this] (int row)
    {
        return juce::isPositiveAndBelow (row, (int) library.songs.size())
                   ? library.songs[(size_t) row].name : juce::String();
    };
    libModel.onSelect      = [this] (int row) { libSel = row; };
    libModel.onDoubleClick = [this] (int row) { libSel = row; addSelectedSong(); };

    addAndMakeVisible (libCaption);
    addAndMakeVisible (libList);
    libList.setModel (&libModel);
    libList.setRowHeight (28);

    addAndMakeVisible (addBtn);
    addBtn.onClick = [this] { addSelectedSong(); };

    refresh();
}

//==============================================================================
Setlist* SetlistsView::currentSetlist()
{
    if (juce::isPositiveAndBelow (selected, (int) library.setlists.size()))
        return &library.setlists[(size_t) selected];
    return nullptr;
}

juce::String SetlistsView::songNameFor (const juce::String& id) const
{
    for (auto& s : library.songs)
        if (s.id == id)
            return s.name;
    return "(introuvable)"_t;
}

void SetlistsView::notifyChanged()
{
    library.save();
    if (onLibraryChanged)
        onLibraryChanged();
}

//==============================================================================
void SetlistsView::newSetlist()
{
    Setlist s;
    s.name = "Setlist " + juce::String ((int) library.setlists.size() + 1);
    library.setlists.push_back (s);
    notifyChanged();
    setlistList.updateContent();
    setlistList.selectRow ((int) library.setlists.size() - 1);
}

void SetlistsView::deleteSetlist()
{
    if (currentSetlist() != nullptr)
    {
        library.setlists.erase (library.setlists.begin() + selected);
        selected = -1;
        notifyChanged();
        setlistList.updateContent();
        refresh();
    }
}

void SetlistsView::selectSetlist (int index)
{
    selected  = index;
    inListSel = -1;

    if (auto* s = currentSetlist())
    {
        nameEditor.setText (s->name, juce::dontSendNotification);
        editorTitle.setText ("Setlist: " + s->name, juce::dontSendNotification);
    }
    else
    {
        nameEditor.clear();
        editorTitle.setText ("Aucune setlist sélectionnée"_t, juce::dontSendNotification);
    }

    const bool has = currentSetlist() != nullptr;
    nameEditor.setEnabled (has);
    upBtn     .setEnabled (has);
    downBtn   .setEnabled (has);
    removeBtn .setEnabled (has);
    addBtn    .setEnabled (has);

    inListList.updateContent();
    inListList.repaint();
}

//==============================================================================
void SetlistsView::addSelectedSong()
{
    auto* s = currentSetlist();
    if (s != nullptr && juce::isPositiveAndBelow (libSel, (int) library.songs.size()))
    {
        s->songIds.add (library.songs[(size_t) libSel].id);
        notifyChanged();
        inListList.updateContent();
        inListList.selectRow (s->songIds.size() - 1);
    }
}

void SetlistsView::removeSelectedSong()
{
    auto* s = currentSetlist();
    if (s != nullptr && juce::isPositiveAndBelow (inListSel, s->songIds.size()))
    {
        s->songIds.remove (inListSel);
        if (inListSel >= s->songIds.size())
            inListSel = s->songIds.size() - 1;
        notifyChanged();
        inListList.updateContent();
        inListList.selectRow (inListSel);
    }
}

void SetlistsView::moveSelectedSong (int delta)
{
    auto* s = currentSetlist();
    if (s == nullptr)
        return;

    const int to = inListSel + delta;
    if (juce::isPositiveAndBelow (inListSel, s->songIds.size())
        && juce::isPositiveAndBelow (to, s->songIds.size()))
    {
        s->songIds.move (inListSel, to);
        inListSel = to;
        notifyChanged();
        inListList.updateContent();
        inListList.selectRow (inListSel);
    }
}

//==============================================================================
void SetlistsView::refresh()
{
    setlistList.updateContent();
    setlistList.repaint();
    libList.updateContent();
    libList.repaint();
    selectSetlist (selected);
}

void SetlistsView::resized()
{
    auto area = getLocalBounds().reduced (10);

    // Left: setlist list + buttons
    auto left = area.removeFromLeft (220);
    auto leftButtons = left.removeFromBottom (36);
    newSetlistBtn.setBounds (leftButtons.removeFromLeft (130));
    leftButtons.removeFromLeft (6);
    delSetlistBtn.setBounds (leftButtons);
    left.removeFromBottom (6);
    setlistList.setBounds (left);

    area.removeFromLeft (12);

    // Right: editor
    auto right = area;
    editorTitle.setBounds (right.removeFromTop (28));
    right.removeFromTop (8);

    auto nameRow = right.removeFromTop (28);
    nameCaption.setBounds (nameRow.removeFromLeft (50));
    nameEditor.setBounds (nameRow.removeFromLeft (300));
    right.removeFromTop (12);

    // Two columns: songs-in-setlist (with reorder buttons) | library (with add)
    auto captionRow = right.removeFromTop (24);
    const int colW = (right.getWidth() - 12) / 2;
    inListCaption.setBounds (captionRow.removeFromLeft (colW));
    captionRow.removeFromLeft (12);
    libCaption.setBounds (captionRow);

    auto inCol  = right.removeFromLeft (colW);
    right.removeFromLeft (12);
    auto libCol = right;

    // In-setlist column: list on top, reorder/remove buttons at bottom
    auto inButtons = inCol.removeFromBottom (36);
    upBtn.setBounds      (inButtons.removeFromLeft (90));
    inButtons.removeFromLeft (6);
    downBtn.setBounds    (inButtons.removeFromLeft (90));
    inButtons.removeFromLeft (6);
    removeBtn.setBounds  (inButtons);
    inCol.removeFromBottom (6);
    inListList.setBounds (inCol);

    // Library column: list on top, add button at bottom
    auto libButtons = libCol.removeFromBottom (36);
    addBtn.setBounds (libButtons.removeFromLeft (120));
    libCol.removeFromBottom (6);
    libList.setBounds (libCol);
}
