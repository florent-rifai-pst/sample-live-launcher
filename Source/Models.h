#pragma once

#include <JuceHeader.h>

//==============================================================================
// Plain data model for the library. No audio objects here — the AudioEngine
// turns a Song into playing tracks. Persisted as JSON in the user data folder.

struct TrackData
{
    juce::String     name;
    juce::String     filePath;
    std::vector<int> channels { 0, 1 };   // 0-based physical output indices
    float            gain = 1.0f;
};

struct Song
{
    juce::String           id   { juce::Uuid().toString() };
    juce::String           name { "Nouveau morceau" };
    std::vector<TrackData> tracks;
};

struct Setlist
{
    juce::String     id   { juce::Uuid().toString() };
    juce::String     name { "Nouvelle setlist" };
    juce::StringArray songIds;             // ordered references to Song::id
};

//==============================================================================
class Library
{
public:
    std::vector<Song>    songs;
    std::vector<Setlist> setlists;

    static juce::File defaultFile();

    void load();
    void save() const;

    Song*    findSong (const juce::String& id);
    Setlist* findSetlist (const juce::String& id);
};
