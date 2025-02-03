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
 * The easiest approach is Averaging, where the distance between successive clocks
 * is measured and a running average is maintained.
 *
 * This duplicates some of the logic found in other MIDI byte stream analyzers
 * but I want to keep this self contained.
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
     * It goes back to zero and counts up from there.
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
     * True if it is still in the process of filling the sample window.
     * Tempo will be less reliabla.
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
     * The maximum number of clock averaging samples that can be maintained.
     */
    static const int ClockSampleMax = 256;

    /**
     * The clock delta samples.
     */
    double clockSamples[ClockSampleMax];

    /**
     * The number of clock samples to use in averaging.
     * This can be tuned but must be less than or equal to ClockSampleMax
     * and normally significantly higher than zero.
     *
     * Larger numbers will result in smoother tempos but take longer to adapt to changes.
     * At a tempo of 120 there are 2 beats per second or 48 midi clocks per second
     */
    int windowSize = 128;

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
