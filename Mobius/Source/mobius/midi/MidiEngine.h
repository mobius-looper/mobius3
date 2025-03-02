/**
 * Factory for configuring MidiTracks for TrackManager/LogicalTrack
 *
 * THIS IS NO LONGER USED
 *
 * I don't think the notion of a track factor is all that useful,
 * TrackManager/LogicalTrack can just instantiate them and have more control
 * over when the session is loaded.
 */

#pragma once

#include <JuceHeader.h>

#include "../track/TrackEngine.h"

class MidiEngine : public TrackEngine
{
  public:

    MidiEngine();
    ~MidiEngine();

    class BaseTrack* newTrack(class TrackManager* tm, class LogicalTrack* lt, class Session::Track* def);
    
  private:
    
};

        
    
 
