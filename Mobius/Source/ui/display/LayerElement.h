/**
 * A display for the layers in a loop.
 *
 */

#pragma once

#include <JuceHeader.h>

#include "../../model/MobiusState.h"

// temporary test hack
#include "../../Supervisor.h"

#include "LayerModel.h"
#include "StatusElement.h"

class LayerElement : public StatusElement, public Supervisor::ActionListener
{
  public:
    
    LayerElement(class StatusArea* area);
    ~LayerElement();

    void update(class MobiusState* state) override;
    int getPreferredWidth() override;
    int getPreferredHeight() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

    // intercept some of the ActionButtons to simulate layer state
    bool doAction(class UIAction* a) override;
    
  private:

    // change detection state
    int lastTrack = 0;
    int lastLoop = 0;
    int lastLayerCount = 0;
    int lastLostCount = 0;
    bool lastCheckpoint = false;

    // model/view helper
    LayerView view;

    // view base the last time we were displayed
    int lastViewBase = 0;
    
    // redirected loop state for testing
    MobiusLoopState testLoop;
    bool doTest = false;

    // the last loop state used by update()
    MobiusLoopState* sourceLoop = nullptr;
    
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
    
