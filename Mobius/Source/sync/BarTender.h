/**
 * Utility class that manages the notion of "bars" within a track and
 * synchronization beats from a synchronization source.
 *
 * This is mostly a placeholder for Bar computation which is a surprisingly complex
 * problem.  It doesn't do much right now beyond hold track-specific time signatures
 * and the math necessary to calculate normalized beat/bar counts for the UI.
 *
 * Thoughts:
 *
 * Tracks receive synchronization pulses (beats) from a SyncSource 
 * Some SyncSources may have a native notion of beatsPerBar which may
 * be used by the track, or it may be overridden.  The priority for determining
 * beatsPerBar within a track are:
 *
 *      1) BPB defined on the track
 *      2) Native BPB defined by the sync source
 *      3) BPB defined by the Transport
 *      4) BPB defined globaly in the Session
 *
 */

#pragma once

class BarTender
{
  public:

    BarTender() {}
    ~BarTender() {}

    // the track number this is associated with
    // this is used in callbacks to SyncMaster to find information about the track
    // and used in Trace
    void setNumber(int n);

    void setBeatsPerBar(int n);

    // optional setting that gives the owner the notion of a loop pulse
    // consistenting of multiple bars
    void setBarsPerLoop(int n);

    // reorient counters and position on this raw beat number
    void orient(int rawBeat);

    // consume one beat pulse, adjust the internal counters, and
    // annotate the Pulse with bar markers
    void annotate(class Pulse* p);


    int getBeat();
    int getBar();
    int getBeatsPerBar();
    int getBarsPerLoop();

  private:

    int number = 0;

    // the beatsPerBar override
    // when zero it defaults to the common BPB defined by the session
    int beatsPerBar = 0;

    // number of bars in a loop
    int barsPerLoop = 0;

    // unaltered elapsed number of beats
    int rawBeat = 0;

    // the relative beat number we are currently on
    int beat = 0;

    // normalized bar number we are currently in
    int bar = 0;

    // the loop number if barsPerLoop is specified
    int loop = 0;

    void reconfigure();

};
