/**
 * An interface implemented by each SyncSource analyzer to make them
 * easier to deal with generically without a bunch of "if host do this, if midi do this"
 * logic in Pulsator and elsewhere.
 *
 */

#pragma once

class SyncAnalyzer
{
  public:

    virtual ~SyncAnalyzer() {}

    /**
     * Perform analysis on a logical audio block of the given length.
     * The contents of the block are not important, only the size.
     */
    void analyze(int blockFrames) = 0;

    /**
     * Return the results of the previous block analysis.
     */
    SyncSourceResult* getResult() = 0;

    /**
     * True if the source is in a a Running state.
     * This means it can be expected to produce beat pulses
     * at the defined tempo.
     */
    bool isRunning() = 0;

    /**
     * True if the source supports native Beat numbers.
     * This is true for Host and Transport, false for Midi.
     */
    bool hasNativeBeat() = 0;

    /**
     * Return the native beat count if one is defined.
     * Beats start from zero so hasNativeBeat must be used to
     * determine if this number is meaningful.
     */
    int getNativeBeat() = 0;

    /**
     * True if the source supports native Bar numberes.
     * This is always true for Transport.  In theory it may be true
     * for Host, though not all hosts support native bars, to a degree
     * that it is all but useless.
     */
    bool hasNativeBar() = 0;
      
    /**
     * Return the native bar count if one is defined.
     * Bar numbers start from zero so hasNativeBar must be used to determine
     * if this is meeaningful.
     */
    int getNativeBar() = 0;

    /**
     * For sources that do not support native beat counts, this
     * will be the number of beats that have elapsed sine the last
     * Start Point.  All sources support this.
     */
    int getElapsedBeats() = 0;

    /**
     * Return true if this host supports a native time signature.
     * When this is true and the time signature changes, the
     * timeSignatureChanged flag is set in the SyncSourceResult.
     */
    bool hasNativeTimeSignature() = 0;

    /**
     * For sources that support a native time signature, the number
     * of beats in one bar (aka the time signature denominator)
     * For hosts that do not reliably return a NativeBar this can be
     * combined with the NativeBeat number to derive native bar locations.
     */
    int getNativeBeatsPerBar() = 0;

    /**
     * All sources must provide a tempo.  This will either be a fixed
     * quality of the source (e.g. Host almost always has a specified tempo)
     * or derived by measuring the distance between beats.
     * Tempo may fluctuate over time.  This is intended for display purposes
     * only, for synchronization, you must use unitLength.
     */
    float getTempo() = 0;

    /**
     * All sources monitor a fluctuating tempo and derive a unitLength
     * in samples.  In practice this is the length of one Beat in samples.
     * The unitLength will only change when tempo fluctations exceed a threshold.
     *
     * Some sources may need time to monitor synchronization pulses and make an
     * accurate tempo determination.  During this period getUnitLenght returns zero
     * and the application should not expect to receive accurate beat pulses.
     */
    int getUnitLength() = 0;

    // todo: Consider we need sub-beet units

    /**
     * All sources will monitor drift once the unitLength has been calculated.
     * This is the amount of the drift.
     *
     * When the drift exceeds a threshold the unitLength will be recalculated
     * and the tempoChanged flag will be set in SyncSourceResult.
     *
     * todo: needs work, it may be best to not allow sources to jump tempos
     * by themselves.  They report drift and the application may then decide
     * to act on it, and once it does call back to correctDrift()
     * Host needs thought because tempo can change dramatically at any time.
     *
     * There is a difference between drift correction and tempo changes.
     */
    int getDrift() = 0;

    //
    // Section of Necessary Thought
    //
    // When a source resumes from the Stopped state it can pick up
    // logically at the beginning of time, or it can be in the middle of
    // a larger time region, such as a host track.  It would be interesting
    // to know where that is so the application can make corresponding adjustments
    // to the playback location of the loops.
    //
    // This is basically the MIDI "song position" which for Host would be the
    // starting beat number when the transport started.
    //
    // Would this go here or in the SyncSourceResult?
    //

    //int getStartingBeat() = 0;
    
};
