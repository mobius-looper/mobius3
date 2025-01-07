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
 * for MIDI clocks (MidiRealizer) and the plugin host (HostSyncState).
 * It may later have pulses added to it by the audio/midi tracks as they cross synchronization
 * boundaries during their block advance.  These are called Followers and Leaders.
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
 * What Pulsator does is allow the internal tracks to register a Follower.  A follwer
 * is given the size of a unit of audio data in samples (e.g. the loop length) and the number
 * of pulses that unit contained while it was being recorded.  As future audio blocks are
 * received it compares the rate at which samples are being received against the rate
 * at which pulses are being received and if they start to diverge it calculates the
 * amount of drift. When this drift exceeds a threshold, it can inform the track that
 * it needs to adjust it's playback position to remain in alignment with the external
 * pulse generator.
 *
 * While conceptually pulses may be very small, such as individual MIDI clocks, in practice
 * loop tracks only care about coarse grained "beat" pulses.  Since you can't in practice
 * have multiple beat pulses per audio block, it greatly simplifies the internal models
 * if it only needs to detect one pulse from each source per block.
 */

#pragma once

#include <JuceHeader.h>

#include "Pulse.h"
#include "Leader.h"
#include "Follower.h"

class Pulsator
{
  public:

    /**
     * The sync state maintained for each source
     */
    class SyncState {
      public:
        float tempo = 0.0f;
        int beat = 0;
        int bar = 0;
        int beatsPerBar = 0;
        Pulse pulse;
    };
    
    Pulsator(SyncMaster* sm);
    ~Pulsator();

    void loadSession(class Session* s);

    void interruptStart(class MobiusAudioStream* stream);

    Pulse* getRelevantBlockPulse(Follower* f);
    Pulse* getOutBlockPulse();

    // register a follow for an external sync source
    void follow(int follower, Pulse::Source source, Pulse::Type type);

    // register a follow for an internal leader
    void follow(int follower, int leader, Pulse::Type type);

    void start(int follower);
    void lock(int follower, int frames);
    void unlock(int follower);
    
    // stop following the source
    void unfollow(int follower);

    bool shouldCheckDrift(int follower);
    int getDrift(int follower);
    void correctDrift(int follower, int frames);
    
    // called by leaders to register a pulse in this block
    void addLeaderPulse(int leader, Pulse::Type type, int frameOffset);

    //
    // State of the various sources
    //

    SyncState* getHostState() {
        return &host;
    }

    SyncState* getMidiInState() {
        return &midiIn;
    }

    //Pulse* getBlockPulse(Pulse::Source src);
    Follower* getFollower(int id, bool warn = true);
    Leader* getLeader(int id);

  private:

    class SyncMaster* syncMaster = nullptr;
    juce::OwnedArray<Leader> leaders;
    juce::OwnedArray<Follower> followers;
    
    // captured configuration
    int driftThreshold = 1000;
    
    // random statistics
    int lastMillisecond = 0;
    int millisecond = 0;
    int interruptFrames = 0;

    // master tracks
    int outSyncMaster = 0;
    
    // true when the host transport was advancing in the past
    bool hostPlaying = false;
    SyncState host;
    SyncState transport;
    SyncState midiIn;
    SyncState midiOut;

    void reset();
    void advance(int blockFrames);
    
    void gatherTransport();
    void gatherHost();
    void gatherMidi();
    bool detectMidiBeat(class MidiSyncEvent* mse, Pulse::Source src, Pulse* pulse);

    Pulse* getPulseObject(Pulse::Source source, int leader);
    Pulse* getBlockPulse(Pulse::Source source, int leader);
    Pulse* getAnyBlockPulse(Follower* f);
    bool isRelevant(Pulse* p, Pulse::Type followType);
    
    void trace();
    void trace(Pulse& p);
    void traceFollowChange(Follower* f, Pulse::Source source, int leader,Pulse::Type type);
    const char* getSourceName(Pulse::Source source);
    const char* getPulseName(Pulse::Type type);

    // formerly used by MidiTrack, now internal and some of it is unnecessary?
    int getPulseFrame(int follower);
    int getPulseFrame(int followerId, Pulse::Type type);

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
