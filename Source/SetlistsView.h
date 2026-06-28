#pragma once

#include <JuceHeader.h>
#include "Models.h"
#include "AudioEngine.h"
#include "Text.h"

//==============================================================================
// Generic ListBoxModel driven by lambdas, so one view can host several lists
// without a dedicated model class for each.
class SimpleListModel  : public juce::ListBoxModel
{
public:
    std::function<int()>                          getCount;
    std::function<juce::String (int)>             getText;
    std::function<void (int)>                     onSelect;
    std::function<void (int)>                     onDoubleClick;
    bool                                          draggable = false;

    int getNumRows() override
    {
        return getCount ? getCount() : 0;
    }

    juce::var getDragSourceDescription (const juce::SparseSet<int>& rows) override
    {
        if (draggable && rows.size() > 0)
            return rows[0];
        return {};
    }

    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
    {
        if (selected)
        {
            g.setColour (juce::Colours::cornflowerblue.withAlpha (0.4f));
            g.fillRect (0, 0, w, h);
        }

        g.setColour (juce::Colours::white);
        if (getText)
            g.drawText (getText (row), 8, 0, w - 12, h, juce::Justification::centredLeft);
    }

    void selectedRowsChanged (int lastRow) override
    {
        if (onSelect)
            onSelect (lastRow);
    }

    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override
    {
        if (onDoubleClick)
            onDoubleClick (row);
    }
};

//==============================================================================
// A ListBox whose rows can be reordered by drag'n'drop. Reports the move via
// onReorder (from row index -> insertion index).
class ReorderableListBox  : public juce::ListBox,
                            public juce::DragAndDropTarget
{
public:
    std::function<void (int from, int to)> onReorder;

    bool isInterestedInDragSource (const SourceDetails& d) override
    {
        return d.sourceComponent != nullptr
               && (d.sourceComponent.get() == this || isParentOf (d.sourceComponent.get()));
    }

    void itemDropped (const SourceDetails& d) override
    {
        const int from = (int) d.description;
        const int to   = getInsertionIndexForPosition (d.localPosition.x, d.localPosition.y);
        if (onReorder)
            onReorder (from, to);
    }
};

//==============================================================================
// Admin view for setlists: left = list of setlists, right = editor for the
// selected setlist (name, ordered songs with reorder/remove, and the library
// of songs to add from).
class SetlistsView  : public juce::Component,
                      public juce::DragAndDropContainer
{
public:
    SetlistsView (Library& library, AudioEngine& engine);

    void refresh();
    void resized() override;

    std::function<void()> onLibraryChanged;

private:
    void newSetlist();
    void deleteSetlist();
    void selectSetlist (int index);
    void addSelectedSong();
    void removeSelectedSong();
    void moveSelectedSong (int delta);
    void notifyChanged();                        // mark dirty + refresh others (no disk write)
    void setDirty (bool);
    void save();                                 // write library to disk, clear dirty

    Setlist* currentSetlist();
    juce::String songNameFor (const juce::String& id) const;

    Library&     library;
    AudioEngine& engine;

    // Left: setlists
    juce::ListBox    setlistList;
    SimpleListModel  setlistModel;
    juce::TextButton newSetlistBtn { "Nouvelle setlist"_t };
    juce::TextButton delSetlistBtn { "Supprimer" };

    // Right: editor
    juce::Label      editorTitle { {}, "Setlist" };
    juce::Label      nameCaption { {}, "Nom:" };
    juce::TextEditor nameEditor;

    juce::Label         inListCaption  { {}, "Morceaux de la setlist"_t };
    ReorderableListBox  inListList;
    SimpleListModel     inListModel;
    juce::TextButton upBtn     { "Monter"_t };
    juce::TextButton downBtn   { "Descendre" };
    juce::TextButton removeBtn { "Retirer" };

    juce::Label      libCaption { {}, "Bibliothèque"_t };
    juce::ListBox    libList;
    SimpleListModel  libModel;
    juce::TextButton addBtn { "Ajouter ->"_t };

    juce::TextButton saveBtn { "Sauvegarder" };

    int  selected   = -1;  // selected setlist index
    int  inListSel  = -1;  // selected row within the setlist's songs
    int  libSel     = -1;  // selected row within the library
    bool dirty      = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SetlistsView)
};
