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

    SyncEvent() {}
    SyncEvent(Type t) {type = t;}

    Type type;

    // for Extend, Stop, Finalize the number of
    // record units that have elapsed, this should
    // become the loop's cycle count
    int elapsedUnits = 0;

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
        elapsedUnits = 0;
        finalLength = 0;
        error = false;
        ended = false;
    }

    const char* getName() {
        const char* name = "???";
        switch (type) {
            case None: name = "None"; break;
            case Start: name = "Start"; break;
            case Stop: name = "Stop"; break;
            case Finalize: name = "Finalize"; break;
            case Extend: name = "Extend"; break;
            case Realign: name = "Realign"; break;
        }
        return name;
    }
};
