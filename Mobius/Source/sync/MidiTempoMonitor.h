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

    void reset();
    
    /**
     * This is called for every MIDI realtime message received.
     */
    void consume(const juce::MidiMessage& msg);

    void checkStop();

    /**
     * Trie if clocks are being received at regular intervals.
     */
    bool isReceiving();

    /**
     * Return the average tempo of the measured clock distances.
     */
    float getAverageTempo();

    /**
     * Return the unit length calculated from the tempo.
     */
    int getAverageUnitLength(int sampleRate);

    /**
     * Reverse calculate tempo from unit length after rounding.
     */
    float unitLengthToTempo(int length, int sampleRate);

  private:

    /**
     * True if we are receiving clocks
     */
    bool receiving = false;

    /**
     * The maximum number of clock averaging samples that can be maintained.
     */
    static const int ClockSampleMax = 100;

    /**
     * The clock delta samples.
     */
    double clockSamples[ClockSampleMax];

    /**
     * The number of clock samples to use in averaging.
     * This can be tuned but must be less than or equal to ClockSampleMax
     * and normally significantly higher than zero.
     */
    int windowSize = 50;

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
     * The last time we traced what we're doing if trace was enabled.
     */
    double lastTrace = 0.0f;

    /**
     * True to enable detailed trace
     */
    bool traceEnabled = false;

    bool looksReasonable(double delta);
     
};
