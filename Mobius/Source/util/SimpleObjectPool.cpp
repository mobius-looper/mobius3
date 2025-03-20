
#include <JuceHeader.h>

#include "Trace.h"

#include "SimpleObjectPool.h"

//////////////////////////////////////////////////////////////////////
//
// SimplePooledObject
//
//////////////////////////////////////////////////////////////////////

SimplePooledObject::SimplePooledObject()
{
    // hang breakpoints here
}

SimplePooledObject::~SimplePooledObject()
{
    // it is permissible to simply delete a formerly pooled object
    // but might want to check execution context and warn
    // if we're in an audio thread
}

void SimplePooledObject::setPoolChain(SimplePooledObject* obj)
{
    mPoolChain = obj;
}

SimplePooledObject* SimplePooledObject::getPoolChain()
{
    return mPoolChain;
}

/**
 * Convenience method to return the object to the pool from whence it came.
 * If the object doesn't know, could delete it, but if we're in the audio
 * thread it may be better to leak it.  Not supposed to happen.
 */
void SimplePooledObject::poolCheckin()
{
    if (mPool == nullptr) {
        Trace(1, "SimplePooledObject: I have no pool and I must scream\n");
    }
    else {
        mPool->checkin(this);
    }
}

//////////////////////////////////////////////////////////////////////
//
// SimpleObjectPool
//
//////////////////////////////////////////////////////////////////////

SimpleObjectPool::SimpleObjectPool(bool kernel)
{
    isKernel = kernel;
}

SimpleObjectPool::~SimpleObjectPool()
{
    // full stats when debugging, could simplify to
    // just tracing anomolies when things stabalize
    traceStatistics();
    flush();
}

SimplePooledObject* SimpleObjectPool::checkout()
{
    SimplePooledObject* obj = pool;
    if (obj != nullptr) {
        pool = obj->getPoolChain();
        poolSize--;
        if (poolSize < minSize)
          minSize = poolSize;

        obj->poolInit();
    }
    else {
        if (isKernel)
          Trace(1, "ObjectPool: %s Emergency kernel allocation", name);
        
        // subclass must overload this
        obj = alloc();
        totalCreated++;
    }
    obj->setPool(this);
    obj->setPooled(false);
    totalRequested++;
    
    return obj;
}

void SimpleObjectPool::checkin(SimplePooledObject* obj)
{
    if (obj != nullptr) {
        // if obj->getPool() != this we're swapping pools which
        // is allowed but might be interesting to trace
        if (obj->isPooled()) {
            Trace(1, "Checking in pooled object that thinks it's already pooled!\n");
            // leak it?
        }
        else {
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
 *
 * !! no, it can't work this way
 * the shell needs to allocate a chain of objects and pass it down
 */
void SimpleObjectPool::fluff()
{
    if (isKernel)
      Trace(1, "ObjectPool: %s Fluff called in the kernel", name);
    
    if (totalCreated == 0) {
        // we're initialzing
        for (int i = 0 ; i < initialSize ; i++) {
            SimplePooledObject* obj = alloc();
            checkin(obj);
        }
        minSize = initialSize;
        totalCreated = initialSize;
    }
    else if (poolSize < sizeConcern) {
        Trace(2, "SimpleObjectPool: %s pool extension by %ld from %ld\n", name, (long)reliefSize, (long)poolSize);

        for (int i = 0 ; i < reliefSize ; i++) {
            SimplePooledObject* obj = alloc();
            totalCreated++;
            checkin(obj);
        }
        extensions++;
    }
}

int SimpleObjectPool::getPoolSize()
{
    return poolSize;
}

/**
 * This is the method for the kernel.
 * Take a previously allocated list of objects.
 */
void SimpleObjectPool::fluff(SimplePooledObject* neu)
{
    while (neu != nullptr) {
        SimplePooledObject* next = neu->getPoolChain();
        neu->setPoolChain(pool);
        // not technically created but also not returned
        totalCreated++;
        neu = next;
    }
    extensions++;
}

/**
 * Return all object in the pool to the system memory manager
 * This is intended only for the shutdown phase and must not
 * be called on the audio thread or when there could be
 * any pool contention.
 */
void SimpleObjectPool::flush()
{
    while (pool != nullptr) {
        SimplePooledObject* next = pool->getPoolChain();
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
void SimpleObjectPool::traceStatistics()
{
    char tracebuf[1024];
    snprintf(tracebuf, sizeof(tracebuf),
             "SimpleObjectPool %s: Created %d Pool %d Min %d Extensions %d Size %d",
             name, totalCreated, poolSize, minSize, extensions, objectSize);

    Trace(2, tracebuf);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
