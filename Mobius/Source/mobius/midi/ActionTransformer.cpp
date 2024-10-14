
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/UIAction.h"
#include "../../model/SymbolIds.h"
#include "../../model/Symbol.h"

#include "TrackScheduler.h"

#include "ActionTransformer.h"

ActionTransformer::ActionTransformer(MidiTrack* t, TrackScheduler* s)
{
    track = t;
    scheduler = s;
}

void ActionTransformer::initialize(UIActionPool* apool)
{
    actionPool = apool;
}

void ActionTransformer::doKernelActions(UIAction* actions)
{
    doActions(actions, false);
}

void ActionTransformer::doSchedulerActions(UIActions* actions)
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
    // todo: examine the entire list and do filtering or transformation

    for (UIAction* a = list ; a != nullptr ; a = a->next) {

        UIAction* copy = actionPool->newAction();
        copy->copy(a);

        doOneAction(a);
    }
}

void ActionTransformer::doOneAction(UIAction* a)
{
    if (a->longPress) {
        // don't have many of these
        if (a->symbol->id == FuncRecord) {
            if (a->longPressCount == 0)
              // loop reset
              doReset(a, false);
            else if (a->longPressCount == 1)
              // track reset
              doReset(a, true);
            else {
                // would be nice to have this be GlobalReset but
                // would have to throw that back to Kernel
            }
        }
        else {
            // these are good to show to the user
            char msgbuf[128];
            snprintf(msgbuf, sizeof(msgbuf), "Unsupported long press function: %s",
                     a->symbol->getName());
            alert(msgbuf);
            Trace(1, "MidiTrack: %s", msgbuf);
        }
    }
}
