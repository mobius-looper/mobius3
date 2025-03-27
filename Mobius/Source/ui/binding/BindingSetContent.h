/**
 * Simple wrapper around whatever we want to show for one BindingSet when
 * it is selected in the BindingSetTable
 */

#pragma once

#include <JuceHeader.h>

#include "BindingTable.h"

class BindingSetContent : public juce::Component
{
  public:

    BindingSetContent();

    void load(class BindingEditor* ed, class BindingSet* set);
    void cancel();
    void resized();
    
  private:

    BindingTable table;
};
