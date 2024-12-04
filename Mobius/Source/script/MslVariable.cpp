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
