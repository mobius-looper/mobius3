/**
 * Class encapsulating lookup services for parameter values at runtime.
 *
 * The representation of parameter values, where they live, and how they can be
 * scoped is evolving.  All new code not involved in editing configuration objects should
 * obtain parameter values through this interface.
 *
 * Besides supporting future parameter binding mechanisms, it assists digging enumerated
 * parameters out of ValueSets, and does validatio against the parameter definition
 * associated with parameter symbols.
 *
 * One of these is created by Supervisor and shared throughout the kernel layers.
 * It is not currently necessary for this abstraction to used by the UI layer.
 *
 * Access methods will grow over time as need arises.
 * Most access will take place in the audio thread.
 *
 * There is conceptual overlap between this and core/ParameterSource.  Need to merge.
 */

#pragma once

#include "model/ParameterConstants.h"
#include "model/Session.h"

class ParameterFinder
{
  public:

    ParameterFinder(class Provider* p);
    ~ParameterFinder();

    //
    // Group 1, used by MIDI tracks
    //

    SyncSource getSyncSource(Session::Track* trackdef, SyncSource dflt);
    SyncTrackUnit getTrackSyncUnit(Session::Track* trackdef, SyncTrackUnit dflt);
    SyncUnit getSlaveSyncUnit(Session::Track* trackdef, SyncUnit dflt);
    
  private:

    class Provider* provider = nullptr;

};
