/**
 * Sub component of BindingPanel to show available binding targets
 */

#pragma once

#include <JuceHeader.h>

#include "../common/SimpleTabPanel.h"
#include "../common/SimpleListBox.h"

class BindingTargetPanel : public SimpleTabPanel, public SimpleListBox::Listener,
                           public juce::DragAndDropContainer
{
  public:

    BindingTargetPanel();
    ~BindingTargetPanel();
    
    void load();
    void capture(class Binding* b);
    void select(class Binding* b);
    void reset();
    
    bool isTargetSelected();
    juce::String getSelectedTarget();

    // temporary: used only for Button since it doesn't use Binding yet
    // probably no longer necessary with Symbols
    bool isValidTarget(juce::String name);
    
    void showSelectedTarget(juce::String name);
    
    void selectedRowsChanged(SimpleListBox* box, int lastRow);
        
  private:

    void initBox(SimpleListBox* box);
    void deselectOtherTargets(SimpleListBox* active);
    
    SimpleListBox functions;
    SimpleListBox controls;
    SimpleListBox scripts;
    SimpleListBox parameters;
    SimpleListBox configurations;

    // array of pointers to the above for convenient iteration
    juce::Array<SimpleListBox*> boxes;
};
    
