/**
 * The ButtonsEditor is just like the BindingEditor except that
 * it translates the ButtonSets and DisplayButtons from the UIConfig
 * to make them look like BindingSets and Bindings
 */

#pragma once

#include <JuceHeader.h>

#include "BindingEditor.h"

class ButtonsEditor : public BindingEditor
{
  public:

    ButtonsEditor(class Supervisor* s);

    juce::String getTitle() override {return "Buttons";}
    
    void load() override;
    void save() override;

  private:
    
    class BindingSets* convert(class UIConfig* uicon);
    void unconvert(class BindingSets* src, juce::Array<class ButtonSet*>& dest);

};
