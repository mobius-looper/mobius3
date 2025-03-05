/**
 * Base class of a component that combinds a ParameterTree with
 * a ParameterFormCollection.  Mostly just provides provides a wrapper
 * component with a slider bar between them.
 */

#pragma once

#include <JuceHeader.h>

#include "ParameterTree.h"
#include "ParameterFormCollection.h"

class ParameterTreeForms : public juce::Component
{
  public:
    
    ParameterTreeForms();
    ~ParameterTreeForms();

    void resized() override;
    
  protected:

    ParameterTree tree;
    ParameterFormCollection forms;

    juce::StretchableLayoutManager verticalLayout;
    std::unique_ptr<juce::StretchableLayoutResizerBar> verticalDividerBar;
    
};
