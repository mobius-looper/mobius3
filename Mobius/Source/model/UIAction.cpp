/**
 * A model for actions that can be sent through the application to cause
 * something to happen.
 *
 * Greatly simplified from the original Action model, and some things
 * still need a lot of refinement.  Old Action logic was insanely complex,
 * especially around trigger properties and sustain.
 *
 */

#include "../util/Util.h"
#include "../util/Trace.h"

#include "Binding.h"
#include "Symbol.h"

#include "UIAction.h"


UIAction::UIAction()
{
    strcpy(arguments, "");
    strcpy(result, "");
}

UIAction::~UIAction()
{
}

/**
 * Clear action state after it has been used.
 */
void UIAction::reset()
{
    // note: do NOT disturb the PooledObject fields

    requestId = 0;
    symbol = nullptr;
    value = 0;
    strcpy(arguments, "");
    strcpy(result, "");
    scopeTrack = 0;
    scopeGroup = 0;
    sustain = false;
    sustainEnd = false;
    sustainId = 0;
    longPress = false;
    noQuantize = false;
    noLatency = false;
    noSynchronization = false;
    next = nullptr;
    owner = nullptr;
    track = nullptr;
    noGroup = false;
}

/**
 * Copy one UIAction into another.
 * Used with the UIActionPool to copy a source action
 * with limited lifespan to a pooled action with indefinite
 * lifetime.
 */
void UIAction::copy(UIAction* src)
{
    // leave PooledObject state intact
    requestId = src->requestId;
    symbol = src->symbol;
    value = src->value;
    strcpy(arguments, src->arguments);
    scopeTrack = src->scopeTrack;
    scopeGroup = src->scopeGroup;
    sustain = src->sustain;
    sustainEnd = src->sustainEnd;
    sustainId = src->sustainId;
    longPress = src->longPress;
    noQuantize = src->noQuantize;
    noLatency = src->noLatency;
    noSynchronization = src->noSynchronization;
    noGroup = src->noGroup;
    
    // these never convey
    next = nullptr;
    owner = nullptr;
    track = nullptr;
    strcpy(result, "");
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
