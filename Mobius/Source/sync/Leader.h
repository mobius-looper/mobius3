/**
 * A Leader is an internal object that can generate sync pulses.
 * In practice, only audio and midi tracks can be leaders though I suppose
 * there could be other forms of pulse generators in the future.
 *
 * A leader maintains state (normally a loop) that advances with the audio
 * stream and are therefore always in perfect sync with the stream.
 *
 * On each audio block a leader advances it's internal state, and when it
 * crosses a synchronization boundary, it informs the Pulsator.  A leader may
 * have any number of Followers that watch for pulses from the leader.
 *
 * This is more general than it needs to be since Leaders and Followers will
 * all be just tracks, but I'm keeping the options open and it reduces code
 * confusion if you make the roles clearer.
 *
 */

#pragma once

#include "SyncConstants.h"

class Leader
{
  public:

    // the leader "id", which must currently be a track number
    // starting from 1, might want this to be symbolic track ids eventually
    int id = 0;

    // the pulse this leader may register on each audio block
    Pulse pulse;

    void reset() {
        // if the pulse was marked pending, leave it active at the start
        // of the next block
        if (pulse.pending) {
            pulse.pending = false;
        }
        else {
            pulse.source = SyncSourceNone;
        }
    }
    
  private:

};    
