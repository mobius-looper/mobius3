/**
 * Early prototype of tree and form definitions for the UI.
 *
 * Used initially for the Session editor, expected to be generalized.
 *
 * Combining these for now since they go together and might be interesting
 * visually as well trees that expand to have forms IN them rather than controlling
 * a side panel containing the form?
 *
 * The definition of a form is inflexible but concise and works well enough for the
 * immediate purpose.
 *
 * A form essentially is a labeled list of fields with the fields
 * being editing components for parameter Symbols.a
 *
 * In a TreeForm definition the symbols to include are specified as
 * an array symbol names.  This is represented in XML as a CSV.
 *
 * Within this array may appear special tokens to indiciate that a spacer
 * or section label should be added.
 *
 * The form may also be given a title which would be displayed more prominantly
 * than a section label.
 *
 * The definition of a tree is just a tree of "nodes" with each node having a name.
 * Nodes may have an array of symbols to generate a form.  Or it may reference
 * a Form definition by name.
 *
 * Keep It Simple Stupid
 */

#pragma once

#include <JuceHeader.h>

class TreeNode
{
  public:
    
    juce::String name;
    juce::String formName;
    juce::StringArray symbols;
    
    juce::OwnedArray<TreeNode> nodes;
    std::unique_ptr<class TreeForm> form;

    void parseXml(juce::XmlElement* root, juce::StringArray& errors);
};

class TreeForm
{
  public:

    // special names that can be injected into the symbol name list
    // to insert spacers and section labels
    // these need a prefix that won't conflict with symbol names that include package
    // prefixes
    constexpr static const char* Spacer = "*spacer*";
    constexpr static const char* Section = "*section*";

    juce::String name;
    juce::String title;
    juce::StringArray symbols;
    juce::String suppressPrefix;
    
    void parseXml(juce::XmlElement* root, juce::StringArray& errors);
};



