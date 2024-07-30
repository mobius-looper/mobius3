/**
 * A collection of object pools managed by the MslEnvironment.
 */

#include "../util/Trace.h"

#include "MslValue.h"
#include "MslError.h"
#include "MslResult.h"
#include "MslStack.h"
#include "MslSession.h"
#include "MslBinding.h"

#include "MslPools.h"

///////////////////////////////////////////////////////////////////////////////
//
// Global Pool Management
//
///////////////////////////////////////////////////////////////////////////////

MslPools::MslPools(MslEnvironment* env)
{
    environment = env;
}

MslPools::~MslPools()
{
    Trace(2, "MslPools: destructing");
    // try to do these in reverse depenndency order, though shouldn't be necessary
    flushSessions();
    flushStacks();
    flushBindings();
    flushResults();
    flushErrors();
    flushValues();

    traceStatistics();
}

/**
 * Obviously not thread safe, but intended for use only during shutdown
 */
void MslPools::traceStatistics()
{
    Trace(2, "MslPools: created/requested/returned/deleted/pooled");

    // sigh, it sure would be nice if these had a common superclass for the chain pointer
    int count = 0;
    for (MslValue* obj = valuePool ; obj != nullptr ; obj = obj->next) count++;
    Trace(2, "  values: %d %d %d %d",
          valuesCreated, valuesRequested, valuesReturned, valuesDeleted);

    count = 0;
    for (MslError* obj = errorPool ; obj != nullptr ; obj = obj->next) count++;
    Trace(2, "  errors: %d %d %d %d",
          errorsCreated, errorsRequested, errorsReturned, errorsDeleted);

    count = 0;
    for (MslResult* obj = resultPool ; obj != nullptr ; obj = obj->next) count++;
    Trace(2, "  results: %d %d %d %d",
          resultsCreated, resultsRequested, resultsReturned, resultsDeleted);

    count = 0;
    for (MslBinding* obj = bindingPool ; obj != nullptr ; obj = obj->next) count++;
    Trace(2, "  bindings: %d %d %d %d",
          bindingsCreated, bindingsRequested, bindingsReturned, bindingsDeleted);

    count = 0;
    for (MslStack* obj = stackPool ; obj != nullptr ; obj = obj->parent) count++;
    Trace(2, "  stacks: %d %d %d %d",
          stacksCreated, stacksRequested, stacksReturned, stacksDeleted);

    count = 0;
    for (MslSession* obj = sessionPool ; obj != nullptr ; obj = obj->next) count++;
    Trace(2, "  sessions: %d %d %d %d",
          sessionsCreated, sessionsRequested, sessionsReturned, sessionsDeleted);
}

void MslPools::initialize()
{
    // todo: get initializes of these from a config
}

void MslPools::fluff()
{
    // todo: have the maintenance thread call this
    // with a configurable set of thresholds and growth sizes
}

///////////////////////////////////////////////////////////////////////////////
//
// Values
//
///////////////////////////////////////////////////////////////////////////////

/**
 * MslValue will cascade delete the chain, but for some reason I decided to
 * make them go one at a time here.
 * Should just reduce to deleting the valuePool.
 */ 
void MslPools::flushValues()
{
    while (valuePool != nullptr) {
        MslValue* next = valuePool->next;
        valuePool->next = nullptr;
        delete valuePool;
        valuePool = next;
        valuesDeleted++;
    }
}

MslValue* MslPools::allocValue()
{
    MslValue* v = nullptr;

    if (valuePool != nullptr) {
        v = valuePool;
        valuePool = valuePool->next;
        v->next = nullptr;
        // this does full initialization
        v->setNull();
    }
    else {
        // complain loudly that the fluffer isn't doing it's job
        v = new MslValue();
        valuesCreated++;
    }
    valuesRequested++;
    return v;
}

/**
 * Values are complex because they can be ON a list and HAVE a list.
 * The entire lists are freeded along with the containing value.
 *
 * It is usually convenient to handle the next chain for these though
 * the others don't do that.
 */
void MslPools::free(MslValue* v)
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
        valuesReturned++;
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
        errorsDeleted++;
    }
}

MslError* MslPools::allocError()
{
    MslError* e = nullptr;

    if (errorPool != nullptr) {
        e = errorPool;
        errorPool = errorPool->next;
        e->next = nullptr;

        e->init();
    }
    else {
        e = new MslError();
        errorsCreated++;
    }
    errorsRequested++;
    return e;
}

void MslPools::free(MslError* e)
{
    if (e != nullptr) {
        e->next = errorPool;
        errorPool = e;
        errorsReturned++;
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
        resultsDeleted++;
    }
}

MslResult* MslPools::allocResult()
{
    MslResult* r = nullptr;

    if (resultPool != nullptr) {
        r = resultPool;
        resultPool = resultPool->next;
        r->next = nullptr;

        r->init();
    }
    else {
        // complain loudly that the fluffer isn't doing it's job
        r = new MslResult();
        resultsCreated++;
    }
    resultsRequested++;
    return r;
}

void MslPools::free(MslResult* r)
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
        resultsReturned++;
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
        // bindings cascade delete
        // though we might want to override that to gather statistics?
        MslBinding* next = bindingPool->next;
        bindingPool->next = nullptr;
        delete bindingPool;
        bindingPool = next;
        bindingsDeleted++;
    }
}

MslBinding* MslPools::allocBinding()
{
    MslBinding* b = nullptr;

    // todo: need a csect here
    if (bindingPool != nullptr) {
        b = bindingPool;
        bindingPool = bindingPool->next;
        b->next = nullptr;

        if (b->value != nullptr) {
            // should have been cleansed by now
            Trace(1, "Dirty Binding in the pool");
            free(b->value);
            b->value = nullptr;
        }

        b->init();
    }
    else {
        // complain
        b = new MslBinding();
        bindingsCreated++;
    }
    bindingsRequested++;
    return b;
}

void MslPools::free(MslBinding* b)
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
        bindingsReturned++;
    }
}

//////////////////////////////////////////////////////////////////////
//
// Stack
//
//////////////////////////////////////////////////////////////////////

/**
 * Stacks are unusual because the chain pointer is "parent" rather than "next"
 * They do NOT cascade delete
 */
void MslPools::flushStacks()
{
    while (stackPool != nullptr) {
        MslStack* next = stackPool->parent;
        stackPool->parent = nullptr;
        delete stackPool;
        stackPool = next;
        stacksDeleted++;
    }
}

MslStack* MslPools::allocStack()
{
    MslStack* s = nullptr;
    if (stackPool != nullptr) {
        s = stackPool;
        stackPool = stackPool->parent;
        s->parent = nullptr;

        // check dangling resources that shouldn't be here
        if (s->childResults != nullptr) {
            Trace(1, "MslPools: Lingering child result in pooled stack");
            free(s->childResults);
            s->childResults = nullptr;
        }
        if (s->bindings != nullptr) {
            Trace(1, "MslPools: Lingering child bindings in pooled stack");
            free(s->bindings);
            s->bindings = nullptr;
        }

        s->init();
    }
    else {
        s = new MslStack();
        stacksCreated++;
    }
    stacksRequested++;
    return s;
}

/**
 * Stacks don't have the usual "next" pointer, they have a "parent" pointer.
 * It is the norm to free a stack frame but keep the parent frames so the
 * default free does NOT cascade.
 */
void MslPools::free(MslStack* s)
{
    if (s != nullptr) {
        // release resources as soon as it goes back in the pool
        free(s->childResults);
        s->childResults = nullptr;
        free(s->bindings);
        s->bindings = nullptr;

        free(s->inList);
        s->inList = nullptr;
        s->inPtr = nullptr;

        s->parent = stackPool;
        stackPool = s;
        stacksReturned++;
    }
}

/**
 * In cases where you DO want to cascade free the stack list, use this.
 * Should only be done by MslSession itself.
 */
void MslPools::freeList(MslStack* s)
{
    if (s != nullptr) {
        while (s != nullptr) {
            MslStack* next = s->parent;
            s->parent = nullptr;
            free(s);
            s = next;
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Session
//
//////////////////////////////////////////////////////////////////////

void MslPools::flushSessions()
{
    while (sessionPool != nullptr) {
        MslSession* next = sessionPool->next;
        sessionPool->next = nullptr;
        delete sessionPool;
        sessionPool = next;
        sessionsDeleted++;
    }
}

MslSession* MslPools::allocSession()
{
    MslSession* s = nullptr;
    if (sessionPool != nullptr) {
        s = sessionPool;
        sessionPool = sessionPool->next;
        s->next = nullptr;

        // check dangling resources that shouldn't be here
        if (s->rootValue != nullptr) {
            Trace(1, "MslPools: Lingering root result in pooled session");
            free(s->rootValue);
            s->rootValue = nullptr;
        }
        if (s->stack != nullptr) {
            Trace(1, "MslPools: Lingering stack in pooled stack");
            freeList(s->stack);
            s->stack = nullptr;
        }
        if (s->errors != nullptr) {
            Trace(1, "MslPools: Lingering errors in pooled stack");
            free(s->errors);
            s->errors = nullptr;
        }

        s->init();
    }
    else {
        // this one is unusual because it requires an environment in the constructor
        s = new MslSession(environment);
        sessionsCreated++;
    }
    
    sessionsRequested++;
    return s;
}

void MslPools::free(MslSession* s)
{
    if (s != nullptr) {
        // release the stacks, note use of freeList
        freeList(s->stack);
        s->stack = nullptr;

        // release the root value, this one cascades
        free(s->rootValue);
        s->rootValue = nullptr;

        // errors also cascade
        free(s->errors);
        s->errors = nullptr;

        s->next = sessionPool;
        sessionPool = s;
        
        sessionsReturned++;
    }
}
        
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
