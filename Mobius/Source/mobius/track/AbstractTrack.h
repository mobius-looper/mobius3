/**
 * The interface of an object that exhibits looping track behavior,
 * either audio or MIDI.
 *
 * Might want to refactor this when we starting adding other track
 * types that consume the audio stream and take actions, but which
 * don't behave as loopers.
 */

#pragma once

#include "../../model/ParameterConstants.h"
#include "../../model/MobiusState.h"
#include "TrackProperties.h"

class AbstractTrack
{
  public:
    virtual ~AbstractTrack() {}


    // Temporary, shouldn't be here
    virtual void alert(const char* msg) = 0;
    virtual class TrackEventList* getEventList() = 0;

    //
    // Generic Actions
    // These are the things TrackManager and LogicalTrack use
    //
    
    virtual void setNumber(int n) = 0;
    virtual int getNumber() = 0;
    virtual int getGroup() = 0;
    virtual bool isFocused() = 0;

    virtual void getTrackProperties(TrackProperties& props) = 0;
    virtual void doAction(class UIAction* a) = 0;
    virtual bool doQuery(class Query* q) = 0;

    virtual bool scheduleWaitFrame(class MslWait* w, int frame) = 0;
    virtual bool scheduleWaitEvent(class MslWait* w) = 0;

    //
    // Looping Track Interface
    // These are used mostly by Scheduler and a few by MSL when scheduling waits
    //
    
    // Loop state

    virtual MobiusState::Mode getMode() = 0;
    virtual int getLoopCount() = 0;
    virtual int getLoopIndex() = 0;
    virtual int getLoopFrames() = 0;
    virtual int getFrame() = 0;
    virtual int getCycleFrames() = 0;
    virtual int getCycles() = 0;
    virtual int getSubcycles() = 0;
    virtual int getModeStartFrame() = 0;
    virtual int getModeEndFrame() = 0;
    virtual int extendRounding() = 0;

    // utility we need o a few places
    int getSubcycleFrames() {
        int subcycleFrames = 0;
        int cycleFrames = getCycleFrames();
        if (cycleFrames > 0) {
            int subcycles = getSubcycles();
            if (subcycles > 0)
              subcycleFrames = cycleFrames / subcycles;
        }
        return subcycleFrames;
    }
    
    // Mode transitions
    
    virtual void startRecord() = 0;
    virtual void finishRecord() = 0;

    virtual void startMultiply() = 0;
    virtual void finishMultiply() = 0;
    virtual void unroundedMultiply() = 0;
    
    virtual void startInsert() = 0;
    virtual int extendInsert() = 0;
    virtual void finishInsert() = 0;
    virtual void unroundedInsert() = 0;

    virtual void toggleOverdub() = 0;
    virtual void toggleMute() = 0;
    virtual void toggleReplace() = 0;
    virtual void toggleFocusLock() = 0;

    virtual void finishSwitch(int target) = 0;
    virtual void loopCopy(int previous, bool sound) = 0;

    virtual bool isPaused() = 0;
    virtual void startPause() = 0;
    virtual void finishPause() = 0;
   
    // simple one-shot actions
    virtual void doParameter(class UIAction* a) = 0;
    virtual void doPartialReset() = 0;
    virtual void doReset(bool full) = 0;
    virtual void doStart() = 0;
    virtual void doStop() = 0;
    virtual void doPlay() = 0;
    virtual void doUndo() = 0;
    virtual void doRedo() = 0;
    virtual void doDump() = 0;
    virtual void doInstantMultiply(int n) = 0;
    virtual void doInstantDivide(int n) = 0;
    virtual void doHalfspeed() = 0;
    virtual void doDoublespeed() = 0;
    
    // leader stuff
    virtual void leaderReset(class TrackProperties& props) = 0;
    virtual void leaderRecordStart() = 0;
    virtual void leaderRecordEnd(class TrackProperties& props) = 0;
    virtual void leaderMuteStart(class TrackProperties& props) = 0;
    virtual void leaderMuteEnd(class TrackProperties& props) = 0;
    virtual void leaderResized(class TrackProperties& props) = 0;
    virtual void leaderMoved(class TrackProperties& props) = 0;
    
    // advance play/record state between events
    virtual bool isExtending() = 0;
    virtual void advance(int newFrames) = 0;
    virtual void loop() = 0;

    virtual float getRate() = 0;
    virtual int getGoalFrames() = 0;
    virtual void setGoalFrames(int f) = 0;

    //
    // Configuration
    //
    virtual bool isNoReset() = 0;
    
};
