#include "Models.h"

//==============================================================================
namespace
{
    juce::var trackToVar (const TrackData& t)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("name", t.name);
        o->setProperty ("file", t.filePath);

        juce::Array<juce::var> ch;
        for (int c : t.channels)
            ch.add (c);
        o->setProperty ("channels", ch);

        o->setProperty ("gain", t.gain);
        o->setProperty ("mute", t.mute);
        o->setProperty ("solo", t.solo);
        o->setProperty ("duration", t.durationSeconds);
        return juce::var (o);
    }

    juce::var clickSegmentToVar (const ClickSegment& s)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("barFrom", s.barFrom);
        o->setProperty ("barTo",   s.barTo);
        o->setProperty ("num",     s.numerator);
        o->setProperty ("den",     s.denominator);
        o->setProperty ("bpm",     s.bpm);
        return juce::var (o);
    }

    ClickSegment clickSegmentFromVar (const juce::var& v)
    {
        ClickSegment s;
        s.barFrom     = (int)    v.getProperty ("barFrom", 1);
        s.barTo       = (int)    v.getProperty ("barTo",   8);
        s.numerator   = (int)    v.getProperty ("num",     4);
        s.denominator = (int)    v.getProperty ("den",     4);
        s.bpm         = (double) v.getProperty ("bpm",     120.0);
        return s;
    }

    juce::var clickToVar (const ClickTrack& c)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("enabled", c.enabled);
        o->setProperty ("gain",    c.gain);

        juce::Array<juce::var> ch;
        for (int x : c.channels)
            ch.add (x);
        o->setProperty ("channels", ch);

        juce::Array<juce::var> segs;
        for (auto& s : c.segments)
            segs.add (clickSegmentToVar (s));
        o->setProperty ("segments", segs);

        o->setProperty ("soundBankId", c.soundBankId);
        return juce::var (o);
    }

    ClickTrack clickFromVar (const juce::var& v)
    {
        ClickTrack c;
        c.enabled = (bool)  v.getProperty ("enabled", false);
        c.gain    = (float) (double) v.getProperty ("gain", 1.0);
        c.soundBankId = v.getProperty ("soundBankId", "").toString();

        c.channels.clear();
        if (auto* arr = v.getProperty ("channels", juce::var()).getArray())
            for (auto& x : *arr)
                c.channels.push_back ((int) x);
        if (c.channels.empty())
            c.channels = { 0, 1 };

        if (auto* arr = v.getProperty ("segments", juce::var()).getArray())
            for (auto& s : *arr)
                c.segments.push_back (clickSegmentFromVar (s));

        return c;
    }

    TrackData trackFromVar (const juce::var& v)
    {
        TrackData t;
        t.name     = v.getProperty ("name", "").toString();
        t.filePath = v.getProperty ("file", "").toString();
        t.gain     = (float) (double) v.getProperty ("gain", 1.0);
        t.mute     = (bool) v.getProperty ("mute", false);
        t.solo     = (bool) v.getProperty ("solo", false);
        t.durationSeconds = (double) v.getProperty ("duration", 0.0);

        t.channels.clear();
        if (auto* arr = v.getProperty ("channels", juce::var()).getArray())
            for (auto& c : *arr)
                t.channels.push_back ((int) c);

        if (t.channels.empty())
            t.channels = { 0, 1 };

        return t;
    }

    juce::var songToVar (const Song& s)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("id", s.id);
        o->setProperty ("name", s.name);
        o->setProperty ("click", clickToVar (s.click));

        juce::Array<juce::var> tr;
        for (auto& t : s.tracks)
            tr.add (trackToVar (t));
        o->setProperty ("tracks", tr);

        return juce::var (o);
    }

    Song songFromVar (const juce::var& v)
    {
        Song s;
        s.id   = v.getProperty ("id", juce::Uuid().toString()).toString();
        s.name = v.getProperty ("name", "Morceau").toString();

        if (v.hasProperty ("click"))
            s.click = clickFromVar (v.getProperty ("click", juce::var()));

        if (auto* arr = v.getProperty ("tracks", juce::var()).getArray())
            for (auto& t : *arr)
                s.tracks.push_back (trackFromVar (t));

        return s;
    }

    juce::var setlistToVar (const Setlist& s)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("id", s.id);
        o->setProperty ("name", s.name);

        juce::Array<juce::var> ids;
        for (auto& id : s.songIds)
            ids.add (id);
        o->setProperty ("songIds", ids);

        return juce::var (o);
    }

    juce::var clickBankToVar (const ClickSoundBank& b)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("id",     b.id);
        o->setProperty ("name",   b.name);
        o->setProperty ("accent", b.accentFile);
        o->setProperty ("normal", b.normalFile);
        return juce::var (o);
    }

    ClickSoundBank clickBankFromVar (const juce::var& v)
    {
        ClickSoundBank b;
        b.id         = v.getProperty ("id", juce::Uuid().toString()).toString();
        b.name       = v.getProperty ("name", "Banque").toString();
        b.accentFile = v.getProperty ("accent", "").toString();
        b.normalFile = v.getProperty ("normal", "").toString();
        return b;
    }

    Setlist setlistFromVar (const juce::var& v)
    {
        Setlist s;
        s.id   = v.getProperty ("id", juce::Uuid().toString()).toString();
        s.name = v.getProperty ("name", "Setlist").toString();

        if (auto* arr = v.getProperty ("songIds", juce::var()).getArray())
            for (auto& id : *arr)
                s.songIds.add (id.toString());

        return s;
    }
}

//==============================================================================
juce::File Library::defaultFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("SampleLiveLauncher")
               .getChildFile ("library.json");
}

void Library::load()
{
    songs.clear();
    setlists.clear();
    clickBanks.clear();

    auto file = defaultFile();
    if (! file.existsAsFile())
        return;

    auto parsed = juce::JSON::parse (file);
    if (! parsed.isObject())
        return;

    if (auto* arr = parsed.getProperty ("songs", juce::var()).getArray())
        for (auto& s : *arr)
            songs.push_back (songFromVar (s));

    if (auto* arr = parsed.getProperty ("setlists", juce::var()).getArray())
        for (auto& s : *arr)
            setlists.push_back (setlistFromVar (s));

    if (auto* arr = parsed.getProperty ("clickBanks", juce::var()).getArray())
        for (auto& b : *arr)
            clickBanks.push_back (clickBankFromVar (b));
}

void Library::save() const
{
    auto* root = new juce::DynamicObject();

    juce::Array<juce::var> s;
    for (auto& song : songs)
        s.add (songToVar (song));
    root->setProperty ("songs", s);

    juce::Array<juce::var> sl;
    for (auto& list : setlists)
        sl.add (setlistToVar (list));
    root->setProperty ("setlists", sl);

    juce::Array<juce::var> cb;
    for (auto& b : clickBanks)
        cb.add (clickBankToVar (b));
    root->setProperty ("clickBanks", cb);

    auto file = defaultFile();
    file.getParentDirectory().createDirectory();
    file.replaceWithText (juce::JSON::toString (juce::var (root)));
}

Song* Library::findSong (const juce::String& id)
{
    for (auto& s : songs)
        if (s.id == id)
            return &s;
    return nullptr;
}

Setlist* Library::findSetlist (const juce::String& id)
{
    for (auto& s : setlists)
        if (s.id == id)
            return &s;
    return nullptr;
}

ClickSoundBank* Library::findClickBank (const juce::String& id)
{
    for (auto& b : clickBanks)
        if (b.id == id)
            return &b;
    return nullptr;
}

//==============================================================================
double fileDurationSeconds (const juce::String& path)
{
    if (path.isEmpty())
        return 0.0;

    juce::File f (path);
    if (! f.existsAsFile())
        return 0.0;

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (f));
    if (reader != nullptr && reader->sampleRate > 0.0)
        return (double) reader->lengthInSamples / reader->sampleRate;

    return 0.0;
}

double clickTrackDurationSeconds (const ClickTrack& click)
{
    if (! click.enabled)
        return 0.0;

    double total = 0.0;
    for (auto& s : click.segments)
    {
        const int bars = juce::jmax (0, s.barTo - s.barFrom + 1);
        if (s.bpm <= 0.0)
            continue;
        const double beat = 60.0 / s.bpm;                 // denominator unit
        total += bars * s.numerator * beat;
    }
    return total;
}

double songDurationSeconds (Song& song)
{
    double longest = clickTrackDurationSeconds (song.click);
    for (auto& t : song.tracks)
    {
        if (t.durationSeconds <= 0.0)            // fill cache lazily for old data
            t.durationSeconds = fileDurationSeconds (t.filePath);
        longest = juce::jmax (longest, t.durationSeconds);
    }
    return longest;
}

double setlistDurationSeconds (Library& library, const Setlist& setlist)
{
    double total = 0.0;
    for (auto& id : setlist.songIds)
        if (auto* s = library.findSong (id))
            total += songDurationSeconds (*s);
    return total;
}

juce::String formatDuration (double seconds)
{
    if (seconds <= 0.0)
        return "--:--";

    const int total = (int) std::round (seconds);
    return juce::String (total / 60) + ":" + juce::String (total % 60).paddedLeft ('0', 2);
}
