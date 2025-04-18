/**
 * Model to represent a scheduled operation that happens within
 * a track at a certain time.
 *
 * A simplification and eventual replacement for mobius/core/Event.h
 *
 * Events are pooled objects and have a union structure for various event types.
 *
 */

#pragma once

#include "../../model/ObjectPool.h"
#include "../../model/ParameterConstants.h"
#include "../../model/SymbolId.h"

class TrackEvent : public PooledObject
{
  public:

    typedef enum {

        // BaseScheduler events
        EventNone,
        EventSync,
        EventAction,
        EventWait,

        // LooperScheduler events
        EventRecord,
        EventRound,
        EventSwitch
        
    } Type;
    
    TrackEvent();
    ~TrackEvent();
    void poolInit() override;
    
    // chain pointer for an event list
    TrackEvent* next = nullptr;

    // what it is
    Type type = EventNone;

    // where it is
    int frame = 0;

    // when where is unknown
    bool pending = false;

    // when it is waiting for a sync pulse
    bool pulsed = false;

    // for Round events, indiciates this is an extension point
    bool extension = false;

    // for MSL wait events
    class MslWait* wait = nullptr;

    // stacked actions
    class UIAction* primary = nullptr;
    class UIAction* stacked = nullptr;
    void stack(class UIAction* a);

    // when this is a pending follower event with a leader
    // event scheduled
    int correlationId = 0;
    
    //
    // Extra state 
    //
    
    // positive for rounding events to convey the multiples
    // used only for display
    int multiples = 0;

    // for EventSwitch, the index of the target loop
    int switchTarget = 0;

    // for EventSwitch true if this swithch was scheduled
    // for SwitchDuration=Once, e.g. a "Return" event
    bool isReturn = false;

    static int getQuantizedFrame(int loopFrames, int cycleFrames, int currentFrame,
                                 int subcycles, QuantizeMode q, bool after);

    static QuantizeMode convertQuantize(SwitchQuantize sq);


    
};

class TrackEventPool : public ObjectPool
{
  public:

    TrackEventPool();
    virtual ~TrackEventPool();

    class TrackEvent* newEvent();

  protected:
    
    virtual PooledObject* alloc() override;
    
};

class TrackEventList
{
  public:

    TrackEventList();
    ~TrackEventList();
    void initialize(TrackEventPool* pool);
    
    void clear();
    TrackEvent* getEvents() {
        return events;
    }
    void add(TrackEvent* e, bool priority = false);
    TrackEvent* find(TrackEvent::Type type);
    TrackEvent* findLast(SymbolId id);
    TrackEvent* findLast();
    TrackEvent* consumePendingLeader(int frame);
    TrackEvent* find(int startFrame, int endFrame);
    TrackEvent* remove(TrackEvent::Type type);

    bool isScheduled(TrackEvent* e);
    
    TrackEvent* consume(int startFrame, int endFrame);
    void remove(TrackEvent* e);
    void shift(int delta);
    TrackEvent* consumePulsed();

  private:

    TrackEventPool* pool = nullptr;
    TrackEvent* events = nullptr;
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
