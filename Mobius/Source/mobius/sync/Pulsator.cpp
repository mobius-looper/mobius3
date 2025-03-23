/**
 * Fingering the pulse, of the world.
 *
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/SessionConstants.h"
#include "../../model/Session.h"
#include "../../model/SyncConstants.h"
#include "../MobiusInterface.h"

#include "../track/LogicalTrack.h"
#include "../track/TrackManager.h"

#include "Pulse.h"

#include "MidiAnalyzer.h"
#include "MidiRealizer.h"
#include "HostAnalyzer.h"
#include "Transport.h"
#include "SyncAnalyzerResult.h"
#include "SyncMaster.h"

#include "Pulsator.h"

Pulsator::Pulsator(SyncMaster* sm, TrackManager* tm, BarTender* bt)
{
    syncMaster = sm;
    trackManager = tm;
    barTender = bt;
}

Pulsator::~Pulsator()
{
}

void Pulsator::loadSession(Session* s)
{
    (void)s;
}

//////////////////////////////////////////////////////////////////////
//
// Block Lifecycle
//
//////////////////////////////////////////////////////////////////////

/**
 * Called at the beginning of each audio block to gather sync pulses from various
 * sources and identify the ones of interest to the followers.
 */
void Pulsator::advance(int frames)
{
    // used to timestamp Pulses, not sure why I felt it necessary to
    // ensure that they all had the same timestamp, but if this is
    // important, it should be a higher level capture, in SyncMaster or Kernel
	millisecond = juce::Time::getMillisecondCounter();

    // used for verification in addLeaderPulse
	blockFrames = frames;

    reset();

    gatherHost();
    gatherMidi();
    gatherTransport();
    
    // leader pulses are added as the tracks advance
}

/**
 * Reset pulse tracking state at the beginning of each block.
 */
void Pulsator::reset()
{
    midiPulse.reset();
    hostPulse.reset();
    transportPulse.reset();

    // this could just be done by TrackManager, but I like having
    // all Pulse management here
    juce::OwnedArray<LogicalTrack>& tracks = trackManager->getTracks();
    for (auto t : tracks) {
        Pulse* p = t->getLeaderPulse();
        if (p->pending)
          p->pending = false;
        else
          p->reset();
    }
}

/**
 * Called by Leaders (tracks or other internal objects) to register the crossing
 * of a synchronization boundary after they were allowed to consume
 * this audio block.
 *
 * This is similar to the old Synchronizer::trackSyncEvent
 *
 * It is quite common for old Mobius to pass in a frameOffset that is 1+ the
 * last bufrfer frame, especially for Loop events where the input latency is the
 * same as the block size resulting in a loop that exactly a block multiple.  I
 * can't figure out why that is, and it's too crotchety to mess with.  So for
 * a block of 256, frameOffset will be 256 while the last addressable frame
 * is 255.  This is related to whether events on the loop boundary happen before
 * or after the loop wraps.  For sizing loops it shouldn't matter but if this
 * becomes a more general event scheduler, may need before/after flags.
 *
 * Adjusting it down to the last frame doesn't work because it will split at that
 * point with the event happening BEFORE the last frame.  The event really needs
 * to be processed at frame zero of the next buffer.
 *
 * UPDATE: The only leaders are now LogicalTracks which maintain their own Pulse.
 * Formerly had a parallel leader array here but it was too fragile.
 * So while comments may say that a "leader" is more general than a "track", there
 * are really only two types of leadership.  Either SyncSources that are not tracks
 * or leader tracks.
 */
void Pulsator::addLeaderPulse(int leaderId, SyncUnit unit, int frameOffset)
{
    LogicalTrack* lt = trackManager->getLogicalTrack(leaderId);
    if (lt != nullptr) {
        Pulse* pulse = lt->getLeaderPulse();
        
        pulse->reset(SyncSourceTrack, millisecond);
        pulse->unit = unit;
        pulse->blockFrame = frameOffset;

        if (frameOffset >= blockFrames) {
            // leave it pending and adjust for the next block
            pulse->pending = true;
            int wrapped = frameOffset - blockFrames;
            pulse->blockFrame = wrapped;
            if (wrapped != 0){
                // went beyond just the end of the block, I don't
                // think this should happen
                Trace(1, "Pulsator: Leader wants a pulse deep into the next block");
                // might be okay if it will still happen in the next block but if this
                // is larger than the block size, it's a serious error
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Pulse Gathering
//
//////////////////////////////////////////////////////////////////////

/**
 * Convert a SyncAnalyzerResult from an analyzer into a Pulse
 */
void Pulsator::convertPulse(SyncSource source, SyncAnalyzerResult* result, Pulse& pulse)
{
    if (result != nullptr) {

        // SyncAnalyzerResult doesn't return beat/bar numbers
        // it should return the beat number, but bars are ambiguous
        
        if (result->beatDetected) {

            pulse.reset(source, millisecond);
            pulse.blockFrame = result->blockOffset;

            // it's starts as a Beat, BarTender may change this later
            pulse.unit = SyncUnitBeat;

            // convey these, if they happen at the same time
            // blow off continue, too hard
            pulse.start = result->started;
            pulse.stop = result->stopped;

            // Transport and in theory Host can detect
            // bars natively, pass those along and let
            // BarTender sort it out
            if (result->loopDetected)
              pulse.unit = SyncUnitLoop;
            else if (result->barDetected)
              pulse.unit = SyncUnitBar;
        }
        else if (result->started) {
            // start without a beat, this can be okay, it just
            // means we're starting in the middle of a beat
            // todo: don't have a Pulse for start that isn't also
            // a UnitBeat, may need one
            pulse.reset(source, millisecond);
            pulse.unit = SyncUnitBeat;
            pulse.start = true;
            // doesn't really matter what this is
            pulse.blockFrame = 0;
        }
        else if (result->stopped) {
            // do we actually need a pulse for these?
            // unlike Start, Stop can happen pretty randomly
            // let BarTender sort it out
            pulse.reset(source, millisecond);
            pulse.unit = SyncUnitBeat;
            pulse.stop = true;
            // doesn't really matter what this is
            pulse.blockFrame = 0;
        }
    }
}

void Pulsator::gatherHost()
{
    HostAnalyzer* analyzer = syncMaster->getHostAnalyzer();
    SyncAnalyzerResult* result = analyzer->getResult();

    convertPulse(SyncSourceHost, result, hostPulse);
}

void Pulsator::gatherMidi()
{
    MidiAnalyzer* analyzer = syncMaster->getMidiAnalyzer();
    SyncAnalyzerResult* result = analyzer->getResult();

    convertPulse(SyncSourceMidi, result, midiPulse);
}    

void Pulsator::gatherTransport()
{
    Transport* t = syncMaster->getTransport();
    SyncAnalyzerResult* result = t->getResult();

    convertPulse(SyncSourceTransport, result, transportPulse);
}

void Pulsator::notifyTransportStarted()
{
    gatherTransport();
}

//////////////////////////////////////////////////////////////////////
//
// Pulse Detection
//
//////////////////////////////////////////////////////////////////////

/**
 * Return any block pulse that may be relevant for a follower.
 */
Pulse* Pulsator::getAnyBlockPulse(LogicalTrack* t)
{
    Pulse* pulse = nullptr;
    if (t != nullptr) {

        // once the follower is locked, you can't change the source out
        // from under it
        // ?? why was this necessary
        SyncSource source = t->getSyncSourceNow();
        int leader = 0;
        if (source == SyncSourceTrack) {

            // !! originally this factored in MIDI track leader settings
            // to determine what to synchronize recordings with, but that's wrong
            // TrackSync should always use the TrackSyncMaster consistently,
            // what leader/follower does is independent of synchronized recording
            // the difference between SyncSource and having a Leader track is messy
            // and needs more thought
            //leader = t->getSyncLeaderNow();
            //if (leader == 0)
            leader = syncMaster->getTrackSyncMaster();
        }

        // special case, if the leader is the follower, it means we couldn't find
        // a leader after starting which means it self-leads and won't have pulses
        if (leader != t->getNumber()) {

            pulse = getBlockPulse(source, leader);
        }
    }
    return pulse;
}

/**
 * Return the pulse object for a source if it is active in this block.
 */
Pulse* Pulsator::getBlockPulse(SyncSource source, int leader)
{
    Pulse* pulse = getPulseObject(source, leader);
    if (pulse != nullptr) {
        if (pulse->source == SyncSourceNone)
          pulse = nullptr;
        else if (pulse->pending)
          pulse = nullptr;
        else {
            int x = 0; // break here
            (void)x;
        }
    }
    return pulse;
}

/**
 * Return the pulse tracking object for a particular source.
 */
Pulse* Pulsator::getPulseObject(SyncSource source, int leader)
{
    Pulse* pulse = nullptr;

    switch (source) {
        case SyncSourceNone:
            break;
                    
        case SyncSourceMidi:
            pulse = &(midiPulse);
            break;
                    
        case SyncSourceHost:
            pulse = &(hostPulse);
            break;
                    
        case SyncSourceMaster: 
        case SyncSourceTransport:
            pulse = &(transportPulse);
            break;
                    
        case SyncSourceTrack: {
            // leader can be zero here if there was no track sync leader
            // in which case there won't be a pulse
            // don't call getLeader with zero or it traces an error
            if (leader > 0) {
                LogicalTrack* lt = trackManager->getLogicalTrack(leader);
                if (lt != nullptr) {
                    pulse = lt->getLeaderPulse();
                }
            }
        }
            break;
    }

    return pulse;
}

// old, SyncMaster now does this
#if 0
/**
 * This is where it all comes together...
 *
 * TimeSlicer wants to know if a given track has any sync pulses in this block
 * that are RELEVANT to what this track is synchronizing on.  Relevance involves
 * both the SyncSource and the SyncUnit.   SyncAnalyzers give us beat pulses
 * that have been captured and converted into Pulse objects for each source.
 *
 * Those pulses now pass through BarTender which determines whether those pulses
 * are more than just beats.  A massaged pulse is then returned for TimeSlicer
 * to do its thing.
 *
 * Note: For SyncSourceTrack, leader pulses will not be available until the
 * leader tracks advance so TimeSlicer needs to order them according to
 * leader dependencies.
 *
 * Control flow is annoying here.  SyncMaster owns BarTender which we
 * need, in order to pass a result back to SyncMaster.  Perhaps it would
 * be better if Pulsator was just in charge of getAnyBlockPulse,
 * then let SyncMaster and/or BarTender be in control over relevance checking.
 */
Pulse* Pulsator::getBlockPulse(LogicalTrack* t, SyncUnit unit)
{
    Pulse* pulse = nullptr;
    
    // start by locating the base pulse for the source this
    // track is following
    Pulse* base = getAnyBlockPulse(t);
    if (base != nullptr) {

        // derive a track-specific Pulse with bar awareness
        // this object is transient and maintained by BarTender,
        // it must be consumed before calling BarTender again
        Pulse* annotated = barTender->annotate(t, base);

        if (annotated != nullptr) {

            // filter out pulses that don't match the given unit

            // there was a pulse from this source
            if (unit == SyncUnitBeat) {
                // anything is a beat
                pulse = annotated;
            }
            else if (unit == SyncUnitBar) {
                // loops are also bars
                if (annotated->unit == SyncUnitBar || annotated->unit == SyncUnitLoop)
                  pulse = annotated;
            }
            else {
                // only loops will do
                if (annotated->unit == SyncUnitLoop) {
                    pulse = annotated;
                }
                // formerly had a fallback to accept Bar units if the
                // host didn't support the concept of a Loop, but they
                // all should now and BarTender will flag it
            }
        }
    }
    return pulse;
}
#endif

//////////////////////////////////////////////////////////////////////
//
// Diagnostics
//
//////////////////////////////////////////////////////////////////////

void Pulsator::trace()
{
    if (hostPulse.source != SyncSourceNone) trace(hostPulse);
    if (midiPulse.source != SyncSourceNone) trace(midiPulse);
    if (transportPulse.source != SyncSourceNone) trace(transportPulse);

    // don't own the leaders any more, could go through the LogicalTracks
#if 0    
    for (auto leader : leaders) {
        if (leader->pulse.source != SyncSourceNone)
          trace(leader->pulse);
    }
#endif    
}

void Pulsator::trace(Pulse& p)
{
    juce::String msg("Pulsator: ");

    msg += getSourceName(p.source);
    msg += " ";
    msg += getUnitName(p.unit);

    if (p.start) msg += " Start ";
    if (p.stop) msg += " Stop ";
    //if (p.mcontinue) msg += "Continue ";

    Trace(2, "%s", msg.toUTF8());
}

const char* Pulsator::getSourceName(SyncSource source)
{
    const char* name = "???";
    switch (source) {
        case SyncSourceNone: name = "None"; break;
        case SyncSourceTransport: name = "Transport"; break;
        case SyncSourceTrack: name = "Leader"; break;
        case SyncSourceHost: name = "Host"; break;
        case SyncSourceMidi: name = "Midi"; break;
        case SyncSourceMaster: name = "Master"; break;
    }
    return name;
}

const char* Pulsator::getUnitName(SyncUnit unit)
{
    const char* name = "???";
    switch (unit) {
        case SyncUnitBeat: name = "Beat"; break;
        case SyncUnitBar: name = "Bar"; break;
        case SyncUnitLoop: name = "Loop"; break;
        case SyncUnitNone: name = "None"; break;
    }
    return name;
}    

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

