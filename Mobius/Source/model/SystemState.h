/**
 * An object representing the state of Kernel components at a moment in time.
 * One of these will be maintained by Supervisor and passed down to the Kernel periodically.
 * Each component may then contribute it's state.  THe state refresh is handled during
 * block processing in the audio thread, and then passed back to the UI where it
 * can drive the refresh of the UI.
 *
 * You might think of it like a very large Query result, where there is a single query
 * to refresh state rather than hundreds of individual Query's to access each piece.
 *
 * This is the latest of state redesigned, destined OldMobiusState.
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

    // the reference number of the track that has focus
    // this is an argument to the query passed from UI to kernel and determines
    // what is left in FocusedTrackState
    int focusedTrack = 0;

    // the number of tracks of each type, returned by the engine
    int audioTracks = 0;
    int midiTracks = 0;

    // ordinal of the active Setup
    // used to trigger track name refresh
    // needs to be phased out for the Session
    int setupOrdinal = 0;

    // an OldMobiusState flag
    // true if Mobius is in "capturing" mode, OldMobiusState called
    // this "globalRecording"
    bool audioCapturing = false;

    // another OldMobiusState holdover
    // among the audio tracks only, this one is considered active
    int activeAudioTrack = 0;

    // full state for each track
    juce::OwnedArray<TrackState> tracks;

    // details for the focused track only
    FocusedTrackState focusedState;

    // common synchronization state that is not track related
    SyncState syncState;

    // until the old core is converted, return a pointer to the old state model
    class OldMobiusState* oldState = nullptr;
};

   
    
    
