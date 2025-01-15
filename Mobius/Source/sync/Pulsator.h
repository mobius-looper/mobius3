/**
 * The purpose of the Pulsator is to analyze synchronization data from
 * various sync sources and distill it into a "pulse" that happens within
 * the current audio block.  These pulses may then be used to trigger synchronization
 * events within the audio or midi tracks of the Mobius Engine.
 *
 * Pulsator does it's analysis at the begining of each audio block, consuming events
 * for MIDI clocks (MidiAnalyzer) the plugin host (HostAnalyzer), and the Transport.
 * 
 * It may later have pulses added to it by the audio/midi tracks as they cross synchronization
 * boundaries during their block advance.  These are called Followers and Leaders.
 */

#pragma once

#include <JuceHeader.h>

#include "SyncConstants.h"
#include "Pulse.h"
#include "Leader.h"
#include "Follower.h"
#include "BarTender.h"

class Pulsator
{
  public:

    Pulsator(SyncMaster* sm);
    ~Pulsator();

    // Configuration
    
    void loadSession(class Session* s);

    void userSetBeatsPerBar(int bpb, bool action);
    void propagateTransportTimeSignature(int bpb);

    // Block Lifecycle

    void advance(int blockSize);
    
    // called by leaders to register a pulse in this block
    void addLeaderPulse(int leader, SyncUnit unit, int blockOffset);

    Pulse* getRelevantBlockPulse(int follower);

    // Following

    // register a follow for an external sync source
    void follow(int follower, SyncSource source, SyncUnit unit);

    // register a follow for an internal leader
    void follow(int follower, int leader, SyncUnit unit);

    void unfollow(int follower);

    Follower* getFollower(int id, bool warn = true);
    Leader* getLeader(int id);

  private:

    class SyncMaster* syncMaster = nullptr;
    class BarTender* barTender = nullptr;
    juce::OwnedArray<Leader> leaders;
    juce::OwnedArray<Follower> followers;

    // captured during advance
    int millisecond = 0;
    int blockFrames = 0;

    Pulse hostPulse;
    Pulse midiPulse;
    Pulse transportPulse;
    
    void reset();
    
    void convertPulse(class SyncAnalyzerResult* result, Pulse& pulse);
    void gatherTransport();
    void gatherHost();
    void gatherMidi();

    Pulse* getPulseObject(SyncSource source, int leader);
    Pulse* getBlockPulse(SyncSource source, int leader);
    Pulse* getAnyBlockPulse(Follower* f);
    bool isRelevant(Follower* f, Pulse* p);

    void trace();
    void trace(Pulse& p);
    const char* getSourceName(SyncSource source);
    const char* getUnitName(SyncUnit unit);

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
