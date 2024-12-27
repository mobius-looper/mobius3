/**
 * An object representing the state of most parts of the system at a moment in time.
 * One of these will be maintained by Supervisor and passed around periodically where each
 * system component may contribute it's state.  This state then drives the construction
 * of the Mobius View which in turn drives most of the UI.
 *
 * You might think of it like a very large Query result, where there is a single query
 * to refresh state rather than hundreds of individual Query's to access each piece.
 *
 * This is the latest of state redesigned, destined to replace MobiusState and OldMobiusState.
 */

#pragma once

#include "../sync/SyncMasterState.h"
#include "TrackState.h"
#incldue "DynamicState.h"

class SystemState
{
  public:

    SystemState() {}
    ~SystemState() {}

    // state for various non-track components
    SyncMasterState syncState;

    // state for each track
    juce::OwnedArray<TrackState> tracks;

    // special variable size state that is harder to export
    // the object this points to is managed by the kernel and will remain stable
    DynamicState* dynamicState = nullptr;

    // until the old core is converted, return a pointer to the old state model
    class OldMobiusState* oldState = nullptr;
};

   
    
    
