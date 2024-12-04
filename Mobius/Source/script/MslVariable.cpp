/**
 * Value handling for static variables.
 *
 * The term "static" applies to any variable whose value is not maintained
 * in a transient MslBinding on the stack.  It is instead maintained inside
 * a shared MslVariable held by the compilation unit (MslCompilation).
 *
 * This includes variables declared as:
 *
 *     static or global
 *       - visible only within this unit
 *     public
 *       - visible to other scripts
 *     export
 *       - visible to other scripts and the containing application
 *
 *     track or scope
 *       - separate value maintained for each application scope, which in current
 *         usage is a "track" though it could be anything
 *
 *
 * todo: need to work out how track variables will be referenced and stored
 *
 */

#include "MslValue.h"
#include "MslVariable.h"

MslVariable::MslVariable()
{
}

MslVariable::~MslVariable()
{
    // if we are a track variable, need to clean up the explored MslValues
    for (int i = 0 ; i < scopedValues.size() ; i++) {
        MslScopedValue& sv = scopedValues[i].getReference();
        if (sv.value != nullptr) {
            delete sv.value;
            sv.value = nullptr;
        }
    }
}

MslVariableNode* MslVariable::getNode()
{
    return node;
}

void MslVariable::setNode(MslVariableNode* v)
{
    node = v;
    if (node->keywordScope) {
        // this is a special scoped variable that needs an array of values
        scopedValues.ensureStorageAllocated(MaxScopes);
    }
}

MslValue* MslVariable::getValue()
{
    return &value;
}
    
void MslVariable::setValue(MslValue* v)
{
    value.copy(v);
    bound = true;
}

bool MslVariable::isBound()
{
    return bound;
}

void MslVariable::unbind()
{
    value.setNull();
    bound = false;
}

void MslVariable::setValue(int scopeId, MslValue* v)
{
    if (scopeId < 0 || scopeId > (scopedValues.size() - 1)) {
        Trace(1, "MslVariable: Scope id out of range %d", scopeId);
    }
    else {
        MslScopedValue& sv = scopedValues[scopeId].getReference();

        if (sv.value != nullptr) {
            // we already promoted it, just stick it there
            sv.value->copy(v);
        }
        else if (v == nullptr) {
            // what does this mean?  not usually unbound, they can set it to nothing
            sv.ival = 0;
        }
        else if (v->type == MslValue::TypeInt || v->type == MslValue::TypeBool) {
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

        // in call cases, once you assign something it is bound
        // and no longer goes back to the default
        sv.bound = true;
    }
}

// think: unscoped getValue should work the same way
void MslVariable::getValue(int scopeId, MslValue* dest)
{
    if (scopeId < 0 || scopeId > (scopedValues.size() - 1)) {
        Trace(1, "MslVariable: Scope id out of range %d", scopeId);
    }
    else {
        MslScopedValue& sv = scopedValues[scopeId].getReference();

        if (!sv.bound) {
            // hasn't been bound yet, go the default static initializer
            dest->copy(&value);
        }
        else if (sv.value != nullptr) {
            // it was promoted
            deat->copy(sv.value);
        }
        else {
            // we lost the fact that this was a bool or int, but it shouldn't matter
            dest->setInt(sv.ival);
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
