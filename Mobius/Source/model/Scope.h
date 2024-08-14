/**
 * Scopes are a large mess right now with several ways of representing them.
 *
 * Historically Binding has kept these as a string where "1" is the first track
 * and "A" is the first group.
 *
 * DisplayButton does the same, except it uses a juce::String rather than a char array.
 *
 * Bindging parses the scope string into two values: trackNumber and groupOrdinal, both
 * integers with a 1 base.  When both numbers are zero, the binding has global scope.
 *
 * UIAction only saves the two numbers, not the string.
 *
 * When working backward from a UIAction to a DisplayButton we have to re-render
 * the two numbers.
 *
 * Scope REALLY needs redesign to either just consistently be a positive or negative
 * integer, or consistently be a string so we don't need these conversions.  It's also
 * kind of a pita when cloning or comparing UIActions to have to deal with two numbers.
 *
 * UPDATE: This has been simplified at the UI level to consistently represent scopes as
 * strings and only convert them to track/group numbers down in core.  We still need
 * basic utilities to do the parsing though, so here they are.
 */

#pragma once

class Scope
{
  public:

    static int getScopeTrack(const char* scope);
    
    // old, remove
#if 0    
    /**
     * Parse a scope string from a character buffer into the two numbers.
     */
    static void parse(const char* buf, int* track, int* group);

    /**
     * Render the two numbers into a char buffer.
     */
    static void render(int track, int group, char* buf, int len);

    /**
     * Render the two numbers as a String
     */
    static juce::String render(int track, int group);
#endif


};
    
