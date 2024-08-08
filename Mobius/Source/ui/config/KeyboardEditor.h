/**
 * Panel to edit keyboard bindings
 */

#pragma once

#include <JuceHeader.h>

#include "../../KeyTracker.h"
#include "../common/SimpleTable.h"
#include "../common/ButtonBar.h"
#include "../common/Field.h"

#include "BindingEditor.h"

class KeyboardEditor : public BindingEditor,
                       public juce::KeyListener,
                       public KeyTracker::Listener
{
  public:
    KeyboardEditor(class Supervisor* s);
    ~KeyboardEditor();

    juce::String getTitle() override {return "Keyboard Bindings";}

    void showing() override;
    void hiding() override;

    juce::String renderSubclassTrigger(Binding* b) override;
    bool isRelevant(class Binding* b) override;
    void addSubclassFields() override;
    bool wantsCapture() override {return true;} 
    void refreshSubclassFields(class Binding* b) override;
    void captureSubclassFields(class Binding* b) override;
    void resetSubclassFields() override;

    bool keyPressed(const juce::KeyPress& key, juce::Component* originator) override;
    bool keyStateChanged(bool isKeyDown, juce::Component* originator) override;

    void keyTrackerDown(int code, int modifiers) override;
    void keyTrackerUp(int code, int modifiers) override;
    
  private:

    Field* key = nullptr;

    int capturedCode = 0;
    
};
