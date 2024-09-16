/**
 * The purpose of the view/viewer is to provide a cleaner base model for the
 * UI to refresh itself and hide the old MobiusState and a few other core classes
 * as those are redesigned.
 *
 * It also does some useful difference detection which could grow into the foundation
 * for MIDI export and OSC export.
 *
 * And finally it combines old Mobius audio tracks with new MIDI tracks so the
 * UI doesn't have to care about the differences.
 */

#pragma once

#include <JuceHeader.h>

#include "../model/Query.h"
#include "../model/MobiusMidiState.h"

class MobiusViewer
{
  public:

    MobiusViewer(class Supervisor* s);
    ~MobiusViewer();

    void initialize(class MobiusView* view);
    void refresh(class MobiusInterface* mobius, class MobiusState* state, class MobiusView* v);
    void forceRefresh(MobiusView* v);
    
  private:

    class Supervisor* supervisor = nullptr;
    Query subcyclesQuery;
    
    void resetRefreshTriggers(class MobiusView* view);

    void refreshAudioTracks(class MobiusInterface* mobius, class MobiusState* state, class MobiusView* view);
    void refreshTrack(class MobiusState* state, class MobiusTrackState* tstate,
                      class MobiusView* mview, class MobiusViewTrack* vt,
                      bool active);

    void refreshTrackName(class MobiusState* state, class MobiusTrackState* tstate,
                          class MobiusView* mview, class MobiusViewTrack* tview);

    void refreshTrackProperties(class MobiusTrackState* tstate, class MobiusViewTrack* tview);
    void refreshSync(class MobiusTrackState* tstate, class MobiusViewTrack* tview);
    void refreshTrackGroups(class MobiusTrackState* tstate,  class MobiusViewTrack* tview);
    void refreshInactiveLoops(class MobiusTrackState* tstate, class MobiusViewTrack* tview);
    void refreshActiveLoop(class MobiusTrackState* tstate, class MobiusLoopState* lstate,
                           bool activeTrack, class MobiusViewTrack* tview);
    void refreshMode(class MobiusTrackState* tstate, class MobiusViewTrack* tview);
    void refreshSubcycles(class MobiusViewTrack* tview);
    void refreshLayers(class MobiusLoopState* lstate, class MobiusViewTrack* tview);
    void refreshEvents(class MobiusLoopState* lstate, class MobiusViewTrack* tview);
    void refreshMinorModes(class MobiusTrackState* tstate, class MobiusLoopState* lstate,
                           class MobiusViewTrack* tview);
    void assembleMinorModes(class MobiusViewTrack* tview);
    void addMinorMode(class MobiusViewTrack* tview, const char* mode);
    void addMinorMode(class MobiusViewTrack* tview, const char* mode, int arg);

    void refreshMidiTracks(class MobiusInterface* mobius, class MobiusView* view);
    void refreshMidiTrack(class MobiusMidiState::Track* tstate, class MobiusViewTrack* tview);
    
};
