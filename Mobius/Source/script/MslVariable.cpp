/**
 * Value handling for static variables.
 *
 * The term "static" applies to any variable whose value is NOT maintained
 * in a transient MslBinding on the stack.  It is instead maintained inside
 * a shared MslVariable held by the compilation unit (MslCompilation).
 *
 * This includes variables declared as:
 *
 *     static or global
 *       - visible only within this unit
 *
 *     public
 *       - visible to other scripts
 *
 *     export
 *       - visible to other scripts and the containing application
 *
 *     track or scope
 *       - separate value maintained for each application scope, which in current
 *         usage is a "track" though it could be anything
 *
 * Most static variables have either public or export scope and have single values.
 * Track variables are unusual in that a distinct value is maintained for each track,
 * though within MSL, the notion of a "track" does not exist.  Instead the application may
 * define variables and perform actions within an abtract "scope" which are identified by number.
 * Within Mobius a scope identifier number is usually the same as a Mobius track number but
 * this is not necessarily the case.  Further scope numbers do not necessarily match the
 * Mobius track numbers displayed in the UI since tracks can be reordered and the number that
 * is visible is mapped to a scope identifier which does not change.
 *
 * Since MSL does not know how many possible scopes an application may have, variables
 * with track scope allocate an array with a slot for each of the possible scope identifiers.
 * This is because the variable array may need to be grown in the audio thread where memory
 * allocation is not allowed.
 */

#include "../util/Trace.h"

#include "MslValue.h"
#include "MslVariable.h"
#include "MslPools.h"

MslVariable::MslVariable()
{
}

/**
 * Preferred constructor that provides the pool to use when allocating
 * MslValues for track scope variables.  This is optional but recommended.
 * if a pool is not available, MslValues will be allocated and deleted using
 * new and delete.
 */
MslVariable::MslVariable(MslPools* argPool)
{
    pool = argPool;
}

void MslVariable::setPool(MslPools* argPool)
{
    pool = argPool;
}

MslVariable::~MslVariable()
{
    // if we are a track variable, need to clean up the explored MslValues
    unbind();
}

void MslVariable::unbind()
{
    value.setNull();
    for (int i = 0 ; i < scopeValues.size() ; i++) {
        MslScopedValue& sv = scopeValues.getReference(i);
        unbind(sv);
    }
    bound = false;
}

void MslVariable::unbind(MslScopedValue& sv)
{
    if (sv.value != nullptr) {
        if (pool != nullptr)
          pool->free(sv.value);
        else
          delete sv.value;
        sv.value = nullptr;
        sv.ival = 0;
        sv.bound = false;
    }
}    

MslVariableNode* MslVariable::getNode()
{
    return node;
}

void MslVariable::setNode(MslVariableNode* v)
{
    node = v;
    name = v->name;
    
    if (node->keywordScope) {
        // pre-allocate the scope value array
        // this can only happen at compile time which always happens
        // in the shell, so we may allocate memory
        // ugh, ensureStoreAllocated doesn't actually set the size, in this
        // usage I'd like it to actually have the max size so it can be treated
        // as a sparse array and doesn't have to be grown every damn time
        //scopeValues.ensureStorageAllocated(MaxScope + 1);
        // fill() doesn't take a size, how exactly are we supposed to do this?
        MslScopedValue empty;
        for (int i = 0 ; i < MaxScope + 1 ; i++)
          scopeValues.add(empty);
    }
}

//
// Non-scoped values
// Should only be used when it is known that track scope does not apply
// Try to get rid of this and always use the scoped accessors
//

void MslVariable::getValueUnscoped(MslValue* dest)
{
    dest->copy(&value);
}
    
void MslVariable::setValueUnscoped(MslValue* v)
{
    value.copy(v);
    bound = true;
}

/**
 * A variable is considered "bound" the first time it is assigned a value.
 * Once a variable is bound, the static initialization expression in the
 * related MslVariableNode will no longer be evaluated if it is encountered
 * during script evaluation.
 *
 * There are other ways to avoid this, MslEnvironment::initialize could simply evaluate
 * the initialization expression alone and do the assignment manually rather than evaluating
 * the MslAssignment that contains both the initializer and the variable LHS.
 */
bool MslVariable::isBoundUnscoped()
{
    return bound;
}

/**
 * Value assigner that factors in a scope Id.
 */
void MslVariable::setValue(int scopeId, MslValue* v)
{
    if (node == nullptr || !node->keywordScope || scopeId == 0) {
        // this is either a simple non-scoped value, or the type is not known
        value.copy(v);
        bound = true;
    }
    else if (scopeId < 0 || scopeId > (scopeValues.size() - 1)) {
        Trace(1, "MslVariable: Scope id out of range %d", scopeId);
    }
    else {
        MslScopedValue& sv = scopeValues.getReference(scopeId);

        if (sv.value != nullptr) {
            // we already promoted it, just stick it there
            sv.value->copy(v);
        }
        else if (v == nullptr) {
            // what does this mean?  not usually unbound, they can set it to nothing
            unbind(sv);
            sv.ival = 0;
        }
        else if (v->type == MslValue::Int || v->type == MslValue::Bool) {
            // the usual case
            sv.ival = v->getInt();
        }
        else {
            // fuck it, we're going MslValue
            MslValue* mv;
            if (pool != nullptr)
              mv = pool->allocValue();
            else
              mv = new MslValue();
            sv.value = mv;
            mv->copy(v);
        }

        // in all cases, once you assign something it is bound
        // and no longer goes back to the default
        sv.bound = true;
    }
}

/**
 * Return the value of a scoped variable, or the single
 * value if unscoped.
 *
 * If this is a scoped value, but the value for that scope is
 * not bound, then return the shared value which will have the results
 * of the static initialization expression if any.
 *
 * This allows scope specific values to all be initialized without actually
 * copying the iniitalizer result N times.
 */
void MslVariable::getValue(int scopeId, MslValue* dest)
{
    // let's treat scopeId as the default value for track variables
    // element zero in the array could also be used for this but that messes
    // up static initializers, ideally this should only be allowed for the
    // static initializer expression but we don't know from here if it is
    if (node == nullptr || !node->keywordScope || scopeId == 0) {
        // normal single-valued scope
        dest->copy(&value);
    }
    else if (scopeId < 0 || scopeId > (scopeValues.size() - 1)) {
        Trace(1, "MslVariable: Scope id out of range %d", scopeId);
    }
    else {
        MslScopedValue& sv = scopeValues.getReference(scopeId);

        if (!sv.bound) {
            // hasn't been bound yet, go the default static initializer
            dest->copy(&value);
        }
        else if (sv.value != nullptr) {
            // it was promoted
            dest->copy(sv.value);
        }
        else {
            // we lost the fact that this was a bool or int, but it shouldn't matter
            dest->setInt(sv.ival);
        }
    }
}

bool MslVariable::isBound(int scopeId)
{
    bool isBound = false;
    
    if (node == nullptr || !node->keywordScope || scopeId == 0) {
        // normal single-valued scope
        isBound = bound;
    }
    else if (scopeId < 0 || scopeId > (scopeValues.size() - 1)) {
        Trace(1, "MslVariable: Scope id out of range %d", scopeId);
    }
    else {
        MslScopedValue& sv = scopeValues.getReference(scopeId);

        // hmm, unclear...I think this should mean that the scope value
        // was explicitly set, not defaulting to the shared value
        isBound = sv.bound;
    }
    return isBound;
}

void MslVariable::unbind(int scopeId)
{
    if (node == nullptr || !node->keywordScope || scopeId == 0) {
        unbind();
    }
    else if (scopeId < 0 || scopeId > (scopeValues.size() - 1)) {
        Trace(1, "MslVariable: Scope id out of range %d", scopeId);
    }
    else {
        MslScopedValue& sv = scopeValues.getReference(scopeId);
        unbind(sv);
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
