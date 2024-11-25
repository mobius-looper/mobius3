/**
 * The base class of tracks that implement Mobius-style looper functionality.
 * This extends ScheduledTrack so it can be used with BaseScheduler.
 */

#pragma once

#include "../../model/MobiusState.h"
#include "BaseTrack.h"
#include "ScheduledTrack.h"

class LooperTrack : public ScheduledTrack
{
  public:

    virtual ~LooperTrack() {}

    // Loop state

    virtual int getLoopCount() = 0;
    virtual int getLoopIndex() = 0;
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
    virtual void doInstantMultiply(int n) = 0;
    virtual void doInstantDivide(int n) = 0;
    virtual void doHalfspeed() = 0;
    virtual void doDoublespeed() = 0;

    // axperimental option that never went anywhere
    virtual bool isNoReset() = 0;

};

