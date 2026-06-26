#include "LiveView.h"

LiveView::LiveView (Library& lib, AudioEngine& eng)
    : library (lib), engine (eng)
{
    addAndMakeVisible (info);
    info.setJustificationType (juce::Justification::centred);
    info.setFont (juce::Font (juce::FontOptions (24.0f, juce::Font::bold)));
}

void LiveView::refresh() {}

void LiveView::resized()
{
    info.setBounds (getLocalBounds());
}
