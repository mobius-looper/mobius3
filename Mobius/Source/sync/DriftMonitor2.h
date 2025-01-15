/**
 * Another stab at drift monitoring during Host sync analysis.
 * Should be usable for MIDI too.
 *
 * Most synchronization sources generate a regular signal (clock, pulse, etc.) with a consistent
 * time distance between them.  These signals can be distilled into logical Beats at at standard
 * musical Tempo.
 *
 * To synchronize digital audio loops, it is important to have a stable tempo, where the tempo
 * can be distilled into a Beat Length in units of samples at the sample rate of the digital
 * audio stream.
 *
 * When synchronizing with a plugin host the beat signals usually remain stable but can have
 * some jitter due to floating point rounding inherent in the way hosts interact with plugins.
 * When synchronizing with MIDI beat signals are MUCH more variable and can have high jitter.
 * With any synchronization source, the tempo may change under the control of the user.
 *
 * Within the Mobius application, the purpose of the Sync Analyzers is to monitor the tempo
 * and beat signals from the source and to "normalize" those into beats that will have a precice
 * length in samples.   Due to various factors such as floating point roundoff and the desire
 * to make beat lengths an even number of samples, the length of a normalized beat in real time
 * may be slighly different than the length of the source beats.  This difference is small but
 * can accumulate over time leading to Drift.  When Drift exceeds a threshold a Correction must
 * be made to realign the normalized beats with the source beats with a corresponding correction
 * in the "playback head" used to generate audio content in the application.
 *
 * The DriftMonitor works like this:
 *
 * A "stream time" in sample is maintained from the last "orientation".
 * Orientation always happens when the host transport starts, and the monitor
 * may be occasionally reoriented.
 *
 * When a source beat is detected, the length of the beat in samples is found by subtracting
 * the stream time from the last beat with the new beat.
 *
 * This is compared to the "unit length" which is the normalized beat length being used for
 * audio synchronization.  If they differ there is drift. This drift accumulates on every beat.
 * Due to jitter it will usually bounce around a center point (preferably zero) or a small number)
 * but if the tempo of the host changes it will start to grow in one direction.
 *
 */

#pragma once

class DriftMonitor2
{
  public:

    DriftMonitor2() {}
    ~DriftMonitor2() {}

    // reorient the monitor
    void orient(int normalizedUnitLength);

    // record the reception of a source beat
    // blockOffset is the offset in the current audio block where the beat occurs
    // streamTime has the base sample position of this block
    // this must be called before advanceStreamTime
    void addBeat(int blockOffset);

    // advance the stream time after the beats have been detected
    void advanceStreamTime(int blockOffset);

    int getDrift();

    int getStreamTime() {
        return streamTime;
    }

  private:

    // the number of samples that have elapsed since the Start Point
    // (or call to orient)
    int streamTime = 0;

    // the length of the normalized beat unit in samples
    int normalizedUnit = 0;

    // the stream time of the last source beat
    int lastBeatTime = 0;
    
    // last drift calculated
    int drift = 0;

};


    
