/**
 * Sub component of BindingEditor to show available binding targets
 */

#pragma once

#include <JuceHeader.h>

#include "../common/SimpleTabPanel.h"
#include "../common/SimpleListBox.h"

class OldBindingTargetSelector : public SimpleTabPanel,
                                 public SimpleListBox::Listener,
                                 public juce::DragAndDropContainer
{
  public:

    class Listener {
      public:
        virtual ~Listener() {}
        virtual void bindingTargetClicked(OldBindingTargetSelector* bts) = 0;
    };

    OldBindingTargetSelector(class Supervisor* s);
    ~OldBindingTargetSelector();

    void setListener(Listener* l) {
        listener = l;
    }

    void load();
    void capture(class Binding* b);
    void select(class Binding* b);
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
    
