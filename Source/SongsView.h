#pragma once

#include <JuceHeader.h>
#include "Models.h"
#include "AudioEngine.h"
#include "TrackRow.h"
#include "ClickTrackPanel.h"
#include "SequencerView.h"

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
// Base for a small icon button with the same flat rounded background as the
// transport button. Subclasses just draw a glyph inside `glyphArea`.
class IconButton  : public juce::Button
{
public:
    explicit IconButton (const juce::String& name) : juce::Button (name) {}

    bool highlighted = false;   // for toggle-style buttons

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto base = highlighted ? juce::Colours::cornflowerblue.withAlpha (0.55f)
                                : juce::Colours::white.withAlpha (down ? 0.25f : over ? 0.18f : 0.10f);
        g.setColour (base);
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);

        g.setColour (isEnabled() ? juce::Colours::white : juce::Colours::grey);
        drawGlyph (g, getLocalBounds().toFloat().reduced (getWidth() * 0.26f, getHeight() * 0.26f));
    }

    virtual void drawGlyph (juce::Graphics&, juce::Rectangle<float> a) = 0;
};

// Floppy-disk save icon.
class SaveButton  : public IconButton
{
public:
    SaveButton() : IconButton ("save") {}
    void drawGlyph (juce::Graphics& g, juce::Rectangle<float> a) override
    {
        juce::Path body;
        const float cut = a.getWidth() * 0.22f;
        body.startNewSubPath (a.getX(), a.getY());
        body.lineTo (a.getRight() - cut, a.getY());
        body.lineTo (a.getRight(), a.getY() + cut);
        body.lineTo (a.getRight(), a.getBottom());
        body.lineTo (a.getX(), a.getBottom());
        body.closeSubPath();
        g.strokePath (body, juce::PathStrokeType (1.4f));

        // shutter (top) and label (bottom)
        g.fillRect (juce::Rectangle<float> (a.getX() + a.getWidth() * 0.30f, a.getY(),
                                            a.getWidth() * 0.28f, a.getHeight() * 0.30f));
        g.drawRect (juce::Rectangle<float> (a.getX() + a.getWidth() * 0.20f, a.getY() + a.getHeight() * 0.52f,
                                            a.getWidth() * 0.60f, a.getHeight() * 0.40f), 1.2f);
    }
};

// Plus icon.
class PlusButton  : public IconButton
{
public:
    PlusButton() : IconButton ("plus") {}
    void drawGlyph (juce::Graphics& g, juce::Rectangle<float> a) override
    {
        const float t = juce::jmax (1.6f, a.getWidth() * 0.16f);
        g.fillRect (juce::Rectangle<float> (a.getCentreX() - t * 0.5f, a.getY(), t, a.getHeight()));
        g.fillRect (juce::Rectangle<float> (a.getX(), a.getCentreY() - t * 0.5f, a.getWidth(), t));
    }
};

// Metronome icon (trapezoid body + pendulum).
class MetronomeButton  : public IconButton
{
public:
    MetronomeButton() : IconButton ("metronome") {}
    void drawGlyph (juce::Graphics& g, juce::Rectangle<float> a) override
    {
        juce::Path body;
        body.startNewSubPath (a.getCentreX() - a.getWidth() * 0.16f, a.getY());
        body.lineTo (a.getCentreX() + a.getWidth() * 0.16f, a.getY());
        body.lineTo (a.getRight(), a.getBottom());
        body.lineTo (a.getX(), a.getBottom());
        body.closeSubPath();
        g.strokePath (body, juce::PathStrokeType (1.4f));
        g.drawLine (a.getCentreX(), a.getBottom() - 1.0f,
                    a.getCentreX() + a.getWidth() * 0.22f, a.getY() + a.getHeight() * 0.22f, 1.4f);
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
    void openSequencer();                        // load song + show the inline sequencer
    void closeSequencer();
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
    PlusButton       addFilesBtn;
    MetronomeButton  clickToggleBtn;            // shows/hides the click panel
    PlayPauseButton  playPauseBtn;
    SaveButton       saveBtn;
    juce::TextButton revertBtn   { juce::String::fromUTF8 ("\xe2\x86\xb6") };  // undo arrow
    juce::TextButton seqBtn      { juce::String::fromUTF8 ("S\xc3\xa9quenceur") };  // open inline sequencer

    juce::Slider     positionSlider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::Label      timeLabel      { {}, "0:00 / 0:00" };

    juce::Viewport   tracksViewport;
    juce::Component  tracksHolder;
    ClickTrackPanel  clickPanel;                 // always first position
    juce::OwnedArray<TrackRow> rows;

    SequencerView    sequencer { engine };       // inline view, hidden until opened

    int  selected = -1;
    bool dirty    = false;
    Song snapshot;                               // song state captured when displayed
    juce::String loadedSongId;                   // which song is currently in the engine
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SongsView)
};
