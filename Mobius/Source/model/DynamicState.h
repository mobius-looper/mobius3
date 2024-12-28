/**
 * A special state object that augments TrackState and contains things that
 * cannot be accessed safely as atomic numeric values.
 *
 * The problem this is solving is surprisingly awkward.  Most of the SystemState
 * object is of fixed size and consists of numbers.  There is always a stable number of tracks
 * and each track has a stable number of loops.  SystemState can be refreshed simply by copying
 * numbers from the live internal objects into the SystemState and this can be done from the UI
 * thread without locking, provided that number copying is atomic.
 *
 * There are a few exceptions: the event list, the layer list, and the edit region list.
 * While information about an event, layer, or region consists of only numbers, the number
 * of them changes frequently as actions are performed and the internal structure used
 * to model them are often linked lists that are not stable between two threads.
 * It is not possible for the UI thread to iterate over a linked list being actively changed
 * by the kernel.
 *
 * Instead, the kernel must periodically "publish" state for these dynamic objects.  This
 * published state uses a model that will remain stable when accessed from the UI thread.  It
 * may be stale at the moment the UI wants to refresh, but its close enough to current for the UI
 * and will be quickly brought up to date on the next refresh cycle.
 *
 * But this published state has threaading issues as well.  It can't use linked lists or
 * juce:: or std:: containers because those are also not thread safe.  The simplest approach
 * is to use an old-fashioned array of a fixed maximum size, fill that with information
 * then publish a count of the number of valid entries in that array.  That works reasonably
 * well when the array is growing, the UI may be reading array elements 1 and 2 while the kernel
 * is adding element 3, and when the kernel is done it sets the object count to 3 for the next
 * UI refresh.  But when the number is shrinking, say from 10 to 2, the UI can be in the middle
 * of reading element 2 while the kernel is writing new information into element 2.  This can
 * result in inconsistent element state which in some cases is nonsensical and can result in
 * UI flicker or display glitches.
 *
 * There are various approaches to this including using pooled objects that must be allocated
 * by the kernel, published for the UI, then freed by the UI.  Since the numbers of these
 * objects is normally small I'm taking a "ring buffer" approach.  Objects that are published
 * are maintained in an array with a "read pointer" and a "write pointer".  The UI is continually
 * reading from the read pointer and the kernel is writing to the write pointer, and when the kernel
 * is finished it atomically updates the read pointer to the last write pointer and moves the
 * write pointer to the end.  The elements of this array are effectively an object pool, but by
 * using array indexes for the two pointers it can be read and written without locking,
 * provided the index updates are atomic.
 */

#pragma once

#include "SymbolId.h"

class DynamicEvent
{
  public:
    
    /**
     * Most events are identified by the SymbolId associated with the
     * function that scheduled the event.  A few are system events
     * that are either unrelated to functions or carry more information
     * than just the function event.
     */
    typedef enum {

        // event type used to mark the end of the read list
        EventNone,

        // catch-all event for internal events that don't have mappings
        EventUnknown,

        // the event is displayed as the name of the symbol
        EventAction,

        // the event is dissplayed as the name of the symbol plus "End"
        // e.g. FuncMultiply would be "End Multiply"
        EventRound,

        // a loop switch, will have an argument
        EventSwitch,
        
        // loop switch variant
        EventReturn,

        // script wait
        EventWait,

        // notify a follower track
        EventFollower
        
    } Type;

    Type type = EventNone;
    SymbolId symbol = SymbolIdNone;
    int argument = 0;
    int frame = 0;
    bool pending = false;
    // true if a script is waiting on this event
    bool waiting = false;

    // just in case we want to show events for all tracks, allow a track number tag
    int track = 0;

    void init() {
        type = EventNone;
        symbol = SymbolIdNone;
        argument = 0;
        frame = 0;
        pending = false;
        waiting = false;
        track = 0;
    }
    
};

/**
 * Regions have a type which can be used for coloring and a span of frames.
 */
class DynamicRegion
{
  public:
    
    /**
     * The type of a Region
     * Not sure how useful this is, in theory we could color these
     * differently but it should be pretty obvious what they are,
     * it's more important to know where they are.
     */
    typedef enum {
        RegionOverdub,
        RegionReplace,
        RegionInsert
    } Type;

    Type type = RegionOverdub;
    int startFrame = 0;
    int endFrame = 0;

    void init() {
        type = RegionOverdub;
        startFrame = 0;
        endFrame = 0;
    }
};

/**
 * We only need to store layer state when there is something interesting about them
 * and the only thing right now is the checkpoint flag.
 */
class DynamicLayer
{
  public:
    int number = 0;
    bool checkpoint = false;

    void init() {
        number = 0;
        checkpoint = false;
    }
};

/**
 * Base class for a ring buffer manager, the subclass implements the
 * class-specific object array.
 */
class DynamicRing
{
  public:

    // the read pointer, zero is reserved to mean "not set"
    int readHead = 0;
    // the end of the read list and the start of the write list
    int readTail = 0;
    // the end of the write list
    int writeTail = 0;

    // the maximum size of the element array
    virtual int getTotal() = 0;

    void startWrite() {
        writeTail = readTail;
    }

    int nextWriteIndex() {
        int available = -1;
        int newTail = writeTail + 1;
        if (newTail >= getTotal())
          newTail = 0;
        if (newTail == readHead) {
            // buffer overflow
        }
        else {
            available = writeTail;
            writeTail = newTail;
        }
        return available;
    }

    void commitWrite() {
        readHead = readTail;
        readTail = writeTail;
    }

    int nextReadIndex() {
        int available = -1;
        if (readHead != readTail) {
            available = readHead;
            int nextRead = readHead + 1;
            if (nextRead >= getTotal())
              nextRead = 0;
            readHead = nextRead;
        }
        return available;
    }
};        

/**
 * Obviously, these cry out for a template
 */
class DynamicEventRing : public DynamicRing
{
  public:
    // don't think these need to be configurable but they could be
    static const int MaxEvents = 64;

    DynamicEvent elements[MaxEvents];

    int getTotal() override {
        return MaxEvents;
    }

    DynamicEvent* nextWrite() {
        DynamicEvent* el = nullptr;
        int index = nextWriteIndex();
        if (index >= 0) {
            el = &(elements[index]);
            el->init();
        }
        return el;
    }

    DynamicEvent* nextRead() {
        DynamicEvent* el = nullptr;
        int index = nextReadIndex();
        if (index >= 0)
          el = &(elements[index]);
        return el;
    }
    
};

class DynamicLayerRing : public DynamicRing
{
  public:

    // don't think these need to be configurable but they could be
    static const int MaxLayers = 64;

    DynamicLayer elements[MaxLayers];

    int getTotal() override {
        return MaxLayers;
    }
    
    DynamicLayer* nextWrite() {
        DynamicLayer* el = nullptr;
        int index = nextWriteIndex();
        if (index >= 0) {
            el = &(elements[index]);
            el->init();
        }
        return el;
    }
    
    DynamicLayer* nextRead() {
        DynamicLayer* el = nullptr;
        int index = nextReadIndex();
        if (index >= 0)
          el = &(elements[index]);
        return el;
    }
};

class DynamicRegionRing : public DynamicRing
{
  public:

    // don't think these need to be configurable but they could be
    static const int MaxRegions = 64;

    DynamicRegion elements[MaxRegions];
    
    int getTotal() override {
        return MaxRegions;
    }
    
    DynamicRegion* nextWrite() {
        DynamicRegion* el = nullptr;
        int index = nextWriteIndex();
        if (index >= 0) {
            el = &(elements[index]);
            el->init();
        }
        return el;
    }
    
    DynamicRegion* nextRead() {
        DynamicRegion* el = nullptr;
        int index = nextReadIndex();
        if (index >= 0)
          el = &(elements[index]);
        return el;
    }
};

/**
 * And now one state to rule them all
 */
class DynamicState
{
  public:

    DynamicEventRing events;
    DynamicRegionRing regions;
    DynamicLayerRing layers;

    void startWrite();
    DynamicEvent* nextWriteEvent();
    DynamicRegion* nextWriteRegion();
    DynamicLayer* nextWriteLayer();
    void commitWrite();

    DynamicEvent* nextReadEvent();
    DynamicRegion* nextReadRegion();
    DynamicLayer* nextReadLayer();
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
