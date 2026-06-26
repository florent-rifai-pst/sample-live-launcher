#pragma once

#include <JuceHeader.h>

//==============================================================================
// JUCE's String(const char*) decodes bytes as Latin-1 (CharPointer_ASCII), so
// accented UTF-8 literals come out as mojibake. Source is compiled with /utf-8,
// so the literal bytes ARE valid UTF-8 — we just need to tell JUCE to decode
// them as such. Use this user-defined literal for any user-facing string that
// contains non-ASCII characters:   "Réglages"_t
inline juce::String operator""_t (const char* utf8, size_t)
{
    return juce::String (juce::CharPointer_UTF8 (utf8));
}
