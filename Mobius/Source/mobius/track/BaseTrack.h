/**
 * Interface for the most basic track type.
 * Any track implementation that wants to live in TrackManager/LogicalTrack
 * must implement this.
 *
 * BaseTracks are usually also ScheduledTracks and make use of BaseScheduler
 * for synchronization, but that isn't required.
 */

#pragma once

#include "../Notification.h"
#include "../TrackContent.h"

class BaseTrack
{
  public:

    // base tracks are connected to the community
    BaseTrack(class TrackManager* tm, class LogicalTrack* lt) {
        manager = tm;
        logicalTrack = lt;
    }
    virtual ~BaseTrack() {}

    class TrackManager* getTrackManager() {
        return manager;
    }
    class LogicalTrack* getLogicalTrack() {
        return logicalTrack;
    }
    
    // all tracks have a unique number shown in the UI
    int getNumber();

    // tracks are sensitive to change
    virtual void refreshParameters() = 0;

    // tracks may all do things or schedule them
    virtual void doAction(class UIAction* a) = 0;

    // and you can ask them their secrets
    virtual bool doQuery(class Query* q) = 0;

    // they are voracious consumers of audio
    virtual void processAudioStream(class MobiusAudioStream* stream) = 0;

    // and some like MIDI very much
    virtual void midiEvent(class MidiEvent* e) = 0;

    // and they can package up useful information to share with others
    virtual void getTrackProperties(class TrackProperties& props) = 0;

    // other tracks can tell them about the world around them
    virtual void trackNotification(NotificationId notification, TrackProperties& props) = 0;

    // they can have important things to say
    virtual void refreshPriorityState(class PriorityState* state) {(void)state;}
    
    // and can go on and on if you let them
    virtual void refreshState(class TrackState* tstate) = 0;

    // and one can be tmi
    virtual void refreshFocusedState(class FocusedTrackState* state) = 0;
    
    // and sometimes it's spills its guts
    virtual void dump(class StructureDumper& d) = 0;

    // it might like MSL
    virtual class MslTrack* getMslTrack() = 0;

    // it pays attention well
    virtual void syncEvent(class SyncEvent* e) = 0;
    virtual int getSyncLength() = 0;
    virtual int getSyncLocation() = 0;

    // and can really spill its guts
    virtual void gatherContent(class TrackContent* content) = 0;
    virtual void loadContent(class TrackContent* content, class TrackContent::Track* src) = 0;
    
    // and we can watch what it does
    virtual int scheduleFollowerEvent(QuantizeMode q, int follower, int eventId) = 0;

    // replacement for follower events
    virtual bool scheduleWait(class TrackWait& wait) = 0;
    virtual void cancelWait(class TrackWait& wait) = 0;
    virtual void finishWait(class TrackWait& wait) = 0;
    
  protected:

    class TrackManager* manager = nullptr;
    class LogicalTrack* logicalTrack = nullptr;
    
};
