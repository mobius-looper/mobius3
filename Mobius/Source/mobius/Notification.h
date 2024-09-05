/**
 * Class used to maintain a list of interesting things that happen inside the engine
 * as it runs.  Core code (mostly) posts notifications as things happen, and something
 * eventually consumes these and generates suitable actions.
 *
 */

#pragma once

/**
 * There is a set of built-in notification types, these are not user extensible.
 */
typedef enum {

    NotificationNone = 0,

    NotificationLoopStart,
    NotificationLoopEnd,
    NtificationSubcycle,
    NotificationCycle,
    NotificationLoopLocation,
    NotificationLoopNumber,
    
    NotificationFunctionStart,
    NotificationFunctionEnd,
    NotificationModeStart,
    NotificationModeEnd,

    NotificationParameter,
    NotificationControl,

    NotificationBeat,
    NotificationBar,
    NotificationSyncPoint

} NotificationId;

typedef enum {

    NotifierBlock,
    NotifierEvent

} NotifierLocation;


/**
 * Object that captures a notification.
 * These are pooled for fast allocation and reclamation.
 */
class Notification
{
  public:

    Notification() {}
    // MSL pooled objects delete themselves, should we?
    ~Notification() {}

    // pool pointer and chain pointer when active
    Notification* next = nullptr;

    // what this is
    NotificationId id = NotificationNone;

    // the track it was in
    int trackNumber = 0;

    // the frame it happened on
    int loopFrame = 0;

    // Type specific information
    class MobiusMode* mode = nullptr;

    // reset state when pooling
    void init() {
        next = nullptr;
        id = NotificationNone;
        trackNumber = 0;
        loopFrame = 0;
        mode = nullptr;
    }

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/


    


    