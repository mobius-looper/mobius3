/**
 * An extension of BaseScheduler for looping tracks.
 * 
 * This has a combination of functionality found in the old Synchronizer and EventManager classes
 * plus mode awareness that was strewn about all over in a most hideous way.  It interacts
 * with a LooperTrack that may either be a MIDI or an audio track, since the behavior of event
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

#include "BaseScheduler.h"
#include "LooperSwitcher.h"

class LooperScheduler : public BaseScheduler
{
    friend class LoopSwitcher;
    
  public:

    LooperScheduler(class TrackManager* tm, class LogicalTrack* lt, class LooperTrack* t);
    ~LooperScheduler();

    // BaseScheduler overloads
    void passAction(class UIAction* a) override;
    void passEvent(class TrackEvent* e) override;
    
  protected:

    // things LoopSwitcher needs
    class LooperTrack* track = nullptr;
    
  private:

    // handler for loop switch complexity
    LooperSwitcher loopSwitcher {*this};

    //
    // Scheduling and mode transition guts
    //
    
    bool doTransformation(class UIAction* src);
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

    // temporary
    void doParameter(class UIAction* src);
};

