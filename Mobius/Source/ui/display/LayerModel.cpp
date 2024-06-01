/**
 * Simplified model of loop layer for display in the UI.
 *
 * Math People will point out that the logic in here could
 * be much more concice, but it makes my brain hurt to re-figure
 * that shit out every time I look at it.  It is far more maintainable
 * to spell it all out in exhausting detail.  The redundant
 * calculations and method calls aren't that expensive in the grand
 * scheme of theings.
 *
 * There are some theoretical states that can be assumed to not happen
 *
 * layer counts or lost counts < 0
 * layerCount == 0 and lostLayers != 0
 * layerCount == 0 and redoCount, lostRedo != 0
 * etc.
 *
 * To prevent the display from going haywhere when there are bugs building
 * MobiusState we could try to correct those and trace them.
 *  
 */

// floor()
#include <math.h>

#include "../../model/MobiusState.h"
#include "LayerModel.h"

//////////////////////////////////////////////////////////////////////
// LayerModel
//
//////////////////////////////////////////////////////////////////////

/**
 * Ponder the significance of a MobiusLoopState.
 * At runtime it is assumed this will live as long as we do.
 */
void LayerModel::initialize(MobiusLoopState* src)
{
    state = src;

    // precalculate some things we always need
    totalLayers = state->layerCount + state->lostLayers + state->redoCount + state->lostRedo;
    
    // this will be -1 if there are no layers
    activeLayer = state->layerCount + state->lostLayers - 1;
}

int LayerModel::getLayerCount()
{
    return totalLayers;
}

int LayerModel::getActiveLayer()
{
    return activeLayer;
}

bool LayerModel::isVoid(int index)
{
    return (index < 0 || index >= totalLayers);
}

/**
 * Needs to be a valid layer, if getActiveLayer returns -1
 * the isActive must still return false
 */
bool LayerModel::isActive(int index)
{
    return (activeLayer >= 0 && index == activeLayer);
}

bool LayerModel::isCheckpoint(int index)
{
    bool checkpoint = false;
    MobiusLayerState* ls = getState(index);
    if (ls != nullptr)
      checkpoint = ls->checkpoint;
    return checkpoint;
}

/**
 * There are two ghost regions, the undo region represented
 * internally by MobiusLoopState.lostLayers and the redo region
 * represented by MobiusLoopState.lostRedo.
 * Yes, I know the math could be more concise, but my brain works
 * better seeing it all spelled out.
 */
bool LayerModel::isGhost(int index)
{
    bool ghost = false;
    if (index < state->lostLayers) {
        // undo region, technically if the index is negative
        // it is an invalid index and not a ghost
        ghost = true;
    }
    else if (state->lostRedo > 0) {
        int redoGhostStart = activeLayer + state->redoCount + 1;
        if (index >= redoGhostStart && index < totalLayers)
          ghost = true;
    }
    return ghost;
}

/**
 * Lots of "indexes" here, to clarify:
 *
 *   index
 *     index into the full logical layer model of the layer of interest
 *
 *   layerStateIndex
 *     index into the physical MobiusLayerState array state->layers 
 *
 *   firstRedoIndex
 *      logical layer index of the first redo layer
 *      same as activeLayer + 1
 *
 *   redoStateIndex
 *     index into the physical MobiusLayerState array state->redoLayers
 */
MobiusLayerState* LayerModel::getState(int index)
{
    MobiusLayerState* ls = nullptr;

    int layerStateIndex = index - state->lostLayers;
    
    if (layerStateIndex >= 0 && layerStateIndex < state->layerCount) {
        ls = &(state->layers[layerStateIndex]);
    }
    else {
        int firstRedoIndex = state->lostLayers + state->layerCount;
        int redoStateIndex = index - firstRedoIndex;
        if (redoStateIndex >= 0 && redoStateIndex < state->redoCount)
          ls = &(state->redoLayers[redoStateIndex]);
    }
    return ls;
}
        
//////////////////////////////////////////////////////////////////////
// LayerView
////////////////////////////////////////////////////////////////////////

/**
 * Initalize the view from a LoopState and display characteristics.
 */
void LayerView::initialize(MobiusLoopState* lstate, int size, int lastViewBase)
{
    model.initialize(lstate);
    viewSize = size;
    viewBase = lastViewBase;
    orient();
}

int LayerView::getViewBase()
{
    return viewBase;
}

void LayerView::setViewBase(int newBase, bool reorient)
{
    viewBase = newBase;
    if (reorient)
      orient();
}

bool LayerView::isVoid(int bar)
{
    return model.isVoid(viewBase + bar);
}

bool LayerView::isActive(int bar)
{
    return model.isActive(viewBase + bar);
}

bool LayerView::isUndo(int bar)
{
    int logicalIndex = viewBase + bar;

    // active -1 if the loop is empty
    int active = model.getActiveLayer();
    
    return (active >= 0 && logicalIndex >= 0 && logicalIndex < active);
}

bool LayerView::isRedo(int bar)
{
    int logicalIndex = viewBase + bar;
    int active = model.getActiveLayer();
    return (active >= 0 && logicalIndex > active && logicalIndex < model.getLayerCount());
}

bool LayerView::isGhost(int bar)
{
    return model.isGhost(viewBase + bar);
}

bool LayerView::isCheckpoint(int bar)
{
    return model.isCheckpoint(viewBase + bar);
}

int LayerView::getPreLoss()
{
    return preLoss;
}

int LayerView::getPostLoss()
{
    return postLoss;
}

/**
 * Finally we get to the heart of the matter.
 * With most of the math and model transformation out of the way,
 * figure out the best way to display the layers.
 *
 * Ensure the active layer is visible.
 * Try to preserve viewBase to prevent the display from jumping around.
 * 
 * todo: have a minimum number of undo/redo bars on both sides of the
 * active layer to give the user a sense of space around the active layer.
 */
void LayerView::orient()
{
    // viewBase is the logical index of the first visible layer in the view
    
    // the logical index of the active layer
    int activeIndex = model.getActiveLayer();

    if (activeIndex < 0) {
        // the loop empty, rather than handling this below, just
        // initialize everything and bail
        viewBase = 0;
        preLoss = 0;
        postLoss = 0;
        return;
    }
    
    // the logical index of the last visible layer in the view
    int lastVisibleIndex = viewBase + viewSize - 1;

    if (activeIndex >= viewBase && activeIndex <= lastVisibleIndex) {
        // it fits within the current view
        // but it could be at or near an edge
        // consider adding left/right padding, but leave it alone for now
    }
    else {
        // we have to move the base to bring the active layer into view
        // we'll start by just centering it, though we could have
        // a more gradual scroll keeping it nearer the edges
        int center = (int)(floor((float)viewSize / 2.0f));
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
    postLoss = model.getLayerCount() - viewSize - preLoss;

    // postLoss is commonly negative when you're just starting the loop
    // and there aren't many layers.
    // remember the motto: "there is no loss in the void" much like
    // "there's always money in the banana stand"
    if (postLoss < 0)
      postLoss = 0;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
