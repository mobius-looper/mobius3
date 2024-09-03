/**
 * Component displaying details of a script file.
 * Used by both the details popup in the library table and the
 * editor window.
 */

#pragma once

#include <JuceHeader.h>

class ScriptDetails : public juce::Component
{
  public:

    ScriptDetails();
    ~ScriptDetails();

    void resized() override;
    void paint(juce::Graphics& g) override;

    void load(class ScriptRegistry::File* file);

  private:

    class ScriptRegistry::File* regfile = nullptr;
    
    void paintDetail(juce::Graphics& g, juce::Rectangle<int>& area,
                     juce::String label, juce::String text);

    void paintError(juce::Graphics& g, juce::Rectangle<int>& area,
                    class MslError* error);
    
    void paintCollision(juce::Graphics& g, juce::Rectangle<int>& area,
                        class MslCollision* col);

};
