/**
 * Fingering the pulse, of the world.
 *
 */

#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../model/Session.h"

#include "../mobius/MobiusInterface.h"

#include "SyncConstants.h"
#include "MidiSyncEvent.h"
#include "Pulse.h"
#include "Leader.h"
#include "Follower.h"

#include "MidiAnalyzer.h"
#include "MidiRealizer.h"
#include "HostAnalyzer.h"
#include "Transport.h"
//#include "MidiQueue.h"
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

/**
 * Called at the beginning of each audio block to detect sync
 * pulses from the various sources.
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

    // leader pulses are added as the tracks advance

    // advance drift detectors
    advance(interruptFrames);

    // this is good for debugging sources, but adds clutter
    // when things are working
    // watch out for the weird track sync "beat" pulses that only display
    // if they are marked pending because trace happens before the
    // audio tracks advance and have a chance to deposit leader pulses
    //trace();
}

/**
 * Reset pulse tracking state at the beginning of each block.
 */
void Pulsator::reset()
{
    midiPulse.reset();
    midiOutPulse.reset();
    hostPulse.reset();
    transportPulse.reset();
    
    // this is where pending pulses that were just over the last block
    // are activated for this block
    for (auto leader : leaders)
      leader->reset();
}

//////////////////////////////////////////////////////////////////////
//
// Source state
//
//////////////////////////////////////////////////////////////////////

void Pulsator::trace()
{
    if (hostPulse.source != SyncSourceNone) trace(hostPulse);
    if (midiPulse.source != SyncSourceNone) trace(midiPulse);
    if (midiOutPulse.source != SyncSourceNone) trace(midiOutPulse);
    if (transportPulse.source != SyncSourceNone) trace(transportPulse);
    for (auto leader : leaders) {
        if (leader->pulse.source != SyncSourceNone)
          trace(leader->pulse);
    }
}

void Pulsator::trace(Pulse& p)
{
    juce::String msg("Pulsator: ");
    switch (p.source) {
        case SyncSourceMidi: msg += "Midi "; break;
        case SyncSourceMaster: msg += "Master "; break;
        case SyncSourceHost: msg += "Host "; break;
        case SyncSourceTransport: msg += "Transport "; break;
        case SyncSourceTrack: msg += "Internal "; break;
        case SyncSourceNone: msg += "None "; break;
    }

    switch (p.unit) {
        case SyncUnitNone: msg += "None "; break;
        case SyncUnitBeat: msg += "Beat "; break;
        case SyncUnitBar: msg += "Bar "; break;
        case SyncUnitLoop: msg += "Loop "; break;
    }

    if (p.start) msg += "Start ";
    if (p.stop) msg += "Stop ";
    if (p.mcontinue) msg += "Continue ";

    Trace(2, "%s", msg.toUTF8());
}

//////////////////////////////////////////////////////////////////////
//
// Host
//
//////////////////////////////////////////////////////////////////////

/**
 * Host events
 */
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

            if (result->onLoop)
              hostPulse.unit = SyncUnitLoop;
            else if (result->onBar)
              hostPulse.unit = SyncUnitBar;
            else
              hostPulse.unit = SyncUnitBeat;

            // convey these, if they happen at the same time
            // blow off continue, too hard
            hostPulse.start = result->started;
            hostPulse.stop = result->stopped;
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// MIDI
//
//////////////////////////////////////////////////////////////////////

/**
 * Assimilate queued MIDI realtime events from the MIDI transport.
 *
 * Old code generated events for each MIDI clock and there could be more than
 * one per block.  Now, we only care about beat pulses.
 */
void Pulsator::gatherMidi()
{
    midiPulse.reset();
    midiOutPulse.reset();
    
    MidiAnalyzer* analyzer = syncMaster->getMidiAnalyzer();
    analyzer->startEventIterator();
    MidiSyncEvent* mse = analyzer->nextEvent();
    while (mse != nullptr) {
        detectMidiBeat(mse, SyncSourceMidi, &(midiPulse));
        mse = analyzer->nextEvent();
    }
    analyzer->flushEvents();

    // the output events aren't used directly any more, but Transport
    // needs them for drift detection
    MidiRealizer* realizer = syncMaster->getMidiRealizer();
    realizer->startEventIterator();
    mse = realizer->nextEvent();
    while (mse != nullptr) {
        detectMidiBeat(mse, SyncSourceMaster, &(midiOutPulse));
        mse = realizer->nextEvent();
    }
    realizer->flushEvents();
}

/**
 * Convert a MidiSyncEvent from the MidiRealizer into a beat pulse.
 * todo: this is place where we should try to offset the event into the buffer
 * to make it align more accurately with real time.
 *
 * Note that MidiSyncEvent captured it's own millisecond counter so we
 * don't use the one we got at the start of this block.
 */
bool Pulsator::detectMidiBeat(MidiSyncEvent* mse, SyncSource src, Pulse* pulse)
{
    // not expecting more than one now that we don't return clocks
    bool alreadyDetected = (pulse->source != SyncSourceNone);
    bool detected = false;
    
    if (mse->isStop) {
        pulse->reset(src, mse->millisecond);
        pulse->unit = SyncUnitBeat;
        pulse->stop = true;
        detected = true;
    }
    else if (mse->isStart) {
        // MidiRealizer deferred this until the first clock
        // after the start message, so it is a true beat
        pulse->reset(src, mse->millisecond);
        pulse->unit = SyncUnitBeat;
        pulse->start = true;
        //pulse->beat = mse->beat;
        detected = true;
    }
    else if (mse->isContinue) {
        // only pay attention to this if this is also a beat pulse
        // not sure if this will work, but I don't want to fuck with continue right now
        if (mse->isBeat) {
            pulse->reset(src, mse->millisecond);
            pulse->unit = SyncUnitBeat;
            //pulse->beat = mse->beat;
            pulse->mcontinue = true;
            // what the hell is this actually, it won't be a pulse count so
            // need to divide MIDI clocks per beat?
            pulse->continuePulse = mse->songClock;
            detected = true;
        }
    }
    else {
        // ordinary clock
        // ignore if this isn't also a beat
        if (mse->isBeat) {
            pulse->reset(src, mse->millisecond);
            pulse->unit = SyncUnitBeat;
            //pulse->beat = mse->beat;
            detected = true;
        }
    }

    // upgrade Beat pulses to Bar pulses if we're on a bar
    if (detected && !pulse->stop) {
        int bpb = syncMaster->getBeatsPerBar(src);
        if (bpb > 0 && ((mse->beat % bpb) == 0))
          pulse->unit = SyncUnitBar;
    }

    if (detected && alreadyDetected) {
        // not expecting this to happen if we're only pulsing beats
        Trace(1, "Pulsator: Pulse overflow");
    }
    
	return detected;
}

//////////////////////////////////////////////////////////////////////
//
// Transport
//
//////////////////////////////////////////////////////////////////////

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

//////////////////////////////////////////////////////////////////////
//
// Leaders
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

//////////////////////////////////////////////////////////////////////
//
// Followers
//
//////////////////////////////////////////////////////////////////////

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
 * Register intent to following a sync source
 *
 * Registering a follower is normally done as soon as a track is configured to
 * have a sync source.  For most sources, simply registering does nothing.
 * When following another track "leader", this causes that track to be processed
 * before the follower in each audio block so that the leader has a chance to
 * deposit leader pulses that the follower wants.
 *
 * When the follower is ready to start a synchronied recording, it calls start().
 * When the follower has finished a synchronized recording, it calls lock().
 *
 * Once a follow has been started or locked the source should not be changed as it
 * would confuse the meaning of pulse monitoring and drift detection.  The track
 * will continue to follow the original source until it is restarted or unlocked.
 * hen the new source request is activated.
 */
void Pulsator::follow(int followerId, SyncSource source, SyncUnit unit)
{
    Follower* f = getFollower(followerId);
    if (f != nullptr) {
        
        if (f->started) {
            traceFollowChange(f, source, 0, unit);
        }
        else {
            f->source = source;
            f->leader = 0;
            f->unit = unit;

            char tracebuf[1024];
            snprintf(tracebuf, sizeof(tracebuf),
                     "Pulsator: Follower %d following %s pulse %s",
                     followerId, getSourceName(source), getPulseName(unit));
            Trace(2, tracebuf);
        }
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
        
        if (f->started) {
            traceFollowChange(f, SyncSourceTrack, leaderId, unit);
            // allow pulse unit to be changed in case they want to start
            // on Bar but end on Beat, might be useful
            f->unit = unit;
        }
        else {
            char tracebuf[1024];
            snprintf(tracebuf, sizeof(tracebuf),
                     "Pulsator: Follower %d following Leader %d pulse %s",
                     followerId, leaderId, getPulseName(unit));
            Trace(2, tracebuf);

            f->source = SyncSourceTrack;
            f->leader = leaderId;
            f->unit = unit;
        }
    }
}

/**
 * When attempting to change a follow source after a recording has started
 * we defer the change until after the follower is unlocked.
 * This is unusual but allowed, say something about it.
 */
void Pulsator::traceFollowChange(Follower* f, SyncSource source, int leaderId,
                                 SyncUnit unit)
{
    char tracebuf[1024];
    strcpy(tracebuf, "");

    if (f->started) {

        if (leaderId == 0)
          leaderId = syncMaster->getTrackSyncMaster();

        if (f->lockedSource != source) {
            snprintf(tracebuf, sizeof(tracebuf),
                     "Pulsator: Follower %d deferring locked source change from %s to %s",
                     f->id, getSourceName(f->lockedSource), getSourceName(source));
        }
        else if (f->lockedSource == SyncSourceTrack && f->lockedLeader != leaderId) {
            snprintf(tracebuf, sizeof(tracebuf),
                     "Pulsator: Follower %d deferring locked leader change from %d to %d",
                     f->id, f->lockedLeader, leaderId);
        }
        else if (f->unit != unit) {
            snprintf(tracebuf, sizeof(tracebuf),
                     "Pulsator: Follower %d changing pulse unit from %s to %s",
                     f->id, getPulseName(f->unit), getPulseName(unit));
        }

        if (tracebuf[0] != 0)
          Trace(2, tracebuf);
    }
}

const char* Pulsator::getSourceName(SyncSource source)
{
    const char* name = "???";
    switch (source) {
        case SyncSourceNone: name = "None"; break;
        case SyncSourceMidi: name = "Midi"; break;
        case SyncSourceMaster: name = "Master"; break;
        case SyncSourceHost: name = "Host"; break;
        case SyncSourceTrack: name = "Leader"; break;
        case SyncSourceTransport: name = "Transport"; break;
    }
    return name;
}

const char* Pulsator::getPulseName(SyncUnit unit)
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

/**
 * A follower wants to beging recording
 *
 * At this point the source is locked and can't be changed
 * We beging keeping track of beat pulses
 */
void Pulsator::start(int followerId)
{
    Follower* f = getFollower(followerId);
    if (f != nullptr) {

        if (f->source == SyncSourceNone) {
            // not following anything, common for tracks to call start()
            // unconditionally so ignore it
        }
        else {
        
            if (f->started) {
                // this is most likely a coding error in the follower
                // it is supposed to call unlock() if it resets and stops recording
                Trace(1, "Pulsator: Restarting follower %d", followerId);
            }

            // determine the track sync leader if we want one
            bool sourceAvailable = true;
            int leader = 0;
            if (f->source == SyncSourceTrack) {
                leader = f->leader;
                if (leader == 0)
                  leader = syncMaster->getTrackSyncMaster();
                if (leader == 0)
                  sourceAvailable = false;
            }

            if (sourceAvailable) {
                // the source is now locked
                f->lockedSource = f->source;
                f->lockedLeader = leader;
            }
            else {
                // we wanted track sync but there aren't any masters
                // this means we get to be free and will probably become
                // master, follow thyself
                f->lockedSource = f->source;
                f->lockedLeader = followerId;
                Trace(2, "Pulsator: Follower %d wanted a leader but there was none, lead thyself",
                      followerId);
            }

            // since we were called on a pulse, the lock starts with 1 pulse
            f->locked = false;
            f->pulse = 0;
            f->frames = 0;
            f->frame = 0;
            f->started = true;

            // if we actually were tracking pulses then start with 1
            if (sourceAvailable) {
                f->pulses = 1;
                Trace(2, "Pulsator: Follower %d starting", followerId);
            }
        }
    }
}

/**
 * A follower has finished recording
 * 
 * The frames argument has the length of the follower, it will normally
 * be the same as the frames accumulated during the follow.
 */
void Pulsator::lock(int followerId, int frames)
{
    Follower* f = getFollower(followerId);
    if (f != nullptr) {

        if (f->source == SyncSourceNone) {
            // not following anything, common for this to be called unconditionally
            // when it might be following so ignore it
        }
        else {
        
            if (!f->started) {
                Trace(1, "Pulsator: Follower %d not started and can't be locked", followerId);
            }
            else if (f->locked) {
                Trace(1, "Pulsator: Follower %d is already locked", followerId);
            }
            else {
                if (f->source == SyncSourceTrack && f->leader == followerId) {
                    // we were self leading, don't bother with trace
                }
                else {
                    Trace(2, "Pulsator: Follower %d locking after %d pulses %d frames",
                          followerId, f->pulses, frames);
                }
                
                // reset drift state
                f->frames = frames;
                f->frame = 0;
                f->pulse = 0;
                f->locked = true;
            }
        }
    }
}

/**
 * A Follower is supposed to call this after a reset or re-record.
 * Cancel drift monitoring and return to an unstarted state
 */
void Pulsator::unlock(int followerId)
{
    Follower* f = getFollower(followerId, false);
    if (f != nullptr) {

        if (!f->started) {
            // this is okay, may just be reconfiguring and making sure
            // the follower is unlocked
        }
        else {
            Trace(2, "Pulsator: Unlocking follower %d", followerId);
        }

        f->started = false;
        f->lockedSource = SyncSourceNone;
        f->lockedLeader = 0;
        f->locked = false;
        f->pulses = 0;
        f->pulse = 0;
        f->frames = 0;
        f->frame = 0;
        f->drift = 0;
        f->shouldCheckDrift = false;
    }
}

/**
 * Stop following something after track reconfiguration
 *
 * This is mostly the same as unlocking, except that if this was following
 * an internal track it can also result in simplification of the leader
 * order dependencies.  There is no hard requirement to do this but
 * it is best.  Be best.
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
        f->started = false;
        f->lockedSource = SyncSourceNone;
        f->lockedLeader = 0;
        f->locked = false;
        f->pulses = 0;
        f->frames = 0;
        f->pulse = 0;
        f->frame = 0;
        f->drift = 0;
        f->shouldCheckDrift = false;
    }
}

//////////////////////////////////////////////////////////////////////
//
// Pulse Detection
//
//////////////////////////////////////////////////////////////////////

/**
 * Return the block pulse for the internal clock generator.
 * This is not a Pulse source any more, it is used only by the Transport.
 */
Pulse* Pulsator::getOutBlockPulse()
{
    Pulse* pulse = &(midiOutPulse);
    // only if active
    if (pulse->source == SyncSourceNone || pulse->pending)
      pulse = nullptr;
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
        if (f->lockedSource != SyncSourceNone) {
            source = f->lockedSource;
            leader = f->lockedLeader;
        }
        else if (f->source == SyncSourceTrack) {
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

/**
 * Return the block pulse that IS relevant for a follower.
 */
Pulse* Pulsator::getRelevantBlockPulse(Follower* f)
{
    Pulse* pulse = getAnyBlockPulse(f);

    // filter  based on the desired pulse unit
    if (pulse != nullptr && !isRelevant(pulse, f->unit))
      pulse = nullptr;

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
// Drift
//
//////////////////////////////////////////////////////////////////////

/**
 * Advance locked follower state for one block
 *
 * For pulse counting, we always track the smallest unit of beats
 * even though the follower may be syncing on bars or loops.
 */
void Pulsator::advance(int blockFrames)
{
    for (int i = 1 ; i < followers.size() ; i++) {
        Follower* f = followers[i];

        // hack, if the track is locked for Leader sync, and there was
        // no leader at the time, it is self-leading
        // then we don't accumululate pulses or drift
        // this feels crotchety, need to clarify this special state
        bool selfLeading = (f->locked &&
                            f->lockedSource == SyncSourceTrack &&
                            f->lockedLeader == i);
        
        if (f->locked && !selfLeading) {

            // was there a beat in this block?
            Pulse* p = getAnyBlockPulse(f);
            if (p != nullptr) {

                f->pulse += 1;

                // this is how long the follower will advance
                // when it gets around to processing the block
                f->frame += blockFrames;

                // wrap the pulse
                bool checkpoint = false;
                if (f->pulse >= f->pulses) {
                    f->pulse = 0;
                    checkpoint = true;
                    // the pulse wrapped, calculate drift
                    // if the frame is beyond the end it is rushing
                    // if beyind the end it is lagging
                    f->drift = f->frame - f->frames;
                }

                // wrap the frame
                if (f->frame >= f->frames) {
                    f->frame = f->frame - f->frames;
                }

                if (checkpoint) {
                    if (f->drift < driftThreshold) {
                        // these can be noisy in the logs so may want to
                        // disable it if the drift is small
                        Trace(2, "Pulsator: Follower %d drift %d", i, f->drift);
                    }
                    else {
                        Trace(1, "Pulsator: Follower %d drift threshold exceeded %d", f->drift);
                        f->shouldCheckDrift = true;

                        // this is the point where old Mobius would retrigger the loop to
                        // bring it back into alignment
                        // how should we signal that?
                        // could register a callback or just make the track
                        // ask for the drift and do the ajustments?
                    }
                }
            }
        }
    }
}

/**
 * Expected to be called by the follower on every block to
 * see if we're ready to drift correct.
 * Old Mobius had a lot of complex options about where the correction
 * could happen, now it just happens on the loop boundary.
 * It can call getDrift and corrrectDrift at any time though.
 */
bool Pulsator::shouldCheckDrift(int followerId)
{
    bool should = false;
    Follower* f = getFollower(followerId);
    if (f != nullptr)
      should = f->shouldCheckDrift;
    return should;
}

/**
 * Expected to be called by the followeer at the start of it's block processing
 * If it decides to realign, it needs to call back to correctDrift()
 * to tell us what it did.
 */
int Pulsator::getDrift(int followerId)
{
    int drift = 0;
    
    Follower* f = getFollower(followerId);
    if (f != nullptr) {

        if (f->locked) {
            drift = f->drift;
        }
    }
    return drift;
}

/**
 * Expected to be called by the follower after it decides there was enough
 * drift and it did a correction.
 *
 * todo: here we have a lot of math to do
 * We could just trust that the track did something appropriate and
 * reset our state. Or we could make the track pass in the frame adjustment
 * and assimilate that.  
 */
void Pulsator::correctDrift(int followerId, int frames)
{
    Follower* f = getFollower(followerId);
    if (f != nullptr) {

        if (!f->locked) {
            Trace(1, "Pulsator: Follower %d not locked, ignoring drift correction",
                  followerId);
        }
        else if (frames == 0) {
            // it didn't say, assume it knows what it's doing
            f->pulse = 0;
            f->frame = 0;
            f->drift = 0;
        }
        else {
            f->pulse = 0;
            f->frame += frames;
            f->drift = f->frame - f->frames;
            Trace(2, "Pulsator: Follower %d correct drift to %d, does this look right?",
                  f->drift);
        }

        f->shouldCheckDrift = false;
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

