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

class TrackEvent : public PooledObject
{
  public:

    typedef enum {
        EventNone,
        EventPulse,
        EventRecord
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
    void add(TrackEvent* e, bool priority = false);
    void flush();

    TrackEvent* consume(int startFrame, int blockFrames);
    TrackEvent* consumePulsed();
    
  private:

    TrackEventPool* pool = nullptr;
    TrackEvent* events = nullptr;
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
