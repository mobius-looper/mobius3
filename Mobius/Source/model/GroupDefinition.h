/**
 * Model for track groups.
 */

#pragma once

#include <JuceHeader.h>

class GroupDefinition
{
  public:

    GroupDefinition();
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
     * Functions to auto-replicate to other group members.
     */
    juce::StringArray replicatedFunctions;

    /**
     * Internal ordinal - auto-assigned
     */
    int ordinal = 0;

};
