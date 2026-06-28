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
    // Re-sort once editing is done (not per keystroke, so the row doesn't jump while typing).
    nameEditor.onReturnKey  = [this] { nameEditor.giveAwayKeyboardFocus(); };
    nameEditor.onFocusLost  = [this] { refresh(); };

    addAndMakeVisible (addFilesBtn);
    addFilesBtn.setTooltip ("Ajouter fichier(s)"_t);
    addFilesBtn.onClick = [this] { addFiles(); };

    addAndMakeVisible (clickToggleBtn);
    clickToggleBtn.setTooltip ("Afficher / masquer la piste click"_t);
    clickToggleBtn.onClick = [this]
    {
        const bool show = ! clickPanel.isVisible();
        clickPanel.setVisible (show);
        if (show)
            clickPanel.setExpanded (true);   // reveal sections immediately
        clickToggleBtn.highlighted = show;
        clickToggleBtn.repaint();
        resized();
    };

    addAndMakeVisible (playPauseBtn);
    playPauseBtn.onClick = [this]
    {
        auto* s = currentSong();
        if (s == nullptr)
            return;

        if (engine.isPlaying())
        {
            engine.pauseAll();
        }
        else
        {
            // Reload when starting fresh (different song, nothing loaded, or at the
            // very start) so edits are reflected; otherwise resume from current pos.
            const bool fresh = engine.getNumTracks() == 0
                            || loadedSongId != s->id
                            || engine.getCurrentPositionSeconds() <= 0.0;
            if (fresh)
            {
                auto cf = clickBankFiles (*s);
                engine.loadSong (*s, cf.first, cf.second);
                loadedSongId = s->id;
                engine.setPositionSeconds (positionSlider.getValue());
            }
            engine.playAll();
        }
        updatePlayIcon();
    };

    addAndMakeVisible (saveBtn);
    saveBtn.setTooltip ("Sauvegarder"_t);
    saveBtn.onClick = [this] { save(); };
    saveBtn.setEnabled (false);

    addAndMakeVisible (revertBtn);
    revertBtn.onClick = [this] { revert(); };
    revertBtn.setTooltip ("Rétablir"_t);
    revertBtn.setEnabled (false);

    addAndMakeVisible (seqBtn);
    seqBtn.setTooltip ("Ouvrir le s\xc3\xa9quenceur"_t);
    seqBtn.onClick = [this] { openSequencer(); };
    seqBtn.setEnabled (false);

    addChildComponent (sequencer);               // hidden until openSequencer()
    sequencer.onClose  = [this] { closeSequencer(); };
    sequencer.onChange = [this] { notifyChanged(); };

    addAndMakeVisible (positionSlider);
    positionSlider.setRange (0.0, 1.0, 0.01);
    positionSlider.setEnabled (false);
    positionSlider.onValueChange = [this]
    {
        if (engine.getNumTracks() > 0)   // seek live, whether playing or paused
            engine.setPositionSeconds (positionSlider.getValue());
        updateTimeLabel();
    };

    addAndMakeVisible (timeLabel);
    timeLabel.setJustificationType (juce::Justification::centredRight);

    addAndMakeVisible (tracksViewport);
    tracksViewport.setViewedComponent (&tracksHolder, false);
    tracksViewport.setScrollBarsShown (true, false);

    tracksHolder.addChildComponent (clickPanel);   // hidden until the metronome button
    clickPanel.onChange = [this]
    {
        if (auto* s = currentSong())
        {
            // refresh the play bar range (click length may have changed) and push
            // live edits to the engine when this song is loaded but stopped
            const double dur = songDurationSeconds (*s);
            positionSlider.setRange (0.0, juce::jmax (1.0, dur), 0.01);
            if (loadedSongId == s->id && ! engine.isPlaying())
            {
                auto cf = clickBankFiles (*s);
                engine.setClick (s->click, cf.first, cf.second);
            }
            updateTimeLabel();
            notifyChanged();
        }
    };
    clickPanel.onHeightChanged = [this] { resized(); };

    startTimerHz (20);
    refresh();
}

//==============================================================================
Song* SongsView::currentSong()
{
    if (juce::isPositiveAndBelow (selected, (int) library.songs.size()))
        return &library.songs[(size_t) selected];
    return nullptr;
}

std::pair<juce::File, juce::File> SongsView::clickBankFiles (const Song& s)
{
    if (auto* b = library.findClickBank (s.click.soundBankId))
        return { juce::File (b->accentFile), juce::File (b->normalFile) };
    return {};   // default synthesized beep
}

void SongsView::notifyChanged()
{
    setDirty (true);
    if (onLibraryChanged)
        onLibraryChanged();
}

void SongsView::setDirty (bool d)
{
    dirty = d;
    saveBtn.setEnabled (d);
    revertBtn.setEnabled (d);
}

void SongsView::save()
{
    library.save();
    setDirty (false);
}

void SongsView::revert()
{
    if (auto* s = currentSong())
    {
        *s = snapshot;
        if (onLibraryChanged)
            onLibraryChanged();
        setDirty (false);
        selectSong (selected);   // reload editor fields + track rows from restored song
    }
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
    selected = (int) library.songs.size() - 1;   // point at the new song...
    notifyChanged();
    refresh();                                   // ...refresh sorts + keeps it selected by id
}

void SongsView::deleteSong()
{
    auto* s = currentSong();
    if (s == nullptr)
        return;

    const juce::String id   = s->id;
    const juce::String name = s->name;

    auto opts = juce::MessageBoxOptions()
                    .withIconType (juce::MessageBoxIconType::WarningIcon)
                    .withTitle ("Supprimer le morceau"_t)
                    .withMessage ("Supprimer \xc2\xab "_t + name + " \xc2\xbb ?\nCette action est irr\xc3\xa9versible."_t)
                    .withButton ("Supprimer"_t)
                    .withButton ("Annuler"_t)
                    .withAssociatedComponent (this);

    juce::AlertWindow::showAsync (opts, [this, id] (int result)
    {
        if (result != 1)                       // 1 = first button (Supprimer)
            return;

        for (int i = 0; i < (int) library.songs.size(); ++i)
            if (library.songs[(size_t) i].id == id)
            {
                library.songs.erase (library.songs.begin() + i);
                break;
            }

        selected = -1;
        if (onLibraryChanged)
            onLibraryChanged();          // refresh the other tabs
        save();                          // persist immediately: a confirmed delete is final
        songList.updateContent();
        refresh();
    });
}

void SongsView::selectSong (int index)
{
    selected = index;

    if (auto* s = currentSong())
    {
        snapshot = *s;   // remember display-time state for Rétablir
        nameEditor.setText (s->name, juce::dontSendNotification);
        editorTitle.setText ("Morceau: " + s->name, juce::dontSendNotification);

        const double dur = songDurationSeconds (*s);
        positionSlider.setRange (0.0, juce::jmax (1.0, dur), 0.01);
        positionSlider.setValue (0.0, juce::dontSendNotification);
    }
    else
    {
        nameEditor.clear();
        editorTitle.setText ("Aucun morceau sélectionné"_t, juce::dontSendNotification);
    }

    const bool has = currentSong() != nullptr;
    nameEditor.setEnabled (has);
    addFilesBtn.setEnabled (has);
    playPauseBtn.setEnabled (has);
    seqBtn.setEnabled (has);
    positionSlider.setEnabled (has);
    updateTimeLabel();
    updatePlayIcon();

    clickPanel.setClickTrack (currentSong() != nullptr ? &currentSong()->click : nullptr);
    clickPanel.setSoundBanks (library.clickBanks);
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
                td.name            = file.getFileName();
                td.filePath        = file.getFullPathName();
                td.channels        = { 0, 1 };
                td.gain            = 1.0f;
                td.durationSeconds = fileDurationSeconds (td.filePath);
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
        int trackIndex = 0;
        for (auto& td : s->tracks)
        {
            auto* row = new TrackRow (td.name, td.channels, td.channelGains, td.gain, td.mute, td.solo);
            row->setIndex (trackIndex++);
            row->setNumOutputs (engine.getNumOutputChannels());
            row->onLayoutChanged = [this] { resized(); };

            row->onRemove = [this] (TrackRow* r)
            {
                const int idx = rows.indexOf (r);
                if (auto* song = currentSong(); song != nullptr && idx >= 0)
                {
                    // Defer: rebuildTrackRows() deletes the row (and the button firing
                    // this callback) -> use-after-free if done inline.
                    const juce::String songId = song->id;
                    juce::MessageManager::callAsync ([this, songId, idx]
                    {
                        if (auto* s = currentSong(); s != nullptr && s->id == songId
                                && juce::isPositiveAndBelow (idx, (int) s->tracks.size()))
                        {
                            s->tracks.erase (s->tracks.begin() + idx);
                            notifyChanged();
                            rebuildTrackRows();
                        }
                    });
                }
            };
            row->onChannelsChanged = [this] (TrackRow* r, std::vector<int> chans)
            {
                const int idx = rows.indexOf (r);
                if (auto* song = currentSong(); song != nullptr && idx >= 0)
                {
                    song->tracks[(size_t) idx].channels = chans;
                    if (loadedSongId == song->id)            // apply live (even while playing)
                        engine.setTrackChannels (idx, std::move (chans));
                    notifyChanged();
                }
            };
            row->onChannelGainsChanged = [this] (TrackRow* r, std::vector<float> gains)
            {
                const int idx = rows.indexOf (r);
                if (auto* song = currentSong(); song != nullptr && idx >= 0)
                {
                    song->tracks[(size_t) idx].channelGains = gains;
                    if (loadedSongId == song->id)            // apply live (even while playing)
                        engine.setTrackChannelGains (idx, std::move (gains));
                    notifyChanged();
                }
            };
            row->onGainChanged = [this] (TrackRow* r, float gain)
            {
                const int idx = rows.indexOf (r);
                if (auto* song = currentSong(); song != nullptr && idx >= 0)
                {
                    song->tracks[(size_t) idx].gain = gain;
                    if (loadedSongId == song->id)            // apply live (even while playing)
                        engine.setTrackGain (idx, gain);
                    notifyChanged();
                }
            };
            row->onMuteChanged = [this] (TrackRow* r, bool mute)
            {
                const int idx = rows.indexOf (r);
                if (auto* song = currentSong(); song != nullptr && idx >= 0)
                {
                    song->tracks[(size_t) idx].mute = mute;
                    if (loadedSongId == song->id)            // apply live (even while playing)
                        engine.setTrackMute (idx, mute);
                    notifyChanged();
                }
            };
            row->onSoloChanged = [this] (TrackRow* r, bool solo)
            {
                const int idx = rows.indexOf (r);
                if (auto* song = currentSong(); song != nullptr && idx >= 0)
                {
                    song->tracks[(size_t) idx].solo = solo;
                    if (loadedSongId == song->id)            // apply live (even while playing)
                        engine.setTrackSolo (idx, solo);
                    notifyChanged();
                }
            };
            row->onReorder = [this] (int from, int to)
            {
                if (auto* song = currentSong())
                {
                    auto& t = song->tracks;
                    if (! juce::isPositiveAndBelow (from, (int) t.size()))
                        return;

                    auto item = t[(size_t) from];
                    t.erase (t.begin() + from);
                    const int ins = juce::jlimit (0, (int) t.size(), from < to ? to - 1 : to);
                    t.insert (t.begin() + ins, item);
                    notifyChanged();
                    rebuildTrackRows();
                }
            };

            rows.add (row);
            tracksHolder.addAndMakeVisible (row);
        }
    }

    resized();
}

//==============================================================================
void SongsView::openSequencer()
{
    auto* s = currentSong();
    if (s == nullptr)
        return;

    // Make sure the song is the one loaded in the engine (reflects latest edits),
    // then hand it to the sequencer. Reuse the existing position if already loaded.
    auto cf = clickBankFiles (*s);
    if (loadedSongId != s->id || engine.getNumTracks() == 0)
    {
        engine.loadSong (*s, cf.first, cf.second);
        loadedSongId = s->id;
    }

    sequencer.setSong (s, cf.first, cf.second);
    sequencer.setVisible (true);
    sequencer.toFront (false);
    resized();
}

void SongsView::closeSequencer()
{
    engine.pauseAll();
    engine.setLoop (false);                      // don't leave the play bar looping
    sequencer.setSong (nullptr, {}, {});
    sequencer.setVisible (false);
    updatePlayIcon();
    refresh();                                   // editor may reflect click edits
}

//==============================================================================
void SongsView::timerCallback()
{
    if (engine.isPlaying() && ! positionSlider.isMouseButtonDown())
    {
        const double len = engine.getLengthSeconds();
        if (len > 0.0 && std::abs (positionSlider.getMaximum() - len) > 0.05)
            positionSlider.setRange (0.0, len, 0.01);

        positionSlider.setValue (engine.getCurrentPositionSeconds(), juce::dontSendNotification);
        updateTimeLabel();
    }

    updatePlayIcon();   // revert to play glyph when playback ends on its own

    // Drive the per-track level meters (zero them when this song isn't the one playing).
    auto* s = currentSong();
    const bool live = s != nullptr && loadedSongId == s->id && engine.isPlaying();
    for (int i = 0; i < rows.size(); ++i)
        rows[i]->setLevel (live ? engine.getTrackLevel (i) : 0.0f);
}

void SongsView::updatePlayIcon()
{
    const bool playing = engine.isPlaying();
    if (playPauseBtn.showPause != playing)
    {
        playPauseBtn.showPause = playing;
        playPauseBtn.repaint();
    }
}

void SongsView::updateTimeLabel()
{
    auto fmt = [] (double s)
    {
        const int t = juce::jmax (0, (int) std::round (s));
        return juce::String (t / 60) + ":" + juce::String (t % 60).paddedLeft ('0', 2);
    };

    timeLabel.setText (fmt (positionSlider.getValue()) + " / " + fmt (positionSlider.getMaximum()),
                       juce::dontSendNotification);
}

//==============================================================================
void SongsView::refresh()
{
    // Keep the list sorted alphabetically (case-insensitive, ascending), preserving
    // the current selection by id since sorting moves rows around.
    const juce::String selId = juce::isPositiveAndBelow (selected, (int) library.songs.size())
                                 ? library.songs[(size_t) selected].id : juce::String();

    std::stable_sort (library.songs.begin(), library.songs.end(),
                      [] (const Song& a, const Song& b)
                      { return a.name.compareIgnoreCase (b.name) < 0; });

    selected = -1;
    for (int i = 0; i < (int) library.songs.size(); ++i)
        if (library.songs[(size_t) i].id == selId)
            { selected = i; break; }

    songList.updateContent();
    songList.repaint();

    if (juce::isPositiveAndBelow (selected, (int) library.songs.size()))
        songList.selectRow (selected, true, true);
    else
        songList.deselectAllRows();

    selectSong (selected);
}

void SongsView::resized()
{
    if (sequencer.isVisible())                    // inline sequencer takes the whole view
    {
        sequencer.setBounds (getLocalBounds());
        return;
    }

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
    addFilesBtn.setBounds (btnRow.removeFromLeft (40));
    btnRow.removeFromLeft (6);
    clickToggleBtn.setBounds (btnRow.removeFromLeft (40));
    btnRow.removeFromLeft (16);
    playPauseBtn.setBounds (btnRow.removeFromLeft (44));
    btnRow.removeFromLeft (6);
    saveBtn.setBounds (btnRow.removeFromLeft (40));
    btnRow.removeFromLeft (6);
    revertBtn.setBounds (btnRow.removeFromLeft (36));
    seqBtn.setBounds (btnRow.removeFromRight (110));
    right.removeFromTop (8);

    // Play bar: seekable position slider + time readout
    auto barRow = right.removeFromTop (28);
    timeLabel.setBounds (barRow.removeFromRight (110));
    barRow.removeFromRight (8);
    positionSlider.setBounds (barRow);
    right.removeFromTop (8);

    tracksViewport.setBounds (right);

    const int width  = tracksViewport.getMaximumVisibleWidth();
    const int clickH = clickPanel.isVisible() ? clickPanel.preferredHeight() : 0;

    int contentH = clickH > 0 ? clickH + 6 : 0;
    for (auto* r : rows)
        contentH += r->getPreferredHeight() + 4;

    tracksHolder.setSize (width, juce::jmax (right.getHeight(), contentH));

    int y = 0;
    if (clickPanel.isVisible())
    {
        clickPanel.setBounds (0, 0, width, clickH);
        y = clickH + 6;
    }

    for (auto* r : rows)
    {
        const int h = r->getPreferredHeight();
        r->setBounds (0, y, width, h);
        y += h + 4;
    }
}
