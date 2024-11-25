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

class MslTrack
{
  public:

    virtual ~MslTrack() {}

    // used by TrackMslHandler

    virtual getSubcycleFrames() = 0;
    virtual getCycleFrames() = 0;
    virtual getLoopFrames() = 0;
    virtual getFrame() = 0;
    virtual getRate() = 0;
    
    virtual TrackEventList* getEventList() = 0;
    virtual bool scheduleWaitFrame(class MslWait* w, int frame) = 0;
    virtual bool scheduleWaitEvent(class MslWait* w) = 0;

    // used by TrackMslVariableHandler

    virtual getLoopCount() = 0;
    virtual getLoopIndex() = 0;
    virtual getCycles() = 0;
    virtual getSubcycles() = 0;
    virtual getMode() = 0;
    virtual isPaused() = 0;

};


        


        
    
