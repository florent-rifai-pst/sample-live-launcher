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
        bool         mute = false;
        bool         solo = false;
        float        level = 0.0f;             // last block peak (post-gain), 0 if silent

        std::unique_ptr<juce::AudioFormatReaderSource> reader;
        std::unique_ptr<juce::AudioTransportSource>    transport;
    };

    AudioEngine();
    ~AudioEngine() override;

    void setFormatManager (juce::AudioFormatManager* fm) { formatManager = fm; }

    //== message-thread API ===================================================
    // clickAccent/clickNormal are the optional sample files for the song's chosen
    // sound bank; empty = synthesized beep.
    void loadSong (const Song& song,
                   const juce::File& clickAccent = {},
                   const juce::File& clickNormal = {});
    int  addTrack (const juce::File& file);    // returns new index, or -1 on failure
    void removeTrack (int index);
    void clear();

    void setTrackChannels (int index, std::vector<int> channels);
    void setTrackGain (int index, float gain);
    void setTrackMute (int index, bool mute);
    void setTrackSolo (int index, bool solo);

    // Rebuild the metronome. Empty files = synthesized beep; otherwise the two
    // samples are used for strong/weak beats.
    void setClick (const ClickTrack& click,
                   const juce::File& accentFile = {},
                   const juce::File& normalFile = {});

    void playAll();
    void pauseAll();                            // stop transports, keep position
    void stopAll();
    bool isPlaying() const;

    //== transport position (seconds) =========================================
    double getLengthSeconds() const;            // longest track
    double getCurrentPositionSeconds() const;
    void   setPositionSeconds (double seconds); // seek all tracks together

    int    getNumTracks() const;
    Track* getTrack (int index);                // valid until list mutated
    float  getTrackLevel (int index) const;     // last-block peak for the meter (0..~1)
    int    getNumOutputChannels() const;        // physical outputs the device exposes

    //== AudioSource ==========================================================
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;

private:
    void prepareTrack (Track& t);

    //== synthesized click track ==============================================
    struct BeatEvent { juce::int64 sample; bool accent; };

    void rebuildClickSchedule();                // (re)build beat times at current sample rate
    void triggerClickVoice (bool accent);       // start a tone or sample for this beat
    void loadClickSample (const juce::File&, juce::AudioBuffer<float>& dest, double& destRate);

    juce::AudioFormatManager* formatManager = nullptr;
    juce::OwnedArray<Track>   tracks;
    juce::CriticalSection     lock;

    juce::TimeSliceThread     readThread { "audio-read" };
    double currentSampleRate = 44100.0;
    int    currentBlockSize  = 512;
    int    currentNumOutputs = 2;               // updated from the device callback

    juce::AudioBuffer<float>  scratch;          // pre-allocated per-track read buffer

    // Master gate. Tracks are only pumped/summed while this is true; pausing just
    // flips it (instant, all tracks together) instead of calling the blocking
    // AudioTransportSource::stop(), which busy-waits ~1s per track on this thread.
    bool                      playing = false;

    ClickTrack                clickConfig;       // current song's click settings
    std::vector<BeatEvent>    clickSchedule;     // beat onsets (in samples) for the song
    juce::int64               clickLengthSamples = 0;
    juce::int64               clickPlayhead      = 0;   // device-clock sample position
    size_t                    clickCursor        = 0;   // next unplayed beat
    bool                      clickPlaying       = false;

    // Synth voice (default beep)
    int    clickVoiceRemaining = 0;             // samples left in the current tone
    int    clickVoiceLen       = 1;
    double clickPhase          = 0.0;
    double clickPhaseInc       = 0.0;
    float  clickVoiceAmp       = 0.0f;

    // Sample voice (chosen sound bank). Buffers are mono; played back with
    // on-the-fly rate conversion to the device sample rate.
    bool                            clickUseSamples = false;
    juce::AudioBuffer<float>        clickAccentBuf, clickNormalBuf;
    double                          clickAccentRate = 44100.0, clickNormalRate = 44100.0;
    const juce::AudioBuffer<float>* clickSampleBuf  = nullptr;   // currently sounding, or null
    double                          clickSamplePos  = 0.0;
    double                          clickSampleStep = 1.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
};
