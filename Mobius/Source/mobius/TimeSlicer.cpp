/**
 * Start by carving up the audio block based on sync pulses.
 * Will eventually need to MSL Session waits and inter-track quantization.
 *
 */
 
#include <JuceHeader.h>

#include "MobiusInterface.h"
#include "MobiusKernel.h"

#include "../sync/SyncMaster.h"
#include "track/TrackManager.h"

TimeSlicer::TimeSlicer(MobiusKernel* k, SyncMaster* sm, TrackManager* tm)
{
    kernel = k;
    syncMaster = sm;
    trackManager = tm;
}

TimeSlicer::~TimeSlicer()
{
}

/**
 * Where the rubber meets the road and/or the shit hits the fan.
 *
 */
void TimeSlicer::processAudioStream(MobiusAudioStream* stream)
{
    // stub, this is what Kernel used to do
    trackManager->processAudioStream(stream);

    // what TM currently does is:
    // advance the LongWatcher, can be pullled out into a preparation phase
    // calls mCore->processAudioStream to advance ALL audio tracks
    // calls LogicalTrack::processAudioStream but only for MIDI tracks

    // Mobius::processAudioStream first looks for the trackSyncMaster which
    // has the "priority" flag set, then it calls track->processAudioStream
    // on that, then on the others
    // it finishes with endAudiioStream which does notifications and a few
    // other things unrelated to track advance

}

                     
