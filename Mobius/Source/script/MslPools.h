/**
 * A collection of object pools for all of the pooled objects used within MSL.
 *
 * While each object may reference it's own pool, it may also have a pointer
 * to the "pool manager" that knows about all the other pools.  This alllows a few
 * objects that have inter-dependencies to free themselves cleanly and allocate
 * new child objects from the type-specific pool.
 *
 * Newer object pools are implemented with MslObjectPool.
 *
 * Older pooled objects are implemented with a much more rough pooling convention
 * that will be replaced over time.
 * 
 * !! Need csects around most of these since there can be concurrent access between
 * the audio and shell threads.
 *
 * Design Notes
 *
 * MSL differs from typical scripting systems in that it was designed to
 * run in the Audio Thread.  This is the high priority system thread that is responding
 * to block requests from the audio interface.  If you're an old fart, you might think
 * of this as an "interrupt handler". One of the important characteristics of code
 * running in the audio thread is that it must be fast, and it must make very minimal
 * use of system services, in particular dynamic memory allocation.  malloc/free/new/delete
 * often work in the audio thread, and I have indeed used them extensivily in the past.
 *
 * But they don't always.  Every now and then this can result in "thread inversion" or time
 * consuming restructuring of the free memory pool.  When that happens the audio thread may
 * not be able to respond to a block request from the audio interface within the alotted time,
 * which is a Very Bad Thing.  It results in discontinuity in the audio block stream,
 * which to the end user sounds like "clicks and pops".  This problem gets worse as the
 * audio buffer size decreases and everyone wants the lowest possible buffer size to
 * reduce latency.
 *
 * So here we are.   The MSL runtime memory model is going to look extremely primitive
 * to younger eyes accustomed to std::vector, std::string, juce::OwnedArray, garbage collection,
 * or any of the other modern convenieniences we usually make use of.  I'm sure there are
 * more elegant and flexible approaches or libraries to do this sort of thing, but this
 * gets the job done, is fast, memory efficient, and allows easy monitoring of what is
 * happening in the MSL environment.  And it really isn't that complicated so I'm not going
 * to waste time trying to find something that might be better in some ways.
 *
 * Objects in this model should operate consistently in these ways:
 *
 *     - self contained with no references to other objects not in this model
 *     - where strings are necessary, fixed width buffers of reasonable size
 *     - collections implemented as simple linked lists
 *     - "delete" cascades and also deletes the objects it is connected to (usually)
 *     - delete should normally be used only during system shutdown when disposing of the pools
 *     - runtime reclamation of objects should return them to the pool rather than deleting them
 *
 * The pools are accessed by multiple threads so must use critical sections when
 * managing lists.  There are two categories of threads that I'm calling "shell" and "kernel".
 * A Kernel thread is a high priority thread, in this context almost always the audio thread
 * but could also be a MIDI device thread.  Kernel threads must always use the pool to allocate
 * and free objects.  A Shell thread is anything that doesn't have the runtime restrictions
 * of kernel threads, normally there are two, the "UI" thread which here is the Juce message
 * thread and the "maintenance" thread that does periodic housekeeping in coordination with
 * the UI thread(s).  The maintenance thread is expected to run at regular intervals
 * (currently every 1/10th second) and one of the things it does is "fluff" the object pools.
 * Fluffing involves checking each pool to see if the number of available objects falls
 * below a certain threshold and using dynamic memory allocation to put more objects
 * into the pool.  If things are working well, the object pools will grow slowly over time
 * depending on how the user makes use of scripts then stabilizies without the need for
 * further allocation.  But things don't always work well, so in hopefully rare cases
 * kernel threads may have to "panic" and resort to newing up objects just so the interpreter
 * can continue.  Statistics are (or will be) meintained when this happens so adjustments
 * to the initial allocation and fluff factor can be made.
 *
 * You may notice that I don't use "smart" pointers, because, well I hate the kinds of syntax
 * they often inject into the process, and I'm a grown up that doesn't need help remembering
 * when to delete things.  And of course RAII and/or stack allocation is of limited use
 * in this context.  I've found occasional use for pass-by-value for some of these, but
 * it is rare since they almost always need to to be passed around and have indefinite lifespan.
 *
 */

#pragma once

#include "MslObjectPool.h"

#include "MslObject.h"

class MslPools
{
  public :

    MslPools(class MslEnvironment* env);
    ~MslPools();

    // fill out the initial set of pooled objects
    void initialize();
    
    // called in the shell maintenance thread to replenish the pools
    // if they dip below their pool threshold
    void fluff();

    class MslValue* allocValue();
    void free(class MslValue* v);
    void clear(class MslValue* v);
    
    class MslError* allocError();
    void free(class MslError* v);
    
    class MslResult* allocResult();
    void free(class MslResult* r);

    class MslBinding* allocBinding();
    void free(class MslBinding* b);

    class MslStack* allocStack();
    void free(class MslStack* s);
    void freeList(class MslStack* s);

    class MslSession* allocSession();
    void free(class MslSession* s);

    class MslObject* allocObject();
    void free(class MslObject* o);

    class MslAttribute* allocAttribute();
    void free(class MslAttribute* a);

    void traceSizes();
    void traceStatistics();

  private:

    class MslEnvironment* environment = nullptr;

    MslAttributePool attributePool;
    MslObjectValuePool objectPool;

    // old, horrible pools that need to die

    // in retrospect, I like the way ObjectPools works MUCH better
    class MslValue* valuePool = nullptr;
    class MslError* errorPool = nullptr;
    class MslResult* resultPool = nullptr;
    class MslBinding* bindingPool = nullptr;
    class MslStack* stackPool = nullptr;
    class MslSession* sessionPool = nullptr;

    // god this needs a common superclass...
    int valuesCreated = 0;
    int valuesRequested = 0;
    int valuesReturned = 0;
    int valuesDeleted = 0;
    
    int errorsCreated = 0;
    int errorsRequested = 0;
    int errorsReturned = 0;
    int errorsDeleted = 0;
    
    int resultsCreated = 0;
    int resultsRequested = 0;
    int resultsReturned = 0;
    int resultsDeleted = 0;
    
    int bindingsCreated = 0;
    int bindingsRequested = 0;
    int bindingsReturned = 0;
    int bindingsDeleted = 0;
    
    int stacksCreated = 0;
    int stacksRequested = 0;
    int stacksReturned = 0;
    int stacksDeleted = 0;
    
    int sessionsCreated = 0;
    int sessionsRequested = 0;
    int sessionsReturned = 0;
    int sessionsDeleted = 0;

    void flushSessions();
    void flushStacks();
    void flushBindings();
    void flushResults();
    void flushErrors();
    void flushValues();
    
};
    
