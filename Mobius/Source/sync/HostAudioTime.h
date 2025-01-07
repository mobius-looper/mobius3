/**
 * A model containing synchronizataion state from the plugin host.
 * Maintained by HostAnalyzer.
 * 
 * This is an older model that sat between Synchronizer and VstTimeInfo.
 * Due for an overhaul but works well enough for now.
 */

#pragma once

class HostAudioTime {

  public:

	/**
	 * Host tempo.
	 */
	double tempo = 0.0;

	/**
	 * The "beat position" of the current audio buffer.
     * 
	 * For VST hosts, this is VstTimeInfo.ppqPos.
	 * It starts at 0.0 and increments by a fraction according
	 * to the tempo.  When it crosses a beat boundary the integrer
     * part is incremented.
     *
     * For AU host the currentBeat returned by CallHostBeatAndTempo
     * works the same way.
	 */
	double beatPosition = -1.0;

	/**
	 * True if the host transport is "playing".
	 */
	bool playing = false;

	/**
	 * True if there is a beat boundary in this buffer.
	 */
	bool beatBoundary = false;

	/**
	 * True if there is a bar boundary in this buffer.
	 */
	bool barBoundary = false;

	/**
	 * Frame offset to the beat/bar boundary in this buffer.
     * note: this never worked right and it will always be zero
     * see extensive comments in HostSyncState
	 */
	int boundaryOffset = 0;

	/**
	 * Current beat.
	 */
	int beat = 0;

	/**
	 * Current bar.
     * This is the bar the host provides if it can.
     * For pattern-based hosts like FL Studio the bar may stay at zero all the time.
	 */
	int bar;

    /**
     * Number of beats in one bar.  If zero it is undefined, beat should
     * increment without wrapping and bar should stay zero.
     * Most hosts can convey the transport time signature but not all do.
     */
    int beatsPerBar = 0;

    // TODO: also capture host time signture if we can
    // may need some flags to say if it is reliable

	void init() {
		tempo = 0.0;
		beatPosition = -1.0;
		playing = false;
		beatBoundary = false;
		barBoundary = false;
		boundaryOffset = 0;
		beat = 0;
		bar = 0;
        beatsPerBar = 0;
	}

};
