/**
 * The interface of any track that wants to play with MSL
 *
 * Formerly in AbstractTrack which is now BaseTrack but I don't
 * want to clutter BaseTrack with too much.
 *
 * This is what is passed to TrackMslHandler and TrackMslVariableHandler
 *
 */

#pragma once

#include "../../model/TrackState.h"

class MslTrack
{
  public:

    virtual ~MslTrack() {}

    // used by TrackMslHandler

    virtual int getSubcycleFrames() = 0;
    virtual int getCycleFrames() = 0;
    virtual int getFrames() = 0;
    virtual int getFrame() = 0;
    virtual float getRate() = 0;
    
    virtual bool scheduleWaitFrame(class MslWait* w, int frame) = 0;
    virtual bool scheduleWaitEvent(class MslWait* w) = 0;

    // used by TrackMslVariableHandler

    virtual int getLoopCount() = 0;
    virtual int getLoopIndex() = 0;
    virtual int getCycles() = 0;
    virtual int getSubcycles() = 0;
    virtual TrackState::Mode getMode() = 0;
    virtual bool isPaused() = 0;
    virtual bool isOverdub() = 0;
    virtual bool isMuted() = 0;

};


        


        
    
