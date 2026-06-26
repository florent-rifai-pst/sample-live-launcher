#pragma once

#include <JuceHeader.h>
#include "Models.h"
#include "AudioEngine.h"
#include "SongsView.h"
#include "SetlistsView.h"
#include "LiveView.h"

//==============================================================================
// Wraps the audio device selector so it can live in a tab.
class SettingsView  : public juce::Component
{
public:
    explicit SettingsView (juce::AudioDeviceManager& dm)
        : selector (dm, 0, 0, 2, 256, false, false, false, false)
    {
        addAndMakeVisible (selector);
    }

    void resized() override { selector.setBounds (getLocalBounds().reduced (10)); }

private:
    juce::AudioDeviceSelectorComponent selector;
};

//==============================================================================
class MainComponent  : public juce::AudioAppComponent
{
public:
    MainComponent();
    ~MainComponent() override;

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    void resized() override;

private:
    static juce::File audioSettingsFile();
    void saveDeviceState();

    Library                  library;
    juce::AudioFormatManager formatManager;
    AudioEngine              engine;

    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    SongsView    songsView    { library, engine };
    SetlistsView setlistsView { library, engine };
    LiveView     liveView     { library, engine };
    SettingsView settingsView { deviceManager };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
