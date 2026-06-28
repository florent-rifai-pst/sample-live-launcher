#pragma once

#include <JuceHeader.h>
#include "Models.h"
#include "AudioEngine.h"
#include "TrackRow.h"
#include "ClickTrackPanel.h"

//==============================================================================
// Icon-only transport button: draws a play triangle, or two pause bars when
// showPause is true.
class PlayPauseButton  : public juce::Button
{
public:
    PlayPauseButton() : juce::Button ("playpause") {}

    bool showPause = false;

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        g.setColour (juce::Colours::white.withAlpha (down ? 0.25f : over ? 0.18f : 0.10f));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);

        auto a = getLocalBounds().toFloat().reduced (getWidth() * 0.30f, getHeight() * 0.26f);
        g.setColour (isEnabled() ? juce::Colours::white : juce::Colours::grey);

        if (showPause)
        {
            const float gap  = a.getWidth() * 0.34f;
            const float barW = (a.getWidth() - gap) * 0.5f;
            g.fillRect (juce::Rectangle<float> (a.getX(),               a.getY(), barW, a.getHeight()));
            g.fillRect (juce::Rectangle<float> (a.getRight() - barW,    a.getY(), barW, a.getHeight()));
        }
        else
        {
            juce::Path p;
            p.addTriangle (a.getX(), a.getY(), a.getX(), a.getBottom(), a.getRight(), a.getCentreY());
            g.fillPath (p);
        }
    }
};

//==============================================================================
// Admin view for songs: left = list of songs, right = editor for the selected
// song (name, its tracks with output channels + gain, and a local Play/Stop
// that loads the song into the audio engine).
class SongsView  : public juce::Component,
                   public juce::ListBoxModel,
                   public juce::DragAndDropContainer,
                   private juce::Timer
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
    std::pair<juce::File, juce::File> clickBankFiles (const Song&);   // resolve chosen bank
    void selectSong (int index);
    void addFiles();
    void rebuildTrackRows();
    void timerCallback() override;               // drives the play bar
    void updateTimeLabel();
    void updatePlayIcon();                       // play/pause glyph from engine state
    void notifyChanged();                        // mark dirty + refresh others (no disk write)
    void setDirty (bool);
    void save();                                 // write library to disk, clear dirty
    void revert();                               // restore the song to its display-time state

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
    PlayPauseButton  playPauseBtn;
    juce::TextButton saveBtn     { "Sauvegarder" };
    juce::TextButton revertBtn   { juce::String::fromUTF8 ("\xe2\x86\xb6") };  // undo arrow

    juce::Slider     positionSlider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::Label      timeLabel      { {}, "0:00 / 0:00" };

    juce::Viewport   tracksViewport;
    juce::Component  tracksHolder;
    ClickTrackPanel  clickPanel;                 // always first position
    juce::OwnedArray<TrackRow> rows;

    int  selected = -1;
    bool dirty    = false;
    Song snapshot;                               // song state captured when displayed
    juce::String loadedSongId;                   // which song is currently in the engine
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SongsView)
};
