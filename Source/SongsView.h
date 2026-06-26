#pragma once

#include <JuceHeader.h>
#include "Models.h"
#include "AudioEngine.h"
#include "TrackRow.h"

//==============================================================================
// Admin view for songs: left = list of songs, right = editor for the selected
// song (name, its tracks with output channels + gain, and a local Play/Stop
// that loads the song into the audio engine).
class SongsView  : public juce::Component,
                   public juce::ListBoxModel
{
public:
    SongsView (Library& library, AudioEngine& engine);

    void refresh();                              // reload list + editor from library
    void resized() override;

    std::function<void()> onLibraryChanged;      // host saves + refreshes other views

    //== ListBoxModel =========================================================
    int  getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void selectedRowsChanged (int lastRow) override;

private:
    void newSong();
    void deleteSong();
    void selectSong (int index);
    void addFiles();
    void rebuildTrackRows();
    void notifyChanged();                        // save + refresh others

    Song* currentSong();

    Library&     library;
    AudioEngine& engine;

    juce::ListBox    songList;
    juce::TextButton newSongBtn { "Nouveau morceau" };
    juce::TextButton delSongBtn { "Supprimer" };

    juce::Label      editorTitle { {}, "Morceau" };
    juce::Label      nameCaption { {}, "Nom:" };
    juce::TextEditor nameEditor;
    juce::TextButton addFilesBtn { "Ajouter fichier(s)..." };
    juce::TextButton playBtn     { "Play" };
    juce::TextButton stopBtn     { "Stop" };

    juce::Viewport   tracksViewport;
    juce::Component  tracksHolder;
    juce::OwnedArray<TrackRow> rows;

    int selected = -1;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SongsView)
};
