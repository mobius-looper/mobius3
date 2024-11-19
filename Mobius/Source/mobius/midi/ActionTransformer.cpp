
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/UIAction.h"
#include "../../model/SymbolId.h"
#include "../../model/Symbol.h"

#include "TrackScheduler.h"
#include "AbstractTrack.h"

#include "ActionTransformer.h"

ActionTransformer::ActionTransformer(AbstractTrack* t, TrackScheduler* s)
{
    track = t;
    scheduler = s;
}

ActionTransformer::~ActionTransformer()
{
}

void ActionTransformer::initialize(UIActionPool* apool, SymbolTable* st)
{
    actionPool = apool;
    symbols = st;
}

void ActionTransformer::doKernelActions(UIAction* actions)
{
    doActions(actions, false);
}

void ActionTransformer::doSchedulerActions(UIAction* actions)
{
    doActions(actions, true);
}

/**
 * The primary entry point for track actions.
 * Most actions will be singles, but it is possible to pass them
 * in bulk on a linked list.
 *
 * List passing will be done for stacked actions from the scheduler.
 * todo: Also use this to pass all queued actions at the beginning
 * of each audio block from MobiusKernel.
 *
 * If the owned flag is false, it means the actions are comming from MobiusKernel
 * and are still owned by MobiusKernel.  If the owned flag is true, it means
 * the actions are comming from TrackScheduler and have already been copied.
 * todo: not liking ownership ambiguity, once it hits MidiTrack should assume
 * it was always a track-local copy?
 */
void ActionTransformer::doActions(UIAction* list, bool owned)
{
    (void)owned;
    // todo: examine the entire list and do filtering or transformation

    for (UIAction* a = list ; a != nullptr ; a = a->next) {

        // don't copy yet, let Scheduler do it
        //UIAction* copy = actionPool->newAction();
        //copy->copy(a);

        doOneAction(a);
    }
}

void ActionTransformer::doOneAction(UIAction* a)
{
    //Trace(2, "ActionTransformer::doOneAction %s", a->symbol->getName());
    
    if (a->symbol->parameterProperties) {
        // a parameter assignment, no transformations yet
        // scheduler may quantize these
        scheduler->doParameter(a);
    }
    else if (a->sustainEnd) {
        // filter these out for now, no SUS functions yet so don't confuse things
        //Trace(2, "ActionTransformer: Filtering sustain end action");
    }
    else if (a->symbol->id == FuncRecord) {
        // record has special meaning, before scheduler gets it
        auto mode = track->getMode();
        
        if (mode == MobiusMidiState::ModeMultiply) {
            UIAction temp;
            temp.symbol = symbols->getSymbol(FuncUnroundedMultiply);
            scheduler->doAction(&temp);
        }
        else if (mode == MobiusMidiState::ModeInsert) {
            // unrounded insert
            UIAction temp;
            temp.symbol = symbols->getSymbol(FuncUnroundedInsert);
            scheduler->doAction(&temp);
        }
        else {
            scheduler->doAction(a);
        }
    }
    else {
        scheduler->doAction(a);
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
