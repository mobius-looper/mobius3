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

#include "../../model/SyncConstants.h"
#include "Pulse.h"
#include "BarTender.h"

class Pulsator
{
  public:

    Pulsator(class SyncMaster* sm, class TrackManager* tm, class BarTender* bt);
    ~Pulsator();

    // Configuration
    
    void loadSession(class Session* s);

    // Block Lifecycle

    void advance(int blockSize);
    
    // called by leaders to register a pulse in this block
    void addLeaderPulse(int leader, SyncUnit unit, int blockOffset);

    // called indirectly by Transport when it is started
    void notifyTransportStarted();

    // called by SyncMaster to get the relevant pulse for a track
    //Pulse* getBlockPulse(class LogicalTrack* t, SyncUnit unit);

    // called by SyncMaster when it needs to be smarter about adjusting
    // pulse widths
    Pulse* getAnyBlockPulse(class LogicalTrack* t);

  private:

    class SyncMaster* syncMaster = nullptr;
    class TrackManager* trackManager = nullptr;
    class BarTender* barTender = nullptr;

    // captured during advance
    int millisecond = 0;
    int blockFrames = 0;

    Pulse hostPulse;
    Pulse midiPulse;
    Pulse transportPulse;
    
    void reset();
    
    void convertPulse(SyncSource source, class SyncAnalyzerResult* result, Pulse& pulse);
    void gatherTransport();
    void gatherHost();
    void gatherMidi();

    Pulse* getBlockPulse(SyncSource source, int leader);
    Pulse* getPulseObject(SyncSource source, int leader);

    void trace();
    void trace(Pulse& p);
    const char* getSourceName(SyncSource source);
    const char* getUnitName(SyncUnit unit);

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
