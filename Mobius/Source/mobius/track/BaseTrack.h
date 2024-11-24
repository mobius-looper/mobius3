/**
 * Interface for the most basic track type.
 */

#pragma once

#include "../../model/Session.h"

class BaseTrack
{
  public:

    virtual ~BaseTrack() {}

    // tracks come in many shapes and sizes
    virtual void loadSssion(class Session::Track* def) = 0;

    // all tracks have a unique number shown in the UI
    // these are assigned by Trackmanager
    void setNumber(int n) {number = n;}
    virtual int getNumber() {return number;}

    // tracks may all do things
    virtual void doAction(class UIAction* a) = 0;

    // and you can ask them their secrets
    virtual bool doQuery(class Query* q) = 0;

    // they are voracious consumers of audio
    virtual void processAudioStream(class MobiusAudioStream* stream) = 0;

    // and some like MIDI very much
    virtual void midiEvent(class MidiEvent* e) = 0;

    // they normally have a size and position
    virtual int getFrames() = 0;
    virtual int getFrame() = 0;

    // and they can package up useful information to share with others
    virtual void getTrackProperties(class TrackProperties& props) = 0;

    // other tracks can tell them about the world around them
    virtual void trackNotification(NotificationId notification, TrackProperties& props) = 0;

    // sometimes they gather in groups, but it isn't suspicious or anything
    virtual int getGroup() = 0;

    // and some feel left out if they aren't included
    virtual bool isFocused() = 0;

    // MSL will wait patiently for them, like a good dad
    virtual bool scheduleWaitFrame(class MslWait* w, int frame) = 0;
    virtual bool scheduleWaitEvent(class MslWait* w) = 0;

    // they can have important things to say
    virtual void refreshPriorityState(class MobiusState::Track* tstate) = 0;

    // and can go on and on if you let them
    virtual void refreshState(class MobiusState::Track* tstate) = 0;

  protected:

    int number = 0;
    
};
