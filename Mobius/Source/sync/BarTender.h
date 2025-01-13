/**
 * Utility class that manages the notion of "bars" from a synchronization source,
 * including Tracks.
 *
 * Bars are defined with a beatsPerBar setting and various session parameters.
 * There is a session default beatsPerBar and each Track may override this to
 * define bars of different lengths for polyrhythms.
 *
 * Some SyncSources may have a native bar number, notably HostAnalyzer, and this
 * may be used or overridden.
 *
 * One of these will be maintained by SyncMaster for each track (follower)
 * and one will be maintained within the Transport.
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

};
