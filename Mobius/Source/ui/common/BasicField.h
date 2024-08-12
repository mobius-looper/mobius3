/**
 * Redesign of the original Field making it simpler and easier to deal with.
 * For use inside BasicForm
 *
 * not finished yet...
 */

#pragma once

#include <JuceHeader.h>

class BasicField : public juce::Component
{
  public:

    BasicField(juce::String label);
    ~BasicField();
    
    void setLabelCharWidth(int chars);
    void setLabelColor(juce::Colour c);
    void setLabelRightJustify(bool b);
    
  private:
    
    juce::Label label;
    int labelCharWidth = 0;
};
