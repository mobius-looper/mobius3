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

#include "../../model/MobiusMidiState.h"

class AbstractTrack
{
  public:
    virtual ~AbstractTrack() {}

    // Loop state

    virtual int getNumber() = 0;
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

    virtual bool finishSwitch(int target) = 0;
    
    // simple one-shot actions
    virtual void doParameter(class UIAction* a) = 0;
    virtual void doReset(bool full) = 0;
    virtual void doUndo() = 0;
    virtual void doRedo() = 0;
    virtual void doDump() = 0;


    // advance play/record state between events
    virtual void advance(int newFrames) = 0;

    // misc utilities
    virtual void alert(const char* msg) = 0;

};
