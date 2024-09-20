/**
 * The first purpose of the Pulsator is to analyze synchronization data from
 * various sync sources and distill it into a "pulse" that happens within
 * the current audio block.  These pulses may then be used to trigger synchronization
 * events within the audio or midi tracks of the Mobius Engine.
 *
 * The concepts are a refactoring of a few things originally found in the
 * core class Synchronizer which is too horribly complex for words.
 *
 * Pulsator does it's analysis at the begining of each audio block, consuming events
 * for MIDI clocks (MidiRealizer) and the plugin host (HostSyncState).  It may
 * later have pulses added to it by the audio/midi tracks as they cross synchronization
 * boundaries during their block advance.
 *
 * Things that want to respond to sync pulses typically ask the Pulsator for the
 * locations of pulses of a given type within the block, then segment their block
 * processing around those.
 *
 * The other service Pulsator provides is tracking of "drift" between the pulses
 * and the audio stream.  When dealing with internal pulses or pulses from the plugin
 * host there is normally no drift unless the user is deliberately altering the tempo
 * of the pulses, such as changing the tempo in the host application's transport.
 *
 * When dealing with MIDI clocks however, drift is common.  When defining a block
 * of audio samples whose length is determined by pulses (e.g. a loop), any jitter
 * in the received starting and ending pulse can cause size misclaculations in
 * the resulting loop.  The loop may be slightly shorter or longer than what the
 * device sending the clocks thinks it should be, and over time this can accumulate
 * into a noticeable misalignment between what is heard playing from loop, and
 * what is heard playing from the sequencer or drum machine generating the clocks.
 *
 * Attempting to calculate loop length not with pulses on the edge, but by averaging
 * the pulses over time to guess at the desired tempo is also prone to round off errors
 * and will rarely be exact.
 *
 * What Pulsator does is allow the internal tracks to register a "pulse tracker".  A tracker
 * is given the size of a unit of audio data in samples (e.g. the loop length) and the number
 * of pulses that unit contained while it was being recorded.  As future audio blocks are received
 * it compares the rate at which samples are being received against the rate at which pulses
 * are being received and if they start to diverge it calculates the amount of drift.
 * When this drift exceeds a threshold, it can inform the track that it needs to adjust
 * it's playback position to remain in alignment with the external pulse generator.
 *
 * While conceptually pulses may be very small, such as individual MIDI clocks, in practice
 * loop tracks only care about coarse grained "beat" pulses.  Since you can't in practice
 * have multiple beat pulses per audio block, it greatly simplifies the internal models
 * if it only needs to detect one pulse from each source per block.
 */

#pragma once

#include <JuceHeader.h>

class Pulsator
{
  public:

    /**
     * Several types of pulses are monitored.  An internal track may choose
     * to respond one of these.
     */
    typedef enum {
        // used within a Pulse to indiciate that the pulse has not been detected
        SourceNone,
        SourceMidiIn,
        SourceMidiOut,
        SourceHost,
        SourceInternal
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
    
    /**
     * When a pulse is detected that a track wants to follow, detailed
     * information about the pulse is conveyed in this structure.
     *
     * The frameOffset into the audio block is the most important.
     *
     * The other fields have additional information that internal may
     * wish to respond to but are not required.
     * todo: reconsider whether to even bother with these, intelligent
     * following of external song position or transport location is enormously
     * complex.  For all practical purposes the followers shouldn't really
     * care about whether the transport starts and stops, but Pulsator does
     * so it can decide whether it is worth monitoring drift.
     */
    class Pulse {
      public:
        Pulse() {}
        ~Pulse() {}

        Source source = SourceNone;
        Type type = PulseBeat;

        // the system millisecond at which this pulse was received
        int millisecond = 0;

        // the sample/frame offset into the current audio block where this
        // pulse logically happened
        int frame = 0;

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
        // we're continuing from
        int continuePulse = 0;

        void reset(Source s, int msec) {
            source = s;
            millisecond = msec;
            frame = 0;
            beat = 0;
            bar = 0;
            start = false;
            stop = false;
            mcontinue = false;
            continuePulse = 0;
        }
    };

    /**
     * Each track or other entity interested in following sync pulses is allocated
     * a Follower.
     */
    class Follower {
      public:
        Follower() {}
        ~Follower() {}

        // the unique follower id, normally a track number
        int id = 0;
        
        // true if this follow is enabled, when false the follower is freewheeling
        bool enabled = false;

        // true when this follow is locked and begins drift monitoring
        bool locked = false;

        // the source this follower is following
        Source source = SourceNone;

        // the type of pulse to follow
        Type type = PulseBeat;

        // for SourceInternal an optional specific leader
        // if left zero, a designated common leader is used
        int leader = 0;

        // the number of pulses in the follower's "loop"
        int pulses = 0;

        // the number of frames in the followers loop
        int frames = 0;

        // after locking, the current pulse count being monitored
        int pulse = 0;

        // after locking, the current frame position being monitored
        int frame = 0;

        // last calculated drift
        int drift = 0;

        // used when the follower can also provide internal pulses
        // for others to follow, in that case it is a "leader"
        Pulse internal;
    };
    
    Pulsator(class Provider* p);
    ~Pulsator();
    void configure();
    
    void interruptStart(class MobiusAudioStream* stream);
    juce::Array<int>* getInternalLeaders();
    int getPulseFrame(int follower, Type type);

    void follow(int follower, Source source, Type type);
    void follow(int follower, int leader, Type type);
    void lock(int follower, int frames);
    void unfollow(int follower);
    void setOutSyncMaster(int follower, int frames);
    void addInternalPulse(int follower, Type type, int frameOffset);

    float getTempo(Source src);
    int getBeat(Source src);
    int getBar(Source src);
    int getBeatsPerBar(Source src);
    
  private:

    class Provider* provider = nullptr;
    class MidiRealizer* midiTransport = nullptr;
    juce::OwnedArray<Follower> followers;
    juce::Array<int> leaders;
    
    // configuration
    int driftThreshold = 1000;
    
    // random statistics
    int lastMillisecond = 0;
    int millisecond = 0;
    int interruptFrames = 0;

    //
    // Host sync state
    //

    // true when the host transport was advancing in the past
    bool hostPlaying = false;

    // tempo and time signature from the host
    float hostTempo = 0.0f;
    int hostBeat = 0;
    int hostBar = 0;
    int hostBeatsPerBar = 0;
    Pulse hostPulse;
    
    //
    // MIDI sync state
    //

    int midiInBeat = 0;
    int midiInBar = 0;
    int midiInBeatsPerBar = 0;
    Pulse midiInPulse;
    
    int midiOutBeat = 0;
    int midiOutBar = 0;
    int midiOutBeatsPerBar = 0;
    Pulse midiOutPulse;

    void reset();
    
    void gatherHost(class MobiusAudioStream* stream);
    void gatherMidi();
    bool detectMidiBeat(class MidiSyncEvent* mse, Source src, Pulse* pulse);
    int getBar(int beat, int bpb);
    int getPulseFrame(Pulse* p, Type type);
    void orderInternalLeaders();
    void advance(int blockFrames);
    
    void trace();
    void trace(Pulse& p);

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
