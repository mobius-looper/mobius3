/**
 * Utility class to provide basic services around track groups.
 *
 * Mostly this is hiding MobiusConfig during the transition period.
 * It does NOT provide editing of groups.
 *
 */

#pragma once

#include <JuceHeader.h>

class Grouper
{
  public:

    Grouper(class Provider* p);
    ~Grouper();

    void getGroupNames(juce::StringArray& names);
    
    int getGroupOrdinal(juce::String name);
    
  private:

    class Provider* provider = nullptr;

};
