/**
 * Model for an abstract "pulse" generated by a synchronization source.
 * Synchronization events from various sources are converted into a Pulse
 * and managed by the Pulsator.
 *
 * Pulses contain various information about where the pulse exists in time.
 * The blockFrame offset into the audio block is the most important.
 *
 * The other fields have additional information that followers may
 * wish to respond to but are not required.
 * todo: reconsider whether to even bother with these, intelligent
 * following of external song position or transport location is enormously
 * complex.  For all practical purposes the followers shouldn't really
 * care about whether the transport starts and stops, but Pulsator does
 * so it can decide whether it is worth monitoring drift.
 *
 */

#pragma once

class Pulse
{
  public:

    /**
     * Things within the system that may generate sync pulses.
     * A Follower track may choose to respond to one of these.
     */
    typedef enum {
        // None is used to indiciate that the pulse has not been detected
        SourceNone,
        SourceMidiIn,
        SourceMidiOut,
        SourceHost,
        SourceLeader
    } Source;

    /**
     * Each source may generate several types of pulses.  While logically
     * every pulse represents a "beat" some beat pulses have more
     * significance than others.
     */
    typedef enum {
        
        // the smallest pulse a source can provide
        // for Midi this is determined by the PPQ of the clocks
        // for Host this is determined by ppqPosition from the host
        // for internal Mobius tracks, this corresponds to the Subcycle
        PulseBeat,

        // the pulse represents the location of a time signature bar if
        // the source can supply a time signature
        // for internal Mobius tracks, this corresponds to the Cycle
        PulseBar,

        // the pulse represents the end of a larger collection of beats or bars
        // that has a known length in pulses
        // for internal Mobius tracks, this corresponds to the end of a loop
        // there is no correspondence in MIDI or host pulses
        PulseLoop
        
    } Type;
    
    Pulse() {}
    ~Pulse() {}

    // where the pulse came from
    Source source = SourceNone;

    // the pulse granularity
    Type type = PulseBeat;
    
    // system time this pulse was detected, mostly for debugging
    int millisecond = 0;

    // the sample/frame offset into the current audio block where this
    // pulse logically happened
    int blockFrame = 0;

    // the beat and bar numbers of the external transport if known
    int beat = 0;
    int bar = 0;

    // this pulse also represents the host transport or MIDI clocks
    // moving to their start point
    bool start = false;

    // this pulse also represents the external transport stopping
    // not really a pulse but conveyed as one
    bool stop = false;

    // this pulse also represents the movement of the external transport
    // to a random location
    bool mcontinue = false;

    // when continue is true, the logical pulse in the external sequence
    // we're continuing from, aka the "song position pointer"
    int continuePulse = 0;

    void reset(Source s, int msec) {
        source = s;
        millisecond = msec;
        blockFrame = 0;
        beat = 0;
        bar = 0;
        start = false;
        stop = false;
        mcontinue = false;
        continuePulse = 0;
    }
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
