
#pragma once

#include <JuceHeader.h>
#include "ScriptDetails.h"

class ScriptWindowContent : public juce::Component
{
  public:

    ScriptWindowContent();
    ~ScriptWindowContent();

    void resized() override;

    void load(class ScriptRegistry::File* file);
    
  private:
    
    ScriptDetails details;
    juce::TextEditor editor;
    
};

class ScriptWindow : public juce::DocumentWindow
{
  public:
    ScriptWindow();
    ~ScriptWindow();
    //void resized() override;

    void closeButtonPressed();
    void load(class ScriptRegistry::File* file);

  private:

    ScriptWindowContent content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScriptWindow)
};

