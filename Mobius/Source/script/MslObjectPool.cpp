/**
 * Common base classes for pooled objects.
 */

// for ScopedLock
#include <JuceHeader.h>

#include "../util/Trace.h"

#include "MslObjectPool.h"

//////////////////////////////////////////////////////////////////////
//
// MslPooledObject
//
//////////////////////////////////////////////////////////////////////

MslPooledObject::MslPooledObject()
{
    // hang breakpoints here
}

MslPooledObject::~MslPooledObject()
{
    // it is permissible to simply delete a formerly pooled object
    // but might want to check execution context and warn
    // if we're in an audio thread
}

void MslPooledObject::setPoolChain(MslPooledObject* obj)
{
    mPoolChain = obj;
}

MslPooledObject* MslPooledObject::getPoolChain()
{
    return mPoolChain;
}

/**
 * Convenience method to return the object to the pool from whence it came.
 * If the object doesn't know, could delete it, but if we're in the audio
 * thread it may be better to leak it.  Not supposed to happen.
 */
void MslPooledObject::poolCheckin()
{
    if (mPool == nullptr) {
        Trace(1, "MslPooledObject: I have no pool and I must scream\n");
    }
    else {
        mPool->checkin(this);
    }
}

//////////////////////////////////////////////////////////////////////
//
// MslObjectPool
//
//////////////////////////////////////////////////////////////////////

MslObjectPool::MslObjectPool()
{
}

MslObjectPool::~MslObjectPool()
{
    // full stats when debugging, could simplify to
    // just tracing anomolies when things stabalize
    traceStatistics();
    flush();
}

MslPooledObject* MslObjectPool::checkout()
{
    // if we have to allocate, could do that outside the csect
    // but it makes statistics management  messier
    juce::ScopedLock lock (criticalSection);
    
    MslPooledObject* obj = pool;
    if (obj != nullptr) {
        pool = obj->getPoolChain();
        poolSize--;
        if (poolSize < minSize)
          minSize = poolSize;

        obj->poolInit();
    }
    else {
        // subclass must overload this
        obj = alloc();
        totalCreated++;
    }
    obj->setPool(this);
    obj->setPooled(false);
    totalRequested++;
    
    return obj;
}

void MslObjectPool::checkin(MslPooledObject* obj)
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

        // keep it clean in the pool for debugging
        obj->poolInit();
        
        totalReturned++;
    }
}

/**
 * Ensure that the pool has a comfortable number of objects
 * available for use.
 */
void MslObjectPool::fluff()
{
    if (totalCreated == 0) {
        // we're initialzing
        for (int i = 0 ; i < initialSize ; i++) {
            MslPooledObject* obj = alloc();
            checkin(obj);
        }
        minSize = initialSize;
        totalCreated = initialSize;
    }
    else if (poolSize < sizeConcern) {
        Trace(2, "MslObjectPool: %s pool extension by %ld from %ld\n", name, (long)reliefSize, (long)poolSize);

        for (int i = 0 ; i < reliefSize ; i++) {
            MslPooledObject* obj = alloc();
            totalCreated++;
            checkin(obj);
        }
        extensions++;
    }
}

/**
 * Return all object in the pool to the system memory manager
 * This is intended only for the shutdown phase and must not
 * be called on the audio thread or when there could be
 * any pool contention.
 */
void MslObjectPool::flush()
{
    while (pool != nullptr) {
        MslPooledObject* next = pool->getPoolChain();
        pool->setPoolChain(nullptr);
        delete pool;
        pool = next;
    }
}

/**
 * Trace interesting statistics about the pool
 * Not bothering with a csect on this one but it's easy enough.
 * Depending on the trace interval it's going to hard to catch this
 * in action, but the maximums are interesting.
 */
void MslObjectPool::traceStatistics()
{
    char tracebuf[1024];
    snprintf(tracebuf, sizeof(tracebuf),
             "MslObjectPool %s: Created %d Pool %d Min %d Extensions %d Size %d",
             name, totalCreated, poolSize, minSize, extensions, objectSize);

    Trace(2, tracebuf);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
