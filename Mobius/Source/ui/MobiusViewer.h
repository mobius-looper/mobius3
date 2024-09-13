/**
 * Intermediary to refresh the MobiusView from Mobius internal state
 * including the emerging MIDI tracks.
 */

#pragma once

#include <JuceHeader.h>

class MobiusViewer
{
  public:

    MobiusViewer(class Supervisor* s);
    ~MobiusViewer();

    void refresh(class MobiusInterface* mobius, class MobiusView* v);

  private:

    class Supervisor* supervisor = nullptr;
    
    // various state maintained for difference detection
    // might be better to have these in the view itself?
    int setupOrdinal = -1;
    int setupVersion = -1;

    void refreshTrack(class MobiusState* state, class MobiusTrackState* tstate, bool active,
                      class MobiusViewTrack* vt);

    void refreshTrackName(class MobiusState* state, class MobiusTrackState* tstate, class MobiusViewTrack* tview);
    void refreshMode(class MobiusTrackState* track, class MobiusViewTrack* vt);

};
