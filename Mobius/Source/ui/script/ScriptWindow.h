 /**
 * ScriptWindow is a DocumentWindow which works differently than other components.
 * DocumentWindow is a component but you don't just addAndMakeVisible on it.
 * It is supposed to have a single content component set with setContentOwned()
 * and THAT is what you treat as the container for the interesting stuff.
 *
 * Sadly the rest of the system only wants to interact with ScriptEditor so
 * we have to forward the calls along.  Ugly.  
 *
 */
#pragma once

#include <JuceHeader.h>
#include "ScriptEditor.h"

/**
 * DocumentWindows are supposed to have a single content component
 * where you do all the work.
 */
class ScriptWindowContent : public juce::Component
{
  public:

    ScriptWindowContent(class Supervisor* s);
    ~ScriptWindowContent();

    void resized() override;

    void load(class ScriptRegistry::File* file);
    void newScript();
    
  private:

    ScriptEditor editor;
};

class ScriptWindow : public juce::DocumentWindow
{
  public:
    ScriptWindow(class Supervisor* s);
    ~ScriptWindow();
    //void resized() override;

    void closeButtonPressed();
    void load(class ScriptRegistry::File* file);
    void newScript();
    
  private:
    
    class Supervisor* supervisor = nullptr;
    ScriptWindowContent content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScriptWindow)
};

