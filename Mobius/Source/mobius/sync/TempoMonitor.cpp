// NOTE: This is obsolete and should be removed once
// MidiRealizer has some redesign

/**
 * Old utility class to monitor the time differences beteween MIDI
 * clock messages to derive the tempo.
 *
 * This was dug out of the old MidiTimer class.
 */

#include "../../util/Trace.h"

#include "TempoMonitor.h"


TempoMonitor::TempoMonitor()
{
	reset();
    // note that this is an integer 10x the actual float tempo
	mSmoothTempo = 0;
}

/**
 * Reset the tracker but leave teh last tempo in place until
 * we can start calculating a new one.
 */
void TempoMonitor::reset()
{
	mLastTime = 0;
	mPulse = 0.0f;
	mJitter = 0;
	initSamples();
}

void TempoMonitor::initSamples()
{
	mSample = 0;
	mTotal = 0;
	mDivisor = 0;
	mJitter = 0;
	for (int i = 0 ; i < MIDI_TEMPO_SAMPLES ; i++)
	  mSamples[i] = 0;
}

TempoMonitor::~TempoMonitor()
{
}

float TempoMonitor::getPulseWidth()
{
	return mPulse;
}

float TempoMonitor::getTempo()
{
	// 2500 / mPulse works too, but this is more obvious
	float msecPerBeat = mPulse * 24.0f;
	return (60000.0f / msecPerBeat);
}

int TempoMonitor::getSmoothTempo()
{
	return mSmoothTempo;
}

/**
 * If we are syncing to a device that does not send clocks when the
 * transport is stopped (I'm looking at you Ableton Live) when 
 * the transport stats again, TempoMonitor::clock will be called with
 * an abnormally long delta since the last clock.  We want to ignore this
 * delta so it doesn't throw the tempo smoother way out of line.  

 * At 60 BPM there is one beat per second or 24 MIDI clocks per second.
 * Each MIDI clock should ideally be 41.666r milliseconds apart.  This
 * will round to an average of 41 msec per clock.
 *
 * 30 PM would be 82 msec per clock.  15 BPM = 164 mpc,  5 BPM = 492 mpc.
 *
 * If we get a clock delta above 500 it is almost certainly because
 * the clocks have been paused and rseumed.  If we actually needed to 
 * support tempos under 5 BPM we could make this configurable, but it's
 * unlikely
 *
 * Ableton can also send out clocks very close together when it starts
 * which results in an extremely large tempo that then influences
 * the average for a few seconds. 120 BPM is 40.5 msec per clock, 240
 * is 20.25 msec, 480 is 10 msec.  Let's assume that anything under 5 msec
 * is noise and ignored.
 * 
 */
#define MAX_CLOCK_DELTA 500
#define MIN_CLOCK_DELTA 5

void TempoMonitor::clock(long msec, double juceTime)
{
    (void)juceTime;
    // trace time lag for awhile
    // juceTime is the timestamp from the MidiMessage which when coming
    // from a MidiInput the docs say:
    // "a value equivalent to (Time::getMillisecondCounter() / 1000.0)"
#if 0    
    if (timeTraceCount < 50) {
        juce::uint32 umsec = juce::Time::getMillisecondCounter();
        double dmsec = juce::Time::getMillisecondCounterHiRes();

        char buf[128];
        sprintf(buf, "%u %f %f", umsec, dmsec, juceTime);
        Trace(2, buf);
        timeTraceCount++;
    }
#endif
    
	if (mLastTime == 0) {
		// first one, wait for another
        Trace(3, "TempoMonitor: Clocks start at msec %ld\n", msec);
	}
	else if (msec < mLastTime) {
		// not supposed to go back in time, reset but leave last tempo
        Trace(2, "TemoMonitor: Clocks rewinding at msec %ld\n", msec);
		initSamples();
	}
	else {
		long delta = msec - mLastTime;

        if (delta > MAX_CLOCK_DELTA) {
            // drop this a level, it happens normally when using transports that
            // stop clocks in between stop/start
            Trace(3, "TempoMonitor: Ignoring random clock delta %ld\n", delta);
            initSamples();
        }
        else if (delta < MIN_CLOCK_DELTA) {
            // this is relatively unusual, see often during app startup
            // probably some clocks get queueued if a device is sending during startup
            Trace(2, "TempoMonitor: Ignoring clock burst delta %ld\n", delta);
            initSamples();
        }
        else {
            mTotal -= mSamples[mSample];
            mTotal += delta;
            mSamples[mSample] = delta;

            mSample++;
            if (mSample >= MIDI_TEMPO_SAMPLES)
              mSample = 0;

            if (mDivisor < MIDI_TEMPO_SAMPLES)
              mDivisor++;

            /*
              long total = 0;
              for (int i = 0 ; i < MIDI_TEMPO_SAMPLES ; i++)
              total += mSamples[i];
              if (total != mTotal)
              Trace(2, "Totals don't match %d %d\n", mTotal, total);
            */

            // maintain the average pulse width
            mPulse = (float)mTotal / (float)mDivisor;

            bool clockTrace = false;

            if (clockTrace)
              Trace(2, "TempoMonitor: Clock msec delta %ld total %ld divisor %ld width %ld (x1000)\n", 
                    (long)delta, (long)mTotal, (long)mDivisor, 
                    (long)(mPulse * 1000));

            // I played around with smoothing the pulse width but we have to 
            // be careful as this number needs at least 2 digits of precision
            // and probably 4.  Averaging seems to smooth it well enough.
            // And the tempo smoothing below keeps the display from jittering.

            // calculate temo
            float msecPerBeat = mPulse * 24.0f;
            float newTempo = (60000.0f / msecPerBeat);

            if (clockTrace)
              Trace(2, "TempoMonitor: Clock tempo (x100) %ld\n", 
                    (long)(newTempo * 1000));
            
            // Tempo jitters around by about .4 plus or minus the center
            // Try to maintain a relatively stable number for display
            // purposes.  
		
            // If we notice a jump larger than this, just go there
            // immediately rather than changing gradually.
            int tempoJumpThreshold = 10;

            // The number of times we need to see a jitter in one direction
            // to consider it a "trend" that triggers a tempo change in that
            // direction.  Started with 4 which works okay but it still
            // bounces quite a bit, at 120 BPM from Ableton get frequent
            // bounce between 120 and 119.9.
            // One full beat shold be enough, this would be a good thing
            // to expose as a tunable parameter
            int jitterTrendThreshold = 24;

            // remember that this is an integer 10x the actual float tempo
            int smoothTempo = mSmoothTempo;
            int itempo = (int)(newTempo * 10);
            int diff = itempo - mSmoothTempo;
            int absdiff = (diff > 0) ? diff : -diff;

            if (absdiff > tempoJumpThreshold) {
                smoothTempo = itempo;
                // reset jitter?
                mJitter = 0;
            }
            else if (diff > 0) {
                mJitter++;
                if (mJitter > jitterTrendThreshold) {
                    smoothTempo++; 
                }
            }
            else if (diff < 0) {
                mJitter--;
                if (mJitter < -jitterTrendThreshold) {
                    smoothTempo--;
                }
            }
            else {
                // stability moves it closer to the center?
                if (mJitter > 0)
                  mJitter--;
                else if (mJitter < 0)
                  mJitter++;
            }

            /*
              Trace(2, "MidiIn: tempo %ld (x100) diff %ld jitter %ld smooth %ld\n",
			  (long)(newTempo * 100),
			  (long)diff,
			  (long)mJitter,
			  (long)smoothTempo);
            */

            // this changes enough to be annoying to trace all the time
            bool smoothTempoTrace = false;

            if (smoothTempo != mSmoothTempo) {
                if (smoothTempoTrace)
                  Trace(2, "TempoMonitor: Tempo changing from %ld to %ld (x10)\n", 
                        (long)mSmoothTempo,
                        (long)smoothTempo);

                mSmoothTempo = smoothTempo;
                mJitter = 0;
            }
        }
    }
	mLastTime = msec;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
