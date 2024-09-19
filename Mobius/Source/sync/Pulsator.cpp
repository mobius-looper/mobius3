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

#include "../Supervisor.h"

#include "MidiSyncEvent.h"
#include "MidiQueue.h"

#include "Pulsator.h"

Pulsator::Pulsator(Supervisor* s)
{
    supervisor = s;
    midiTransport = supervisor->getMidiRealizer();
}

Pulsator::~Pulsator()
{
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
    trace();
}

void Pulsator::reset()
{
    hostPulseDetected = false;
    midiInPulseDetected = false;
    midiOutPulseDetected = false;
    internalPulseDetected = false;
}

/**
 * Called by tracks or other internal objects to register the crossing
 * of a synchronization boundary after they were allowed to consume
 * this audio block.
 *
 * These "master" tracks must be processed first so they can register
 * sync events that the other tracks will follow.
 *
 * At the moment, there can only be one TrackSyncMaster so we only need
 * one pulse event, but in the future may want any track to follow any
 * other tracks and they'll all want to register events.
 */
void Pulsator::addInternal(int frameOffset, Type type)
{
    if (internalPulseDetected)
      Trace(1, "Pulsator: More than one master tried to register an internal pulse");
    
    internalPulse.reset(SourceInternal, millisecond);
    internalPulse.type = type;
    internalPulse.frame = frameOffset;
    internalPulseDetected = true;
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
    if (hostPulseDetected) trace(hostPulse);
    if (midiInPulseDetected) trace(midiInPulse);
    if (midiOutPulseDetected) trace(midiOutPulse);
    if (internalPulseDetected) trace(internalPulse);
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
            hostTempo = hostTime->tempo;
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
            hostPulseDetected = true;
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
            hostPulseDetected = true;
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

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

