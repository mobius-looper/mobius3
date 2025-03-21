/**
 * Sub component of BindingEditor to show available binding targets
 */

#pragma once

#include <JuceHeader.h>

#include "../common/SimpleTabPanel.h"
#include "../common/SimpleListBox.h"

class BindingTargetSelector : public SimpleTabPanel,
                              public SimpleListBox::Listener,
                              public juce::DragAndDropContainer
{
  public:

    class Listener {
      public:
        virtual ~Listener() {}
        virtual void bindingTargetClicked(BindingTargetSelector* bts) = 0;
    };

    BindingTargetSelector(class Supervisor* s);
    ~BindingTargetSelector();

    void setListener(Listener* l) {
        listener = l;
    }

    void load();
    void capture(class OldBinding* b);
    void select(class OldBinding* b);
    void reset();
    
    bool isTargetSelected();
    juce::String getSelectedTarget();

    bool isValidTarget(juce::String name);
    
    void showSelectedTarget(juce::String name);
    
    // SimpleListBox::Listener
    void selectedRowsChanged(SimpleListBox* box, int lastRow);
    void listBoxItemClicked(SimpleListBox* box, int row);
    
  private:

    class Supervisor* supervisor = nullptr;

    void initBox(SimpleListBox* box);
    void deselectOtherTargets(SimpleListBox* active);

    Listener* listener = nullptr;
    SimpleListBox functions;
    SimpleListBox controls;
    SimpleListBox scripts;
    SimpleListBox parameters;
    SimpleListBox configurations;

    // array of pointers to the above for convenient iteration
    juce::Array<SimpleListBox*> boxes;
};
    
