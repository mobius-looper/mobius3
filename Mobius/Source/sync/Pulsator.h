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

    void setBeatsPerBar(SyncSource src, int bpb);
    void setBarsPerLoop(SyncSource src, int bpl);

    // Block Lifecycle

    void advance(class MobiusAudioStream* stream);
    
    // called by leaders to register a pulse in this block
    void addLeaderPulse(int leader, SyncUnit unit, int frameOffset);

    Pulse* getRelevantBlockPulse(int follower);


    // Following

    // register a follow for an external sync source
    void follow(int follower, SyncSource source, SyncUnit unit);

    // register a follow for an internal leader
    void follow(int follower, int leader, SyncUnit unit);

    void unfollow(int follower);

    Follower* getFollower(int id, bool warn = true);
    Leader* getLeader(int id);

    // granular state
    // hating how this is falling out
    
    int getBeat(int trackId);
    int getBeat(SyncSource src);
    
    int getBar(int trackId);
    int getBar(SyncSource src);
    
    int getBeatsPerBar(int trackId);
    int getBeatsPerBar(SyncSource src);

  private:

    class SyncMaster* syncMaster = nullptr;
    juce::OwnedArray<Leader> leaders;
    juce::OwnedArray<Follower> followers;
    
    // random statistics
    int lastMillisecond = 0;
    int millisecond = 0;
    int interruptFrames = 0;

    // things from the session
    int sessionBeatsPerBar = 0;
    bool sessionOverrideHostBar = false;

    Pulse hostPulse;
    BarTender hostBarTender;
    
    Pulse midiPulse;
    BarTender midiBarTender;
    
    Pulse transportPulse;

    void reset();
    
    void gatherTransport();
    void gatherHost();
    void gatherMidi();
    void gatherFollowerPulses();

    Pulse* getPulseObject(SyncSource source, int leader);
    Pulse* getBlockPulse(SyncSource source, int leader);
    Pulse* getAnyBlockPulse(Follower* f);
    bool isRelevant(Pulse* p, SyncUnit followUnit);

    class BarTender* getBarTender(SyncSource src);
    
    void trace();
    void trace(Pulse& p);
    const char* getSourceName(SyncSource source);
    const char* getUnitName(SyncUnit unit);

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
