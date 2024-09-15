/**
 * Simple layer list.
 *
 * Loop layers can be unbounded but we only show a subset of them.
 * The layer display "scrolls" so that the active layer is always visible
 * with undo and redo layers on each side.  Layers that are marked as checkpoints
 * are highlighted.
 * 
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../JuceUtil.h"
#include "../MobiusView.h"

#include "Colors.h"
#include "StatusArea.h"

#include "LayerElement.h"

const int LayerBarHeight = 30;
const int LayerBarWidth = 10;
const int LayerBarMax = 20;
const int LayerBarGap = 2;
const int LayerInset = 2;
const int LayerLossHeight = 12;

LayerElement::LayerElement(StatusArea* area) :
    StatusElement(area, "LayerElement")
{
}

LayerElement::~LayerElement()
{
}

int LayerElement::getPreferredHeight()
{
    return LayerBarHeight + LayerLossHeight + (LayerInset * 2);
}

int LayerElement::getPreferredWidth()
{
    return (LayerBarMax * LayerBarWidth) +
        ((LayerBarMax - 1) * LayerBarGap) +
        (LayerInset * 2);
}

/**
 * This one doesn't resize though I suppose it could
 */
void LayerElement::resized()
{
    // no Component substructure
}

/**
 * For change detection need at minimum look at:
 *    activeTrack, activeLoop, layerCount, lostLayers
 *
 * Redo counts can't change without also changing layer counts.
 * Example from layerCount=10 the active layer is always
 * index 9.  If you Undo, layerCount drops to 9 and
 * redoCount increases by 1.  
 *
 * You can't create more redo layers without "moving" the
 * active layer.  You can in theory reduce the redoCount
 * though an action that prunes them but we don't have that yet.
 * If we ever do, then will have to include redo counts in
 * refresh detection.
 *
 * Checkpoint state can only change in what was previously the
 * active layer.  You can't randomly toggle checkpoint status
 * on other layers.  So while each layer has a checpointed flag,
 * we only need to remember the state of the last active one.
 */
void LayerElement::update(MobiusView* mview)
{
    MobiusViewTrack* track = mview->track;
    
    bool needsRepaint = (mview->trackChanged ||
                         track->loopChanged ||
                         lastLayerCount != track->layerCount ||
                         lastActive != track->activeLayer);

    if (!needsRepaint) {
        // Also ponder checkpoints
        // Until we can randomly turn these on and off without
        // also impacting the layer count, it's enough just
        // count the number of checkpoints.  Should that stop
        // being true, this will need to be much more compllicated.
        needsRepaint = lastCheckpointCount != track->checkpoints.size();
    }

    if (needsRepaint) {
        lastLayerCount = track->layerCount;
        lastActive = track->activeLayer;
        lastCheckpointCount = track->checkpoints.size();
        
        repaint();
    }
}

void LayerElement::paint(juce::Graphics& g)
{
    MobiusViewTrack* track = getMobiusView()->track;
    
    // borders, labels, etc.
    StatusElement::paint(g);
    if (isIdentify()) return;

    // ponder the latest layer state and figure out what to draw
    orient(track);
    
    if (preLoss > 0) {
        g.setFont(JuceUtil::getFont(LayerLossHeight));
        g.setColour(juce::Colours::white);
        g.drawText(juce::String(preLoss),
                   LayerInset, LayerInset,
                   30, LayerLossHeight,
                   juce::Justification::left);
    }

    if (postLoss) {
        g.setFont(JuceUtil::getFont(LayerLossHeight));
        g.setColour(juce::Colours::white);
        g.drawText(juce::String(postLoss),
                   getWidth() - LayerInset - 30, LayerInset,
                   30, LayerLossHeight, 
                   juce::Justification::right);
    }

    int barLeft = LayerInset;
    int barTop = LayerInset + LayerLossHeight;
    
    for (int i = 0 ; i < LayerBarMax ; i++) {

        int layerIndex = viewBase + i;

        if (isCheckpoint(track, layerIndex))
          g.setColour(juce::Colours::red);
        else 
          g.setColour(juce::Colours::grey);

        g.drawRect(barLeft, barTop, LayerBarWidth, LayerBarHeight);

        if (!isVoid(track, layerIndex)) {
            if (isActive(track, layerIndex))
              g.setColour(juce::Colours::yellow);
            else
              g.setColour(juce::Colours::yellow.darker());
            
            g.fillRect(barLeft+1, barTop+1, LayerBarWidth-2, LayerBarHeight-2);
        }
        barLeft += LayerBarWidth + LayerBarGap;
    }

    // remember the possibly adjusted viewBase for next time
    lastViewBase = viewBase;
}

/**
 * Wake up and figure out where the "view" over the entire layer space
 * should be.  This must keep the active layer in view, and tries to avoid
 * excessive jumping around.
 */
void LayerElement::orient(MobiusViewTrack* track)
{
    // viewBase is the logical index of the first visible layer in the view
    viewBase = lastViewBase;
    
    // the logical index of the active layer
    int activeIndex = track->activeLayer;
    
    if (activeIndex < 0) {
        // the loop empty, rather than handling this below, just
        // initialize everything and bail
        viewBase = 0;
        preLoss = 0;
        postLoss = 0;
        return;
    }
    
    // the logical index of the last visible layer in the view
    int lastVisibleIndex = viewBase + LayerBarMax - 1;

    if (activeIndex >= viewBase && activeIndex <= lastVisibleIndex) {
        // it fits within the current view
        // but it could be at or near an edge
        // consider adding left/right padding, but leave it alone for now
    }
    else {
        // we have to move the base to bring the active layer into view
        // we'll start by just centering it, though we could have
        // a more gradual scroll keeping it nearer the edges
        int center = (int)(floor((float)LayerBarMax / 2.0f));
        viewBase = activeIndex - center;
        if (viewBase < 0) {
            // centering pushed us off the edge?
            viewBase = 0;
        }
    }

    // deal with the tragic loss

    // preLoss is normally just viewBase, unless for some reason
    // you wanted to have a negative viewBase for right justification
    // or centering, if we're viewing into the void there is no loss
    // which I'm thinking would a good motto
    preLoss = 0;
    if (viewBase > 0)
      preLoss = viewBase;

    // total number of layers minus the number we can see minus
    // the number of layers hidden on the left (preLoss)
    postLoss = track->layerCount - LayerBarMax - preLoss;

    // postLoss is commonly negative when you're just starting the loop
    // and there aren't many layers.
    // remember the motto: "there is no loss in the void" much like
    // "there's always money in the banana stand"
    if (postLoss < 0)
      postLoss = 0;
}

bool LayerElement::isCheckpoint(MobiusViewTrack* track, int layerIndex)
{
    return (track->checkpoints.size() > 0 &&
            track->checkpoints.contains(layerIndex));
}

bool LayerElement::isActive(MobiusViewTrack* track, int layerIndex)
{
    return (track->activeLayer == layerIndex);
}

bool LayerElement::isVoid(MobiusViewTrack* track, int layerIndex)
{
    return (layerIndex < 0 || layerIndex >= track->layerCount);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
