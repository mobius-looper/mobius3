/**
 * Utilities to deal with scope strings.
 */

#include <JuceHeader.h>

#include "../util/Trace.h"
#include "GroupDefinition.h"
#include "Scope.h"

//////////////////////////////////////////////////////////////////////
//
// Scope
//
//////////////////////////////////////////////////////////////////////

/**
 * Parse a scope string to a track number.
 * There can be at most 2 digits and if empty it is 0 meaning the active track.
 * Anything else is considered a group name.
 *
 * Not doing space trimming, which I suppose we could.
 */
int Scope::parseTrackNumber(const char* scope)
{
    int trackNumber = -1;

    if (scope == nullptr) {
        // shouldn't be happening, but treat as unspecified
        trackNumber = 0;
    }
    else {
        size_t len = strlen(scope);
        if (len == 0) {
            // unspecified means active track
            trackNumber = 0;
        }
        else if (len < 3) {
            if (juce::CharacterFunctions::isDigit(scope[0]) &&
                (len == 1 || juce::CharacterFunctions::isDigit(scope[1]))) {
                trackNumber = atoi(scope);
            }
        }
    }
    return trackNumber;
}

//////////////////////////////////////////////////////////////////////
//
// Obsolete
//
//////////////////////////////////////////////////////////////////////

// old utilities from when we parsed them in the UI, remove when ready

#if 0
/**
 * Implementation of this is old and brouhgt over form Binding.
 * Tracks are expected to be identified with integers starting
 * from 1.  Groups are identified with upper case letters A-Z.
 */
void Scope::parse(const char* buf, int* trackptr, int* groupptr)
{
    int track = 0;
    int group = 0;

    if (buf != nullptr) {
        size_t len = strlen(buf);
        if (len > 1) {
            // must be a number 
            track = atoi(buf);
        }
        else if (len == 1) {
            char ch = buf[0];
            if (ch >= 'A') {
                group = (ch - 'A') + 1;
            }
            else {
                // normally an integer, anything else
                // collapses to zero
                track = atoi(buf);
            }
        }
    }

    if (trackptr != nullptr) *trackptr = track;
    if (groupptr != nullptr) *groupptr = group;
}

void Scope::render(int track, int group, char* buf, int len)
{
    if (track > 0) {
        snprintf(buf, len, "%d", track);
    }
    else if (group > 0) {
        snprintf(buf, len, "%c", (char)('A' + (group - 1)));
    }
    else {
        strcpy(buf, "");
    }
}

juce::String Scope::render(int track, int group)
{
    juce::String scope;

    if (track > 0) {
        scope = juce::String(track);
    }
    else if (group > 0) {
        scope = juce::String((char)('A' + (group - 1)));
    }

    return scope;
}
#endif
