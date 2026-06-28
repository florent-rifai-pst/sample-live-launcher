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
void AudioEngine::loadSong (const Song& song, const juce::File& clickAccent, const juce::File& clickNormal)
{
    clear();
    setClick (song.click, clickAccent, clickNormal);
    for (auto& td : song.tracks)
    {
        const int idx = addTrack (juce::File (td.filePath));
        if (idx >= 0)
        {
            setTrackChannels (idx, td.channels);
            setTrackChannelGains (idx, td.channelGains);
            setTrackGain (idx, td.gain);
            setTrackMute (idx, td.mute);
            setTrackSolo (idx, td.solo);
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
    {
        t->outChannels = std::move (channels);
        t->outGains.resize (t->outChannels.size(), 1.0f);   // keep per-output gains aligned
    }
}

void AudioEngine::setTrackChannelGains (int index, std::vector<float> gains)
{
    const juce::ScopedLock sl (lock);
    if (auto* t = getTrack (index))
    {
        t->outGains = std::move (gains);
        t->outGains.resize (t->outChannels.size(), 1.0f);
    }
}

void AudioEngine::setTrackGain (int index, float gain)
{
    const juce::ScopedLock sl (lock);
    if (auto* t = getTrack (index))
        t->gain = gain;
}

void AudioEngine::setTrackMute (int index, bool mute)
{
    const juce::ScopedLock sl (lock);
    if (auto* t = getTrack (index))
        t->mute = mute;
}

void AudioEngine::setTrackSolo (int index, bool solo)
{
    const juce::ScopedLock sl (lock);
    if (auto* t = getTrack (index))
        t->solo = solo;
}

//==============================================================================
// Reads a click sample, mono-summed and capped to 2 s, into dest (loaded outside
// the audio lock so file IO never blocks the audio thread).
void AudioEngine::loadClickSample (const juce::File& f, juce::AudioBuffer<float>& dest, double& destRate)
{
    dest.setSize (0, 0);
    destRate = currentSampleRate;

    if (formatManager == nullptr || ! f.existsAsFile())
        return;

    std::unique_ptr<juce::AudioFormatReader> r (formatManager->createReaderFor (f));
    if (r == nullptr || r->lengthInSamples <= 0)
        return;

    const int len = (int) juce::jmin ((juce::int64) (currentSampleRate * 2.0), r->lengthInSamples);
    const int srcCh = juce::jmax (1, (int) r->numChannels);

    juce::AudioBuffer<float> tmp (srcCh, len);
    r->read (&tmp, 0, len, 0, true, true);

    dest.setSize (1, len);
    dest.clear();
    for (int ch = 0; ch < srcCh; ++ch)
        dest.addFrom (0, 0, tmp, ch, 0, len, 1.0f / (float) srcCh);   // downmix to mono

    destRate = r->sampleRate > 0.0 ? r->sampleRate : currentSampleRate;
}

// Procedurally generated default click (used when no sound bank file is set), so
// the default uses the exact same sample-playback path as a real bank.
void AudioEngine::makeDefaultClick (juce::AudioBuffer<float>& dest, bool accent)
{
    const double sr   = currentSampleRate > 0.0 ? currentSampleRate : 44100.0;
    const int    len  = juce::jmax (1, (int) (0.045 * sr));     // 45 ms
    const double freq = accent ? 1760.0 : 1175.0;
    const double inc  = juce::MathConstants<double>::twoPi * freq / sr;
    const float  amp  = accent ? 0.9f : 0.7f;

    dest.setSize (1, len);
    float* d = dest.getWritePointer (0);
    for (int i = 0; i < len; ++i)
    {
        const float env = std::exp (-5.0f * (float) i / (float) len);   // fast decay
        d[i] = (float) std::sin (inc * i) * env * amp;
    }
}

void AudioEngine::setClick (const ClickTrack& click, const juce::File& accentFile, const juce::File& normalFile)
{
    // Decode outside the lock.
    juce::AudioBuffer<float> aBuf, nBuf;
    double aRate = currentSampleRate, nRate = currentSampleRate;
    loadClickSample (accentFile, aBuf, aRate);
    loadClickSample (normalFile, nBuf, nRate);

    // Fall back to the built-in beep for any beat without a sample file.
    if (aBuf.getNumSamples() == 0) { makeDefaultClick (aBuf, true);  aRate = currentSampleRate; }
    if (nBuf.getNumSamples() == 0) { makeDefaultClick (nBuf, false); nRate = currentSampleRate; }

    const juce::ScopedLock sl (lock);
    clickConfig = click;

    clickAccentBuf  = std::move (aBuf);  clickAccentRate = aRate;
    clickNormalBuf  = std::move (nBuf);  clickNormalRate = nRate;
    clickUseSamples = true;   // buffers always populated now (bank files or default beep)

    rebuildClickSchedule();
    clickPlayhead       = 0;
    clickCursor         = 0;
    clickVoiceRemaining = 0;
    clickSampleBuf      = nullptr;
}

// Builds the list of beat onsets (in samples at the current sample rate) by
// laying the bricks end to end. BPM counts the denominator unit, so a beat is
// 60/bpm seconds and a bar is numerator beats.
void AudioEngine::rebuildClickSchedule()
{
    clickSchedule.clear();
    clickLengthSamples = 0;

    if (! clickConfig.enabled)
        return;

    auto segs = clickConfig.segments;
    std::sort (segs.begin(), segs.end(),
               [] (const ClickSegment& a, const ClickSegment& b) { return a.barFrom < b.barFrom; });

    double timeSamples = 0.0;
    for (auto& s : segs)
    {
        if (s.bpm <= 0.0 || s.numerator <= 0)
            continue;

        const int    bars       = juce::jmax (0, s.barTo - s.barFrom + 1);
        const double beatSamples = (60.0 / s.bpm) * currentSampleRate;

        for (int bar = 0; bar < bars; ++bar)
            for (int beat = 0; beat < s.numerator; ++beat)
            {
                clickSchedule.push_back ({ (juce::int64) llround (timeSamples), beat == 0 });
                timeSamples += beatSamples;
            }
    }

    clickLengthSamples = (juce::int64) llround (timeSamples);
}

void AudioEngine::triggerClickVoice (bool accent)
{
    clickVoiceRemaining = 0;       // silence the synth voice
    clickSampleBuf      = nullptr; // and the sample voice, then arm one of them

    if (clickUseSamples)
    {
        auto&        buf  = accent ? clickAccentBuf  : clickNormalBuf;
        const double rate = accent ? clickAccentRate : clickNormalRate;
        if (buf.getNumSamples() > 0)
        {
            clickSampleBuf  = &buf;
            clickSamplePos  = 0.0;
            clickSampleStep = (rate > 0.0 ? rate : currentSampleRate) / currentSampleRate;
            return;
        }
        // fall through to the synth if this particular sample is missing
    }

    clickVoiceLen       = juce::jmax (1, (int) (0.030 * currentSampleRate));   // 30 ms tone
    clickVoiceRemaining = clickVoiceLen;
    clickPhase          = 0.0;
    clickPhaseInc       = juce::MathConstants<double>::twoPi
                            * (accent ? 1760.0 : 1175.0) / currentSampleRate;
    clickVoiceAmp       = accent ? 0.9f : 0.6f;
}

//==============================================================================
// NOTE: start()/stop() are NOT called under `lock`. AudioTransportSource::stop()
// blocks until the audio thread has processed a block; if we held `lock` the audio
// thread would be stuck waiting for it in getNextAudioBlock -> deadlock + dropout.
// The tracks array is only mutated from the message thread (same thread as these),
// so iterating it here without the lock is safe.
void AudioEngine::playAll()
{
    // Starts from the current position (loadSong creates transports at 0, and the
    // play bar can seek beforehand), so all tracks stay synchronised.
    for (auto* t : tracks)
        t->transport->start();   // non-blocking; arms the transport for pumping

    const juce::ScopedLock sl (lock);   // flip the gate; the audio thread starts mixing
    playing      = true;
    clickPlaying = true;
}

// Pause/stop just lower the gate under the lock. We deliberately do NOT call
// AudioTransportSource::stop(): it busy-waits up to ~1s PER track on this thread
// (it spins until the audio callback flags it stopped, which never happens once
// we stop pumping it) -> long freeze + tracks dying one by one. Flipping the gate
// stops every track on the very next audio block, together and instantly.
void AudioEngine::pauseAll()
{
    const juce::ScopedLock sl (lock);
    playing      = false;       // transports keep their position
    clickPlaying = false;
}

void AudioEngine::stopAll()
{
    const juce::ScopedLock sl (lock);
    playing      = false;
    for (auto* t : tracks)
        if (t->transport != nullptr)
            t->transport->setPosition (0.0);   // non-blocking seek to start

    clickPlaying        = false;
    clickPlayhead       = 0;
    clickCursor         = 0;
    clickVoiceRemaining = 0;
}

void AudioEngine::setLoop (bool enabled, double startSeconds, double endSeconds)
{
    const juce::ScopedLock sl (lock);
    loopEnabled      = enabled;
    loopStartSamples = (juce::int64) llround (juce::jmax (0.0, startSeconds) * currentSampleRate);
    loopEndSamples   = (juce::int64) llround (juce::jmax (0.0, endSeconds)   * currentSampleRate);
}

bool AudioEngine::isPlaying() const
{
    const juce::ScopedLock sl (lock);
    if (clickPlaying)
        return true;
    if (! playing)
        return false;
    for (auto* t : tracks)                 // playing, but report false once all finish
        if (t->transport != nullptr && t->transport->isPlaying())
            return true;
    return false;
}

double AudioEngine::getLengthSeconds() const
{
    const juce::ScopedLock sl (lock);
    double len = currentSampleRate > 0.0 ? clickLengthSamples / currentSampleRate : 0.0;
    for (auto* t : tracks)
        if (t->transport != nullptr)
            len = juce::jmax (len, t->transport->getLengthInSeconds());
    return len;
}

double AudioEngine::getCurrentPositionSeconds() const
{
    const juce::ScopedLock sl (lock);
    for (auto* t : tracks)
        if (t->transport != nullptr)
            return t->transport->getCurrentPosition();

    // Click-only song: report the metronome's own playhead.
    return currentSampleRate > 0.0 ? clickPlayhead / currentSampleRate : 0.0;
}

void AudioEngine::setPositionSeconds (double seconds)
{
    const juce::ScopedLock sl (lock);
    seconds = juce::jmax (0.0, seconds);

    for (auto* t : tracks)
        if (t->transport != nullptr)
            t->transport->setPosition (seconds);

    clickPlayhead       = (juce::int64) llround (seconds * currentSampleRate);
    clickVoiceRemaining = 0;
    clickCursor         = 0;
    while (clickCursor < clickSchedule.size()
           && clickSchedule[clickCursor].sample < clickPlayhead)
        ++clickCursor;
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

float AudioEngine::getTrackLevel (int index) const
{
    const juce::ScopedLock sl (lock);
    return juce::isPositiveAndBelow (index, tracks.size()) ? tracks[index]->level : 0.0f;
}

int AudioEngine::getNumOutputChannels() const
{
    const juce::ScopedLock sl (lock);
    return currentNumOutputs;
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

    rebuildClickSchedule();   // beat times depend on the sample rate
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
    currentNumOutputs = numOut;   // track the device's real output channel count

    // Loop: if the playhead has reached the loop end, jump tracks + click back to
    // the loop start before mixing this block. Block-granular (not sample-accurate),
    // which is fine for a practice looper. Master clock = first transport, or the
    // click playhead for a click-only song.
    if (playing && loopEnabled && loopEndSamples > loopStartSamples)
    {
        juce::int64 masterPos = clickPlayhead;
        for (auto* t : tracks)
            if (t->transport != nullptr)
                { masterPos = (juce::int64) llround (t->transport->getCurrentPosition() * currentSampleRate); break; }

        if (masterPos >= loopEndSamples)
        {
            const double startSec = loopStartSamples / currentSampleRate;
            for (auto* t : tracks)
                if (t->transport != nullptr)
                    t->transport->setPosition (startSec);

            clickPlayhead       = loopStartSamples;
            clickVoiceRemaining = 0;
            clickSampleBuf      = nullptr;
            clickCursor         = 0;
            while (clickCursor < clickSchedule.size()
                   && clickSchedule[clickCursor].sample < clickPlayhead)
                ++clickCursor;
        }
    }

    // Solo overrides mute: if any track is soloed, only soloed tracks are heard.
    bool anySolo = false;
    for (auto* t : tracks)
        if (t->solo)
            { anySolo = true; break; }

    for (auto* t : tracks)
    {
        t->level = 0.0f;   // silent unless we actually route it below

        if (! playing || t->transport == nullptr || ! t->transport->isPlaying())
            continue;

        const int srcCh = juce::jmin (t->srcChannels, scratch.getNumChannels());

        // Build a non-owning view over the scratch buffer to read this track.
        float* chans[8];
        for (int c = 0; c < srcCh; ++c)
            chans[c] = scratch.getWritePointer (c);

        juce::AudioBuffer<float> view (chans, srcCh, num);
        juce::AudioSourceChannelInfo info (&view, 0, num);
        t->transport->getNextAudioBlock (info);   // always pump, so muted tracks stay in sync

        // Muted (or not soloed while another track is soloed): advanced above but
        // not routed to the output.
        if (anySolo ? ! t->solo : t->mute)
            continue;

        t->level = view.getMagnitude (0, num) * t->gain;   // post-gain peak for the meter

        // Route source channels into the chosen physical outputs, applying the
        // global gain and the per-output gain for that destination.
        const auto& dst   = t->outChannels;
        const auto& oGain = t->outGains;
        const int nDst    = (int) dst.size();
        const float g     = t->gain;

        if (nDst == 0)
            continue;

        auto gainAt = [&] (int pos) -> float
        {
            return g * (pos < (int) oGain.size() ? oGain[(size_t) pos] : 1.0f);
        };

        // dstPos = index into dst/oGain; srcIdx = source channel feeding it.
        auto addTo = [&] (int dstPos, int srcIdx)
        {
            const int dstIdx = dst[(size_t) dstPos];
            if (juce::isPositiveAndBelow (dstIdx, numOut))
                out->addFrom (dstIdx, start, view, srcIdx, 0, num, gainAt (dstPos));
        };

        if (srcCh == 1)
        {
            // mono file -> feed every chosen output
            for (int i = 0; i < nDst; ++i)
                addTo (i, 0);
        }
        else if (nDst == 1)
        {
            // stereo (or more) file -> single output: downmix to mono
            for (int s = 0; s < srcCh; ++s)
                if (juce::isPositiveAndBelow (dst[0], numOut))
                    out->addFrom (dst[0], start, view, s, 0, num, gainAt (0) / (float) srcCh);
        }
        else
        {
            // map one-to-one up to the smaller count
            const int n = juce::jmin (srcCh, nDst);
            for (int i = 0; i < n; ++i)
                addTo (i, i);
        }
    }

    //== synthesized click ====================================================
    if (clickPlaying && clickConfig.enabled)
    {
        const auto& dst = clickConfig.channels;
        const float g   = clickConfig.gain;

        for (int i = 0; i < num; ++i)
        {
            const juce::int64 pos = clickPlayhead + i;

            // Fire any beats due at or before this sample.
            while (clickCursor < clickSchedule.size()
                   && clickSchedule[clickCursor].sample <= pos)
            {
                triggerClickVoice (clickSchedule[clickCursor].accent);
                ++clickCursor;
            }

            float s = 0.0f;

            if (clickSampleBuf != nullptr)
            {
                // Linear-interpolated playback of the bank sample, rate-converted.
                const int   n  = clickSampleBuf->getNumSamples();
                const int   i0 = (int) clickSamplePos;
                if (i0 < n - 1)
                {
                    const float frac = (float) (clickSamplePos - i0);
                    const float* d   = clickSampleBuf->getReadPointer (0);
                    s = (d[i0] + frac * (d[i0 + 1] - d[i0])) * g;
                    clickSamplePos += clickSampleStep;
                }
                else
                {
                    clickSampleBuf = nullptr;   // reached the end
                }
            }
            else if (clickVoiceRemaining > 0)
            {
                const float env = clickVoiceAmp * ((float) clickVoiceRemaining / (float) clickVoiceLen);
                s = (float) std::sin (clickPhase) * env * g;
                clickPhase += clickPhaseInc;
                --clickVoiceRemaining;
            }

            if (s != 0.0f)
                for (int d : dst)
                    if (juce::isPositiveAndBelow (d, numOut))
                        out->addSample (d, start + i, s);
        }

        clickPlayhead += num;

        // Stop the metronome once its schedule is exhausted and no file track is
        // still playing (otherwise let it idle so it resumes correctly on seek).
        if (clickCursor >= clickSchedule.size() && clickVoiceRemaining <= 0 && clickSampleBuf == nullptr)
        {
            bool trackStillPlaying = false;
            for (auto* t : tracks)
                if (t->transport != nullptr && t->transport->isPlaying())
                    { trackStillPlaying = true; break; }

            if (! trackStillPlaying)
                clickPlaying = false;
        }
    }
}
