/**
 * Model for track groups.
 */

#pragma once

#include <JuceHeader.h>

class GroupDefinition
{
  public:

    GroupDefinition();
    GroupDefinition(GroupDefinition* src);
    ~GroupDefinition();

    /**
     * User defined display name.
     */
    juce::String name;

    /**
     * User defined color.
     */
    int color = 0;

    /**
     * True if function replication is enabled.
     */
    bool replicationEnabled = false;

    /**
     * Functions to auto-replicate to other group members.
     */
    juce::StringArray replicatedFunctions;

    /**
     * Parameters to auto-replicate to other group members.
     */
    juce::StringArray replicatedParameters;

    /**
     * Internal ordinal - auto-assigned
     */
    int ordinal = 0;

    /**
     * Utility to generate a group letter name from an ordinal.
     * The need for this should gradually fade as we start using
     * GroupDefinition.name everywhere.
     */
    static juce::String getInternalName(int ordinal) {
        // passing a char to the String constructor didn't work,
        // it rendered the numeric value of the character
        // operator += works for some reason
        // juce::String((char)('A' + i)));
        juce::String letter;
        letter += (char)('A' + ordinal);
        return letter;
    }

};
