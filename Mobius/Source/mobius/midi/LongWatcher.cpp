/**
 * Conceptually the same as core/TriggerState but not dependent on the
 * older models.  Uses Symbols and FunctionProperties to determine
 * long pressability, assumes the Listener knows what to do with it.
 */

#include "../../util/Trace.h"
#include "../../model/Session.h"
#include "../../model/UIAction.h"
#include "../../model/Symbol.h"
#include "../../model/FunctionProperties.h"

#include "LongWatcher.h"

LongWatcher::LongWatcher()
{
}

LongWatcher::~LongWatcher()
{
    reclaim(pool);
    pool = nullptr;
    reclaim(presses);
    presses = nullptr;
}

void LongWatcher::reclaim(State* list)
{
    while (list != nullptr) {
        State* next = list->next;
        list->next = nullptr;
        delete list;
        list = next;
    }
}

/**
 * Initialize must be called during the initialization phase of the
 * shell, where we will allocate memory for the little State pool.
 *
 * It may be called after that to adapt to global parameter changes
 * in the Session, where we calculate the long press threshold.
 * 
 * todo: pass in the Session so we can have a global parameter
 * that controls long press duration
 */
void LongWatcher::initialize(Session* session, int rate)
{
    (void)session;
    
    if (sampleRate == 0)
      sampleRate = 44100;
    else
      sampleRate = rate;

    // this needs to come from the Session
    int longMsecs = 1000;

    float longSeconds = (float)longMsecs / 1000.0f;
    
    threshold = (int)((float)sampleRate * longSeconds);

    if (pool == nullptr) {
        // should only be during shell initialization so we can allocate
        for (int i = 0 ; i < MaxPool ; i++) {
            State* s = new State();
            s->next = pool;
            pool = s;
        }
    }
}

void LongWatcher::setListener(Listener* l)
{
    listener = l;
}

void LongWatcher::watch(UIAction* a)
{
    if (a->symbol != nullptr &&
        a->symbol->functionProperties != nullptr &&
        a->symbol->functionProperties->longPressable) {
        // the target is a function that has long press behavior

        if (a->sustain && a->sustainId > 0) {
            // Binderator determined that the trigger supports sustaining

            // do we already have one?
            State* existing = nullptr;
            State* prev = nullptr;
            for (State* s = presses ; s != nullptr ; s = s->next) {
                if (s->sustainId == a->sustainId) {
                    existing = s;
                    break;
                }
                else {
                    prev = s;
                }
            }

            if (existing == nullptr) {
                if (a->sustainEnd) {
                    // this is an up transition that we weren't watching
                    // this is normal if you decide to remove the press state
                    // when the long press is detected,
                    // abnormal if it was left there
                    Trace(1, "LongWatcher: Release transition not tracked");
                }
                else {
                    // going down...
                    if (pool == nullptr) {
                        Trace(1, "LongWatcher: Watch pool exhausted");
                    }
                    else {
                        State* state = pool;
                        pool = pool->next;
                        state->sustainId = a->sustainId;
                        state->symbol = a->symbol;
                        state->frames = 0;
                        state->notifications = 0;

                        // todo: scope can only be a track number right now
                        // I don't think this needs to support symbolic scopes
                        // at this point?  wouldn't they have been transformed
                        // by now?
                        state->scope = a->getScopeTrack();
                        
                        state->next = presses;
                        presses = state;
                    }
                }
            }
            else if (a->sustainEnd) {
                // normal case, it went up
                if (prev != nullptr)
                  prev->next = existing->next;
                else
                  presses = existing->next;
                    
                existing->next = pool;
                pool = existing;
            }
            else {
                // it went down again without going up
                // shouldn't happen
                Trace(1, "LongWatcher: New trigger for existing event");
                // I guess reset the timeout
                existing->frames = 0;
                if (existing->symbol != a->symbol) {
                    Trace(1, "LongWatcher: Changing symbol for existing event");
                    existing->symbol = a->symbol;
                }
            }
        }
    }
}

void LongWatcher::advance(int frames)
{
    State* prev = nullptr;
    State* next = nullptr;
    for (State* state = presses ; state != nullptr ; state = next) {
        next = state->next;

        state->frames += frames;
        if (state->frames < threshold) {
            // not there yet
            prev = state;
        }
        else {
            if (listener != nullptr) {
                UIAction action;
                action.symbol = state->symbol;
                action.setScopeTrack(state->scope);
                action.longPress = true;
                action.longPressCount = state->notifications;
                listener->longPressDetected(&action);
            }

            // formerly removed it, but I'd rather leave it in place
            // and wait for the up?  it might be interesting to let it
            // fire more than once, long and REALLY long?
            bool allowVeryLong = true;
            if (allowVeryLong) {
                state->frames = 0;
                state->notifications++;
            }
            else {
                if (prev != nullptr)
                  prev->next = next;
                else
                  presses = next;
                state->next = pool;
                pool = state;
            }
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
