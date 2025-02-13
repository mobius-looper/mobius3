/**
 * Utility class that organizes the notion of "bars" within a track.
 *
 * What a "bar" is is surpsignly complicated among the sync sources, and
 * the various configuration options desired to let the user decide where
 * usable sync boundaries are.
 *
 * BarTender encapsulates that mess into one place, and provides the model for
 * tracks to define their own ideas for what "beats per bar" and "bars per loop"
 * look like.
 */

#pragma once

#include "../../model/SyncConstants.h"
#include "Pulse.h"

class BarTender
{
  public:

    /**
     * Each track may override the system default time signature
     * and/or any time signature advertised by the sync source.
     * This will be loaded from the Session.
     *
     * note: track overrides not currently expected, but it's there
     * if you need it
     */
    class Track {
      public:
        // when non-zero, this track defines it's own bar length
        int beatsPerBar = 0;
        // when non-zero, this track defines it's own loop length
        // the default is one bar per loop
        int barsPerLoop = 0;
    };

    BarTender(class SyncMaster* sm, class TrackManager* tm);
    ~BarTender();

    void loadSession(class Session* s);
    void globalReset();
    bool doAction(class UIAction* a);
    bool doQuery(class Query* q);
    
    void advance(int blockFrames);
    Pulse* annotate(class LogicalTrack* t, Pulse* p);

    int getBeat(int trackNumber);
    int getBeat(class LogicalTrack* t);
    int getBeat(SyncSource src);
    
    int getBar(int trackNumber);
    int getBar(class LogicalTrack* t);
    int getBar(SyncSource src);
    
    int getLoop(int trackNumber);
    int getLoop(class LogicalTrack* t);
    int getLoop(SyncSource src);
    
    int getBeatsPerBar(int trackNumber);
    int getBeatsPerBar(class LogicalTrack* t);
    int getBeatsPerBar(SyncSource src);
        
    int getBarsPerLoop(int trackNumber);
    int getBarsPerLoop(class LogicalTrack* t);
    int getBarsPerLoop(SyncSource src);

  private:

    class SyncMaster* syncMaster = nullptr;
    class TrackManager* trackManager = nullptr;
    class Session* session = nullptr;
    
    int hostBeatsPerBar = 0;
    int hostBarsPerLoop = 0;
    bool hostOverride = false;
    
    int midiBeatsPerBar = 0;
    int midiBarsPerLoop = 0;

    // the annotated Pulse passed back to TimeSlicer
    Pulse annotated;

    void cacheSessionParameters();
    void setHostBeatsPerBar(int bpb);
    void setHostBarsPerLoop(int bpl);
    void setHostOverride(bool b);
    void setMidiBeatsPerBar(int bpb);
    void setMidiBarsPerLoop(int bpl);

    void detectHostBar(bool& onBar, bool& onLoop);
    
    int getHostBeatsPerBar();
    int getHostBarsPerLoop();
    int getMidiBeatsPerBar();
    int getMidiBarsPerLoop();
};
