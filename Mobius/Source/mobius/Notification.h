/**
 * Class used to maintain a list of interesting things that happen inside the engine
 * as it runs.  Core code (mostly) posts notifications as things happen, and something
 * (Notifier) eventually consumes these and generates suitable actions.
 */

#pragma once

#include "../model/ObjectPool.h"

/**
 * There is a set of built-in notification types, these are not user extensible.
 */
typedef enum {

    NotificationNone = 0,

    // Notifications used by MIDI followers

    NotificationFollower,
    NotificationReset,
    NotificationRecordStart,
    NotificationRecordEnd,
    NotificationMuteStart,
    NotificationMuteEnd,

    // catch all notification for any abrupt change that may
    // have impacted the loop size
    // this includes loop switch, undo, redo, unrounded multiply, unrounded insert
    // multiply/insert do not need notification because they preserved the cycle
    // length, but that might be interesting too
    NotificationLoopSize,

    // Notifications of interest to event scripts

    NotificationModeStart,
    NotificationModeEnd,
    NotificationLoopStart,
    NotificationCycle,
    NotificationSubcycle,
    
} NotificationId;

/**
 * Payload of random things that need to be passed with the notification.
 * This augments what is in the TrackProperties.
 * This should be kept small and passable by value.
 */
class NotificationPayload
{
  public:

    // for NotificationModeStart/End
    class MobiusMode* mode = nullptr;
    
};

//////////////////////////////////////////////////////////////////////
//
// Old, currently unused
//
//////////////////////////////////////////////////////////////////////

typedef enum {

    NotifierBlock,
    NotifierEvent

} NotifierLocation;


/**
 * Object that captures a notification.
 * These are pooled for fast allocation and reclamation.
 */
class Notification : public PooledObject
{
  public:

    Notification();
    ~Notification();
    void poolInit();
    
    // chain pointer when active
    Notification* next = nullptr;

    // what this is
    NotificationId id = NotificationNone;

    // the track it was in
    int trackNumber = 0;

    // the frame it happened on
    int loopFrame = 0;

    // Type specific information
    class MobiusMode* mode = nullptr;

};

class NotificationPool : public ObjectPool
{
  public:

    NotificationPool();
    virtual ~NotificationPool();

    class Notification* newNotification();

  protected:
    
    virtual PooledObject* alloc() override;
    
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/


    


    
