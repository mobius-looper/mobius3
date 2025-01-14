/**
 * Fingering the pulse, of the world.
 *
 */

#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../model/SessionConstants.h"
#include "../model/Session.h"
#include "../mobius/MobiusInterface.h"

#include "SyncConstants.h"
#include "Pulse.h"
#include "Leader.h"
#include "Follower.h"

#include "MidiAnalyzer.h"
#include "MidiRealizer.h"
#include "HostAnalyzer.h"
#include "Transport.h"
#include "SyncAnalyzerResult.h"
#include "SyncMaster.h"

#include "Pulsator.h"

Pulsator::Pulsator(SyncMaster* sm)
{
    syncMaster = sm;
    // annoying dependency
    barTender = syncMaster->getBarTender();
}

Pulsator::~Pulsator()
{
}

/**
 * Called during initialization and after anything changes that might
 * impact the leader or follower count.  Ensure that the arrays are large enough
 * to accept any registration of followers or leaders.
 *
 * In current use, followers and leaders are always audio or midi tracks
 * and ids are always track numbers.  This simplification may not always
 * hold true.
 */
void Pulsator::loadSession(Session* s)
{
    int numFollowers = 0;
    numFollowers = s->audioTracks;
    numFollowers += s->midiTracks;

    // ensure the array is big enough
    // !! this is potentially dangerous if tracks are actively registering
    // follows and the array needs to be resized.  Would be best to allow this
    // only when in a state of GlobalReset
    // +1 is because follower ids are 1 based and are array indexes
    // follower id 0 is reserved
    numFollowers++;
    for (int i = followers.size() ; i < numFollowers ; i++) {
        Follower* f = new Follower();
        f->id = i;
        followers.add(f);
    }

    // leaders are the same as followers
    int numLeaders = numFollowers;
    for (int i = leaders.size() ; i < numLeaders ; i++) {
        Leader* l = new Leader();
        l->id = i;
        leaders.add(l);
    }
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
    
    // this is where pending pulses that were just over the last block
    // are activated for this block
    for (auto leader : leaders)
      leader->reset();
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
 */
void Pulsator::addLeaderPulse(int leaderId, SyncUnit unit, int frameOffset)
{
    Leader* l = getLeader(leaderId);
    if (l != nullptr) {
        l->pulse.reset(SyncSourceTrack, millisecond);
        l->pulse.unit = unit;
        l->pulse.blockFrame = frameOffset;

        if (frameOffset >= blockFrames) {
            // leave it pending and adjust for the next block
            l->pulse.pending = true;
            int wrapped = frameOffset - blockFrames;
            l->pulse.blockFrame = wrapped;
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
Pulse* Pulsator::getRelevantBlockPulse(int trackNumber)
{
    Pulse* pulse = nullptr;
    
    Follower* f = getFollower(trackNumber);
    if (f != nullptr) {

        // start by locating the base pulse for the source this
        // track is following
        Pulse* base = getAnyBlockPulse(f);

        if (base != nullptr) {

            // derive a track-specific Pulse with bar awareness
            // this object is transient and maintained by BarTender,
            // it must be consumed before calling BarTender again
            Pulse* annotated = barTender->annotate(f, base);

            if (annotated != nullptr) {

                if (isRelevant(f, annotated)) {

                    pulse = annotated;
                }
            }
        }
    }
    return pulse;
}

//////////////////////////////////////////////////////////////////////
//
// Pulse Gathering
//
//////////////////////////////////////////////////////////////////////

/**
 * Convert a SyncAnalyzerResult from an analyzer into a Pulse
 */
void Pulsator::convertPulse(SyncAnalyzerResult* result, Pulse& pulse)
{
    if (result != nullptr) {

        // SyncAnalyzerResult doesn't return beat/bar numbers
        // it should return the beat number, but bars are ambiguous
        
        if (result->beatDetected) {

            pulse.reset(SyncSourceHost, millisecond);
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
            // !! don't have a Pulse for start that isn't also
            // a UnitBeat, may need one
            pulse.reset(SyncSourceHost, millisecond);
            pulse.unit = SyncUnitBeat;
            pulse.start = true;
            // doesn't really matter what this is
            pulse.blockFrame = 0;
        }
        else if (result->stopped) {
            // do we actually need a pulse for these?
            // unlike Start, Stop can happen pretty randomly
            // let BarTender sort it out
            pulse.reset(SyncSourceHost, millisecond);
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

    convertPulse(result, hostPulse);
}

void Pulsator::gatherMidi()
{
    MidiAnalyzer* analyzer = syncMaster->getMidiAnalyzer();
    SyncAnalyzerResult* result = analyzer->getResult();

    convertPulse(result, midiPulse);
}    

void Pulsator::gatherTransport()
{
    Transport* t = syncMaster->getTransport();
    SyncAnalyzerResult* result = t->getResult();

    convertPulse(result, transportPulse);
}

//////////////////////////////////////////////////////////////////////
//
// Pulse Detection
//
//////////////////////////////////////////////////////////////////////

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
                Leader* l = getLeader(leader);
                if (l != nullptr) {
                    pulse = &(l->pulse);
                }
            }
        }
            break;
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
        if (pulse->source == SyncSourceNone || pulse->pending) {
            // pulse either not detected, or spilled into the next block
            pulse = nullptr;
        }
    }
    return pulse;
}

/**
 * Return any block pulse that may be relevant for a follower.
 */
Pulse* Pulsator::getAnyBlockPulse(Follower* f)
{
    Pulse* pulse = nullptr;
    if (f != nullptr) {

        // once the follower is locked, you can't change the source out
        // from under it
        // ?? why was this necessary
        SyncSource source = f->source;
        int leader = 0;
        if (f->source == SyncSourceTrack) {
            leader = f->leader;
            if (leader == 0)
              leader = syncMaster->getTrackSyncMaster();
        }

        // special case, if the leader is the follower, it means we couldn't find
        // a leader after starting which means it self-leads and won't have pulses
        if (leader != f->id) {

            pulse = getBlockPulse(source, leader);
        }
    }
    return pulse;
}

bool Pulsator::isRelevant(Follower* f, Pulse* p)
{
    bool relevant = false;
    if (p != nullptr && !p->pending && p->source != SyncSourceNone) {
        // there was a pulse from this source
        if (f->unit == SyncUnitBeat) {
            // anything is a beat
            relevant = true;
        }
        else if (f->unit == SyncUnitBar) {
            // loops are also bars
            relevant = (p->unit == SyncUnitBar || p->unit == SyncUnitLoop);
        }
        else {
            // only loops will do
            if (p->unit == SyncUnitLoop) {
                relevant = true;
            }
            else {
                // ugh, this only makes sense for sources that support loops
                // but you can configure the follower to watch for them even though
                // the source doesn't support it
                // when that happens, treat bars as loops
                // might be better to let BarTender sort that out?
                if (p->source != SyncSourceTrack &&
                    p->source != SyncSourceTransport) {

                    relevant = (p->unit == SyncUnitBar);
                }
            }
        }
    }
    return relevant;
}

//////////////////////////////////////////////////////////////////////
//
// Leaders and Followers
//
//////////////////////////////////////////////////////////////////////

Leader* Pulsator::getLeader(int leaderId)
{
    Leader* l = nullptr;

    // note that leader zero doesn't exist, it means default
    if (leaderId <= 0 || leaderId >= leaders.size()) {
        Trace(1, "Pulsator: Leader id out of range %d", leaderId);
    }
    else {
        l = leaders[leaderId];
    }
    return l;
}

Follower* Pulsator::getFollower(int followerId, bool warn)
{
    Follower* f = nullptr;

    if (followerId >= followers.size()) {
        // could grow this, but we're in the audio thread and not supposed to
        // should have been caught during configuration
        if (warn)
          Trace(1, "Pulsator: Follower id out of range %d", followerId);
    }
    else {
        f = followers[followerId];
    }
    return f;
}

/**
 * Register intent to follow a sync source.
 * 
 * Registering a follower is normally done as soon as a track is configured to
 * have a sync source.  For most sources, simply registering does nothing.
 * When following another track "leader", this causes that track to be processed
 * before the follower in each audio block so that the leader has a chance to
 * deposit leader pulses that the follower wants.  This ordering is performed
 * by TimeSlicer.
 */
void Pulsator::follow(int followerId, SyncSource source, SyncUnit unit)
{
    Follower* f = getFollower(followerId);
    if (f != nullptr) {
        
        f->source = source;
        f->leader = 0;
        f->unit = unit;

        char tracebuf[1024];
        snprintf(tracebuf, sizeof(tracebuf),
                 "Pulsator: Follower %d following %s pulse %s",
                 followerId, getSourceName(source), getUnitName(unit));
        Trace(2, tracebuf);
    }
}

/**
 * Register following an internal sync leader
 */
void Pulsator::follow(int followerId, int leaderId, SyncUnit unit)
{
    Follower* f = getFollower(followerId);
    if (f != nullptr) {
        // leaderId may be zero to mean "default" so don't call getLeader()
        if (leaderId >= leaders.size()) {
            Trace(1, "Pulsator::follow Leader %d out of range", leaderId);
            leaderId = 0;
        }
        
        char tracebuf[1024];
        snprintf(tracebuf, sizeof(tracebuf),
                 "Pulsator: Follower %d following Leader %d pulse %s",
                 followerId, leaderId, getUnitName(unit));
        Trace(2, tracebuf);

        f->source = SyncSourceTrack;
        f->leader = leaderId;
        f->unit = unit;
    }
}

/**
 * Stop following something after track reconfiguration.
 */
void Pulsator::unfollow(int followerId)
{
    Follower* f = getFollower(followerId);
    if (f != nullptr) {

        if (f->source != SyncSourceNone) {
            char tracebuf[1024];
            snprintf(tracebuf, sizeof(tracebuf),
                     "Pulsator: Follower %d unfollowing %s",
                     followerId, getSourceName(f->source));
            Trace(2, tracebuf);
        }
        
        f->source = SyncSourceNone;
        f->leader = 0;
        f->unit = SyncUnitBeat;
    }
}

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
    
    for (auto leader : leaders) {
        if (leader->pulse.source != SyncSourceNone)
          trace(leader->pulse);
    }
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
        case SyncUnitNone: name = "None"; break;
        case SyncUnitBeat: name = "Beat"; break;
        case SyncUnitBar: name = "Bar"; break;
        case SyncUnitLoop: name = "Loop"; break;
    }
    return name;
}    

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

