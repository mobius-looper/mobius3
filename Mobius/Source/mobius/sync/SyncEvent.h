/**
 * Message passed from SyncMaster into the BaseTrack when a synchronization
 * operation is to be performed.
 */

#pragma once

class SyncEvent
{
  public:

    typedef enum {
        None,
        Start,
        Stop,
        Finalize,
        Extend,
        Realign
    } Type;

    Type type;

    // for Finalize, the length the loop should have
    int finalLength = 0;

    // Return values frrom the track

    // true if there was an error processing the event
    // SM should absndon hope
    bool error = false;

    // true if the track decided to end recording
    // optional, and try to get rid of this
    // track must also call notifyRecordEnded
    bool ended = false;

    void reset() {
        type = None;
        finalLength = 0;
        error = false;
        ended = false;
    }
};
