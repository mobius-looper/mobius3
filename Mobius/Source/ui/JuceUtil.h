/**
 * Random debugging shit to figure out Juce.
 */

#pragma once

#include <JuceHeader.h>

class JuceUtil
{
  public:

    static juce::Font getFont(int height);
    static juce::Font getFont(int height, int styleFlags);
    static juce::Font getFontf(float height);
    static juce::Font getFontf(float height, int styleFlags);

    static juce::String getComponentClassName(juce::Component* c);
    static void dumpComponent(juce::Component* c, int indent = 0);
    static void dumpComponent(const char* title, juce::Component* c, int indent = 0);

    /**
     * Convert a String containing a CSV into a StringArray
     */
    static void CsvToArray(juce::String csv, juce::StringArray& values);
    static juce::String ArrayToCsv(juce::StringArray& values);

    // centering

    static void center(juce::Component* c);

    static int getCenterLeft(juce::Component* container, juce::Component* c);
    static int getCenterTop(juce::Component* container, juce::Component* c);
    static void centerInParent(juce::Component* c);

    // popup windows

    static juce::Rectangle<int> getDisplayArea();
    
};
