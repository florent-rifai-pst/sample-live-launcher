#pragma once

#include <JuceHeader.h>
#include "Models.h"

//==============================================================================
// Multi-track router/mixer.
//
// A "song" is a set of tracks. Each track = one audio file routed to one or
// more PHYSICAL output channels of the sound card (0-based indices), with a
// per-track gain. All tracks of the song start together (synchronised).
//
// In getNextAudioBlock we read each track into a scratch buffer, then sum it
// into the chosen physical channels of the device output buffer.
class AudioEngine  : public juce::AudioSource
{
public:
    struct Track
    {
        juce::String name;
        juce::File   file;
        int          srcChannels = 2;          // channels in the source file
        std::vector<int> outChannels { 0, 1 }; // physical output indices (0-based)
        float        gain = 1.0f;

        std::unique_ptr<juce::AudioFormatReaderSource> reader;
        std::unique_ptr<juce::AudioTransportSource>    transport;
    };

    AudioEngine();
    ~AudioEngine() override;

    void setFormatManager (juce::AudioFormatManager* fm) { formatManager = fm; }

    //== message-thread API ===================================================
    void loadSong (const Song& song);          // replaces current tracks with the song's
    int  addTrack (const juce::File& file);    // returns new index, or -1 on failure
    void removeTrack (int index);
    void clear();

    void setTrackChannels (int index, std::vector<int> channels);
    void setTrackGain (int index, float gain);

    void playAll();
    void stopAll();
    bool isPlaying() const;

    int    getNumTracks() const;
    Track* getTrack (int index);                // valid until list mutated

    //== AudioSource ==========================================================
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;

private:
    void prepareTrack (Track& t);

    juce::AudioFormatManager* formatManager = nullptr;
    juce::OwnedArray<Track>   tracks;
    juce::CriticalSection     lock;

    juce::TimeSliceThread     readThread { "audio-read" };
    double currentSampleRate = 44100.0;
    int    currentBlockSize  = 512;

    juce::AudioBuffer<float>  scratch;          // pre-allocated per-track read buffer

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
};
