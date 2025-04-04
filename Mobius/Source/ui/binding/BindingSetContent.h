/**
 * Simple wrapper around whatever we want to show for one BindingSet when
 * it is selected in the BindingSetTable
 */

#pragma once

#include <JuceHeader.h>

#include "../common/BasicTabs.h"
#include "BindingTable.h"

class BindingSetContent : public juce::Component,
                          public juce::DragAndDropTarget
{
  public:

    BindingSetContent();

    void load(class BindingEditor* ed, class BindingSet* set);
    void cancel();
    void resized();
    
    bool isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails& details) override;
    void itemDropped (const juce::DragAndDropTarget::SourceDetails&) override;
    
  private:

    BindingEditor* editor = nullptr;
    BindingSet* bindingSet = nullptr;
    
    BasicTabs tabs;
    BindingTable midiTable;
    BindingTable keyTable;
    BindingTable hostTable;
    
};
