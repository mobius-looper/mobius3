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
        EventNone,
        EventPulse,
        EventSync,
        EventRecord,
        EventAction,
        EventRound,
        EventSwitch,
        
        // weed
        EventRecord,
        EventReturn,
        EventFunction,
        EventRound,

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

    // stacked actions
    class UIAction* actions = nullptr;
    void stack(class UIAction* a);
    
    //
    // Extra state to display
    //
    
    // true for rounding events to convey the multiples
    int multiples = 0;

    // switch arguments
    int switchTarget = 0;
    SwitchQuantize switchQuantize = SWITCH_QUANT_OFF;

    // function arguments
    SymbolId symbolId = SymbolIdNone;

    static int getQuantizedFrame(int loopFrames, int cycleFrames, int currentFrame,
                                 int subcycles, QuantizeMode q, bool after);
    
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
    TrackEvent* findRounding(SymbolId id);

    TrackEvent* consume(int startFrame, int blockFrames);
    void shift(int delta);
    TrackEvent* consumePulsed();

  private:

    TrackEventPool* pool = nullptr;
    TrackEvent* events = nullptr;
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
