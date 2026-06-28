#include "SequencerView.h"

namespace
{
    // Lane colours, cycled across the song's tracks. The click lane uses cornflower.
    const juce::Colour laneColours[] =
    {
        juce::Colour (0xff4f8fef), juce::Colour (0xffe05a5a), juce::Colour (0xffe0902a),
        juce::Colour (0xff3fbf6f), juce::Colour (0xff9b6bd6), juce::Colour (0xff2ab0c0),
        juce::Colour (0xffd0c040), juce::Colour (0xffe06aa0)
    };

    constexpr int numLaneColours = (int) (sizeof (laneColours) / sizeof (laneColours[0]));
    constexpr int kHandleHit = 12;   // px each side of a loop edge that grabs it
}

//==============================================================================
SequencerView::SequencerView (AudioEngine& eng)
    : engine (eng)
{
    addAndMakeVisible (backBtn);
    backBtn.onClick = [this] { if (onClose) onClose(); };

    addAndMakeVisible (clickToggle);
    clickToggle.setTooltip ("Activer / d\xc3\xa9sactiver le clic"_t);
    clickToggle.onClick = [this]
    {
        if (song == nullptr)
            return;
        song->click.enabled = clickToggle.getToggleState();
        rebuildClickInEngine();
        if (onChange) onChange();
    };

    addAndMakeVisible (playBtn);
    playBtn.onClick = [this]
    {
        if (engine.isPlaying())
            engine.pauseAll();
        else
            engine.playAll();
        updateTransportButtons();
    };

    addAndMakeVisible (stopBtn);
    stopBtn.setButtonText (juce::String::fromUTF8 ("\xe2\x96\xa0"));   // ■
    stopBtn.onClick = [this]
    {
        engine.stopAll();
        updateTransportButtons();
        repaint();
    };

    addAndMakeVisible (loopToggle);
    loopToggle.setTooltip ("Boucler sur la s\xc3\xa9lection"_t);
    loopToggle.onClick = [this]
    {
        loopOn = loopToggle.getToggleState();
        applyLoopToEngine();
        repaint();
    };

    addAndMakeVisible (hScroll);
    hScroll.setAutoHide (false);
    hScroll.addListener (this);

    addChildComponent (infoLabel);
    infoLabel.setJustificationType (juce::Justification::centred);
    infoLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
    infoLabel.setText ("Ajoutez des sections au clic pour afficher la grille de mesures."_t,
                       juce::dontSendNotification);

    updateTransportButtons();
    startTimerHz (25);
}

//==============================================================================
void SequencerView::setSong (Song* s, juce::File accent, juce::File normal)
{
    song        = s;
    clickAccent = accent;
    clickNormal = normal;

    if (song != nullptr)
        clickToggle.setToggleState (song->click.enabled, juce::dontSendNotification);

    rebuildGrid();
    updateTransportButtons();
    resized();
    repaint();
}

void SequencerView::rebuildGrid()
{
    boundaries  = song != nullptr ? clickBarBoundaries (song->click) : std::vector<double> { 0.0 };
    totalBars   = juce::jmax (0, (int) boundaries.size() - 1);
    fallbackDur = song != nullptr ? songDurationSeconds (*song) : 0.0;

    loopFromBar = 1;
    loopToBar   = juce::jmax (1, totalBars);
    loopOn      = false;
    loopToggle.setToggleState (false, juce::dontSendNotification);
    loopToggle.setEnabled (totalBars > 0);
    applyLoopToEngine();

    zoom    = 1.0f;                  // reset to fit-to-width
    scrollX = 0;
    updateScrollBar();

    infoLabel.setVisible (totalBars == 0);
}

void SequencerView::rebuildClickInEngine()
{
    const double pos = engine.getCurrentPositionSeconds();
    engine.setClick (song->click, clickAccent, clickNormal);
    engine.setPositionSeconds (pos);   // setClick rewinds the metronome; restore it
}

void SequencerView::applyLoopToEngine()
{
    if (loopOn && totalBars > 0
        && juce::isPositiveAndBelow (loopFromBar - 1, (int) boundaries.size())
        && juce::isPositiveAndBelow (loopToBar,       (int) boundaries.size()))
        engine.setLoop (true, boundaries[(size_t) (loopFromBar - 1)], boundaries[(size_t) loopToBar]);
    else
        engine.setLoop (false);
}

void SequencerView::updateTransportButtons()
{
    playBtn.setButtonText (engine.isPlaying() ? juce::String::fromUTF8 ("\xe2\x8f\xb8")    // ⏸
                                              : juce::String::fromUTF8 ("\xe2\x96\xb6"));   // ▶
}

//==============================================================================
float SequencerView::gridWidth() const
{
    return (float) juce::jmax (1, getWidth() - gridLeft - gridRight);
}

float SequencerView::baseBarWidth() const
{
    return totalBars > 0 ? gridWidth() / (float) totalBars : gridWidth();
}

float SequencerView::barWidth() const
{
    return baseBarWidth() * zoom;
}

float SequencerView::contentWidth() const
{
    return totalBars > 0 ? totalBars * barWidth() : gridWidth();
}

// Continuous bar position (0 = bar 1 start) for a time, walking the tempo map.
double SequencerView::timeToBars (double seconds) const
{
    if (totalBars <= 0)
        return 0.0;

    const double tot = boundaries.back();
    seconds = juce::jlimit (0.0, tot, seconds);
    for (int i = 0; i < totalBars; ++i)
    {
        const double a = boundaries[(size_t) i], b = boundaries[(size_t) (i + 1)];
        if (seconds <= b || i == totalBars - 1)
            return (double) i + (b > a ? (seconds - a) / (b - a) : 0.0);
    }
    return 0.0;
}

double SequencerView::barsToTime (double bars) const
{
    if (totalBars <= 0)
        return 0.0;

    bars = juce::jlimit (0.0, (double) totalBars, bars);
    const int    i    = juce::jlimit (0, totalBars - 1, (int) bars);
    const double frac = bars - i;
    return boundaries[(size_t) i] + frac * (boundaries[(size_t) (i + 1)] - boundaries[(size_t) i]);
}

float SequencerView::timeToX (double seconds) const
{
    if (totalBars > 0)
        return (float) gridLeft + (float) timeToBars (seconds) * barWidth() - (float) scrollX;

    const double tot = juce::jmax (0.001, fallbackDur);
    return (float) gridLeft + (float) (juce::jlimit (0.0, 1.0, seconds / tot)) * gridWidth();
}

double SequencerView::xToTime (int x) const
{
    if (totalBars > 0)
        return barsToTime ((x - gridLeft + scrollX) / (double) barWidth());

    const double tot = juce::jmax (0.001, fallbackDur);
    return juce::jlimit (0.0, tot, (x - gridLeft) / (double) gridWidth() * tot);
}

int SequencerView::xToBar (int x) const
{
    if (totalBars <= 0)
        return 1;
    const int bar = (int) ((x - gridLeft + scrollX) / barWidth()) + 1;
    return juce::jlimit (1, totalBars, bar);
}

//==============================================================================
void SequencerView::clampScroll()
{
    const int maxScroll = juce::jmax (0, (int) (contentWidth() - gridWidth()));
    scrollX = juce::jlimit (0, maxScroll, scrollX);
}

void SequencerView::updateScrollBar()
{
    const bool needed = contentWidth() > gridWidth() + 1.0f;
    hScroll.setVisible (needed);
    hScroll.setRangeLimits (0.0, juce::jmax ((double) gridWidth(), (double) contentWidth()),
                            juce::dontSendNotification);
    hScroll.setCurrentRange (scrollX, gridWidth(), juce::dontSendNotification);
}

void SequencerView::zoomAround (float newZoom, int anchorX)
{
    if (totalBars <= 0)
        return;

    newZoom = juce::jlimit (1.0f, 60.0f, newZoom);
    const double barsAtAnchor = (anchorX - gridLeft + scrollX) / (double) barWidth();

    zoom = newZoom;
    scrollX = (int) std::llround (gridLeft + barsAtAnchor * barWidth() - anchorX);
    clampScroll();
    updateScrollBar();
    repaint();
}

void SequencerView::scrollBarMoved (juce::ScrollBar*, double newStart)
{
    scrollX = (int) std::llround (newStart);
    clampScroll();
    repaint();
}

void SequencerView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (totalBars <= 0)
        return;

    if (e.mods.isCtrlDown() || e.mods.isCommandDown())
        zoomAround (zoom * (w.deltaY >= 0.0f ? 1.12f : 1.0f / 1.12f), e.x);
    else
    {
        scrollX -= (int) (w.deltaY * barWidth() * 3.0f);
        clampScroll();
        updateScrollBar();
        repaint();
    }
}

void SequencerView::mouseMagnify (const juce::MouseEvent& e, float scaleFactor)
{
    zoomAround (zoom * scaleFactor, e.x);
}

juce::Rectangle<int> SequencerView::rulerArea() const
{
    return { 0, 52, getWidth(), 30 };
}

juce::Rectangle<int> SequencerView::lanesArea() const
{
    return getLocalBounds().withTrimmedTop (rulerArea().getBottom() + 2)
                           .withTrimmedBottom (16)            // room for the scrollbar
                           .reduced (0, 4);
}

//==============================================================================
void SequencerView::resized()
{
    auto top = getLocalBounds().removeFromTop (44).reduced (8, 6);

    backBtn    .setBounds (top.removeFromLeft (90));
    top.removeFromLeft (16);
    clickToggle.setBounds (top.removeFromLeft (70));
    top.removeFromLeft (16);
    playBtn    .setBounds (top.removeFromLeft (48));
    top.removeFromLeft (6);
    stopBtn    .setBounds (top.removeFromLeft (48));
    top.removeFromLeft (16);
    loopToggle .setBounds (top.removeFromLeft (80));

    hScroll.setBounds (gridLeft, getHeight() - 14, juce::jmax (1, getWidth() - gridLeft - gridRight), 12);

    clampScroll();
    updateScrollBar();
    infoLabel.setBounds (lanesArea());
}

void SequencerView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff232730));

    auto ruler = rulerArea();
    auto lanes = lanesArea();

    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.fillRect (ruler);

    // Lane list: click first (if it has a grid), then each track.
    struct Lane { juce::String name; double dur; juce::Colour col; };
    std::vector<Lane> list;
    if (song != nullptr)
    {
        if (boundaries.size() > 1)
            list.push_back ({ "Click", boundaries.back(), juce::Colours::cornflowerblue });
        int c = 0;
        for (auto& t : song->tracks)
            list.push_back ({ t.name, t.durationSeconds, laneColours[(c++) % numLaneColours] });
    }

    const int laneH = list.empty() ? 0
                    : juce::jlimit (18, 34, lanes.getHeight() / (int) list.size() - 4);

    // Lane name gutter (always visible, never scrolled).
    g.setFont (juce::Font (juce::FontOptions (12.0f)));
    {
        int y = lanes.getY();
        for (auto& ln : list)
        {
            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.drawText (ln.name, 6, y, gridLeft - 10, laneH, juce::Justification::centredLeft, true);
            y += laneH + 4;
        }
    }

    // Everything on the time grid is clipped to the area right of the gutter so
    // zoomed/scrolled content never bleeds over the lane names.
    {
        juce::Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (gridLeft, ruler.getY(), getWidth() - gridLeft - gridRight,
                            lanes.getBottom() - ruler.getY());

        // Loop region (behind the grid lines).
        if (loopOn && totalBars > 0)
        {
            const float xa = timeToX (boundaries[(size_t) (loopFromBar - 1)]);
            const float xb = timeToX (boundaries[(size_t) loopToBar]);
            g.setColour (juce::Colours::yellow.withAlpha (0.14f));
            g.fillRect (juce::Rectangle<float> (xa, (float) ruler.getY(), xb - xa,
                                                (float) (lanes.getBottom() - ruler.getY())));
            g.setColour (juce::Colours::yellow.withAlpha (0.85f));
            g.fillRect (juce::Rectangle<float> (xa - 2.0f, (float) ruler.getY(), 4.0f, (float) ruler.getHeight()));
            g.fillRect (juce::Rectangle<float> (xb - 2.0f, (float) ruler.getY(), 4.0f, (float) ruler.getHeight()));
        }

        // Bar grid + numbers. Label every `step` bars so multi-digit numbers fit.
        if (totalBars > 0)
        {
            const float bw   = barWidth();
            const int   step = juce::jmax (1, (int) std::ceil (26.0f / bw));
            g.setFont (juce::Font (juce::FontOptions (12.0f)));

            for (int bar = 0; bar <= totalBars; ++bar)
            {
                const float x = (float) gridLeft + bar * bw - (float) scrollX;
                if (x < gridLeft - 1.0f || x > getWidth() - gridRight + 1.0f)
                    continue;

                g.setColour (juce::Colours::white.withAlpha (0.10f));
                g.drawVerticalLine ((int) x, (float) ruler.getY(), (float) lanes.getBottom());

                if (bar < totalBars && bar % step == 0)
                {
                    g.setColour (juce::Colours::white.withAlpha (0.75f));
                    g.drawText (juce::String (bar + 1),
                                juce::Rectangle<float> (x + 3.0f, (float) ruler.getY(),
                                                        bw * step - 4.0f, (float) ruler.getHeight()),
                                juce::Justification::centredLeft, false);
                }
            }
        }

        // Lane blocks.
        int y = lanes.getY();
        for (auto& ln : list)
        {
            const float x0 = timeToX (0.0);
            const float x1 = timeToX (ln.dur);
            juce::Rectangle<float> block (x0, (float) y + 2.0f, juce::jmax (2.0f, x1 - x0), (float) laneH - 4.0f);
            g.setColour (ln.col.withAlpha (0.55f));
            g.fillRoundedRectangle (block, 3.0f);
            g.setColour (ln.col);
            g.drawRoundedRectangle (block, 3.0f, 1.0f);
            y += laneH + 4;
        }

        // Playhead.
        const float px = timeToX (engine.getCurrentPositionSeconds());
        g.setColour (juce::Colours::white);
        g.drawVerticalLine ((int) px, (float) ruler.getY(), (float) lanes.getBottom());
        juce::Path tri;
        tri.addTriangle (px - 5.0f, (float) ruler.getY(), px + 5.0f, (float) ruler.getY(), px, (float) ruler.getY() + 6.0f);
        g.fillPath (tri);
    }
}

//==============================================================================
void SequencerView::mouseDown (const juce::MouseEvent& e)
{
    drag      = Drag::none;
    dragMoved = false;

    if (rulerArea().contains (e.getPosition()) && totalBars > 0)
    {
        if (loopOn)
        {
            const float xa = timeToX (boundaries[(size_t) (loopFromBar - 1)]);
            const float xb = timeToX (boundaries[(size_t) loopToBar]);
            if (std::abs (e.x - xa) <= kHandleHit) { drag = Drag::moveStart; return; }
            if (std::abs (e.x - xb) <= kHandleHit) { drag = Drag::moveEnd;   return; }
        }
        drag          = Drag::newSel;
        dragAnchorBar = xToBar (e.x);
    }
    else if (lanesArea().contains (e.getPosition()))
    {
        drag = Drag::scrub;
        engine.setPositionSeconds (xToTime (e.x));
        repaint();
    }
}

void SequencerView::mouseDrag (const juce::MouseEvent& e)
{
    if (drag == Drag::none)
        return;

    if (drag == Drag::scrub)
    {
        engine.setPositionSeconds (xToTime (e.x));
        repaint();
        return;
    }

    const int bar = xToBar (e.x);

    if (drag == Drag::moveStart)
    {
        loopFromBar = juce::jmin (bar, loopToBar);
    }
    else if (drag == Drag::moveEnd)
    {
        loopToBar = juce::jmax (bar, loopFromBar);
    }
    else if (drag == Drag::newSel)
    {
        if (bar != dragAnchorBar)
            dragMoved = true;
        loopFromBar = juce::jmin (dragAnchorBar, bar);
        loopToBar   = juce::jmax (dragAnchorBar, bar);
        loopOn      = true;
    }

    if (loopOn)
    {
        loopToggle.setToggleState (true, juce::dontSendNotification);
        applyLoopToEngine();
    }
    repaint();
}

void SequencerView::mouseUp (const juce::MouseEvent&)
{
    // A plain tap on the ruler (no drag) just moves the playhead to that bar.
    if (drag == Drag::newSel && ! dragMoved)
        engine.setPositionSeconds (boundaries[(size_t) (dragAnchorBar - 1)]);

    drag = Drag::none;
    repaint();
}

//==============================================================================
void SequencerView::timerCallback()
{
    updateTransportButtons();
    repaint();
}
