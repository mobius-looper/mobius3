/**
 * The purpose of the view/viewer is to provide a cleaner base model for the
 * UI to refresh itself and hide the old OldMobiusState and a few other core classes
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

class MobiusViewer
{
  public:

    MobiusViewer(class Provider* p);
    ~MobiusViewer();

    void initialize(class MobiusView* view);
    void configure(class MobiusView* view);
    void refresh(class SystemState* state, class MobiusView* v);
    void forceRefresh(MobiusView* v);
    
  private:

    class Provider* provider = nullptr;
    Query subcyclesQuery;

    void resetRefreshTriggers(class MobiusView* view);

    void refreshAudioTracks(class OldMobiusState* state, class MobiusView* view);
    void refreshTrack(class OldMobiusState* state, class OldMobiusTrackState* tstate,
                      class MobiusView* mview, class MobiusViewTrack* vt,
                      bool active);

    //void refreshTrackName(class OldMobiusState* state, class OldMobiusTrackState* tstate,
    //class MobiusView* mview, class MobiusViewTrack* tview);

    //void refreshTrackProperties(class OldMobiusTrackState* tstate, class MobiusViewTrack* tview);
    //void refreshSync(class OldMobiusTrackState* tstate, class MobiusViewTrack* tview);
    //void refreshTrackGroups(class OldMobiusTrackState* tstate,  class MobiusViewTrack* tview);
    //void refreshInactiveLoops(class OldMobiusTrackState* tstate, class MobiusViewTrack* tview);
    void refreshActiveLoop(class OldMobiusTrackState* tstate, class OldMobiusLoopState* lstate,
                           bool activeTrack, class MobiusViewTrack* tview);
    //void refreshMode(class OldMobiusTrackState* tstate, class MobiusViewTrack* tview);
    void refreshSubcycles(class MobiusViewTrack* tview);
    void refreshLayers(class OldMobiusLoopState* lstate, class MobiusViewTrack* tview);
    //void refreshEvents(class OldMobiusLoopState* lstate, class MobiusViewTrack* tview);
    //void refreshMinorModes(class OldMobiusTrackState* tstate, class OldMobiusLoopState* lstate,
    //class MobiusViewTrack* tview);

    //
    // New State model
    // Originally just MIDI tracks, now both
    //
    
    void refreshAllTracks(class SystemState* state, class MobiusView* view);
    //void refreshMidiTracks(class SystemState* state, class MobiusView* view);
    void refreshTrack(class SystemState* state, class TrackState* tstate,
                      class MobiusView* mview, class MobiusViewTrack* tview);
    void refreshTrackName(class SystemState* state, class TrackState* tstate,
                          class MobiusView* mview, class MobiusViewTrack* tview);
    void refreshMinorModes(class SystemState* state, class TrackState* tstate, class MobiusViewTrack* tview);
    void assembleMinorModes(class MobiusViewTrack* tview);
    void addMinorMode(class MobiusViewTrack* tview, const char* mode);
    void addMinorMode(class MobiusViewTrack* tview, const char* mode, int arg);
    
    void refreshSync(class SystemState* state, class TrackState* tstate, class MobiusViewTrack* tview);
    void refreshTrackGroups(class TrackState* tstate,  class MobiusViewTrack* tview);

    //
    // Focused Track
    //
    
    void refreshLayers(class FocusedTrackState* tstate, class MobiusViewTrack* tview);
    void refreshRegions(class FocusedTrackState* tstate, class MobiusViewTrack* tview);
    void refreshEvents(class FocusedTrackState* tstate, class MobiusViewTrack* tview);
    void expandEventName(class TrackState::Event& e, juce::String& name);
    
};

