/**
 * An extension of BaseTrack for tracks that wish to play with
 * BaseScheduler.
 */

#pragma once

#include "../../model/TrackState.h"

#include "BaseTrack.h"

class ScheduledTrack : public BaseTrack
{
  public:

    ScheduledTrack(class TrackManager* tm, class LogicalTrack* lt) : BaseTrack(tm,lt) {}
    virtual ~ScheduledTrack() {}

    // various bits of track state required for scheduling
    virtual int getFrames() = 0;
    virtual int getFrame() = 0;
    virtual TrackState::Mode getMode() = 0;
    virtual bool isExtending() = 0;
    virtual bool isPaused() = 0;
    virtual float getRate() = 0;
    
    // primary actions
    virtual void doActionNow(class UIAction* a) = 0;
    virtual void advance(int frames) = 0;;
    virtual void reset() = 0;
    virtual void loop() = 0;

    // leader responses
    virtual void leaderReset(class TrackProperties& props) = 0;
    virtual void leaderRecordStart() = 0;
    virtual void leaderRecordEnd(class TrackProperties& props) = 0;
    virtual void leaderMuteStart(class TrackProperties& props) = 0;
    virtual void leaderMuteEnd(class TrackProperties& props) = 0;
    virtual void leaderResized(class TrackProperties& props) = 0;
    virtual void leaderMoved(class TrackProperties& props) = 0;

};
