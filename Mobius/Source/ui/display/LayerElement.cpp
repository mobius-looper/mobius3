/**
 * Simple layer list.
 * Most of the mind numbing math is in LayerView and LayerModel.
 *
 * For testing, this has tempoorary hacks to intercept UIActions sent
 * by the ActionButtons and make them simulate layer state which is
 * much easier than trying to build complex layers live.
 * This can all be ripped out eventually.
 *
 * We're hijacking the following function actions:
 *
 *   Coverage - activate test mode with an empty loop
 *   Debug - add a layer to the test loop
 *   Undo - pretend undo
 *   Redo - pretend redo
 * 
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/MobiusState.h"
#include "../../model/UIAction.h"
#include "../../model/Symbol.h"

// temporary
#include "../../Supervisor.h"

#include "Colors.h"
#include "StatusArea.h"
#include "LayerModel.h"

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
    testLoop.init();

    // intercept our test actions
    area->getSupervisor()->addActionListener(this);
}

LayerElement::~LayerElement()
{
    statusArea->getSupervisor()->removeActionListener(this);
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

void LayerElement::resized()
{
    // StatusElement needs to adjust the Resizer
    StatusElement::resized();
    
    // no Component substructure
}

/**
 * Making the assumption that MobiusState is stable
 * and will live until the call to paint() so we don't
 * have to make a full structure copy.  In theory any
 * display decisions we make here could have changed
 * by the time paint() happens but any anomolies
 * wouldn't last long.
 *
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
 *
 * Making a further assumption that MobiusState is stable
 * indefintely and will live across calls to update, so we can
 * detect track/loop changes simply by comparing the MobiusLoopState
 * pointer we used the last time.
 *
 * Ugh, lostLayers and lostRedo have to factor into this too.
 * Assuming a display model that looks like this:
 *
 *      .....X....
 *
 * With lostLayers on the left of 1 and lostRedo on the right of 1.
 * If you undo again, and favor putting the active layer in the center
 * the numers on the edges change from 0 to 2.  Actually no, that would
 * change the layer count.  We would end up displaying the same bars
 * but the lost numbers would change.
 * 
 */
void LayerElement::update(MobiusState* state)
{
    MobiusTrackState* track = &(state->tracks[state->activeTrack]);
    MobiusLoopState* loop = &(track->loops[track->activeLoop]);

    // if we're in test mode, redirect to the test loop state
    if (doTest)
      loop = &testLoop;

    // don't need to save lastTrack and lastLoop if we just
    // saved the last MobiusLoopState pointer
    bool needsRepaint = (lastTrack != state->activeTrack ||
                         lastLoop != track->activeLoop ||
                         lastLayerCount != loop->layerCount ||
                         lastLostCount != loop->lostLayers);

    // checkpoint is a little harder
    bool newCheckpoint = false;
    int active = loop->layerCount - 1;
    if (active > 0) {
        newCheckpoint = loop->layers[active].checkpoint;
        if (lastCheckpoint != newCheckpoint)
          needsRepaint = true;
    }

    if (needsRepaint) {
        lastTrack = state->activeTrack;
        lastLoop = track->activeLoop;
        lastLayerCount = loop->layerCount;
        lastLostCount = loop->lostLayers;
        lastCheckpoint = newCheckpoint;
        
        repaint();
    }

    // remember the full state for paint
    // if we decided not to repaint() should we be updating this?
    sourceLoop = loop;
}

/**
 * If we override paint, does that mean we control painting
 * the children, or is that going to cascade?
 */
void LayerElement::paint(juce::Graphics& g)
{
    // borders, labels, etc.
    StatusElement::paint(g);
    if (isIdentify()) return;
    
    // if we're initializing before update() has been called on
    // the refresh thread, just leave it blank
    if (sourceLoop == nullptr)
      return;

    // this does all the hard work
    view.initialize(sourceLoop, LayerBarMax, lastViewBase);

    int preLoss = view.getPreLoss();
    if (preLoss > 0) {
        g.setFont(juce::Font(LayerLossHeight));
        g.setColour(juce::Colours::white);
        g.drawText(juce::String(preLoss),
                   LayerInset, LayerInset,
                   30, LayerLossHeight,
                   juce::Justification::left);
    }

    int postLoss = view.getPostLoss();
    if (postLoss) {
        g.setFont(juce::Font(LayerLossHeight));
        g.setColour(juce::Colours::white);
        g.drawText(juce::String(postLoss),
                   getWidth() - LayerInset - 30, LayerInset,
                   30, LayerLossHeight, 
                   juce::Justification::right);
    }

    int barLeft = LayerInset;
    int barTop = LayerInset + LayerLossHeight;
    
    for (int i = 0 ; i < LayerBarMax ; i++) {

        if (view.isCheckpoint(i))
          g.setColour(juce::Colours::red);
        else 
          g.setColour(juce::Colours::grey);

        g.drawRect(barLeft, barTop, LayerBarWidth, LayerBarHeight);

        if (!view.isVoid(i)) {
            // don't really need to highlight ghost layers, but I like to see
            // them during testing
            if (view.isGhost(i)) {
                g.setColour(juce::Colours::lightblue);
            }
            else if (view.isActive(i))
              g.setColour(juce::Colours::yellow);
            else
              g.setColour(juce::Colours::yellow.darker());
            
            g.fillRect(barLeft+1, barTop+1, LayerBarWidth-2, LayerBarHeight-2);
        }
        barLeft += LayerBarWidth + LayerBarGap;
    }

    // remember the possibly adjusted viewBase for next time
    lastViewBase = view.getViewBase();
}

//////////////////////////////////////////////////////////////////////
// Test Actions
//////////////////////////////////////////////////////////////////////

// UPDATE: This no longer works, we can only intercept actions that
// are at LevelUI and the core actions like "Coverage" we used to intercept
// will not be passed down.  If you need to resurrect this will have to
// add special LevelUI symbols

/**
 * Intercept an ActionButton function action.
 * We're in the UI message loop and not part of the update() thread
 * or the paint() thread.
 */
bool LayerElement::doAction(UIAction* action)
{
    bool handled = false;

    // turn coverage on and off
    Symbol* s = action->symbol;
    
    if (strcmp(s->getName(), "Coverage") == 0) {
        if (doTest) {
            Trace(2, "LayerElement: Disabling layer tests\n");
            doTest = false;
        }
        else {
            Trace(2, "LayerElement: Enabling layer tests\n");
            doTest = true;
            // make testLoop empty
            testLoop.layerCount = 0;
            testLoop.lostLayers = 0;
            testLoop.redoCount = 0;
            testLoop.lostRedo = 0;
        }
        handled = true;
    }

    if (doTest) {
        // combine the state counts into logical counts
        int actualLayers = testLoop.layerCount + testLoop.lostLayers;
        int actualRedos = testLoop.redoCount + testLoop.lostRedo;

        if (strcmp(s->getName(), "Undo") == 0) {
            Trace(1, "LayerElement: Undo\n");
            if (actualLayers > 1) {
                actualLayers--;
                actualRedos++;
            }
            handled = true;
        }
        else if (strcmp(s->getName(), "Redo") == 0) {
            Trace(1, "LayerElement: Redo\n");
            if (actualRedos > 0) {
                actualLayers++;
                actualRedos--;
            }
            handled = true;
        }
        else if (strcmp(s->getName(), "Debug") == 0) {
            Trace(1, "LayerElement: Add layer\n");
            // pretend we have a new layer
            actualLayers++;
            handled = true;
        }

        if (handled) {
            // recalculate loss based on the desired undo/redo sizes
            // and the constraints in the state arrays

            // override MobiusStateMaxLayers and MaxRedo to see loss more easily
            int maxLayers = 5;
            int maxRedo = 5;

            if (actualLayers > maxLayers) {
                testLoop.layerCount = maxLayers;
                testLoop.lostLayers = actualLayers - maxLayers;
            }
            else {
                testLoop.layerCount = actualLayers;
                testLoop.lostLayers = 0;
            }

            if (actualRedos > maxRedo) {
                testLoop.redoCount = maxRedo;
                testLoop.lostRedo = actualRedos - maxRedo;
            }
            else {
                testLoop.redoCount = actualRedos;
                testLoop.lostRedo = 0;
            }
        }
    }
    
    return handled;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
