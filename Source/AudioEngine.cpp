#include "AudioEngine.h"

//==============================================================================
AudioEngine::AudioEngine()
{
    readThread.startThread();
    scratch.setSize (8, 1024);
}

AudioEngine::~AudioEngine()
{
    {
        const juce::ScopedLock sl (lock);
        tracks.clear();
    }
    readThread.stopThread (2000);
}

//==============================================================================
void AudioEngine::loadSong (const Song& song)
{
    clear();
    for (auto& td : song.tracks)
    {
        const int idx = addTrack (juce::File (td.filePath));
        if (idx >= 0)
        {
            setTrackChannels (idx, td.channels);
            setTrackGain (idx, td.gain);
        }
    }
}

//==============================================================================
int AudioEngine::addTrack (const juce::File& file)
{
    jassert (formatManager != nullptr);

    auto* reader = formatManager->createReaderFor (file);
    if (reader == nullptr)
        return -1;

    auto t = std::make_unique<Track>();
    t->name        = file.getFileName();
    t->file        = file;
    t->srcChannels = juce::jmax (1, (int) reader->numChannels);

    t->reader.reset (new juce::AudioFormatReaderSource (reader, true));
    t->transport.reset (new juce::AudioTransportSource());
    t->transport->setSource (t->reader.get(),
                             32768, &readThread,
                             reader->sampleRate,
                             t->srcChannels);

    prepareTrack (*t);

    const juce::ScopedLock sl (lock);
    tracks.add (t.release());
    return tracks.size() - 1;
}

void AudioEngine::removeTrack (int index)
{
    const juce::ScopedLock sl (lock);
    if (juce::isPositiveAndBelow (index, tracks.size()))
        tracks.remove (index);   // OwnedArray deletes it; transport stops on destruction
}

void AudioEngine::clear()
{
    const juce::ScopedLock sl (lock);
    tracks.clear();
}

void AudioEngine::setTrackChannels (int index, std::vector<int> channels)
{
    const juce::ScopedLock sl (lock);
    if (auto* t = getTrack (index))
        t->outChannels = std::move (channels);
}

void AudioEngine::setTrackGain (int index, float gain)
{
    const juce::ScopedLock sl (lock);
    if (auto* t = getTrack (index))
        t->gain = gain;
}

//==============================================================================
void AudioEngine::playAll()
{
    const juce::ScopedLock sl (lock);
    for (auto* t : tracks)
    {
        t->transport->setPosition (0.0);
        t->transport->start();
    }
}

void AudioEngine::stopAll()
{
    const juce::ScopedLock sl (lock);
    for (auto* t : tracks)
    {
        t->transport->stop();
        t->transport->setPosition (0.0);
    }
}

bool AudioEngine::isPlaying() const
{
    const juce::ScopedLock sl (lock);
    for (auto* t : tracks)
        if (t->transport != nullptr && t->transport->isPlaying())
            return true;
    return false;
}

int AudioEngine::getNumTracks() const
{
    const juce::ScopedLock sl (lock);
    return tracks.size();
}

AudioEngine::Track* AudioEngine::getTrack (int index)
{
    return juce::isPositiveAndBelow (index, tracks.size()) ? tracks[index] : nullptr;
}

//==============================================================================
void AudioEngine::prepareTrack (Track& t)
{
    if (t.transport != nullptr)
        t.transport->prepareToPlay (currentBlockSize, currentSampleRate);
}

void AudioEngine::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    currentBlockSize  = samplesPerBlockExpected;
    currentSampleRate = sampleRate;

    scratch.setSize (8, juce::jmax (samplesPerBlockExpected, 1024), false, false, true);

    const juce::ScopedLock sl (lock);
    for (auto* t : tracks)
        prepareTrack (*t);
}

void AudioEngine::releaseResources()
{
    const juce::ScopedLock sl (lock);
    for (auto* t : tracks)
        if (t->transport != nullptr)
            t->transport->releaseResources();
}

void AudioEngine::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    bufferToFill.clearActiveBufferRegion();

    auto* out = bufferToFill.buffer;
    const int numOut = out->getNumChannels();
    const int start  = bufferToFill.startSample;
    const int num    = bufferToFill.numSamples;

    const juce::ScopedLock sl (lock);

    for (auto* t : tracks)
    {
        if (t->transport == nullptr || ! t->transport->isPlaying())
            continue;

        const int srcCh = juce::jmin (t->srcChannels, scratch.getNumChannels());

        // Build a non-owning view over the scratch buffer to read this track.
        float* chans[8];
        for (int c = 0; c < srcCh; ++c)
            chans[c] = scratch.getWritePointer (c);

        juce::AudioBuffer<float> view (chans, srcCh, num);
        juce::AudioSourceChannelInfo info (&view, 0, num);
        t->transport->getNextAudioBlock (info);

        // Route source channels into the chosen physical outputs, with gain.
        const auto& dst = t->outChannels;
        const int nDst  = (int) dst.size();
        const float g   = t->gain;

        if (nDst == 0)
            continue;

        auto addTo = [&] (int dstIdx, int srcIdx)
        {
            if (juce::isPositiveAndBelow (dstIdx, numOut))
                out->addFrom (dstIdx, start, view, srcIdx, 0, num, g);
        };

        if (srcCh == 1)
        {
            // mono file -> feed every chosen output
            for (int d : dst)
                addTo (d, 0);
        }
        else if (nDst == 1)
        {
            // stereo (or more) file -> single output: downmix to mono
            for (int s = 0; s < srcCh; ++s)
                if (juce::isPositiveAndBelow (dst[0], numOut))
                    out->addFrom (dst[0], start, view, s, 0, num, g / (float) srcCh);
        }
        else
        {
            // map one-to-one up to the smaller count
            const int n = juce::jmin (srcCh, nDst);
            for (int i = 0; i < n; ++i)
                addTo (dst[i], i);
        }
    }
}
