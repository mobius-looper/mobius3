/**
 * Subcomponent of MidiAnalyzer to monitor MIDI clocks and guess the tempo.
 * 
 * The juce::MidiMessage timestamp is "milliseconds / 1000.0f",
 * in other words a timestamp in fractions of seconds.  The delta
 * between clocks is then "seconds per clock".  Docs say this is derived
 * from juce::Time::getMillisecondCounterHiRes().
 *
 * There are 24 clocks per quarter note so tempo is:
 *
 *      60 / secsPerClock * 24
 *
 * There is jitter even when using a direct path to the MidiInput
 * callback.  Trace during analyais adds noise so the capture diagnostics
 * defer trace until samples have been captured.
 *
 * Testing with the MC-101 at 90.0 BPM yielded these results on each clock:
 *
 *  90.016129, 89.714100, 91.415700, 89.285714, 89.285714, 89.836927
 *  90.074510, 90.446661, 89.819500, 90.219159, 89.572989, 90.302910
 *  90.456154, 89.710558, 89.877946. 90.039149, 89.891842, 90.047257
 *  89.885053
 *
 * The bottom line is that you can't pick a tempo (and therefore a beat unit length
 * in samples) based on the delta between just two clocks, there is simply too much
 * jitter, either in the sending device itself, or more likely the tortured path through
 * the OS that MIDI messages go through before they reach Juce and are timestamped.
 * I haven't looked at how the Juce code responds to the lowest-level MIDI interrupt
 * handler, but I expect it does it about as well as can be done.  Clock jitter is
 * just inherant with MIDI and you need to take steps to compensate for it.
 *
 * Clock jitter is smoothed using the usual "averaging" approach which makes it better
 * but still not great.  There is a "window" of clock distance samples that is averaged
 * to produce the tempo/unit length.  The number of clocks to include in the window
 * is tunable but typically a number of quarter notes like 4.  A larger window yields
 * smoother tempos but responds to deliberate tempo changes more gradually.
 *
 * For Mobius, I'm erring on the side of user initiated tempo changes being rare.
 * It is far more common for the tempo to just sit there for the duration of a
 * "song" so it is better to eliminate tempo wobble than it is to track it.
 *
 * A state of "Cold Start" exists for devices that do not send clocks when in
 * a stopped state.  When that happens, the monitor resumes from a reset state and
 * begins filling the sample window, this is called the "warmup period".  Decisions
 * made about tempo during this period should be given less emphasis than after.
 *
 * The monitor is called periodically in the audio thread to detect clock stoppages.
 * When a stoppage happens, the state is reset and it will enter the warmup period
 * upon recipt of the next clock.
 *
 */

class MidiTempoMonitor
{
  public:

    MidiTempoMonitor() {}
    ~MidiTempoMonitor() {}

    /**
     * This is called in the MIDI thread for every MIDI clock message.
     * This is the only public method that is called from the MIDI thread.
     */
    void consume(const juce::MidiMessage& msg);

    /**
     * Set the current audio device sample rate for simulating elapsed
     * "stream time" on each clock.
     * update: This turned out to be unuseful.
     */
    void setSampleRate(int rate);

    /**
     * Do a full reset, including the averaging samples.
     * This should only be done after a clock stoppage is detected.
     * Normally averaging continues as long as there are clocks being received.
     */
    void reset();

    /**
     * Reorient the clock counter after the audio stream has locked a unit length.
     * It goes back to zero and counts up from there.  The elapsed clock count
     * from this point forward is used in drift detection.
     */
    void orient();

    /**
     * Start the stream time tracker back to zero.
     * Normally called after Start or Continue is received by MidiEventMonitor.
     */
    void resetStreamTime();

    /**
     * Get the current elapsed clock counter since orientation.
     */
    int getElapsedClocks();

    /**
     * Get the current simulated stream time as of the last clock.
     */
    int getStreamTime();
    
    /**
     * Called periodically from the audio thread to detect when clocks
     * have stopped being received.
     */
    void checkStop();

    /**
     * True if clocks are being received at regular intervals.
     */
    bool isReceiving();

    /**
     * True if it is still in the process of filling the sample window
     * ala the "warmup period". Tempo will be unreliable.
     */
    bool isFilling();

    /**
     * Various averaging calculations
     */
    double getAverageClock();
    double getAverageClockLength();
    float getAverageTempo();
    int getAverageUnitLength();

    /**
     * Utility to reverse calculate tempo from unit length after rounding.
     */
    float unitLengthToTempo(int length);

  private:

    int sampleRate = 0;

    /**
     * True if we are receiving clocks
     */
    bool receiving = false;

    /**
     * The default width of the average window in clocks.
     * 4 beats = 96 clocks.
     *
     * Could allow a parameter to tune what is actually used,
     * but not bothering for now.
     */
    static const int ClockWindowDefault = 96;

    /**
     * The maximum number of clock averaging samples that can be maintained.
     * It doesn't really mattern what this is but the windowSize is not allowed
     * to exceed this.  4 bars should be enough.
     */
    static const int ClockSampleMax = ClockWindowDefault * 4;

    /**
     * The clock delta samples.
     */
    double clockSamples[ClockSampleMax];

    /**
     * The number of clock samples to use in averaging.
     * This can be tuned but must be less than or equal to ClockSampleMax
     * and normally significantly higher than zero.
     */
    int windowSize = ClockWindowDefault;

    /**
     * The location of the next sample to be added.
     * This will loop between zero and windowSize.
     */
    int windowPosition = 0;

    /**
     * Set to true when enough samples have been received to begin
     * doing tempo analysis.  This becomes true the first time
     * windowPosition reaches windowSize and remains true until
     * the analyzer is reset.
     */
    bool windowFull = false;

    /**
     * The timestamp of the last clock received.
     */
    double lastTimeStamp = 0.0f;

    /**
     * The running total and average.
     */
    double runningTotal = 0.0f;
    double runningAverage = 0.0f;

    /**
     * The number of clocks that have been receieved since orientation.
     */
    int clocks = 0;

    // experimental stream time simulation
    int streamTime = 0;

    /**
     * The last time we traced what we're doing if trace was enabled.
     */
    double lastTrace = 0.0f;

    /**
     * True to enable detailed trace
     */
    bool traceEnabled = false;

    bool looksReasonable(double delta);
     
};
