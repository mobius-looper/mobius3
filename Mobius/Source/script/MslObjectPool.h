/**
 * A generic object pool that is extended with subclassing for specific
 * object classes.
 *
 * This started as a copy of the main Mobius model/ObjectPool but I want
 * to keep all MSL related code free of outside dependencies.
 *
 * It uses critical sections for pool locking, need  to revisit this to
 * use a lockless queue.
 *
 * ---
 *
 * Pooled objects are maintained on a simple critical section
 * protected linked list to avoid memory allocation.  Once removed
 * from a pool, a pooled object may be returned to the original pool,
 * any other pool of the same type, or simply deleted.  Deletion and allocation
 * of pooled objects should only be performed outside the audio thread.
 *
 * The pool is typically fluffed by a maintenance thread at regular intervals.
 *
 */

#pragma once

#include <JuceHeader.h>

/**
 * All classes that may be pooled must extend this interface.
 */
class MslPooledObject {

    friend class MslObjectPool;

  public:

    MslPooledObject();
    virtual ~MslPooledObject();

    // called when something leaves the pool to put it in a clean state
    virtual void poolInit() = 0;

    /**
     * Return an object to the pool it came from.
     * Not required, but convenient for deep code that
     * may not have direct access to the pool.  If the mPool
     * poitner is null, the object will simply be deleted.
     */
    void poolCheckin();

  protected:

    // some of these, in particular the pool chain could be of use
    // outside the ObjectPool, but keep the cards close for now
    
    void setPoolChain(MslPooledObject* p);
    MslPooledObject* getPoolChain();

    bool isPooled() {
        return mPooled;
    }

    void setPooled(bool b) {
        mPooled = b;
    }

    class MslObjectPool* getPool() {
        return mPool;
    }

    void setPool(class MslObjectPool* pool) {
        mPool = pool;
    }
    
  private:

    /**
     * Pool this object is currently in or was allocated from.
     * nullptr when the object was simply created with new.
     *
     * An object may be returned to the original pool by calling
     * checkin() but this is not required.
     */
    class MslObjectPool* mPool = nullptr;

    /**
     * Chain pointer for the pool's free list.
     */
    MslPooledObject* mPoolChain = nullptr;

    /**
     * True if the object is in the pool.  Can't use mPoolChain because
     * it will be null for the last object in the list.
     * This is only for a few sanity checks to detect objects returned
     * to a pool but still in use outside.
     */
    bool mPooled = false;
    
};

/**
 * An object pool maintains a linked list of available objects,
 * statistics about pool use, and utilitiies to manage pool size.
 */
class MslObjectPool
{
  public:
 
    /**
     * The inital size of the pool.

     * This should ideally be set high enough to avoid additional
     * allocations during normal use.
     */
    const int DefaultInitialSize = 20;

    /**
     * The threshold for new allocations.
     * If the free pool dips below this size, another block
     * is allocated.
     */
    const int DefaultSizeConcern = 5;

    /**
     * The number of objects to allocate when the SizeConern threshold is reached.
     */
    const int DefaultReliefSize = 10;

    /**
     * The number of returned by this pool and still in use, above which
     * we start to question our life choices.
     *
     * NOTE: This is a holdover from KernelCommunicator and doesn't work as well
     * if we allow pool swapping or deletion.
     */
    const int UseConcern = 3;
    
    MslObjectPool();
    virtual ~MslObjectPool();

    /**
     * Return an available object in the pool.
     * The chain pointer and the pooled flag will be cleared.
     */
    MslPooledObject* checkout();

    /**
     * Return an object to the pool.
     */
    void checkin(MslPooledObject* obj);

    /**
     * Extend the pool when necessary.
     */
    void fluff();

    /**
     * Delete all objects in the pool
     */
    void flush();

    /**
     * How was your day?
     */
    void traceStatistics();

    /**
     * Allocate a new object for this pool.
     * Must be overloaded by the subclass to allocate a suitable
     * MslPooledObject subclass.
     */
    virtual MslPooledObject* alloc() = 0;

    void setName(const char* argName) {
        name = argName;
    }

    void setObjectSize(int size) {
        objectSize = size;
    }
    
  private:

    /**
     * Critical section to surround additions and removals from the pool.
     */
    juce::CriticalSection criticalSection;

    // Sizing parameters, may be overloaded in subclasses
    int initialSize = DefaultInitialSize;
    int sizeConcern = DefaultSizeConcern;
    int reliefSize = DefaultReliefSize;

    // subclass better name this
    const char* name = "???";

    // size of one object, not necessary but useful when detecting
    // memory leaks, expected to be set by the subclass
    int objectSize = 0;

    // the total number of objects created with alloc()
    // normally also maxPool
    int totalCreated = 0;

    // the total number of objects requsted with checkout()
    int totalRequested = 0;

    // the total number of objects returned with return()
    int totalReturned = 0;
    
    // the total number of objects deleted during flush
    int totalDeleted = 0;

    // free pool
    MslPooledObject* pool = nullptr;
    int poolSize = 0;

    // number of objects checked out and still in use
    // todo: not used, and hard to keep track of if you allow
    // pool swapping
    int inUse = 0;

    // minimum size this pool reached
    int minSize = 0;

    // number of times the pool was extended
    int extensions = 0;
    
};    
    
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
