/**
 * Factory for configuring MidiTracks for TrackManager/LogicalTrack
 */

#pragma once

#include <JuceHeader.h>

#include "../track/TrackEngine.h"

class MidiEngine : public TrackEngine
{
  public:

    MidiEngine() {}
    ~MidiEngine();

    class BaseTrack* newTrack(class TrackManager* tm, class LogicalTrack* lt, class Session::Track* def);
    
  private:
    
};

        
    
 
