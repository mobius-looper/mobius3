/**
 * ConfigEditor to edit display layouts
 */

#pragma once

#include <JuceHeader.h>

#include "../common/HelpArea.h"
#include "../common/BasicTabs.h"
#include "../common/BasicInput.h"
#include "../common/BasicForm.h"
#include "MultiSelectDrag.h"

#include "ConfigEditor.h"

class DisplayEditor : public ConfigEditor
{
  public:
    
    DisplayEditor(class Supervisor* s);
    ~DisplayEditor();

    juce::String getTitle() override {return juce::String("Display Layouts");}

    // ConfigEditor overloads
    void prepare() override;
    void load() override;
    void save() override;
    void cancel() override;
    void revert() override;
    
    void objectSelectorSelect(int ordinal) override;
    void objectSelectorNew(juce::String newName) override;
    void objectSelectorDelete() override;
    void objectSelectorRename(juce::String) override;

    void resized() override;
    
  private:

    juce::OwnedArray<class DisplayLayout> layouts;
    juce::OwnedArray<class DisplayLayout> revertLayouts;
    int selectedLayout;

    MultiSelectDrag mainElements;
    MultiSelectDrag dockedStrip;
    MultiSelectDrag floatingStrip;
    MultiSelectDrag instantParameters;

    BasicForm properties;
    BasicInput loopRows {"Loop Rows", 20};
    BasicInput trackRows {"Track Rows", 20};
    BasicInput buttonHeight {"Button Height", 20};
    BasicInput radarDiameter {"Radar Diameter", 30};
    BasicInput alertDuration {"Alert Duration", 20};
    
    BasicTabs tabs;
    
    void refreshObjectSelector();
    
    void loadLayout(int ordinal);

    void initElementSelector(MultiSelectDrag* multi, class UIConfig* config,
                             juce::OwnedArray<class DisplayElement>& elements,
                             bool trackStrip);
    void initParameterSelector(MultiSelectDrag* multi, class UIConfig* config,
                               juce::StringArray values);
    void addParameterDisplayName(juce::String name, juce::StringArray& values);

    void saveLayout(int ordinal);
    
    void saveElements(juce::OwnedArray<DisplayElement>& elements,
                      juce::StringArray names);
    void saveStripElements(juce::OwnedArray<DisplayElement>& elements,
                           juce::StringArray names);
    class Symbol* findSymbolWithDisplayName(juce::String dname);

    
};
