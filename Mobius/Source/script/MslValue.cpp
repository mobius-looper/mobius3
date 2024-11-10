
#include "../util/Trace.h"
#include "../util/Util.h"

#include "MslBinding.h"
#include "MslValue.h"

MslValue::MslValue()
{
    setNull();
}

/**
 * Ownership of the chain pointer and the sublist pointer is touchy.
 * We could do it here, or expect the object pool to deal with it.
 * Normally these would go back ot the pool in an orderly way but
 * I don't want to introduce a pool dependency just to be able to delete
 * them if you hit some situation that doesn't allow orderly cleanup, and
 * you need to cleanup at shutdown to avoid leak warnings.
 */
MslValue::~MslValue()
{
    // the remainder of the list I'm on
    delete next;
    // and the values I contain
    delete list;
}

/**
 * Copy one value to another.
 * Mostly to copy binding values which are expected to be atomic.
 */
void MslValue::copy(MslValue* src)
{
    type = src->type;
    ival = src->ival;
    fval = src->fval;
    strcpy(string, src->string);

    // I suppose we could support these, but needs more thought if you do
    // bindings will always be atomic, right?
    // don't whine about this, it is now being used to copy MslRequest arguemnts
    // into bindings
    //if (src->next != nullptr)
    //Trace(1, "MslValue: Unable to copy value on a list");

    if (src->list != nullptr)
      Trace(1, "MslValue: Unable to copy list value");
      
}

/**
 * List manipulation sucks because we're not keeping a tail pointer.
 * But lists during MSL runtime are almost always very small so it doesn't matter much.
 *
 * Since there isn't a container we've got the usuall starting nullness problem.
 * Caller will have to deal with that.
 */
MslValue* MslValue::getLast()
{
    MslValue* ptr = this;
    while (ptr->next != nullptr)
      ptr = ptr->next;
    return ptr;
}

void MslValue::append(MslValue* v)
{
    getLast()->next = v;
}

int MslValue::size()
{
    int count = 0;
    MslValue* ptr = this;
    while (ptr != nullptr) {
        count++;
        ptr = ptr->next;
    }
    return count;
}

MslValue* MslValue::get(int index)
{
    MslValue* ptr = this;
    for (int i = 0 ; i < index && ptr != nullptr ; i++) {
        ptr = ptr->next;
    }
    return ptr;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
