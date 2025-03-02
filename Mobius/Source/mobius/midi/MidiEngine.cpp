
#include <JuceHeader.h>

#include "../../model/Session.h"

#include "../track/TrackManager.h"
#include "../track/LogicalTrack.h"
#include "../track/BaseTrack.h"

#include "MidiTrack.h"
#include "MidiEngine.h"

// turns out this is transient but keep it around for awhile
// could just have a static factory method instead
MidiEngine::MidiEngine()
{
}

MidiEngine::~MidiEngine()
{
}


/**
 * Now we're finally getting down to the Gordian Knot that is
 * Action scheduling for MIDI tracks.
 *
 * MidiTracks make use of BaseScheduler coupled with LooperScheduler
 * to process actions, schedule events, and advance the audio stream.
 *
 * Still not entirely happy with how this is shaking out, but it's a start.
 */
BaseTrack* MidiEngine::newTrack(TrackManager* tm, LogicalTrack* lt, Session::Track* def)
{
    (void)def;
    MidiTrack* mt = new MidiTrack(tm, lt);

    // loadSession is replaced with refreshParameters
    // but we shouldn't be using this any more
    //mt->loadSession(def);
    
    return mt;
}
    

    


