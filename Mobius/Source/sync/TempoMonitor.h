/**
 * Old utility class to monitor the time differences beteween MIDI
 * clock messages to derive the tempo.
 */

#pragma once

/*
 * The number of tempo "samples" we maintain for the running average.
 * A sample is the time in milliseconds between clocks.
 * 24 would be one "beat", works but is jittery at tempos above 200.
 * Raising this to 96 gave more stability.  The problem is that the
 * percieved tempo changes more slowly as we smooth over an entire bar.
 */
#define MIDI_TEMPO_SAMPLES 24 * 4

/**
 * The number of tempo samples that the tempo has to remain +1 or -1
 * from the last tempo before we change the tempo.
 */
#define MIDI_TEMPO_JITTER 24

/**
 * This is used internally by MidiInput to calculate a smooth tempo from
 * incomming MIDI clocks.
 */
class TempoMonitor {

  public:
 
	TempoMonitor();
	~TempoMonitor();

	void reset();
	void clock(long msec);

	float getPulseWidth();
	float getTempo();
	int getSmoothTempo();

  private:

	void initSamples();

	long mSamples[MIDI_TEMPO_SAMPLES];
	long mLastTime;
	int mSample;
	long mTotal;
	int mDivisor;

	float mPulse;
	int mSmoothTempo;		// stable tempo * 10
	int mJitter;

};
