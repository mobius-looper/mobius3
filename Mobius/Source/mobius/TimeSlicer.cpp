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
 * Next order the tracks according to follower/leader dependencies.  Following
 * relationships can change as tracks are advanced, so the list may need to be
 * reordered during iteration.
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

    // be informed about follower changes
    syncMaster->addListener(this);

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
 */
void TimeSlicer::processAudioStream(MobiusAudioStream* stream)
{
    prepareTracks();
    
    LogicalTrack* track = nextTrack();
    while (track != nullptr) {

        gatherSlices(track);
        
        if (slizes.size() == 0) {
            // just take the whole thing
            advanceTrack(track, stream);
        }
        else {
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

                // now let the track know about this pulse
                notifyPulse(track, s);
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

        track->setAdvanced(true);
        track = nextTrack();
    }
}

/**
 * Advance a track one time slice.
 * During this advance, the track will process it's own internal events
 * which may causes a few changes that impact how we advance the block.
 *
 * If a follow was added, this may change the track dependency order.
 * It's too late for the track currently being advanced, but if the track
 * resumed a script, that could cause follows in other tracks, rare but possible.
 * If this track unfollows, this could relax a dependency but this is rare and
 * unlikely to cause problems.
 *
 * It is more common for a track to add slices.  Since a track can't slice itself
 * this won't impact the current advance, but it may impact the advance the
 * tracks after this one.
 */
void TimeSlicer::advanceTrack(LogicalTrack* track, MobiusAudioStream* stream)
{
    track->processAudioStream(stream);
}

/**
 * Here we've just advanced the track up to the frame where a pulse resides.
 */
void TimeSlicer::notifyPulse(LoticalTrack* track, Slice& slice)
{
    // these can only be Pulses right now, eventually other types of
    // slice may exist
    track->syncPulse(slice.pulse);
}

//////////////////////////////////////////////////////////////////////
//
// Slice Ordering
//
//////////////////////////////////////////////////////////////////////

/**
 * This is duplicating and simplifying some of the logic
 * in Pulsator::getPulseFrame which is what MIDI tracks have been using.
 * That will only return a frame if both the pulse source and pulse TYPE
 * matches (e.g. it won't return Beat pulses if the track wants Bars).
 */
void TimeSlicer::gatherSlices(LogicalTrack* track)
{
    slices.clearQuick();

    // first the sync pulses
    // since these are rare, could have SM just set a flag if any
    // pulses were received on this block and save some effort
    Follower* f = syncMaster->getFollower(track->getNumber());
    insertPulse(syncMaster->getBlockPulse(f));
                
    // todo: now add slices for external quantization points
    // or other more obscure things
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

//////////////////////////////////////////////////////////////////////
//
// Track Dependency Ordering
//
//////////////////////////////////////////////////////////////////////

/**
 * SyncMaster callback whenever follower/leader changes are made.
 */
void TimeSlicer::syncFollowChanges()
{
    ordered = false;
}

/**
 * Called at the start of each block by processAudioStream.
 * Reset the state flags maintained on the LogicalTracks that aid the
 * ordered traversal.
 */
void TimeSlicer::prepareTracks()
{
    juce::OwnedArray<LogicalTrack>* tracks = trackMaster->getTracks();
    for (auto track : tracks) {
        track->setVisited(false);
        track->setAdvanced(false);
    }
    
    if (!ordered)
      orderTracks();
}

/**
 * As usual, the simple case is simple, and the complex case is very complex.
 * We're going to handle the most common cases.  Cycles are dependency cycles
 * are broken and we don't try to be smart about those.
 */
void TimeSlicer::orderTracks()
{
    orderedTracks.clearQuick();

    juce::OwnedArray<LogicalTrack>* tracks = trackMaster->getTracks();
    for (auto track : tracks) {
        orderTracks(track);
    }
    ordered = true;
}

void TimeSlicer::orderTracks(LogicalTrack* t)
{
    if (!t->isVisited()) {
        t->setVisited(true);
        Follower* f = syncMaster->getFollower(t->getNumber());
        if (f != nullptr && f->source == Pulse::SourceLeader) {
            
            int leader = f->leader;
            if (leader == 0)
              leader = syncMaster->getTrackSyncMaster();

            if (leader > 0) {
                LogicalTrack* lt = trackManager->getLogicalTrack(leader);
                if (lt != nullptr) {
                    orderTracks(lt);
                }
            }
        }
        orderedTracks.add(t);
    }
}

/**
 * Return the next track to advance to the outer loop.
 */
LogicalTrack* TimeSlicer::nextTrack()
{
    LogicalTrack* next = nullptr;

    if (!ordered) {
        orderTracks();
        orderIndex = 0;
    }

    while (next == nullptr && orderIndex < orderedTracks.size()) {
        LogicalTrack* lt = orderedTracks[orderedIndex];
        if (!lt->isAdvanced())
          next = lt;
        orderIndex++;
    }

    return next;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
