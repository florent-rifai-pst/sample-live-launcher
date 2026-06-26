#include "SongsView.h"
#include "Text.h"

//==============================================================================
SongsView::SongsView (Library& lib, AudioEngine& eng)
    : library (lib), engine (eng)
{
    addAndMakeVisible (songList);
    songList.setModel (this);
    songList.setRowHeight (28);

    addAndMakeVisible (newSongBtn);
    newSongBtn.onClick = [this] { newSong(); };

    addAndMakeVisible (delSongBtn);
    delSongBtn.onClick = [this] { deleteSong(); };

    addAndMakeVisible (editorTitle);
    editorTitle.setFont (juce::Font (juce::FontOptions (18.0f, juce::Font::bold)));

    addAndMakeVisible (nameCaption);
    nameCaption.setJustificationType (juce::Justification::centredRight);

    addAndMakeVisible (nameEditor);
    nameEditor.onTextChange = [this]
    {
        if (auto* s = currentSong())
        {
            s->name = nameEditor.getText();
            songList.repaintRow (selected);
            notifyChanged();
        }
    };

    addAndMakeVisible (addFilesBtn);
    addFilesBtn.onClick = [this] { addFiles(); };

    addAndMakeVisible (playBtn);
    playBtn.onClick = [this]
    {
        if (auto* s = currentSong())
        {
            engine.loadSong (*s);
            engine.playAll();
        }
    };

    addAndMakeVisible (stopBtn);
    stopBtn.onClick = [this] { engine.stopAll(); };

    addAndMakeVisible (tracksViewport);
    tracksViewport.setViewedComponent (&tracksHolder, false);
    tracksViewport.setScrollBarsShown (true, false);

    refresh();
}

//==============================================================================
Song* SongsView::currentSong()
{
    if (juce::isPositiveAndBelow (selected, (int) library.songs.size()))
        return &library.songs[(size_t) selected];
    return nullptr;
}

void SongsView::notifyChanged()
{
    library.save();
    if (onLibraryChanged)
        onLibraryChanged();
}

//==============================================================================
int SongsView::getNumRows()
{
    return (int) library.songs.size();
}

void SongsView::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool rowSelected)
{
    if (! juce::isPositiveAndBelow (row, (int) library.songs.size()))
        return;

    if (rowSelected)
    {
        g.setColour (juce::Colours::cornflowerblue.withAlpha (0.4f));
        g.fillRect (0, 0, w, h);
    }

    g.setColour (juce::Colours::white);
    g.drawText (library.songs[(size_t) row].name, 8, 0, w - 12, h,
                juce::Justification::centredLeft);
}

void SongsView::selectedRowsChanged (int lastRow)
{
    selectSong (lastRow);
}

//==============================================================================
void SongsView::newSong()
{
    Song s;
    s.name = "Morceau " + juce::String ((int) library.songs.size() + 1);
    library.songs.push_back (s);
    notifyChanged();
    songList.updateContent();
    songList.selectRow ((int) library.songs.size() - 1);
}

void SongsView::deleteSong()
{
    if (auto* s = currentSong())
    {
        library.songs.erase (library.songs.begin() + selected);
        selected = -1;
        notifyChanged();
        songList.updateContent();
        refresh();
    }
}

void SongsView::selectSong (int index)
{
    selected = index;

    if (auto* s = currentSong())
    {
        nameEditor.setText (s->name, juce::dontSendNotification);
        editorTitle.setText ("Morceau: " + s->name, juce::dontSendNotification);
    }
    else
    {
        nameEditor.clear();
        editorTitle.setText ("Aucun morceau sélectionné"_t, juce::dontSendNotification);
    }

    const bool has = currentSong() != nullptr;
    nameEditor.setEnabled (has);
    addFilesBtn.setEnabled (has);
    playBtn.setEnabled (has);
    stopBtn.setEnabled (has);

    rebuildTrackRows();
}

//==============================================================================
void SongsView::addFiles()
{
    chooser = std::make_unique<juce::FileChooser> ("Ajouter des fichiers audio",
                                                   juce::File{},
                                                   "*.wav;*.mp3;*.aiff;*.flac");

    auto openFlags = juce::FileBrowserComponent::openMode
                   | juce::FileBrowserComponent::canSelectFiles
                   | juce::FileBrowserComponent::canSelectMultipleItems;

    chooser->launchAsync (openFlags, [this] (const juce::FileChooser& fc)
    {
        if (auto* s = currentSong())
        {
            for (auto& file : fc.getResults())
            {
                TrackData td;
                td.name     = file.getFileName();
                td.filePath = file.getFullPathName();
                td.channels = { 0, 1 };
                td.gain     = 1.0f;
                s->tracks.push_back (td);
            }
            notifyChanged();
            rebuildTrackRows();
        }
    });
}

void SongsView::rebuildTrackRows()
{
    rows.clear();

    if (auto* s = currentSong())
    {
        for (auto& td : s->tracks)
        {
            auto* row = new TrackRow (td.name, td.channels, td.gain);

            row->onRemove = [this] (TrackRow* r)
            {
                const int idx = rows.indexOf (r);
                if (auto* song = currentSong(); song != nullptr && idx >= 0)
                {
                    song->tracks.erase (song->tracks.begin() + idx);
                    notifyChanged();
                    rebuildTrackRows();
                }
            };
            row->onChannelsChanged = [this] (TrackRow* r, std::vector<int> chans)
            {
                const int idx = rows.indexOf (r);
                if (auto* song = currentSong(); song != nullptr && idx >= 0)
                {
                    song->tracks[(size_t) idx].channels = std::move (chans);
                    notifyChanged();
                }
            };
            row->onGainChanged = [this] (TrackRow* r, float gain)
            {
                const int idx = rows.indexOf (r);
                if (auto* song = currentSong(); song != nullptr && idx >= 0)
                {
                    song->tracks[(size_t) idx].gain = gain;
                    notifyChanged();
                }
            };

            rows.add (row);
            tracksHolder.addAndMakeVisible (row);
        }
    }

    resized();
}

//==============================================================================
void SongsView::refresh()
{
    songList.updateContent();
    songList.repaint();
    selectSong (selected);
}

void SongsView::resized()
{
    auto area = getLocalBounds().reduced (10);

    // Left: song list + buttons
    auto left = area.removeFromLeft (220);
    auto leftButtons = left.removeFromBottom (36);
    newSongBtn.setBounds (leftButtons.removeFromLeft (130));
    leftButtons.removeFromLeft (6);
    delSongBtn.setBounds (leftButtons);
    left.removeFromBottom (6);
    songList.setBounds (left);

    area.removeFromLeft (12);

    // Right: editor
    auto right = area;
    editorTitle.setBounds (right.removeFromTop (28));
    right.removeFromTop (8);

    auto nameRow = right.removeFromTop (28);
    nameCaption.setBounds (nameRow.removeFromLeft (50));
    nameEditor.setBounds (nameRow.removeFromLeft (300));
    right.removeFromTop (8);

    auto btnRow = right.removeFromTop (36);
    addFilesBtn.setBounds (btnRow.removeFromLeft (200));
    btnRow.removeFromLeft (10);
    playBtn.setBounds (btnRow.removeFromLeft (100));
    btnRow.removeFromLeft (6);
    stopBtn.setBounds (btnRow.removeFromLeft (100));
    right.removeFromTop (8);

    tracksViewport.setBounds (right);

    const int rowH = 44;
    tracksHolder.setSize (tracksViewport.getMaximumVisibleWidth(),
                          juce::jmax (right.getHeight(), (int) rows.size() * rowH));
    int y = 0;
    for (auto* r : rows)
    {
        r->setBounds (0, y, tracksHolder.getWidth(), rowH - 4);
        y += rowH;
    }
}
