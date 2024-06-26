/**
 * Component model for Mobius configuration forms.
 * 
 * A form consists of a list of FormPanels.  If there is more than
 * one FormPanel a tabbed component is added to select the visible panel.
 */

#pragma once

#include <JuceHeader.h>

#include "Field.h"
#include "FormPanel.h"

class Form : public juce::Component
{
  public:

    Form();
    ~Form();

    void setHelpArea(class HelpArea* ha) {
        helpArea = ha;
    }

    void add(FormPanel* panel);
    FormPanel* getPanel(juce::String name);

    void addTab(juce::String name, juce::Component* c);

    // will want more options here
    void add(Field* f, const char* tab = nullptr, int column = 0);

    void add(Field* f, int column) {
        add(f, nullptr, column);
    }
    
    void render();
    juce::Rectangle<int> getMinimumSize();
    void gatherFields(juce::Array<Field*>& fields);

    void resized() override;
    void paint (juce::Graphics& g) override;
    
  private:
    
    juce::OwnedArray<FormPanel> panels;
    juce::TabbedComponent tabs;
    class HelpArea* helpArea = nullptr;

    int indentWidth = 0;
    int outlineWidth = 0;

};

