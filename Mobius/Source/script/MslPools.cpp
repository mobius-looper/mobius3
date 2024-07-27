/**
 * A collection of object pools managed by the MslEnvironment.
 */

#include "MslValue.h"
#include "MslError.h"
#include "MslResult.h"
// also includes MslStack
#include "MslSession.h"
#include "MslBinding.h"

#include "MslPools.h"

///////////////////////////////////////////////////////////////////////////////
//
// Global Pool Management
//
///////////////////////////////////////////////////////////////////////////////

MslPools::MslPools()
{
}

MslPools::~MslPools()
{
    // try to do these in reverse depenndency order, though shouldn't be necessary?
    flushSessions();
    flushStacks();
    flushBindings();
    flushResults();
    flushErrors();
    flushValues();
}

void MslPools::initialize()
{
    // todo: get initializes of these from a config
}

void MslPools::fluff()
{
}

///////////////////////////////////////////////////////////////////////////////
//
// Values
//
///////////////////////////////////////////////////////////////////////////////

void MslPools::flushValues()
{
    // assuming that we don't have any child lists to deal with at this point
    while (valuePool != nullptr) {
        MslValue* next = valuePool->next;
        valuePool->next = nullptr;
        delete valuePool;
        valuePool = next;
    }
}

MslValue* MslValuePool::allocValue()
{
    MslValue* v = nullptr;

    if (valuePool != nullptr) {
        v = valuePool;
        valuePool = valuePool->next;
        v->next = nullptr;
        // initialize it
        v->setNull();
    }
    else {
        // complain loudly that the fluffer isn't doing it's job
        v = new MslValue();
    }
    return v;
}

/**
 * Values are complex because they can be ON a list and HAVE a list.
 * The entire lists are freeded along with the containing value.
 *
 * It is usually convenient to handle the next chain for these though
 * the others don't do that.
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
        v->next = valuePool;
        valuePool = v;
    }
}

//////////////////////////////////////////////////////////////////////
//
// Errors
//
//////////////////////////////////////////////////////////////////////

void MslPools::flushErrors()
{
    while (errorPool != nullptr) {
        MslError* next = errorPool->next;
        errorPool->next = nullptr;
        delete errorPool;
        errorPool = next;
    }
}

MslError* MslValuePool::allocError()
{
    MslError* e = nullptr;

    // todo: need a csect here
    if (errorPool != nullptr) {
        e = errorPool;
        errorPool = errorPool->next;
        e->next = nullptr;
        // make sure it goes out initialized
        e->init();
    }
    else {
        e = new MslError();
    }
    return e;
}

void MslValuePool::free(MslError* e)
{
    if (e != nullptr) {
        // release the value back to the pool
        if (r->value != nullptr) {
            // this does the entire list
            free(r->value);
            r->value = nullptr;
        }

        while (r->errors != nullptr) {
            MslError* next = r->errors->next;
            r->errors->next = nullptr;
            free(r->errors);
            r->errors = next;
        }
        
        // and now me
        r->next = errorPool;
        errorPool = r;
    }
}

//////////////////////////////////////////////////////////////////////
//
// Result
//
//////////////////////////////////////////////////////////////////////

void MslPools::flushResults()
{
    while (resultPool != nullptr) {
        MslResult* next = resultPool->next;
        resultPool->next = nullptr;
        delete resultPool;
        resultPool = next;
    }
}

MslResult* MslValuePool::allocResult()
{
    MslResult* r = nullptr;

    // todo: need a csect here
    if (resultPool != nullptr) {
        r = resultPool;
        resultPool = resultPool->next;
        r->next = nullptr;
        // make sure it goes out initialized
        r->init();
    }
    else {
        // complain loudly that the fluffer isn't doing it's job
        r = new MslResult();
    }
    return b;
}

void MslValuePool::free(MslResult* r)
{
    if (r != nullptr) {
        // release the value back to the pool
        if (r->value != nullptr) {
            // this does the entire list
            free(r->value);
            r->value = nullptr;
        }

        while (r->errors != nullptr) {
            MslError* next = r->errors->next;
            r->errors->next = nullptr;
            free(r->errors);
            r->errors = next;
        }
        
        // and now me
        r->next = resultPool;
        resultPool = r;
    }
}


//////////////////////////////////////////////////////////////////////
//
// Bindings
//
//////////////////////////////////////////////////////////////////////

void MslPools::flushBindings()
{
    while (bindingPool != nullptr) {
        MslBinding* next = bindingPool->next;
        bindingPool->next = nullptr;
        delete bindingPool;
        bindingPool = next;
    }
}

MslBinding* MslValuePool::allocBinding()
{
    MslBinding* b = nullptr;

    // todo: need a csect here
    if (bindingPool != nullptr) {
        b = bindingPool;
        bindingPool = bindingPool->next;
        b->next = nullptr;

        // make sure it goes out initialized
        strcpy(b->name, "");
        b->position = 0;
        
        if (b->value != nullptr) {
            // should have been cleansed by now
            Trace(1, "Dirty Binding in the pool");
            free(b->value);
            b->value = nullptr;
        }
delete 
        // complain;
        b = new MslBinding();
    }
    return b;
}

void MslValuePool::free(MslBinding* b)
{
    if (b != nullptr) {
        // release the value back to the pool
        if (b->value != nullptr) {
            free(b->value);
            b->value = nullptr;
        }
        
        // free the remainder of the list
        free(b->next);
        b->next = nullptr;
        // and now me
        b->next = bindingPool;
        bindingPool = b;
    }
}

