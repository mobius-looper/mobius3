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
#include "SyncSourceResult.h"
#include "SyncMaster.h"

#include "Pulsator.h"

Pulsator::Pulsator(SyncMaster* sm)
{
    syncMaster = sm;
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
 *
 * !! Ugh, with BarTender the notion that followers are abstract things that
 * aren't Tracks goes out the window.  This is becomming less general than originally
 * designed, also now that follower arrays are allocated based on track counts.
 *
 * There are no followers that aren't tracks, and BarTender parameters from the
 * session have to correlate to Followers with the same track number.
 * I suppose the assumption could be that follower ids are always track numbers
 * and if we have non-track followers those need to use a special id range and would
 * pull their configutration from elsewhere int he Session.
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

    // todo: Pull beatsPerBar and barsPerLoop overrides from the Session
    // and give them to the BarTender of the corresponding Followers

    sessionBeatsPerBar = s->getInt(SessionBeatsPerBar);
    if (sessionBeatsPerBar == 0)
      sessionBeatsPerBar = 4;

    sessionOverrideHostBar = s->getBool(SessionHostOverrideTimeSignature);

    // Followers of SyncSourceHost will either get BPB from the HostAnalyzer or
    // the Session depending
    
    HostAnalyzer* analyzer = syncMaster->getHostAnalyzer();
    int hostBpb = analyzer->getBeatsPerBar();
    if (hostBpb == 0 || sessionOverrideHostBar)
      hostBpb = sessionBeatsPerBar;

    for (auto follower : followers) {
        if (follower->source == SyncSourceHost)
          follower->barTender.setBeatsPerBar(hostBpb);
        else
          follower->barTender.setBeatsPerBar(sessionBeatsPerBar);
    }
}

/**
 * beatsPerBar can be changed away from the session in two ways.
 *
 * If overrideHostTimeSignature is off and the user manually changes
 * the host time signature, HostAnalyzer must set the timeSignatureChanged
 * flag in the SyncSourceResult and SM will call this to update the bars
 * in the followers.
 *
 * The Transport may have it's beatsPerBar changed through user interaction
 * and conveys this the same way.
 */
void Pulsator::setBeatsPerBar(SyncSource src, int bpb)
{
    // todo: track specific overrides
    
    for (auto follower : followers) {
        if (follower->source == src) {
            if (src != SyncSourceHost || sessionOverrideHostBar)
              follower->barTender.setBeatsPerBar(bpb);
        }
    }
}

/**
 * Bars per loop is only set by the Transport, but I suppose others
 * might want this someday.
 */
void Pulsator::setBarsPerLoop(SyncSource src, int bpl)
{
    for (auto follower : followers) {
        if (follower->source == src) {
            follower->barTender.setBarsPerLoop(bpl);
        }
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
void Pulsator::advance(MobiusAudioStream* stream)
{
    // capture some statistics
	lastMillisecond = millisecond;
	millisecond = juce::Time::getMillisecondCounter();
	interruptFrames = stream->getInterruptFrames();

    reset();
    
    gatherHost();
    gatherMidi();
    gatherTransport();

    // prepare pulses for each follower
    gatherFollowerPulses();

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

        if (frameOffset >= interruptFrames) {
            // leave it pending and adjust for the next block
            l->pulse.pending = true;
            int wrapped = frameOffset - interruptFrames;
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
 * After follower Pulses have been gathered, TimeSlicer will call this
 * to get the pulse for each follower track that is relevant in the block.
 * It should only be calling this once but because BarTender has state,
 * the pulses are copied and annotated once by gatherFollowerPulses,
 * so this could in theory be called more than once if necessary.
 *
 * Twist: gatherFollowerPulses handled pulses from the sync sources,
 * but for SyncSourceTrack, those pulses won't be known until tracks
 * start advancing.  So we need to do the refresh here when they
 * are neeeded, which assumes that TimeSlider is ordering the track advance.
 */
Pulse* Pulsator::getRelevantBlockPulse(int trackNumber)
{
    Pulse* pulse = nullptr;
    
    Follower* f = getFollower(trackNumber);
    if (f != nullptr) {
        pulse = &(f->pulse);
        if (pulse->source == SyncSourceTrack) {
            // deferred gathering and relevance checking
            // for leader pulses
            // these don't need BarTender mutations (yet anyway)
            // so if we have one that matches the follow unit we can
            // just return the shared pulse object rather than the Follower copy
            Pulse* available = getAnyBlockPulse(f);
            if (isRelevant(available, f->unit))
              pulse = available;
            else
              pulse = nullptr;
        }
    }
    return pulse;
}

//////////////////////////////////////////////////////////////////////
//
// Pulse Gathering
//
//////////////////////////////////////////////////////////////////////

void Pulsator::gatherHost()
{
    HostAnalyzer* analyzer = syncMaster->getHostAnalyzer();
    SyncSourceResult* result = analyzer->getResult();

    if (result != nullptr) {

        // SyncSourceResult doesn't return beat/bar numbers
        // it should return the beat number, but bars are ambiguous

        if (result->stopped) {
            // we've been generaing a Pulse for stop, unclear if that's necessary
            // if there is also a beat flag, may replace the unit
            hostPulse.reset(SyncSourceHost, millisecond);
            hostPulse.unit = SyncUnitBeat;
            hostPulse.stop = true;
            // doesn't really matter what this is
            hostPulse.blockFrame = 0;
        }

        if (result->beatDetected) {

            hostPulse.reset(SyncSourceHost, millisecond);
            hostPulse.blockFrame = result->blockOffset;

            // changed to Bar or Loop later by BarTender
            hostPulse.unit = SyncUnitBeat;
            
            // convey these, if they happen at the same time
            // blow off continue, too hard
            hostPulse.start = result->started;
            hostPulse.stop = result->stopped;
        }
    }
}

/**
 * This is now just like HostAnalyzer, merge them!
 * Also make Transport behave the same way.
 */
void Pulsator::gatherMidi()
{
    MidiAnalyzer* analyzer = syncMaster->getMidiAnalyzer();
    SyncSourceResult* result = analyzer->getResult();

    if (result != nullptr) {

        if (result->stopped) {
            // we've been generaing a Pulse for stop, unclear if that's necessary
            // if there is also a beat flag, may replace the unit
            hostPulse.reset(SyncSourceHost, millisecond);
            hostPulse.unit = SyncUnitBeat;
            hostPulse.stop = true;
            // doesn't really matter what this is
            hostPulse.blockFrame = 0;
        }

        if (result->beatDetected) {

            hostPulse.reset(SyncSourceHost, millisecond);
            hostPulse.blockFrame = result->blockOffset;

            // changed to Bar or Loop later by BarTender
            hostPulse.unit = SyncUnitBeat;

            // convey these, if they happen at the same time
            // blow off continue, too hard
            hostPulse.start = result->started;
            hostPulse.stop = result->stopped;
        }
    }
}

/**
 * Transport maintains it's own Pulse so we don't really need to copy
 * it over here, but make it look like everything else.
 */
void Pulsator::gatherTransport()
{
    Transport* t = syncMaster->getTransport();
    
    //transport.tempo = t->getTempo();
    transportPulse = *(t->getPulse());

    // Transport has historically not set this, it could
    if (transportPulse.source != SyncSourceNone)
      transportPulse.millisecond = millisecond;
}

/**
 * After pulse gathering, for every follower that wants a pulse,
 * copy the source pulse to the follower and allow BarTender
 * to annotate it for bar/loop boundaries.  Then do relevance checking
 * and if this pulse isn't being followed reset the pulse.
 *
 * This is what will be returned to TimeSlicer when it eventually
 * calls getRelevantBlockPulse.
 *
 * Subtlety: while the code this calls looks like it handles Leader
 * pulses, those won't actually be defined at the time this is called.
 * Leader pulses are added incrementally as tracks advance.
 * getRelevantBlockPulse deals with that.
 */
void Pulsator::gatherFollowerPulses()
{
    for (auto follower : followers) {
        Pulse* available = getAnyBlockPulse(follower);
        if (available != nullptr) {

            follower->pulse = *available;
            follower->barTender.annotate(&(follower->pulse));

            // verify relevance
            if (!isRelevant(&(follower->pulse), follower->unit))
              follower->pulse.source = SyncSourceNone;
        }
    }
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
 *
 * !! Do we really need this locked source shit?
 * Get rid of it unless there is a good reason for it.
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

bool Pulsator::isRelevant(Pulse* p, SyncUnit followUnit)
{
    bool relevant = false;
    if (p != nullptr && !p->pending && p->source != SyncSourceNone) {
        // there was a pulse from this source
        if (followUnit == SyncUnitBeat) {
            // anything is a beat
            relevant = true;
        }
        else if (followUnit == SyncUnitBar) {
            // loops are also bars
            relevant = (p->unit == SyncUnitBar || p->unit == SyncUnitLoop);
        }
        else {
            // only loops will do
            // this makes sense only when following internal leaders, if this
            // pulse didn't come from a Leader, treat it like Bar
            if (p->source == SyncSourceTrack)
              relevant = (p->unit == SyncUnitLoop);
            else
              relevant = (p->unit == SyncUnitBar || p->unit == SyncUnitLoop);
        }
    }
    return relevant;
}

//////////////////////////////////////////////////////////////////////
//
// Granular State
//
//////////////////////////////////////////////////////////////////////

// ugh, get this working and revisit how this needs to hang together

BarTender* Pulsator::getBarTender(SyncSource src)
{
    BarTender* tender = nullptr;
    
    switch (src) {
        case SyncSourceNone: break;
        case SyncSourceHost: tender = &hostBarTender; break;
        case SyncSourceMidi: tender = &midiBarTender; break;
        case SyncSourceTransport:
        case SyncSourceMaster:
            // todo: Transport needs one of these
            break;
        case SyncSourceTrack:
            // unclear what this is supposed to mean
            // I guess sthe subcycle/cycle
            break;
    }
    return tender;
}

int Pulsator::getBeat(SyncSource src)
{
    int beat = 0;
    BarTender* tender = getBarTender(src);
    if (tender != nullptr)
      beat = tender->getBeat();
    else
      beat = syncMaster->getTransport()->getBeat();
    
    return beat;
}

int Pulsator::getBar(SyncSource src)
{
    int bar = 0;
    BarTender* tender = getBarTender(src);
    if (tender != nullptr)
      bar = tender->getBar();
    else
      bar = syncMaster->getTransport()->getBar();
    return bar;
}

int Pulsator::getBeatsPerBar(SyncSource src)
{
    int bpb = 0;
    BarTender* tender = getBarTender(src);
    if (tender != nullptr)
      bpb = tender->getBeatsPerBar();
    else
      bpb = syncMaster->getTransport()->getBeatsPerBar();
    return bpb;
}

// todo: forward to the Follower BarTender

int Pulsator::getBeatsPerBar(int id)
{
    int bpb = 4;
    Follower* f = getFollower(id);
    if (f != nullptr)
      bpb = getBeatsPerBar(f->source);
    return bpb;
}

int Pulsator::getBeat(int id)
{
    int beat = 0;
    Follower* f = getFollower(id);
    if (f != nullptr)
      beat = getBeat(f->source);
    return beat;
}

int Pulsator::getBar(int id)
{
    int bar = 4;
    Follower* f = getFollower(id);
    if (f != nullptr)
      bar = getBar(f->source);
    return bar;
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

