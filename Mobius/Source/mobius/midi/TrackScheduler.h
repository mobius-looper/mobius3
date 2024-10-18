/**
 * The scheduler is responsible for determining when actions happen and
 * managing the transition between major and minor modes.  In doing so it
 * also coordinate some of the behavior of the Player and Recorder.
 *
 * It manage's the track's EventList and handes the stacking of events.
 * Eventually this will be the component responsible for latency compensation.
 *
 * Because a lot of the complexity around scheduling requires understanding the meaning
 * of various functions, much of what this does has overlap with what old Mobius
 * would call the Function handlers.  This should be generalized as much as possible
 * leaving the Track to decide how to implement the behavior of those functions.
 *
 * This is one of the most subtle parts of track behavior, and what is does is
 * conceptually common to both audio and midi tracks.  In the longer term, try to avoid
 * dependencies on MIDI-specific behavior so that this can eventually be shared
 * by all track types.  To that end, try to abstract the use of MidiPlayer and MidiRecorder
 * and instead ask Track to be the intermediary between logical actions and how
 * they are actually performed.
 */

#pragma once

#include "../../sync/Pulse.h"
#include "../../model/MobiusMidiState.h"
#include "../../model/Session.h"
#include "../../model/ParameterConstants.h"
#include "../../model/SymbolId.h"

#include "TrackEvent.h"

class TrackScheduler
{
  public:

    TrackScheduler(class AbstractTrack* t);
    ~TrackScheduler();
    
    void initialize(class TrackEventPool* epool, class UIActionPool* apool,
                    class Pulsator* p, class Valuator* v, class SymbolTable* st);
    void configure(Session::Track* def);
    void dump(class StructureDumper& d);
    void reset();
    //void shiftEvents(int frames, int remainder);
    void refreshState(class MobiusMidiState::Track* state);
    
    // the main entry point from the track to get things going
    void doAction(class UIAction* a);
    void doParameter(class UIAction* a);
    void advance(class MobiusAudioStream* stream);

    // Track callbacks for that awful multiply termination mode
    bool hasRoundingScheduled();
    void cancelRounding();

  private:

    class AbstractTrack* track = nullptr;
    class TrackEventPool* eventPool = nullptr;
    class UIActionPool* actionPool = nullptr;
    class Pulsator* pulsator = nullptr;
    class Valuator* valuator = nullptr;
    class SymbolTable* symbols = nullptr;
    
    // configuration
    Pulse::Source syncSource = Pulse::SourceNone;
    int syncLeader = 0;

    TrackEventList events;

    // block advance
    void consume(int frames);

    // generic action processing
    void doActionInternal(class UIAction* a);
    void doStacked(class TrackEvent* actions);
    void doActionNow(class UIAction* a);
    void checkModeCancel(class UIAction* a);

    // advance and events
    void doEvent(class TrackEvent* e);
    void dispose(class TrackEvent* e);
    void dispose(class UIAction* a);
    void doPulse(class TrackEvent* e);

    // Rounding and Quantization
    bool isModeEnding(MobiusMidiState::Mode mode);
    void scheduleModeEnd(class UIAction* a, MobiusMidiState::Mode mode);
    bool isQuantized(class UIAction* a);
    void scheduleQuantized(class UIAction* a);
    int getQuantizedFrame(QuantizeMode qmode);
    int getQuantizedFrame(SymbolId func, QuantizeMode qmode);
    void addExtensionEvent(int frame);
    
    // Record
    bool isRecord(class UIAction* a);
    void scheduleRecord(class UIAction* a);
    class TrackEvent* scheduleRecordEvent(class UIAction* a);
    bool isRecordSynced();
    void stackRecord(class TrackEvent* recordEvent, class UIAction* a);
    void doRecord(class TrackEvent* e);

    // Various
    void doMultiply(class UIAction* a);
    void doInsert(class UIAction* a);
    void doReplace(class UIAction* a);
    void doOverdub(class UIAction* a);
    void doMute(class UIAction* a);
    void doInstant(class UIAction* a);
    
    // Switch
    bool isLoopSwitch(class UIAction* a);
    void scheduleSwitch(class UIAction* a);
    int getSwitchTarget(class UIAction* a);
    int getQuantizedFrame(SwitchQuantize squant);
    QuantizeMode convert(SwitchQuantize squant);
    void stackSwitch(class UIAction* a);
    void doSwitch(class TrackEvent* e, int target);

};

