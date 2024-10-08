/**
 * A display for the layers in a loop.
 *
 */

#pragma once

#include <JuceHeader.h>

#include "StatusElement.h"

class LayerElement : public StatusElement
{
  public:
    
    LayerElement(class StatusArea* area);
    ~LayerElement();

    void update(class MobiusView* view) override;
    int getPreferredWidth() override;
    int getPreferredHeight() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

  private:

    // repaint change detection state
    int lastLayerCount = 0;
    int lastActive = -1;
    int lastCheckpointCount = 0;
    int lastViewBase = 0;

    // view base the last time we were displayed
    int viewBase = 0;

    // tranient results from orient()
    int preLoss = 0;
    int postLoss = 0;

    void orient(class MobiusViewTrack* track);
    bool isCheckpoint(class MobiusViewTrack* track, int layerIndex);
    bool isVoid(class MobiusViewTrack* track, int layerIndex);
    bool isActive(class MobiusViewTrack* track, int layerIndex);
    
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
    
