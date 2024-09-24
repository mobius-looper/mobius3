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
 * impact the follower count.  Ensure that the Follow array is large enough.
 * todo: Could avoid config model dependencies by making the caller pass down
 * the follower count instead.
 */
void Pulsator::configure()
{
    int numFollowers = 0;
    MobiusConfig* config = provider->getMobiusConfig();
    numFollowers = config->getCoreTracks();
    
    Session* session = provider->getSession();
    numFollowers += session->tracks.size();

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

    leaders.ensureStorageAllocated(numFollowers+1);
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
    // internals are added as the tracks advance

    // advance follow state for external sources
    advance(interruptFrames);
    
    trace();
}

juce::Array<int>* Pulsator::getInternalLeaders()
{
    return &leaders;
}

void Pulsator::orderInternalLeaders()
{
    // empty but keep storage
    leaders.clearQuick();

    // ugh, hard in the general case since leaders can depend on other leaders
    // and they would need to be done in dependency order, and there could be cycles
    for (int i = 1 ; i < followers.size() ; i++) {
        Follower* f = followers[i];
        if (f->enabled && f->source == SourceInternal && f->leader > 0) {
            if (!leaders.contains(f->leader))
              leaders.add(f->leader);
        }
    }
}

void Pulsator::reset()
{
    hostPulse.source = SourceNone;
    midiInPulse.source = SourceNone;
    midiOutPulse.source = SourceNone;
    for (int i = 1 ; i < followers.size() ; i++) {
        Follower* f = followers[i];
        f->internal.source = SourceNone;
    }
}

/**
 * For the track monitoring UI, return information about the sync source this
 * track is following.
 *
 * For MIDI do we want to return the fluxuating tempo or smooth tempo
 * with only one decimal place?
 */
float Pulsator::getTempo(Source src)
{
    float tempo = 0.0f;
    switch (src) {
        case SourceHost: tempo = hostTempo; break;
            //case SourceMidiIn: tempo = midiTransport->getInputSmoothTempo(); break;
        case SourceMidiIn: tempo = midiTransport->getInputTempo(); break;
        case SourceMidiOut: tempo = midiTransport->getTempo(); break;
        default: break;
    }
    return tempo;
}

/**
 * Internal pulses do not have a beat number, and the current UI won't ask for one.
 * I suppose when it registered the events for subcycle/cycle it could also
 * register the subcycle/cycle numbers.
 */
int Pulsator::getBeat(Source src)
{
    int beat = 0;
    switch (src) {
        case SourceHost: beat = hostBeat; break;
        case SourceMidiIn: beat = midiTransport->getInputRawBeat(); break;
        case SourceMidiOut: beat = midiTransport->getRawBeat(); break;
        default: break;
    }
    return beat;
}

/**
 * Bar numbers depend on a reliable BeatsPerBar, punt
 */
int Pulsator::getBar(Source src)
{
    int bar = 0;
    switch (src) {
        case SourceHost: bar = hostBar; break;
        case SourceMidiIn: bar = getBar(getBeat(src), getBeatsPerBar(src)); break;
        case SourceMidiOut: bar = getBar(getBeat(src), getBeatsPerBar(src)); break;
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
int Pulsator::getBeatsPerBar(Source src)
{
    int bar = 0;
    switch (src) {
        case SourceHost: bar = hostBeatsPerBar; break;
        case SourceMidiIn: bar = 4; break;
        case SourceMidiOut: bar = 4; break;
        default: break;
    }
    return bar;
}

void Pulsator::trace()
{
    if (hostPulse.source != SourceNone) trace(hostPulse);
    if (midiInPulse.source != SourceNone) trace(midiInPulse);
    if (midiOutPulse.source != SourceNone) trace(midiOutPulse);
    for (int i = 1 ; i < followers.size() ; i++) {
        Follower* f = followers[i];
        if (f->internal.source != SourceNone)
          trace(f->internal);
    }
}

void Pulsator::trace(Pulse& p)
{
    juce::String msg("Pulsator: ");
    switch (p.source) {
        case SourceMidiIn: msg += "MidiIn "; break;
        case SourceMidiOut: msg += "MidiOut "; break;
        case SourceHost: msg += "Host "; break;
        case SourceInternal: msg += "Internal "; break;
    }

    switch (p.type) {
        case PulseBeat: msg += "Beat "; break;
        case PulseBar: msg += "Bar "; break;
        case PulseLoop: msg += "Loop "; break;
    }

    if (p.start) msg += "Start ";
    if (p.stop) msg += "Stop ";
    if (p.mcontinue) msg += "Continue ";

    msg += juce::String(p.beat);
    if (p.bar > 0)
      msg += " bar " + juce::String(p.bar);
    
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
	AudioTime* hostTime = stream->getAudioTime();
    if (hostTime == NULL) {
        // can this happen, reset everyting or leave it where it was?
        /*
        mHostTempo = 0.0f;
        mHostBeat = 0;
        mHostBeatsPerBar = 0;
        mHostTransport = false;
        mHostTransportPending = false;
        */
        Trace(1, "Pulsator: Unexpected null AudioTime");
    }
    else {
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
            hostPulse.reset(SourceHost, millisecond);
            hostPulse.frame = 0;
            // doesn't really matter what this is
            hostPulse.type = PulseBeat;
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

            hostPulse.reset(SourceHost, millisecond);
            hostPulse.frame = hostTime->boundaryOffset;
            if (hostTime->barBoundary)
              hostPulse.type = PulseBar;
            else
              hostPulse.type = PulseBeat;

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
        if (detectMidiBeat(mse, SourceMidiIn, &midiInPulse))
          break;
        else
          mse = midiTransport->iterateInputNext();
    }
    
    // again for internal output events
    midiTransport->iterateOutputStart();
    mse = midiTransport->iterateOutputNext();
    while (mse != nullptr) {
        if (detectMidiBeat(mse, SourceMidiOut, &midiOutPulse))
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
bool Pulsator::detectMidiBeat(MidiSyncEvent* mse, Source src, Pulse* pulse)
{
    bool detected = false;
    
    if (mse->isStop) {
        pulse->reset(src, mse->millisecond);
        pulse->type = PulseBeat;
        pulse->stop = true;
        detected = true;
    }
    else if (mse->isStart) {
        // MidiRealizer deferred this until the first clock
        // after the start message, so it is a true beat
        pulse->reset(src, mse->millisecond);
        pulse->type = PulseBeat;
        pulse->start = true;
        pulse->beat = mse->beat;
        detected = true;
    }
    else if (mse->isContinue) {
        // only pay attention to this if this is also a beat pulse
        // not sure if this will work, but I don't want to fuck with continue right now
        if (mse->isBeat) {
            pulse->reset(src, mse->millisecond);
            pulse->type = PulseBeat;
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
            pulse->type = PulseBeat;
            pulse->beat = mse->beat;
            detected = true;
        }
    }

    // upgrade Beat pulses to Bar pulses if we're on a bar
    if (detected && !pulse->stop) {
        int bpb = getBeatsPerBar(src);
        if (bpb > 0 && ((pulse->beat % bpb) == 0))
          pulse->type = PulseBar;
    }
    
	return detected;
}

//////////////////////////////////////////////////////////////////////
//
// Followers
//
//////////////////////////////////////////////////////////////////////

/**
 * Start following a sync source
 */
void Pulsator::follow(int followerId, Source source, Type type)
{
    if (followerId >= followers.size()) {
        // could grow this, but we're in the audio thread and not supposed to
        // should have been caught during configuration
        Trace(1, "Pulsator: Follower id out of range %d", followerId);
    }
    else {
        Follower* f = followers[followerId];
        if (f->enabled) {
            Trace(2, "Pulsator: Follower %d is already following something", followerId);
        }
        else {
            f->enabled = true;
            f->locked = false;
            f->source = source;
            f->type = type;
            f->pulses = 0;
            f->frames = 0;
        }
    }
}

/**
 * Start following another track
 */
void Pulsator::follow(int followerId, int leaderId, Type type)
{
    if (followerId >= followers.size()) {
        // could grow this, but we're in the audio thread and not supposed to
        // should have been caught during configuration
        Trace(1, "Pulsator: Follower id out of range %d", followerId);
    }
    else if (leaderId >= followers.size()) {
        // could grow this, but we're in the audio thread and not supposed to
        // should have been caught during configuration
        Trace(1, "Pulsator: Leader id out of range %d", leaderId);
    }
    else {
        Follower* f = followers[followerId];
        if (f->enabled) {
            Trace(2, "Pulsator: Follower id is already following something");
        }
        else {
            f->enabled = true;
            f->locked = false;
            f->source = SourceInternal;
            f->type = type;
            f->leader = leaderId;
            f->pulses = 0;
            f->frames = 0;

            orderInternalLeaders();
        }
    }
}

/**
 * Stop the follow and begin drift monitoring.
 * The frames argument has the length of the follower, it will normally
 * be the same as the frames accumulated during the follow.
 */
void Pulsator::lock(int followerId, int frames)
{
    if (followerId >= followers.size()) {
        Trace(1, "Pulsator: Follower id out of range %d", followerId);
    }
    else {
        Follower* f = followers[followerId];
        if (!f->enabled) {
            Trace(1, "Pulsator: Follower not enabled %d", followerId);
        }
        else if (f->locked) {
            Trace(1, "Pulsator: Follower is already locked %d", followerId);
        }
        else {
            Trace(2, "Pulsator: Follower %d locking after %d pulses %d frames",
                  followerId, f->pulses, f->frames);
            Trace(2, "Pulsator: Locked frame length %d", frames);
            // todo: need to be clear in the differences allowed between the
            // frames accumulated during the follow, and the length the follower
            // ended up with
            f->frames = frames;
            f->frame = 0;
            f->pulse = 0;
        }
    }
}

void Pulsator::unfollow(int followerId)
{
    if (followerId >= followers.size()) {
        Trace(1, "Pulsator: Follower id out of range %d", followerId);
    }
    else {
        Follower* f = followers[followerId];
        if (!f->enabled) {
            Trace(1, "Pulsator: Follower not enabled %d", followerId);
        }
        else {
            bool wasInternal = (f->source == SourceInternal);
            f->enabled = false;
            f->locked = false;
            f->source = SourceNone;
            f->leader = 0;
            f->frames = 0;
            f->pulses = 0;
            f->frame = 0;
            f->pulse = 0;
            if (wasInternal)
              orderInternalLeaders();
        }
    }
}

/**
 * Called by followers (tracks or other internal objects) to register the crossing
 * of a synchronization boundary after they were allowed to consume
 * this audio block.
 *
 * This is similar to the old Synchronizer::trackSyncEvent
 */
void Pulsator::addInternalPulse(int followerId, Type type, int frameOffset)
{
    if (followerId >= followers.size()) {
        Trace(1, "Pulsator: Follower id out of range %d", followerId);
    }
    else {
        Follower* f = followers[followerId];
        f->internal.reset(SourceInternal, millisecond);
        f->internal.type = type;
        f->internal.frame = frameOffset;
    }
}

int Pulsator::getPulseFrame(int followerId, Type type)
{
    int frame = -1;
    
    if (followerId >= followers.size()) {
        Trace(1, "Pulsator: Follower id out of range %d", followerId);
    }
    else {
        Follower* f = followers[followerId];
        switch (f->source) {
            case SourceNone: /* nothing to say */ break;
            case SourceMidiIn: frame = getPulseFrame(&midiInPulse, type); break;
            case SourceMidiOut: frame = getPulseFrame(&midiOutPulse, type); break;
            case SourceHost: frame = getPulseFrame(&hostPulse, type); break;
            case SourceInternal: {
                if (f->leader > 0) {
                    if (f->leader >= followers.size()) {
                        Trace(1, "Pulsator: Leader id out of range %d", f->leader);
                    }
                    else {
                        Follower* l = followers[f->leader];
                        frame = getPulseFrame(&(l->internal), type);
                    }
                }
            }
                break;
        }
    }
    return frame;
}

int Pulsator::getPulseFrame(Pulse* p, Type type)
{
    int frame = -1;
    if (p->source != SourceNone && p->type == type) {
        frame = p->frame;
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
}

//////////////////////////////////////////////////////////////////////
//
// Drift
//
//////////////////////////////////////////////////////////////////////

/**
 * Advance follower state for the non-internal followers.
 * Once locked, internal followers are assumed to stay in sync
 * since they were frame aligned.  Might still want to drift check
 * track sync followers just in case there were round off errors
 * calculating loop length.
 */
void Pulsator::advance(int blockFrames)
{
    for (int i = 1 ; i < followers.size() ; i++) {
        Follower* f = followers[i];
        if (f->enabled && f->source != SourceInternal) {
            int offset = getPulseFrame(i, f->type);
            if (offset >= 0)
              f->pulse += 1;
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
                if (f->drift > driftThreshold) {
                    Trace(1, "Pulsator: Drift threshold exceeded %d", f->drift);
                }
                else {
                    Trace(2, "Pulsator: Drift checkpoint %d drift %d", i, f->drift);
                }
            }
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

