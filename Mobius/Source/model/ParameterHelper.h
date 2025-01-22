/**
 * Refactoring of some logic for dealing with Structure parameters
 * that used to be buried in UIParameter.
 *
 * This is hacky and not general, but there aren't many of these and it's
 * not worth it.
 */

#pragma once

#include <JuceHeader.h>

class ParameterHelper
{
  public:

    static void getStructureNames(class Provider* p, class Symbol* s, juce::StringArray& result);
    static juce::String getStructureName(class Provider* p, class Symbol* s, int ordinal);
    static int getParameterMax(class Provider* p, class Symbol* s);
    
};
