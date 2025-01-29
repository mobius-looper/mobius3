/**
 * An object representing the state of Kernel components at a moment in time.
 * One of these will be maintained by Supervisor and passed down to the Kernel periodically.
 * Each component may then contribute it's state.  The state refresh is handled during
 * block processing in the audio thread, and then passed back to the UI where it
 * can drive the refresh of the UI.
 *
 * You might think of it like a very large Query result, where there is a single query
 * to refresh state rather than hundreds of individual Query's to access each piece.
 *
 * The state object is allocated by the UI/Shell and must be fleshed out with enough variable
 * length containers to hold what the kernel wants to return.
 *
 * TrackState contains that is needed for all tracks.
 * FocusedTrackState contains additional details that are only gathered for one track.
 */

#pragma once

#include "SyncState.h"
#include "TrackState.h"

class SystemState
{
  public:

    SystemState() {}
    ~SystemState() {}

    // the version number of the Session this state was built with
    // used by MobiusViewer to detect when track configuration has
    // finished being consumed by the engine which needs a full UI refresh
    // also used to ignore old state objects in the queue built with
    // the old version
    int sessionVersion = 0;

    // full state for each track
    juce::OwnedArray<TrackState> tracks;

    // number of tracks used, this may be smaller than the array size
    int totalTracks = 0;

    // the reference number of the track that has focus
    // this is passed down from Supervisor to TrackManager to tell
    // it which track should be used to fill FocusedTrackState
    int focusedTrackNumber = 0;

    // details for the focused track only
    FocusedTrackState focusedState;

    // common synchronization state that is not track related
    SyncState syncState;

    // an OldMobiusState flag
    // true if Mobius is in "capturing" mode, OldMobiusState called
    // this "globalRecording"
    bool audioCapturing = false;
};

   
    
    
