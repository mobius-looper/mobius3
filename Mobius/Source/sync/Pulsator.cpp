/**
 * Fingering the pulse, of the world.
 *
 * Use of the interfaces MobiusAudioStream and AudioTime are from when this code
 * lived under Mobius.  Now that it has been moved up a level, this could be rewritten
 * to directly use what those interfaces hide: JuceAudioStream and HostSyncState.
 * Or better, define local interfaces to hide those two that are not dependent on
 * either Mobius or Supervisor.
 *
 */

#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../model/MobiusConfig.h"
#include "../model/Session.h"

#include "../Provider.h"
#include "../mobius/MobiusInterface.h"

#include "MidiRealizer.h"
#include "MidiSyncEvent.h"
#include "MidiQueue.h"

#include "Pulse.h"
#include "Leader.h"
#include "Follower.h"
#include "Pulsator.h"

Pulsator::Pulsator(Provider* p)
{
    provider = p;
    midiTransport = provider->getMidiRealizer();
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
void Pulsator::configure()
{
    int numFollowers = 0;
    MobiusConfig* config = provider->getMobiusConfig();
    numFollowers = config->getCoreTracks();
    
    Session* session = provider->getSession();
    numFollowers += session->midiTracks;

    // ensure the array is big enough
    // !! this is potentially dangerous if tracks are actively registering
    // follows and the array needs to be resized.  Would be best to allow this
    // only when in a state of GlobalReset
    // +1 is because follower ids are 1 based and are array indexes
    // follower id 0 is reserved
    for (int i = followers.size() ; i < numFollowers + 1 ; i++) {
        Follower* f = new Follower();
        f->id = i;
        followers.add(f);
    }

    // leaders are the same as followers
    int numLeaders = numFollowers;
    for (int i = leaders.size() ; i < numLeaders + 1 ; i++) {
        Leader* l = new Leader();
        l->id = i;
        leaders.add(l);
    }

    orderedLeaders.ensureStorageAllocated(numLeaders+1);
}

void Pulsator::reset()
{
    hostPulse.source = Pulse::SourceNone;
    midiInPulse.source = Pulse::SourceNone;
    midiOutPulse.source = Pulse::SourceNone;

    // this is where pending pulses that were just over the last block
    // are activated for this block
    for (auto leader : leaders)
      leader->reset();
}

void Pulsator::interruptStart(MobiusAudioStream* stream)
{
    // capture some statistics
	lastMillisecond = millisecond;
	millisecond = midiTransport->getMilliseconds();
	interruptFrames = stream->getInterruptFrames();

    reset();
    
    gatherHost(stream);
    gatherMidi();

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

//////////////////////////////////////////////////////////////////////
//
// Source state
//
//////////////////////////////////////////////////////////////////////

/**
 * For the track monitoring UI, return information about the sync source this
 * track is following.
 *
 * For MIDI do we want to return the fluxuating tempo or smooth tempo
 * with only one decimal place?
 */
float Pulsator::getTempo(Pulse::Source src)
{
    float tempo = 0.0f;
    switch (src) {
        case Pulse::SourceHost: tempo = hostTempo; break;
            //case SourceMidiIn: tempo = midiTransport->getInputSmoothTempo(); break;
        case Pulse::SourceMidiIn: tempo = midiTransport->getInputTempo(); break;
        case Pulse::SourceMidiOut: tempo = midiTransport->getTempo(); break;
        default: break;
    }
    return tempo;
}

/**
 * Internal pulses do not have a beat number, and the current UI won't ask for one.
 * I suppose when it registered the events for subcycle/cycle it could also
 * register the subcycle/cycle numbers.
 */
int Pulsator::getBeat(Pulse::Source src)
{
    int beat = 0;
    switch (src) {
        case Pulse::SourceHost: beat = hostBeat; break;
        case Pulse::SourceMidiIn: beat = midiTransport->getInputRawBeat(); break;
        case Pulse::SourceMidiOut: beat = midiTransport->getRawBeat(); break;
        default: break;
    }
    return beat;
}

/**
 * Bar numbers depend on a reliable BeatsPerBar, punt
 */
int Pulsator::getBar(Pulse::Source src)
{
    int bar = 0;
    switch (src) {
        case Pulse::SourceHost: bar = hostBar; break;
        case Pulse::SourceMidiIn: bar = getBar(getBeat(src), getBeatsPerBar(src)); break;
        case Pulse::SourceMidiOut: bar = getBar(getBeat(src), getBeatsPerBar(src)); break;
        default: break;
    }
    return bar;
}

/**
 * Calculate the bar number for a beat with a known time signature.
 */
int Pulsator::getBar(int beat, int bpb)
{
    int bar = 1;
    if (bpb > 0)
      bar = (beat / bpb) + 1;
    return bar;
}

/**
 * Time signature is unreliable when when it is getBar()
 * won't return anything meaningful.
 * Might want an isBarKnown method?
 *
 * The BPB for internal tracks was annoyingly complex, getting it from
 * the Setup or the current value of the subcycles parameter.
 * Assuming for now that internal tracks will deal with that and won't
 * need to be calling up here.
 *
 * Likewise MIDI doesn't have any notion of a reliable time siguature
 * so it would have to come from configuration parameters.
 *
 * Only host can tell us what this is, and even then some hosts may not.
 */
int Pulsator::getBeatsPerBar(Pulse::Source src)
{
    int bar = 0;
    switch (src) {
        case Pulse::SourceHost: bar = hostBeatsPerBar; break;
        case Pulse::SourceMidiIn: bar = 4; break;
        case Pulse::SourceMidiOut: bar = 4; break;
        default: break;
    }
    return bar;
}

void Pulsator::trace()
{
    if (hostPulse.source != Pulse::SourceNone) trace(hostPulse);
    if (midiInPulse.source != Pulse::SourceNone) trace(midiInPulse);
    if (midiOutPulse.source != Pulse::SourceNone) trace(midiOutPulse);
    for (auto leader : leaders) {
        if (leader->pulse.source != Pulse::SourceNone)
          trace(leader->pulse);
    }
}

void Pulsator::trace(Pulse& p)
{
    juce::String msg("Pulsator: ");
    switch (p.source) {
        case Pulse::SourceMidiIn: msg += "MidiIn "; break;
        case Pulse::SourceMidiOut: msg += "MidiOut "; break;
        case Pulse::SourceHost: msg += "Host "; break;
        case Pulse::SourceLeader: msg += "Internal "; break;
        case Pulse::SourceNone: msg += "None "; break;
    }

    switch (p.type) {
        case Pulse::PulseBeat: msg += "Beat "; break;
        case Pulse::PulseBar: msg += "Bar "; break;
        case Pulse::PulseLoop: msg += "Loop "; break;
    }

    if (p.start) msg += "Start ";
    if (p.stop) msg += "Stop ";
    if (p.mcontinue) msg += "Continue ";

    if (p.type != Pulse::PulseLoop) {
        msg += juce::String(p.beat);
        if (p.bar > 0)
          msg += " bar " + juce::String(p.bar);
    }
    
    Trace(2, "%s", msg.toUTF8());
}

//////////////////////////////////////////////////////////////////////
//
// Host
//
//////////////////////////////////////////////////////////////////////

/**
 * Host events
 * 
 * Unlike MIDI events which are quantized by the MidiQueue, these
 * will have been created in the *same* interrupt and will have frame
 * values that are offsets into the current interrupt.
 *
 * It's actually a bit more complicated than this, the "ppqpos" changed the
 * integer value during this block, but when we detect the difference this
 * is a few frames AFTER the pulse actually happened.  So technically we should
 * have caught it on the previous block and anticipated the change.  The delta is
 * so small not to matter though and it will balance out because both the start
 * and end pulses of a loop will be delayed by similar amounts.
 */
void Pulsator::gatherHost(MobiusAudioStream* stream)
{
    // this will come back nullptr if we're not a plugin
	AudioTime* hostTime = stream->getAudioTime();
    if (hostTime != nullptr) {
		hostBeat = hostTime->beat;
        hostBar = hostTime->bar;

        // trace these since I want to know which hosts can provide them
        if (hostTempo != hostTime->tempo) {
            // historically this has been a double, not necessary
            hostTempo = (float)(hostTime->tempo);
            Trace(2, "Pulsator: Host tempo %d (x100)", (int)(hostTempo * 100));
        }
        if (hostBeatsPerBar != hostTime->beatsPerBar) {
            hostBeatsPerBar = hostTime->beatsPerBar;
            Trace(2, "Pulsator: Host beatsPerBar %d", hostBeatsPerBar);
        }
        
        bool starting = false;
        bool stopping = false;
        
        // monitor the host transport
        if (hostPlaying && !hostTime->playing) {
            // the host transport stopped
            stopping = true;
            // generate a pulse for this, may be replace if there is also a beat here
            hostPulse.reset(Pulse::SourceHost, millisecond);
            hostPulse.blockFrame = 0;
            // doesn't really matter what this is
            hostPulse.type = Pulse::PulseBeat;
            hostPulse.stop = true;
            hostPlaying = false;
        }
        else if (!hostPlaying && hostTime->playing) {
            // the host transport is starting
            starting = true;
            // what old code did is save a "transportPending" flag and on the
            // next beat boundary it would generate Start events
            // skipping the generation of these since FL and other pattern
            // based hosts like to jump around and may send spurious transport
            // start/stop that don't mean anything
            hostPlaying = true;
        }

        // what if they stopped the transport at the same time as it reached
        // a beat boundary?  if we're waiting on one, we'll wait forever,
        // but since we can't keep more than one pulse per block, just overwrite it
        if (hostTime->beatBoundary || hostTime->barBoundary) {

            hostPulse.reset(Pulse::SourceHost, millisecond);
            hostPulse.blockFrame = hostTime->boundaryOffset;
            if (hostTime->barBoundary)
              hostPulse.type = Pulse::PulseBar;
            else
              hostPulse.type = Pulse::PulseBeat;

            hostPulse.beat = hostTime->beat;
            hostPulse.bar = hostTime->bar;

            // convey these, though start may be unreliable
            // blow off continue, too hard
            hostPulse.start = starting;
            hostPulse.stop = stopping;
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// MIDI In & Out
//
//////////////////////////////////////////////////////////////////////

/**
 * Assimilate queued MIDI realtime events from the MIDI transport.
 *
 * Old code generated events for each MIDI clock and there could be more than
 * one per block.  Now, we only care about beat pulses and stop if we find one.
 */
void Pulsator::gatherMidi()
{
    midiTransport->iterateInputStart();
    MidiSyncEvent* mse = midiTransport->iterateInputNext();
    while (mse != nullptr) {
        if (detectMidiBeat(mse, Pulse::SourceMidiIn, &midiInPulse))
          break;
        else
          mse = midiTransport->iterateInputNext();
    }
    
    // again for internal output events
    midiTransport->iterateOutputStart();
    mse = midiTransport->iterateOutputNext();
    while (mse != nullptr) {
        if (detectMidiBeat(mse, Pulse::SourceMidiOut, &midiOutPulse))
          break;
        else
          mse = midiTransport->iterateOutputNext();
    }
}

/**
 * Convert a MidiSyncEvent from the MidiRealizer into a beat pulse.
 * todo: this is place where we should try to offset the event into the buffer
 * to make it align more accurately when real time.
 *
 * Note that here the MidiSyncEvent capture it's own millisecond counter so we
 * don't use the one we got at the start of this block.
 */
bool Pulsator::detectMidiBeat(MidiSyncEvent* mse, Pulse::Source src, Pulse* pulse)
{
    bool detected = false;
    
    if (mse->isStop) {
        pulse->reset(src, mse->millisecond);
        pulse->type = Pulse::PulseBeat;
        pulse->stop = true;
        detected = true;
    }
    else if (mse->isStart) {
        // MidiRealizer deferred this until the first clock
        // after the start message, so it is a true beat
        pulse->reset(src, mse->millisecond);
        pulse->type = Pulse::PulseBeat;
        pulse->start = true;
        pulse->beat = mse->beat;
        detected = true;
    }
    else if (mse->isContinue) {
        // only pay attention to this if this is also a beat pulse
        // not sure if this will work, but I don't want to fuck with continue right now
        if (mse->isBeat) {
            pulse->reset(src, mse->millisecond);
            pulse->type = Pulse::PulseBeat;
            pulse->beat = mse->beat;
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
            pulse->type = Pulse::PulseBeat;
            pulse->beat = mse->beat;
            detected = true;
        }
    }

    // upgrade Beat pulses to Bar pulses if we're on a bar
    if (detected && !pulse->stop) {
        int bpb = getBeatsPerBar(src);
        if (bpb > 0 && ((pulse->beat % bpb) == 0))
          pulse->type = Pulse::PulseBar;
    }
    
	return detected;
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
 * This is an array of leader ids in dependency order
 * The component responsible for advancing tracks during each
 * audio block is required to advance the leaders first so they
 * may register pulses that followers want to follow.
 */
juce::Array<int>* Pulsator::getOrderedLeaders()
{
    return &orderedLeaders;
}

/**
 * Analyze the Leader/Follower relationships and determine the
 * order in which tracks need to be advanced.
 *
 * This is hard in the general case since leaders can in theory follow
 * other leaders and there can be cycles in the dependency chain.
 */
void Pulsator::orderLeaders()
{
    // empty but keep storage
    orderedLeaders.clearQuick();

    for (int i = 1 ; i < followers.size() ; i++) {
        Follower* f = followers[i];
        if (f->source == Pulse::SourceLeader) {
            
            int leader = f->leader;
            if (leader == 0)
              leader = trackSyncMaster;

            if (leader > 0) {
                if (!orderedLeaders.contains(leader))
                  orderedLeaders.add(leader);
            }
        }
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
 */
void Pulsator::addLeaderPulse(int leaderId, Pulse::Type type, int frameOffset)
{
    Leader* l = getLeader(leaderId);
    if (l != nullptr) {
        l->pulse.reset(Pulse::SourceLeader, millisecond);
        l->pulse.type = type;
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
void Pulsator::follow(int followerId, Pulse::Source source, Pulse::Type type)
{
    Follower* f = getFollower(followerId);
    if (f != nullptr) {
        
        if (f->started) {
            traceFollowChange(f, source, 0, type);
        }
        else {
            bool wasInternal = (f->source == Pulse::SourceLeader);
        
            f->source = source;
            f->leader = 0;
            f->type = type;

            // if we stopped following a leader, may simplify leader order
            if (wasInternal)
              orderLeaders();

            char tracebuf[1024];
            snprintf(tracebuf, sizeof(tracebuf),
                     "Pulsator: Follower %d following %s pulse %s",
                     followerId, getSourceName(source), getPulseName(type));
            Trace(2, tracebuf);
        }
    }
}

/**
 * Register following an internal sync leader
 */
void Pulsator::follow(int followerId, int leaderId, Pulse::Type type)
{
    Follower* f = getFollower(followerId);
    if (f != nullptr) {
        // leaderId may be zero to mean "default" so don't call getLeader()
        if (leaderId >= leaders.size()) {
            Trace(1, "Pulsator::follow Leader %d out of range", leaderId);
            leaderId = 0;
        }
        
        if (f->started) {
            traceFollowChange(f, Pulse::SourceLeader, leaderId, type);
            // allow pulse type to be changed in case they want to start
            // on Bar but end on Beat, might be useful
            f->type = type;
        }
        else {
            char tracebuf[1024];
            snprintf(tracebuf, sizeof(tracebuf),
                     "Pulsator: Follower %d following Leader %d pulse %s",
                     followerId, leaderId, getPulseName(type));
            Trace(2, tracebuf);

            f->source = Pulse::SourceLeader;
            f->leader = leaderId;
            f->type = type;
    
            orderLeaders();
        }
    }
}

/**
 * When attempting to change a follow source after a recording has started
 * we defer the change until after the follower is unlocked.
 * This is unusual but allowed, say something about it.
 */
void Pulsator::traceFollowChange(Follower* f, Pulse::Source source, int leaderId,
                                 Pulse::Type type)
{
    char tracebuf[1024];
    strcpy(tracebuf, "");

    if (f->started) {

        if (leaderId == 0)
          leaderId = trackSyncMaster;

        if (f->lockedSource != source) {
            snprintf(tracebuf, sizeof(tracebuf),
                     "Pulsator: Follower %d deferring locked source change from %s to %s",
                     f->id, getSourceName(f->lockedSource), getSourceName(source));
        }
        else if (f->lockedSource == Pulse::SourceLeader && f->lockedLeader != leaderId) {
            snprintf(tracebuf, sizeof(tracebuf),
                     "Pulsator: Follower %d deferring locked leader change from %d to %d",
                     f->id, f->lockedLeader, leaderId);
        }
        else if (f->type != type) {
            snprintf(tracebuf, sizeof(tracebuf),
                     "Pulsator: Follower %d changing pulse type from %s to %s",
                     f->id, getPulseName(f->type), getPulseName(type));
        }

        if (tracebuf[0] != 0)
          Trace(2, tracebuf);
    }
}

const char* Pulsator::getSourceName(Pulse::Source source)
{
    const char* name = "???";
    switch (source) {
        case Pulse::SourceNone: name = "None"; break;
        case Pulse::SourceMidiIn: name = "MidiIn"; break;
        case Pulse::SourceMidiOut: name = "MidiOut"; break;
        case Pulse::SourceHost: name = "Host"; break;
        case Pulse::SourceLeader: name = "Leader"; break;
    }
    return name;
}

const char* Pulsator::getPulseName(Pulse::Type type)
{
    const char* name = "???";
    switch (type) {
        case Pulse::PulseBeat: name = "Beat"; break;
        case Pulse::PulseBar: name = "Bar"; break;
        case Pulse::PulseLoop: name = "Loop"; break;
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

        if (f->source == Pulse::SourceNone) {
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
            if (f->source == Pulse::SourceLeader) {
                leader = f->leader;
                if (leader == 0)
                  leader = trackSyncMaster;
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

        if (f->source == Pulse::SourceNone) {
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
                if (f->source == Pulse::SourceLeader && f->leader == followerId) {
                    // we were self leading, don't bother with trace
                }
                else {
                    Trace(2, "Pulsator: Follower %d locking after %d pulses %d frames",
                          followerId, f->pulses, frames);

                    if (f->source == Pulse::SourceLeader)
                      Trace(2, "Pulsator: Track sync master frames were %d", trackSyncMasterFrames);
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
 * Cancel drift monitoring and return to an unstarted state
 *
 * Follower is supposed to call this after a reset or re-record
 * NOTE: MidiTracker resets tracks that aren't actually active so the
 * follower number may be higher than what was registered.  Pass false
 * to getFollower to suppress the warning.
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
        f->lockedSource = Pulse::SourceNone;
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

        if (f->source != Pulse::SourceNone) {
            char tracebuf[1024];
            snprintf(tracebuf, sizeof(tracebuf),
                     "Pulsator: Follower %d unfollowing %s",
                     followerId, getSourceName(f->source));
            Trace(2, tracebuf);
        }
        
        bool wasInternal = (f->source == Pulse::SourceLeader);
        f->source = Pulse::SourceNone;
        f->leader = 0;
        f->type = Pulse::PulseBeat;
        f->started = false;
        f->lockedSource = Pulse::SourceNone;
        f->lockedLeader = 0;
        f->locked = false;
        f->pulses = 0;
        f->frames = 0;
        f->pulse = 0;
        f->frame = 0;
        f->drift = 0;
        f->shouldCheckDrift = false;
        if (wasInternal) {
            // once we stop following a track, the leader dependency
            // order may simplify
            orderLeaders();
        }
    }
}

/**
 * Called by a follower at the beginning of it's block advance
 * to see if there were any sync pulses in this block.
 *
 * This interface uses the pulse type that was registered with follow()
 * The other one allows the follower to pass in a pulse type which may change
 * between start and lock.  Don't need both, probably just the second one.
 */
int Pulsator::getPulseFrame(int followerId)
{
    int frame = -1;
    
    Follower* f = getFollower(followerId);
    if (f != nullptr) {
        frame = getPulseFrame(followerId, f->type);
    }
    return frame;
}

int Pulsator::getPulseFrame(int followerId, Pulse::Type type)
{
    int frame = -1;
    
    Follower* f = getFollower(followerId);
    if (f != nullptr) {

        // once the follower is locked, you can't change the source out
        // from under it
        Pulse::Source source = f->source;
        int leader = 0;
        if (f->lockedSource != Pulse::SourceNone) {
            source = f->lockedSource;
            leader = f->lockedLeader;
        }
        else if (f->source == Pulse::SourceLeader) {
            leader = f->leader;
            if (leader == 0)
              leader = trackSyncMaster;
        }

        // special case, if the leader is the follower, it means we couldn't find
        // a leader after starting which means it self-leads and won't have pulses
        if (leader != followerId) {
        
            switch (source) {
                case Pulse::SourceNone: /* nothing to say */ break;
                case Pulse::SourceMidiIn: frame = getPulseFrame(&midiInPulse, type); break;
                case Pulse::SourceMidiOut: frame = getPulseFrame(&midiOutPulse, type); break;
                case Pulse::SourceHost: frame = getPulseFrame(&hostPulse, type); break;
                case Pulse::SourceLeader: {

                    // leader can be zero here if there was no track sync leader
                    // in which case there won't be a pulse
                    // don't call getLeader with zero or it traces an error
                    if (leader > 0) {
                        Leader* l = getLeader(leader);
                        if (l != nullptr) {
                            frame = getPulseFrame(&(l->pulse), type);
                        }
                    }
                }
                    break;
            }
        }
    }
    return frame;
}

/**
 * Test to see if a pulse was detected and if it was
 * a given type.
 *
 * The Pulse from the source will be Beat, Bar or Loop.
 * When the pulse type of the follower is smaller than the
 * source pulse it matches even though the types aren't exact.
 *
 * For example is something is following Beat pulses, it will
 * also match Bar or Loop pulses from the source since those
 * are always beats.
 *
 * For track sync, Bar also matches Loop.
 *
 */
int Pulsator::getPulseFrame(Pulse* p, Pulse::Type followType)
{
    int frame = -1;
    if (!p->pending && p->source != Pulse::SourceNone) {
        // there was a pulse from this source
        bool accept = false;
        if (followType == Pulse::PulseBeat) {
            // anything is a beat
            accept = true;
        }
        else if (followType == Pulse::PulseBar) {
            // loops are also bars
            accept = (p->type == Pulse::PulseBar || p->type == Pulse::PulseLoop);
        }
        else {
            // only loops will do
            // this makes sense only when following internal leaders, if this
            // pulse didn't come from a Leader, treat it like Bar
            if (p->source == Pulse::SourceLeader)
              accept = (p->type == Pulse::PulseLoop);
            else
              accept = (p->type == Pulse::PulseBar || p->type == Pulse::PulseLoop);
        }

        if (accept)
          frame = p->blockFrame;
    }
    return frame;
}

//////////////////////////////////////////////////////////////////////
//
// Out Sync Master
//
//////////////////////////////////////////////////////////////////////

void Pulsator::setOutSyncMaster(int followerId, int frames)
{
    (void)followerId;
    (void)frames;
    Trace(1, "Pulsator::setOutSyncMaster not implemented");
    outSyncMaster = followerId;
}

int Pulsator::getOutSyncMaster()
{
    return outSyncMaster;
}

/**
 * Set the default leader track when using track sync and the follower
 * didn't specify a specific leader.
 *
 * What the old system called the "track sync master".
 * Note: This can change randomly.  If a track starts out following
 * one track, then is reset and records again, it needs to sync to
 * the new default leader.  For that to happen, leave the Follower.leader
 * field at zero.
 */
void Pulsator::setTrackSyncMaster(int leaderId, int leaderFrames)
{
    trackSyncMaster = leaderId;
    trackSyncMasterFrames = leaderFrames;
}

/**
 * Tracks would call this to see if there is a track sync master
 * if they want to follow one, and if there isn't it can decide whether
 * to wait (unlikely) or just proceed and maybe become the master.
 */
int Pulsator::getTrackSyncMaster()
{
    return trackSyncMaster;
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
                            f->lockedSource == Pulse::SourceLeader &&
                            f->lockedLeader == i);
        
        if (f->locked && !selfLeading) {

            // was there a beat in this block?
            int offset = getPulseFrame(i, Pulse::PulseBeat);
            if (offset >= 0)
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

