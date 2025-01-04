/**
 * Start by carving up the audio block based on sync pulses.
 * Will eventually need to MSL Session waits and inter-track quantization.
 *
 * MobiusAudioStream provides the port buffers and the number of frames in the block.
 * TimeSlider needs to advance each track using only subsets of this block.
 *
 * The float pointers returned by MobiusAudioStream::getInterruptBuffers are normally
 * expected to be fully consumed.  Instead a wrapper around this can be added
 * that takes a block offset.  This offset is used to return pointers into the stream
 * buffers higher than what is availale and reduce the available frame count.
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

    // make sure this is large enough to contain a reasonably high number
    // of slices without dynamic allocation in the audio thread
    int maxSlices = 100;
    slices.ensureStorageAllocated(100);

    test();
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
    gatherSlices();
    orderTracks();
    
    LogicalTrack* track = nextTrack();
    while (track != nullptr) {
        
        if (slizes.size() == 0) {
            // just take the whole thing
            advanceTrack(stream);
        }
        else {
            // in theory advancing this track could result in changes to the track order
            // you can't undo a track advance once it is started some some very rare dependencies
            // won't be handled, though in theory we could half this particular tracks slice
            // advance and allow the new dependency to advance first, should at least warn
            AudioStreamSlicer ass(stream);
            int blockOffset = 0;
        
            for (int i = 0 ; i < slices.size() ; i++) {
                Slice& s = slices.getReference(location);
            
                int sliceLength = s.blockOffset - blockOffset;
                // it is permissible to have a slice of zero if there is more
                // than one pulse on the same frame
                if (sliceLength > 0) {
                    ass.setSlice(blockOffset, sliceLength);
                    advanceTrack(track, &ass);
                    blockOffset += sliceLength;
                }
            }
        }

        int remainder = stream->getInterruptFrames() - blockOffset;
        if(remainder > 0) {
            ass.setSlice(blockOffset, remainder);
            advanceTrack(track, &ass);
        }
        else if (remainder < 0) {
            Trace(1, "TimeSlicer: Block offset math is fucked");
        }

        track = nextTrack();
    }
}

void TimeSlicer::advanceTrack(LogicalTrack* track, MobiusAudioStream* stream)
{
    track->processAudioStream(stream);

    // in theory the track could have changed leader/follower relationships which
    // would require reordering of just the remaining tracks that have not been procssed
    // it is much more common for a track advance to add new slices if a track sync
    // boundary was crossed, or a pending Wait was activated
    // the new slices will only be seen by subsequent tracks
}

/**
 * Determine the initial order for all tracks.
 */
void TimeSlicer::orderTracks()
{
    juce::OwnedArray<LogicalTrack>* trackManager->getTracks();

    
    
}

void TimeSlicer::advanceTracks(juce::OwnedArray<LogicalTrack>* tracks,
                               MobiusAudioStream* stream)
{
    // simulate what Mobius::processAudioStream did for trackSyncMaster
    // todo: order needs to be MUCH more complicated with full leader/follower
    // dependency ordering
    LogicalTrack* master = nullptr;
    for (auto track : tracks) {
        if (track->getNumber() == syncMaster->getTrackSyncMaster()) {
            master = track;
            break;
        }
    }

    if (master != nullptr)
      master->processAudioStream(stream);
    
    for (auto track : tracks) {
        if (track != master)
          track->processAudioStream(stream);
    }
}

void TimeSlicer::gatherSlices()
{
    slices.clearQuick();
    
    // only expecting one pulse per block per type for thee
    insertPulse(syncMaster->getBlockPulse(Pulse::SourceTransport));
    insertPulse(syncMaster->getBlockPulse(Pulse::SourceHost));
    insertPulse(syncMaster->getBlockPulse(Pulse::SourceMidiIn));
}

void TimeSlicer::insertPulse(Pulse* p)
{
    if (p != nullptr) {
        int location = 0;
        while (location < slices.size()) {
            Slice& s = slices.getReference(location);
            if (p->blockFrame < s.blockOffset)
              break;
            else
              location++;
        }

        Slice neu;
        neu.blockOffset = p->blockFrame;
        neu.pulse = p;
        slices.insert(location, neu);

    }
}

void TimeSlicer::test()
{
    slices.clearQuick();
    Pulse p;
    p.blockFrame = 100;
    insertPulse(&p);

    Pulse p2;
    p2.blockFrame = 10;
    insertPulse(&p2);
    
    Pulse p3;
    p3.blockFrame = 200;
    insertPulse(&p3);

    Pulse p4;
    p4.blockFrame = 150;
    insertPulse(&p4);

    Pulse p5;
    p5.blockFrame = 1;
    insertPulse(&p5);

    for (int i = 0 ; i < slices.size() ; i++) {
        Slice& s = slices.getReference(i);
        Trace(2, "TimeSlicer::test %d", s.blockOffset);
    }
}


                
    
