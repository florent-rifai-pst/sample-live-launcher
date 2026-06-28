#pragma once

#include <JuceHeader.h>
#include "Models.h"
#include "AudioEngine.h"
#include "Text.h"

//==============================================================================
// Inline "Séquenceur" view (Cubase-like). Shows the song on a bar grid derived
// from the click track's tempo map, with transport, a metronome enable toggle
// and a loop region the user selects on the bar ruler.
//
// Touch-friendly by design (the app is headed to iPad / Android): JUCE delivers
// touch as mouse events, so a drag-select works on both; we just give the loop
// handles finger-sized hit zones. Tap a bar = move playhead; drag on the ruler
// = pick a loop; drag on a lane = scrub.
//
// The view drives an AudioEngine that already has the song loaded. It only ever
// converts bars <-> seconds; the engine itself stays bar-agnostic.
class SequencerView  : public juce::Component,
                       private juce::Timer,
                       private juce::ScrollBar::Listener
{
public:
    explicit SequencerView (AudioEngine& engine);

    // Bind to a song already loaded into the engine. clickAccent/clickNormal are
    // the resolved sound-bank files so toggling the click rebuilds it correctly.
    void setSong (Song* song, juce::File clickAccent, juce::File clickNormal);

    std::function<void()> onClose;    // "Retour" pressed
    std::function<void()> onChange;   // click enabled toggled -> host marks dirty

    void resized() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp   (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMagnify   (const juce::MouseEvent&, float scaleFactor) override;

private:
    void timerCallback() override;
    void scrollBarMoved (juce::ScrollBar*, double newStart) override;

    void rebuildGrid();               // recompute tempo map from song->click
    void updateTransportButtons();
    void applyLoopToEngine();
    void rebuildClickInEngine();      // after a click toggle, keep the position

    void   zoomAround (float newZoom, int anchorX);   // keep the bar under anchorX fixed
    void   clampScroll();
    void   updateScrollBar();

    float  timeToX (double seconds) const;
    double xToTime (int x) const;
    int    xToBar  (int x) const;     // 1-based, clamped to [1, totalBars]
    double timeToBars (double seconds) const;   // continuous bar position
    double barsToTime (double bars) const;
    float  gridWidth()    const;
    float  baseBarWidth() const;      // bar width at zoom 1 (fit-to-width)
    float  barWidth()     const;      // baseBarWidth * zoom
    float  contentWidth() const;      // total grid width at current zoom

    juce::Rectangle<int> rulerArea() const;
    juce::Rectangle<int> lanesArea() const;

    AudioEngine& engine;
    Song*        song = nullptr;
    juce::File   clickAccent, clickNormal;

    std::vector<double> boundaries { 0.0 };   // bar -> seconds, size totalBars + 1
    int    totalBars   = 0;
    double fallbackDur = 0.0;                  // time scale when there is no click grid

    int gridLeft  = 96;                        // lane-name gutter on the left
    int gridRight = 12;

    float zoom    = 1.0f;                       // 1 = fit to width; >1 = zoomed in
    int   scrollX = 0;                          // horizontal scroll offset (px)
    juce::ScrollBar hScroll { false };          // horizontal scrollbar (touch-draggable)

    bool loopOn      = false;
    int  loopFromBar = 1;                      // inclusive, 1-based
    int  loopToBar   = 1;                      // inclusive, 1-based

    enum class Drag { none, newSel, moveStart, moveEnd, scrub };
    Drag drag          = Drag::none;
    int  dragAnchorBar = 1;
    bool dragMoved     = false;

    juce::TextButton   backBtn  { "\xe2\x86\x90 Retour"_t };   // ← Retour
    juce::ToggleButton clickToggle { "Click" };
    juce::TextButton   playBtn;
    juce::TextButton   stopBtn;
    juce::ToggleButton loopToggle { "Boucle" };
    juce::Label        infoLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SequencerView)
};
