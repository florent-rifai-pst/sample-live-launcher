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
    bool             mute = false;
    bool             solo = false;
    double           durationSeconds = 0.0;   // cached length of the audio file
};

// One brick of the click track: bars [barFrom..barTo] (1-based, inclusive) play
// at this time signature and tempo. BPM counts the denominator unit, so 6/8 @120
// fires 6 clicks per bar. Bricks are composed from bar 1 upward.
struct ClickSegment
{
    int    barFrom     = 1;
    int    barTo       = 8;
    int    numerator   = 4;     // beats per bar
    int    denominator = 4;     // beat unit (4 = quarter, 8 = eighth, ...)
    double bpm         = 120.0;
};

// A user-defined pair of click samples: one for the strong beat (temps fort),
// one for the weak beat (temps faible), plus a label. Lives in the Library and
// can be picked per song. Empty paths fall back to the synthesized beep.
struct ClickSoundBank
{
    juce::String id   { juce::Uuid().toString() };
    juce::String name { "Banque" };       // libellé
    juce::String accentFile;              // temps fort
    juce::String normalFile;              // temps faible
};

// Synthesized metronome that always sits in first position of a song. Beat 1 of
// each bar is accented. Routed to physical outputs like a normal track.
struct ClickTrack
{
    bool                      enabled = false;
    std::vector<ClickSegment> segments;
    std::vector<int>          channels { 0, 1 };   // 0-based physical output indices
    float                     gain = 1.0f;
    juce::String              soundBankId;          // "" = default synthesized beep
};

struct Song
{
    juce::String           id   { juce::Uuid().toString() };
    juce::String           name { "Nouveau morceau" };
    ClickTrack             click;
    std::vector<TrackData> tracks;
};

// Total length of a click track in seconds (sum of all bricks' bar durations).
double clickTrackDurationSeconds (const ClickTrack& click);

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
    std::vector<Song>           songs;
    std::vector<Setlist>        setlists;
    std::vector<ClickSoundBank> clickBanks;

    static juce::File defaultFile();

    void load();
    void save() const;

    Song*           findSong (const juce::String& id);
    Setlist*        findSetlist (const juce::String& id);
    ClickSoundBank* findClickBank (const juce::String& id);
};

//==============================================================================
// Duration helpers. fileDurationSeconds reads the audio header; song/setlist
// helpers fill TrackData::durationSeconds as a cache when it is missing.
double       fileDurationSeconds (const juce::String& path);
double       songDurationSeconds (Song& song);                          // = longest track
double       setlistDurationSeconds (Library& library, const Setlist&); // = sum of songs
juce::String formatDuration (double seconds);                           // "m:ss" or "--:--"
