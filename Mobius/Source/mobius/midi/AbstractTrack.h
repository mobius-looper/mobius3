/**
 * The interface of an object that exhibits looping track behavior,
 * either audio or MIDI.
 *
 * The only implementation of this right now is MidiTrack, but in the long
 * term when the core engine is redesigned, it should be an AbstractTrack
 * and use the same TrackScheduler that MidiTrack does.
 *
 */

#pragma once

#include "../../model/ParameterConstants.h"
#include "../../model/MobiusMidiState.h"
#include "../TrackProperties.h"

class AbstractTrack
{
  public:
    virtual ~AbstractTrack() {}

    // Loop state

    virtual int getNumber() = 0;
    virtual bool isFocused() = 0;
    virtual int getGroup() = 0;
    virtual MobiusMidiState::Mode getMode() = 0;
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
    virtual void doStart() = 0;
    virtual void doStop() = 0;
   
    // simple one-shot actions
    virtual void doParameter(class UIAction* a) = 0;
    virtual void doPartialReset() = 0;
    virtual void doReset(bool full) = 0;
    virtual void doPlay() = 0;
    virtual void doUndo() = 0;
    virtual void doRedo() = 0;
    virtual void doDump() = 0;
    virtual void doInstantMultiply(int n) = 0;
    virtual void doInstantDivide(int n) = 0;

    // leader stuff
    virtual void leaderReset(class TrackProperties& props) = 0;
    virtual void leaderRecordStart() = 0;
    virtual void leaderRecordEnd(class TrackProperties& props) = 0;
    virtual void leaderMuteStart(class TrackProperties& props) = 0;
    virtual void leaderMuteEnd(class TrackProperties& props) = 0;
    virtual void leaderResized(TrackProperties& props) = 0;
    virtual void leaderMoved(TrackProperties& props) = 0;
    
    // advance play/record state between events
    virtual bool isExtending() = 0;
    virtual void advance(int newFrames) = 0;
    virtual void loop() = 0;

    virtual float getRate() = 0;
    //virtual void setRate(float r) = 0;
    virtual int getGoalFrames() = 0;
    virtual void setGoalFrames(int f) = 0;

    //
    // Leader State
    // Unclear if I want these saved on the Track
    // or if we should be going back to the Session
    //

    virtual bool isNoReset() = 0;
    
    // misc utilities
    virtual void alert(const char* msg) = 0;

    // emerging interfaces for MslWait and new track architecture
    virtual class TrackEventList* getEventList() = 0;

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

};
