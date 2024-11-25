/**
 * An extension of BaseTrack for tracks that wish to play with
 * BaseScheduler.
 */

#pragma once

#include "BaseTrack.h"

class ScheduledTrack : public BaseTrack
{
  public:

    virtual ~ScheduledTrack() {}

    // kind of messy, revisit these

    virtual void doActionNow(class UIAction* a) = 0;
    virtual int getFrames() = 0;
    virtual int getFrame() = 0;
    virtual MobusState::Mode getMode() = 0;
    virtual bool isExtending() = 0;
    virtual void reset() = 0;
    virtual void loop() = 0;
    virtual float getRate() = 0;
    virtual void advance(int frames);

    virtual void leaderReset(class TrackProperties& props) = 0;
    virtual void leaderRecordStart() = 0;
    virtual void leaderRecordEnd(class TrackProperties& props) = 0;
    virtual void leaderMuteStart(class TrackProperties& props) = 0;
    virtual void leaderMuteEnd(class TrackProperties& props) = 0;
    virtual void leaderResized(class TrackProperties& props) = 0;
    virtual void leaderMoved(class TrackProperties& props) = 0;

};
