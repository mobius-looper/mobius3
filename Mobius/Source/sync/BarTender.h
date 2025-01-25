/**
 * Utility class that organizes the notion of "bars" within a track and
 * synchronization beats from a synchronization source.
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

#include "SyncConstants.h"
#include "Pulse.h"

class BarTender
{
  public:

    /**
     * Each track may override the system default time signature
     * and/or any time signature advertised by the sync source.
     * This will be loaded from the Session.
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

    void advance(int blockFrames);
    Pulse* annotate(class LogicalTrack* t, Pulse* p);

    // should this take numbers or a follower?
    // Follower is more common internally
    int getBeat(int trackNumber);
    int getBeat(class LogicalTrack* t);
    int getBar(int trackNumber);
    int getBar(class LogicalTrack* t);
    int getLoop(int trackNumber);
    int getLoop(class LogicalTrack* t);
    int getBeatsPerBar(int trackNumber);
    int getBarsPerLoop(int trackNumber);

  private:

    class SyncMaster* syncMaster = nullptr;
    class TrackManager* trackManager = nullptr;
    int hostBeatsPerBar = 0;
    int hostBarsPerLoop = 0;
    bool hostOverride = false;
    
    int midiBeatsPerBar = 0;
    int midiBarsPerLoop = 0;

    // the annotated Pulse passed back to TimeSlicer
    Pulse annotated;

    void detectHostBar(bool& onBar, bool& onLoop);
    
    int getHostBeatsPerBar();
    int getHostBarsPerLoop();
    int getMidiBeatsPerBar();
    int getMidiBarsPerLoop();
    
    SyncSource getSyncSource(int trackNumber);
    void getLeaderProperties(int follower, class TrackProperties& props);
};
