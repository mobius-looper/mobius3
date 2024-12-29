/**
 * Interface for the most basic track type.
 * Any track implementation that wants to live in TrackManager/LogicalTrack
 * must implement this.
 *
 * BaseTracks are usually also ScheduledTracks and make use of BaseScheduler
 * for synchronization, but that isn't required.
 */

#pragma once

#include "../../model/Session.h"
#include "../Notification.h"

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
    // these are assigned by Trackmanager and can change at runtime
    void setNumber(int n) {number = n;}
    virtual int getNumber() {return number;}

    // tracks come in many shapes and sizes
    virtual void loadSession(class Session::Track* def) = 0;


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

    // sometimes they gather in groups, but it isn't suspicious or anything
    virtual int getGroup() = 0;

    // and some feel left out if they aren't included
    virtual bool isFocused() = 0;

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

  protected:

    class TrackManager* manager = nullptr;
    class LogicalTrack* logicalTrack = nullptr;
    int number = 0;
    
};
