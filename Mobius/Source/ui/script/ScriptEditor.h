/**
 * This is effectively the root of the script editor UI, though it is contained
 * under two layers:
 *
 *     ScriptWindow which is a DocumentWindow
 *     ScriptWindowContent which is the single main component for the DocumentWindow
 *
 */

#pragma once

class ScriptEditor : public juce::Component
{
  public:

    ScriptEditor();
    ~ScriptEditor();

    void resized() override;

    void load(class ScriptRegistry::File* file);
    
  private:
    
    ScriptDetails details;
    juce::TextEditor editor;
    
};

