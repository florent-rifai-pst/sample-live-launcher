#include "MainComponent.h"
#include "Text.h"

//==============================================================================
juce::File MainComponent::audioSettingsFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("SampleLiveLauncher")
               .getChildFile ("audio.xml");
}

//==============================================================================
MainComponent::MainComponent()
{
    formatManager.registerBasicFormats();
    engine.setFormatManager (&formatManager);

    library.load();

    // Keep the other views in sync when songs change.
    songsView.onLibraryChanged = [this]
    {
        setlistsView.refresh();
        liveView.refresh();
    };
    setlistsView.onLibraryChanged = [this]
    {
        songsView.refresh();
        liveView.refresh();
    };
    settingsView.onChange = [this]
    {
        library.save();
        songsView.refresh();   // refresh the per-song bank pickers
    };

    addAndMakeVisible (tabs);
    auto bg = getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId);
    tabs.addTab ("Morceaux", bg, &songsView,    false);
    tabs.addTab ("Setlists", bg, &setlistsView, false);
    tabs.addTab ("Live",     bg, &liveView,     false);
    tabs.addTab ("Réglages"_t, bg, &settingsView, false);

    songsView.refresh();
    setlistsView.refresh();
    liveView.refresh();
    settingsView.refresh();

    setSize (920, 740);

    // Restore the saved audio device (or defaults), requesting many outputs.
    auto savedXml = juce::XmlDocument::parse (audioSettingsFile());
    setAudioChannels (0, 32, savedXml.get());
}

MainComponent::~MainComponent()
{
    saveDeviceState();
    shutdownAudio();
}

//==============================================================================
void MainComponent::saveDeviceState()
{
    if (auto xml = deviceManager.createStateXml())
    {
        auto file = audioSettingsFile();
        file.getParentDirectory().createDirectory();
        xml->writeTo (file);
    }
}

//==============================================================================
void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    engine.prepareToPlay (samplesPerBlockExpected, sampleRate);
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    engine.getNextAudioBlock (bufferToFill);
}

void MainComponent::releaseResources()
{
    engine.releaseResources();
}

//==============================================================================
void MainComponent::resized()
{
    tabs.setBounds (getLocalBounds());
}
