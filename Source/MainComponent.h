#pragma once

#include <JuceHeader.h>
#include "Models.h"
#include "AudioEngine.h"
#include "SongsView.h"
#include "SetlistsView.h"
#include "LiveView.h"
#include "SoundBankView.h"

//==============================================================================
// Réglages tab: audio device selector on top, click sound bank manager below.
class SettingsView  : public juce::Component
{
public:
    SettingsView (juce::AudioDeviceManager& dm, Library& library)
        : selector (dm, 0, 0, 2, 256, false, false, false, false),
          banks (library)
    {
        addAndMakeVisible (selector);
        addAndMakeVisible (banks);
        banks.onChange = [this] { if (onChange) onChange(); };
    }

    void refresh() { banks.refresh(); }

    void resized() override
    {
        auto area = getLocalBounds().reduced (10);
        banks.setBounds (area.removeFromBottom (230));
        area.removeFromBottom (10);
        selector.setBounds (area);
    }

    std::function<void()> onChange;   // a sound bank was edited

private:
    juce::AudioDeviceSelectorComponent selector;
    SoundBankView                      banks;
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

    juce::TooltipWindow   tooltipWindow { this, 600 };   // shows icon-button tooltips
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    SongsView    songsView    { library, engine };
    SetlistsView setlistsView { library, engine };
    LiveView     liveView     { library, engine };
    SettingsView settingsView { deviceManager, library };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
