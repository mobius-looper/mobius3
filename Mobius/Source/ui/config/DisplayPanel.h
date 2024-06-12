/**
 * ConfigPanel to edit scripts
 */

#pragma once

#include <JuceHeader.h>

#include "../common/HelpArea.h"
#include "../../test/BasicTabs.h"
#include "../../test/BasicInput.h"
#include "../../test/BasicForm.h"
#include "MultiSelectDrag.h"

#include "ConfigPanel.h"

/**
 * Central editing component.
 */
class DisplayEditor : public juce::Component, public juce::DragAndDropContainer
{
  public:
    
    DisplayEditor();
    ~DisplayEditor();

    void load(class DisplayLayout* layout);
    void save(class DisplayLayout* layout);
    
    void resized() override;
    void paint(juce::Graphics& g) override;

  private:

    MultiSelectDrag mainElements;
    MultiSelectDrag dockedStrip;
    MultiSelectDrag floatingStrip;
    MultiSelectDrag instantParameters;

    BasicForm properties;
    BasicInput trackRows {"Track Rows", 20};
    
    BasicTabs tabs;
    
    HelpArea helpArea;
    
    void initElementSelector(MultiSelectDrag* multi, class UIConfig* config,
                             juce::OwnedArray<class DisplayElement>& elements,
                             bool trackStrip);

    void initParameterSelector(MultiSelectDrag* multi, class UIConfig* config,
                               juce::StringArray values);

    void addParameterDisplayName(juce::String name, juce::StringArray& values);

    class Symbol* findSymbolWithDisplayName(juce::String dname);

    class StringList* toStringList(juce::StringArray& src);

    void saveElements(juce::OwnedArray<DisplayElement>& elements,
                      juce::StringArray names);
    
    void saveStripElements(juce::OwnedArray<DisplayElement>& elements,
                           juce::StringArray names);
    
};

class DisplayPanel : public ConfigPanel 
{
  public:
    DisplayPanel(class ConfigEditor*);
    ~DisplayPanel();

    // overloads called by ConfigPanel
    void load() override;
    void save() override;
    void cancel() override;

    // ObjectSelector overloads
    void selectObject(int ordinal) override;
    void newObject() override;
    void deleteObject() override;
    void revertObject() override;
    void renameObject(juce::String) override;
    
  private:

    juce::OwnedArray<class DisplayLayout> layouts;
    juce::OwnedArray<class DisplayLayout> revertLayouts;
    int selectedLayout;
    
    DisplayEditor displayEditor;

    void refreshObjectSelector();
    void loadLayout(int ordinal);
    void saveLayout(int ordinal);

};
