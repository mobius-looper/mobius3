
#include <JuceHeader.h>

#include "../util/Trace.h"

#include "MidiTempoMonitor.h"

#define MS_CLOCK      0xF8

void MidiTempoMonitor::setSampleRate(int rate)
{
    sampleRate = rate;
}

void MidiTempoMonitor::reset()
{
    windowPosition = 0;
    windowFull = false;
    lastTimeStamp = 0.0f;
    runningTotal = 0.0f;
    runningAverage = 0.0f;
    receiving = false;
    orient();
}

void MidiTempoMonitor::orient()
{
    clocks = 0;
    streamTime = 0;
}

int MidiTempoMonitor::getElapsedClocks()
{
    return clocks;
}

int MidiTempoMonitor::getStreamTime()
{
    return streamTime;
}

void MidiTempoMonitor::resetStreamTime()
{
    streamTime = 0;
}

bool MidiTempoMonitor::isReceiving()
{
    return receiving;
}

bool MidiTempoMonitor::isFilling()
{
    return !windowFull;
}

void MidiTempoMonitor::consume(const juce::MidiMessage& msg)
{
    const juce::uint8* data = msg.getRawData();
    const juce::uint8 status = *data;

    if (status == MS_CLOCK) {

        receiving = true;
        
        double ts = msg.getTimeStamp();

        if (lastTimeStamp == 0.0f) {
            // first one
            Trace(2, "MidiTempoMonitor: Clocks starting");
        }
        else if (ts < lastTimeStamp) {
            // not expecting this
            Trace(1, "MidiTempoMonitor: TimeStamp went back in time");
            reset();
            // this will have reset stream time which will make drift detection
            // way out of wack, but it really shouldn't happen
        }
        else {
            double delta = ts - lastTimeStamp;

            if (looksReasonable(delta)) {

                // if the window is full, we're looping and remove
                // the oldest sample before we add the new one
                if (windowFull) {
                    double old = clockSamples[windowPosition];
                    runningTotal -= old;
                }

                // dsposit the new sample
                clockSamples[windowPosition] = delta;
                windowPosition++;
                if (windowPosition >= windowSize) {
                    windowFull = true;
                    windowPosition = 0;
                }

                // and add it to the total
                runningTotal += delta;

                // calculate the running average
                if (windowFull)
                  runningAverage = runningTotal / (float)windowSize;
                else
                  runningAverage = runningTotal / (float)windowPosition;


                // simulate a corresponding advance in "audio time" based on the time
                // difference between the clocks
                streamTime += (int)((float)sampleRate * delta);
        
                clocks++;
            }
            else {
                // we reoriented, treat this like the first clock
            }
        }
        lastTimeStamp = ts;
    }
}

/**
 * Here on each delta.
 *
 * There are two things we can do here.  First if the delta is outside the expected range
 * we may be picking after a period of clock stoppage and need to reset.
 * This should only happen if the periodic advance() didn't happen, clocks
 * stopped, then picked up again some time later.
 *
 * Second, we could try to suppress the occasional outlier to prevent them from adding
 * noise to the average.  Punting on this, since it is difficult to know whether this is
 * in fact a jitter outlier, or if it represents a deliberate tempo change.  I suppose
 * if we get more than a few outliers in a row then it's a tempo change.  
 *
 * For stop detection, at 30BPM, there are .5 beats per second, or 12 clocks per second
 * each delta would be 1/12 or .0833.  So once the delta passes .1 it's REALLY slow.
 * Still they might want to do that on purpose.  So let's assume if .5 goes by without a clock
 * something is wrong.  Might want this configurable.
 */
bool MidiTempoMonitor::looksReasonable(double delta)
{
    bool reasonable = (delta < 0.5f);
    if (!reasonable) {
        Trace(1, "MidiTempoMonitor: Resetting after extreme delta");
        reset();
    }
    return reasonable;
}

double MidiTempoMonitor::getAverageClock()
{
    return runningAverage;
}

double MidiTempoMonitor::getAverageClockLength()
{
    return (float)sampleRate * runningAverage;
}

/**
 * Supposed to be called periodically, such as on every audo block,
 * to detect whether clocks have stopped. Similar math to looksReasonable.
 */
void MidiTempoMonitor::checkStop()
{
    double now = juce::Time::getMillisecondCounterHiRes();
    
    if (lastTimeStamp != 0.0f) {
        // juce timestamps are / 1000
        double scaledNow = now / 1000.0f;
        double delta = scaledNow  - lastTimeStamp;
        if (delta < 0.0f || delta > 1.0f) {
            Trace(2, "MidiTempoMonitor: Clocks stopped");
            reset();
        }
    }

    if (traceEnabled) {
        double traceDelta = now - lastTrace;
        if (traceDelta > 1000.0f) {
            int unit = getAverageUnitLength();
            if (unit > 0) {
                char buf[64];
                sprintf(buf, "MidiTempoMonitor: Average seconds %f unit %d",
                        runningAverage, unit);
                Trace(2, buf);
            }
            lastTrace = now;
        }
    }
}

/**
 * The runningAverage is secondsPerClock since Juce timestamps
 * are the milliisecond counter / 1000.
 * seconds per beat is that * 24.
 * seconds per beat is that / 1000
 * beats per second is 1 / seconds per beat
 *
 * Returns zero on startup if we haven't received any clocks which
 * should suppress the display.
 */
float MidiTempoMonitor::getAverageTempo()
{
    float tempo = 0.0f;
    if (runningAverage > 0.0f) {
        double secondsPerBeat = runningAverage * 24;
        double beatsPerMinute = 60.0f / secondsPerBeat;
        tempo = (float)beatsPerMinute;
    }
    return tempo;
}

int MidiTempoMonitor::getAverageUnitLength()
{
    int unitLength = 0;
    if (runningAverage > 0.0f) {
        double secondsPerBeat = runningAverage * 24;
        double framesPerBeat = (float)sampleRate * secondsPerBeat;
        unitLength = (int)framesPerBeat;
        // make it even
        if ((unitLength % 2) > 0) unitLength++;
    }
    return unitLength;
}

float MidiTempoMonitor::unitLengthToTempo(int length)
{
    float tempo = 0.0f;
    if (sampleRate <= 0)
      Trace(1, "MidiTempoMonitor::unitLengthToTempo Sample rate not set");
    else {
        double secondsPerBeat = (float)length / (float)sampleRate;
        double beatsPerMinute = 60.0f / secondsPerBeat;
        tempo = (float)beatsPerMinute;
    }
    return tempo;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
