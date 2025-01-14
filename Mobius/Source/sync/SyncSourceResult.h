/**
 * State calculated by a SyncSource at the beginning of each audio block.
 * 
 * This is used internally by Pulsator to determine when the sync source
 * starts and stops, when beats happen, and provides used to determine
 * where "bars" are.
 *
 * The state returned here is transient and meaningful only for duration of
 * one block.  Additional information about the SyncSource is obtained from
 * the SyncSource object and persists across block, e.g. tempo, unitLength
 * time signature.
 */

#pragma once

class SyncSourceResult
{
  public:

    /**
     * True if the host went from a Stopped to Running state in this block.
     * This is typically used to reorient things that want to track the
     * sync sources location.
     *
     * This is often but not necessarily the same has receiving a Beat.
     * The beatDetected flag must be used to determine whether simply starting
     * should be considered a synchronization beat.
     */
    bool started = false;

    /**
     * True if the host went from a Running to a Stopped state in this block.
     * This does not usually mean there is a Beat in this block.
     */
    bool stopped = false;

    /**
     * True if there was a beat detected in this block.
     */
    bool beatDetected = false;

    /**
     * When beatDetected is true, this is the offsets within the
     * block in samples where the beat ocurred.
     */
    int blockOffset = 0;

    /**
     * True if a native bar was detected.  beatDetected will also be true.
     * This is supported only by sources that also support a native beat count
     * and a native time signature, e.g. Host and Transport.  Pulsator may choose
     * to ignore this.
     * todo: decide if this is necessary
     */
    //bool barDetected = false;

    /**
     * True when the source beats have changed tempo and therefore the unit length.
     * Host and Midi sources may change tempo under user control.
     */
    bool tempoChanged = false;

    /**
     * True when the host supports a native time signature (e.g. beatsPerBar)
     * and the time signature changed.
     */
    bool timeSignatureChanged = false;
    
    void reset() {
        
        started = false;
        stopped = false;
        beatDetected = false;
        blockOffset = 0;
        tempoChanged = false;
        timeSignatureChanged = false;
    }

};
    
    
