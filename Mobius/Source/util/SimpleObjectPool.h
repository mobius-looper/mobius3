/**
 * Very basic object pool with pooled object superclass.
 * Lock free and may only be used in one thread.
 */

#pragma once

#include <JuceHeader.h>

/**
 * All classes that may be pooled must extend this interface.
 */
class SimplePooledObject {

    friend class SimpleObjectPool;

  public:

    SimplelPooledObject();
    virtual ~SimplePooledObject();

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
    
    void setPoolChain(SimplePooledObject* p);
    SimplePooledObject* getPoolChain();

    bool isPooled() {
        return mPooled;
    }

    void setPooled(bool b) {
        mPooled = b;
    }

    class SimpleObjectPool* getPool() {
        return mPool;
    }

    void setPool(class SimpleObjectPool* pool) {
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
    class SimpleObjectPool* mPool = nullptr;

    /**
     * Chain pointer for the pool's free list.
     */
    SimplePooledObject* mPoolChain = nullptr;

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
class SimpleObjectPool
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
    
    SimpleObjectPool(bool kernel);
    virtual ~SimpleObjectPool();

    /**
     * Retrn the number of objects in the pool.
     * Used by the shell to determine whether it needs to be fluffed.
     */
    int getPoolSize();
    
    /**
     * Return an available object in the pool.
     * The chain pointer and the pooled flag will be cleared.
     */
    SimplePooledObject* checkout();

    /**
     * Return an object to the pool.
     */
    void checkin(SimplePooledObject* obj);

    /**
     * Extend the pool when necessary.
     * Only for the shell thread.
     */
    void fluff();

    /**
     * Extend the pool in the kernel with objects allocated
     * by the shell.
     */
    void fluff(SimplePooledObject* neu);
    
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
     * SimplePooledObject subclass.
     */
    virtual SimplePooledObject* alloc() = 0;

    void setName(const char* argName) {
        name = argName;
    }

    void setObjectSize(int size) {
        objectSize = size;
    }
    
  private:

    // Sizing parameters, may be overloaded in subclasses
    int initialSize = DefaultInitialSize;
    int sizeConcern = DefaultSizeConcern;
    int reliefSize = DefaultReliefSize;

    // subclass better name this
    const char* name = "???";

    // passed through the construcor
    // indiciates that this pool should not be allocating memory
    bool isKernel = false;

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
    SimplePooledObject* pool = nullptr;
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
