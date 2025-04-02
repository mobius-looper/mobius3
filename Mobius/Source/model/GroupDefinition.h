/**
 * Model for track groups.
 */

#pragma once

#include <JuceHeader.h>

class GroupDefinition
{
  public:

    constexpr static const char* XmlName = "GroupDefinition";
    
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

class GroupDefinitions
{
  public:

    constexpr static const char* XmlName = "GroupDefinitions";

    GroupDefinitions() {}
    GroupDefinitions(GroupDefinitions* src);
    
    juce::OwnedArray<GroupDefinition> groups;

    void add(GroupDefinition* g);
    
    void toXml(juce::XmlElement* root);
    void parseXml(juce::XmlElement* root, juce::StringArray& errors);

    int getGroupIndex(juce::String name);
    GroupDefinition* find(juce::String name);
    GroupDefinition* getGroupByIndex(int index);
    void getGroupNames(juce::StringArray& names);
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
