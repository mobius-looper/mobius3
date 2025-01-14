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

    BarTender(class SyncMaster* sm);
    ~BarTender();

    void loadSession(class Session* s);
    void updateBeatsPerBar(int bpb);

    void advance(int blockFrames);
    Pulse* annotate(class Follower* f, Pulse* p);

    int getBeat(int trackNumber);
    int getBar(int trackNumber);
    int getBeatsPerBar(int trackNumber);
    int getBarsPerLoop(int trackNumber);

  private:

    class SyncMaster* syncMaster = nullptr;

    // options captured from the Session
    int sessionBeatsPerBar = 0;
    bool sessionHostOverride = false;

    // the annotated Pulse passed back to TimeSlicer
    Pulse annotated;

    bool detectHostBar();
    int getHostBeatsPerBar();
    
    SyncSource getSyncSource(int trackNumber);
    void getLeaderProperties(int follower, class TrackProperties& props);
};
