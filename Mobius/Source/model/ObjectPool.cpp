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
void PooledObject::poolCheckin()
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
    flush();
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
            // these aren't bad but can be useful to track pool corruption

            // this one happens normally when you load a MIDI sequence from a file
            // in the UI and then pass it down to the kernel where it is reclaimed
            // need to give it a pool on the transition or find another way to track
            // this to avoid dumping fear into the logs
            //if (obj->getPool() == nullptr)
            //Trace(2, "ObjectPool: Warning: Checking in object without a pool");

            // sigh, this one happens a lot on shutdown when MidiSequences that have been
            // loaded into tracks get destructed by Supervisor
            // 
            //if (obj->getPool() != this)
            //Trace(2, "ObjectPool: Warning: Checking in object from another pool");

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
void ObjectPool::fluff()
{
    if (totalCreated == 0) {
        // we're initialzing
        for (int i = 0 ; i < initialSize ; i++) {
            PooledObject* obj = alloc();
            obj->setPool(this);
            checkin(obj);
        }
        minSize = initialSize;
        totalCreated = initialSize;
    }
    else if (poolSize < sizeConcern) {
        Trace(2, "ObjectPool: %s pool extension by %ld from %ld\n", name, (long)reliefSize, (long)poolSize);

        for (int i = 0 ; i < reliefSize ; i++) {
            PooledObject* obj = alloc();
            obj->setPool(this);
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
void ObjectPool::flush()
{
    while (pool != nullptr) {
        PooledObject* next = pool->getPoolChain();
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
void ObjectPool::traceStatistics()
{
    char tracebuf[1024];
    snprintf(tracebuf, sizeof(tracebuf),
             "ObjectPool %s: Created %d Pool %d Min %d Extensions %d Size %d",
             name, totalCreated, poolSize, minSize, extensions, objectSize);

    Trace(2, tracebuf);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
