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
        return juce::var (o);
    }

    TrackData trackFromVar (const juce::var& v)
    {
        TrackData t;
        t.name     = v.getProperty ("name", "").toString();
        t.filePath = v.getProperty ("file", "").toString();
        t.gain     = (float) (double) v.getProperty ("gain", 1.0);

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
