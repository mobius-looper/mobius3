/**
 * Utility class that does runtime model mapping between
 * the UIAction/Query models used by the Mobius application
 * and the MslEnvironment models.
 */

#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../util/Util.h"

#include "../model/UIAction.h"
#include "../model/Symbol.h"
#include "../model/ScriptProperties.h"

#include "MslEnvironment.h"
#include "MslLinkage.h"
#include "MslCompilation.h"
#include "MslResult.h"

#include "ActionAdapter.h"

/**
 * Process an action on a symbol bound to an MSL script.
 *
 * This is what normally launches a new script session outside of a scriptlet.
 *
 * The context may be the shell when responding to a MIDI event or UI button
 * or it may be the kernel when responding to a MIDI event received
 * through the plugin interface or to an action generated by another
 * script session.
 *
 * You won't be here when a script just calls another script, that is
 * handled through direct linkage within the environment.
 *
 * The session starts in whichever context it is currently in, but it
 * may immediately transition to the other side.
 *
 * If the session runs to completion synchronously, without transitioning
 * or waiting it may either be discarded, or placed on the result list
 * for later inspection.  If the script has errors it is placed on the
 * result list so it can be shown in the ScriptConsole since the UIAction
 * does not have a way to return complex results.
 *
 * If the session suspends due to a wait or a transition, it is placed
 * on the appropriate session list by the MslConductor.
 *
 */
void ActionAdapter::doAction(MslEnvironment* env, MslContext* c, UIAction* action)
{
    // same sanity checking that should have been done by now
    Symbol* s = action->symbol;
    if (s == nullptr) {
        Trace(1, "MslEnironment: Action without symbol");
    }
    else if (s->script == nullptr) {
        Trace(1, "MslEnironment: Action with non-script symbol");
    }
    else if (s->script->mslLinkage == nullptr) {
        Trace(1, "MslEnironment: Action with non-MSL symbol");
    }    
    else {
        MslRequest req;

        req.linkage = s->script->mslLinkage;

        // Really need to support passing MslBindings for named arguments
        // there are two argument conventions used with UIAction a value number
        // and an arguments string.  The number is used often for internal actions
        // but for scripts it's more flexible to require the string.  To make use of this
        // need to support splitting the string either here or with some library functions
        if (strlen(action->arguments) > 0) {
            MslValue* v = env->allocValue();
            v->setString(action->arguments);
            req.arguments = v;
        }

        // if this flag is set, it means the binding expects this to be a sustainable
        // action and sustainId will be set
        // todo: should only get here if the script itself used the #sustain option
        // and advertised itself as sustainable
        // update: UI buttons aren't smart about the sustainability of their targets
        // suppress release actions if one comes in
        bool allowIt = true;
        if (action->sustain || action->sustainEnd) {
            req.triggerId = action->sustainId;

            if (req.linkage->unit == nullptr) {
                Trace(1, "ActionAdapter: Calling MSL with a linkage without a unit");
                allowIt = false;
            }
            else if (req.linkage->unit->sustain) {
                // this will recognize release
                req.release = action->sustainEnd;
            }
            else if (action->sustainEnd) {
                // script isn't expecting release
                // harmless going down, but ignore going up
                Trace(1, "ActionAdapter: Sustainable action used for non-sustainable script %s",
                      s->getName());
                allowIt = false;
            }
        }

        if (allowIt) {
            MslResult* res = env->request(c, &req);

            if (res != nullptr) {
                if (res->value != nullptr)
                  CopyString(res->value->getString(), action->result, sizeof(action->result));
                env->free(res);
            }
        }
    }
}
