/**
 * Common base classes for pooled objects.
 * Also too, the UIActionPool
 */

// for ScopedLock
#include <JuceHeader.h>

#include "../util/Trace.h"
#include "UIAction.h"

#include "ObjectPool.h"

//////////////////////////////////////////////////////////////////////
//
// PooledObject
//
//////////////////////////////////////////////////////////////////////

PooledObject::PooledObject()
{
    // hang breakpoints here
}

PooledObject::~PooledObject()
{
    // it is permissible to simply delete a formerly pooled object
    // but might want to check execution context and warn
    // if we're in an audio thread
}

void PooledObject::setPoolChain(PooledObject* obj)
{
    mPoolChain = obj;
}

PooledObject* PooledObject::getPoolChain()
{
    return mPoolChain;
}

/**
 * Convenience method to return the object to the pool from whence it came.
 * If the object doesn't know, could delete it, but if we're in the audio
 * thread it may be better to leak it.  Not supposed to happen.
 */
void PooledObject::checkin()
{
    if (mPool == nullptr) {
        Trace(1, "PooledObject: I have no pool and I must scream\n");
    }
    else {
        mPool->checkin(this);
    }
}

//////////////////////////////////////////////////////////////////////
//
// ObjectPool
//
//////////////////////////////////////////////////////////////////////

ObjectPool::ObjectPool()
{
}

ObjectPool::~ObjectPool()
{
    // full stats when debugging, could simplify to
    // just tracing anomolies when things stabalize
    traceStatistics();

    // delete any lingering objects still in the pool
    while (pool != nullptr) {
        PooledObject* next = pool->getPoolChain();
        pool->setPoolChain(nullptr);
        delete pool;
        pool = next;
    }
}

PooledObject* ObjectPool::checkout()
{
    // if we have to allocate, could do that outside the csect
    // but it makes statistics management  messier
    juce::ScopedLock lock (criticalSection);
    
    PooledObject* obj = pool;
    if (obj != nullptr) {
        pool = obj->getPoolChain();
        poolSize--;
        if (poolSize < minSize)
          minSize = poolSize;
    }
    else {
        // subclass must overload this
        obj = alloc();
        totalCreated++;
    }
    obj->setPool(this);
    obj->setPooled(false);
    
    return obj;
}

void ObjectPool::checkin(PooledObject* obj)
{
    if (obj != nullptr) {
        // if obj->getPool() != this we're swapping pools which
        // is allowed but might be interesting to trace
        if (obj->isPooled()) {
            Trace(1, "Checking in pooled object that thinks it's already pooled!\n");
            // leak it?
        }
        else {
            juce::ScopedLock lock (criticalSection);
            obj->setPoolChain(pool);
            pool = obj;
            poolSize++;
            obj->setPooled(true);
            obj->setPool(this);
        }
    }
}

/**
 * Ensure that the pool has a comfortable number of objects
 * available for use.
 */
void ObjectPool::checkCapacity()
{
    if (totalCreated == 0) {
        // we're initialzing
        for (int i = 0 ; i < initialSize ; i++) {
            PooledObject* obj = alloc();
            checkin(obj);
        }
        minSize = initialSize;
        totalCreated = initialSize;
    }
    else if (poolSize < sizeConcern) {
        Trace(2, "ObjectPool: %s pool extension by %ld from %ld\n", name, (long)reliefSize, (long)poolSize);

        for (int i = 0 ; i < reliefSize ; i++) {
            PooledObject* obj = alloc();
            totalCreated++;
            checkin(obj);
        }
        extensions++;
    }
}

/**
 * Trace interesting statistics about the pool
 * Not bothering with a csect on this one but it's easy enough.
 * Depending on the trace interval it's going to hard to catch this
 * in action, but the maximums are interesting.
 */
void ObjectPool::traceStatistics()
{
    Trace(2, "ObjectPool %s statistics\n", name);
    Trace(2, "  Created %ld\n", (long)totalCreated);
    Trace(2, "  Pool size %ld\n", (long)poolSize);
    Trace(2, "  Min pool %d\n", (long)minSize);
    Trace(2, "  Extensions %ld\n", (long)extensions);
}

//////////////////////////////////////////////////////////////////////
//
// UIActionPool
//
// This is the first one that started the pooling trend.
// Had to move to model so it could subclass PooledObject, though
// only MoboiusShell should need a pool.
//
//////////////////////////////////////////////////////////////////////

UIActionPool::UIActionPool()
{
    setName("UIAction");
    checkCapacity();
}

UIActionPool::~UIActionPool()
{
}

/**
 * ObjectPool overload to create a new pooled object.
 */
PooledObject* UIActionPool::alloc()
{
    return new UIAction();
}

/**
 * Accessor for most of the code that does the convenient downcast.
 */
UIAction* UIActionPool::newAction()
{
    return (UIAction*)checkout();
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
