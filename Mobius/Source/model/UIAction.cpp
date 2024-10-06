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
#include "Scope.h"

#include "UIAction.h"


UIAction::UIAction()
{
    strcpy(arguments, "");
    strcpy(result, "");
    strcpy(scope, "");
}

UIAction::~UIAction()
{
}

/**
 * Initializer used by the ObjectPool
 */
void UIAction::poolInit()
{
    reset();
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
    strcpy(scope, "");
    sustain = false;
    sustainEnd = false;
    sustainId = 0;
    longPress = false;
    longPressCount = 0;
    noQuantize = false;
    noLatency = false;
    noSynchronization = false;
    next = nullptr;
    owner = nullptr;
    track = nullptr;
    noGroup = false;
    coreEvent = nullptr;
    coreEventFrame = 0;
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
    strcpy(scope, src->scope);
    sustain = src->sustain;
    sustainEnd = src->sustainEnd;
    sustainId = src->sustainId;
    longPress = src->longPress;
    longPressCount = src->longPressCount;
    noQuantize = src->noQuantize;
    noLatency = src->noLatency;
    noSynchronization = src->noSynchronization;
    noGroup = src->noGroup;
    
    // these never convey
    next = nullptr;
    owner = nullptr;
    track = nullptr;
    strcpy(result, "");
    coreEvent = nullptr;
    coreEventFrame = 0;
}

const char* UIAction::getScope()
{
    return scope;
}

void UIAction::setScope(const char* s)
{
    CopyString(s, scope, sizeof(scope));
}

void UIAction::setScopeTrack(int i)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", i);
    setScope(buf);
}

/**
 * Parsing track numbers is relatively easy.  It has to be at most 2 digits.
 */
int UIAction::getScopeTrack()
{
    return Scope::parseTrackNumber(scope);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
