/**
 * An implementation of TrackActionScheduler for looping tracks.
 * MidiTrack initially and eventually what Mobius evolves into.
 * 
 * It has a combination of functionality found in the old Synchronizer and EventManager classes
 * plus mode awareness that was strewn about all over in a most hideous way.  It interacts
 * with an AbstractTrack that may either be a MIDI or an audio track, since the behavior of event
 * scheduling and mode transitions are the same for both.
 *
 */

#pragma once

#include "../../sync/Pulse.h"
#include "../../model/MobiusState.h"
#include "../../model/Session.h"
#include "../../model/ParameterConstants.h"
#include "../../model/SymbolId.h"

#include "../Notification.h"

#include "TrackEvent.h"
#include "LooperSwitcher.h"
#include "TrackTypeScheduler.h"

class LooperScheduler : public TrackTypeScheduler
{
    friend class LoopSwitcher;
    
  public:

    LooperScheduler(class BaseScheduler* bs);
    ~LooperScheduler();

    // ponder whether this should jsut be an extension of
    // BaseScheduler rather than splitting classes
    void setTrack(class LooperTrack* t);

    // here via BaseScheduler after it checks a few things
    void doAction(class UIAction* a);
    void doEvent(class TrackEvent* e);
    
    // here direct from TrackManager
    void doParameter(class UIAction* a);

  protected:

    // things LoopSwitcher needs

    class BaseScheduler* scheduler = nullptr;
    class LooperTrack* track = nullptr;
    
  private:

    // handler for loop switch complexity
    LooperSwitcher loopSwitcher {*this};

    //
    // Scheduling and mode transition guts
    //
    
    void doActionNow(class UIAction* a);
    void checkModeCancel(class UIAction* a);
    
    bool handleExecutiveAction(class UIAction* src);
    void doUndo(class UIAction* src);
    void unstack(class TrackEvent* event);
    void doRedo(class UIAction* src);
    
    bool isReset();
    void handleResetAction(class UIAction* src);
    
    bool isPaused();
    void handlePauseAction(class UIAction* src);
    bool schedulePausedAction(class UIAction* src);
    
    bool isRecording();
    void handleRecordAction(class UIAction* src);
    void scheduleRecordPendingAction(class UIAction* src, class TrackEvent* starting);
    void scheduleRecordEndAction(class UIAction* src, class TrackEvent* ending);
    
    bool isRounding();
    void handleRoundingAction(class UIAction* src);
    bool doRound(class TrackEvent* event);

    void scheduleAction(class UIAction* src);
    void scheduleRounding(class UIAction* src, MobiusState::Mode mode);
    
    QuantizeMode isQuantized(class UIAction* a);
    void scheduleQuantized(class UIAction* src, QuantizeMode q);
    int findQuantizationLeader();
    int getQuantizedFrame(QuantizeMode qmode);
    int getQuantizedFrame(SymbolId func, QuantizeMode qmode);

    //
    // Post-scheduling function handlers
    //

    void scheduleRecord(class UIAction* a);
    class TrackEvent* scheduleRecordEnd();
    class TrackEvent* addRecordEvent();
    bool isRecordSynced();
    void doRecord(class TrackEvent* e);

    void doInsert(class UIAction* a);
    void addExtensionEvent(int frame);

    void doMultiply(class UIAction* a);
    void doReplace(class UIAction* a);
    void doOverdub(class UIAction* a);
    void doMute(class UIAction* a);
    void doInstant(class UIAction* a);
    void doResize(class UIAction* a);

    class UIAction* copyAction(class UIAction* src);

};

