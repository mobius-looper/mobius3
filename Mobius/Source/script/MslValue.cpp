
#include "MslValue.h"


//////////////////////////////////////////////////////////////////////
//
// MslValue
//
//////////////////////////////////////////////////////////////////////

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

//////////////////////////////////////////////////////////////////////
//
// Pool
//
// This isn't actually a good pool yet, it allocates on a whim and
// isn't concerned with thread safety, but enough for initial testing.
//
//////////////////////////////////////////////////////////////////////

MslValuePool::MslValuePool()
{
    pool = nullptr;
}

MslValuePool::~MslValuePool()
{
    // assuming that we don't have any child lists to deal with at this point
    while (pool != nullptr) {
        MslValue* next = pool->next;
        pool->next = nullptr;
        delete pool;
        pool = next;
    }
}

MslValue* MslValuePool::alloc()
{
    MslValue* v = nullptr;

    // todo: need a csect here
    if (pool != nullptr) {
        v = pool;
        pool = pool->next;
        v->next = nullptr;
    }
    else {
        // complain loudly that the fluffer isn't doing it's job
        v = new MslValue();
    }
    return v;
}

/**
 * Here we've go the ownership questions.
 * I'm starting with the expectation that this releases everything
 * connected back to the pool and if you need to retain anything it needs
 * to be spiced out manually.  How often does that happen?
 */
void MslValuePool::free(MslValue* v)
{
    if (v != nullptr) {
        // first the list I HAVE
        free(v->list);
        v->list = nullptr;
        // then the list I'm ON
        free(v->next);
        v->next = nullptr;
        // and now me
        v->next = pool;
        pool = v;
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
