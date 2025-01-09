
#include "../util/Trace.h"

#include "DriftMonitor.h"

DriftMonitor::DriftMonitor()
{
}

DriftMonitor::~DriftMonitor()
{
}

/**
 * Thurns out we don't really need this unless
 * we put more logic down here.
 */
void DriftMonitor::setSampleRate(int rate)
{
    sampleRate = rate;
}

void DriftMonitor::setLoopFrames(int f)
{
    loopFrames = f;
    resync();
}

void DriftMonitor::setPulseFrames(int f)
{
    pulseFrames = f;
    resync();
}

int DriftMonitor::getDrift()
{
    return drift;
}

void DriftMonitor::resync()
{
    syncing = true;
}

/**
 * Advance the audio cursor.
 * Frames is either the full block width if a pulse was not encountered
 * in this block, or the offset to the pulse if one was encountered.
 *
 * Note that it is important to only include the frames UP TO the pulse.
 * If you include the entire block then the audio cursor will be a little
 * ahead of the pulse frame but this does not imply drift.
 */
bool DriftMonitor::advance(int frames)
{
    bool looped = false;
    
    audioFrame += frames;
    if (audioFrame >= loopFrames) {
        audioFrame = audioFrame - loopFrames;
        looped = true;
        loops++;
    }
    return looped;
}

/**
 * If we're syncing, initialize the location state on the first pulse,
 * normally after any kind of tempo change or stop/start.
 */
void DriftMonitor::pulse()
{
    pulseCount++;
       
    if (syncing) {
        Trace(2, "DriftMonitor: Resyncing with loopFrames %d and pulseFrames %d",
              loopFrames, pulseFrames);
        audioFrame = 0;
        pulseFrame = 0;
        loops = 0;
        drift = 0;
        syncing = false;
    }
    else {
        // the width of one pulse
        pulseFrame += pulseFrames;

        if (pulseFrame > loopFrames) {
            pulseFrame = pulseFrame - loopFrames;

            // on each pulse loop, calculate drift
            drift = audioFrame - pulseFrame;

            // todo: keep a running average like we do for TempoSmoother?
        }
    }
}

float DriftMonitor::advance(int blockFrames, int pulseOffset)
{
    float adjust = 0.0f;
    bool looped = false;
    
    if (pulseOffset < 0) {
        looped = advance(blockFrames);
    }
    else {
        int framesUpToPulse = blockFrames - pulseOffset;
        looped = advance(framesUpToPulse);
        pulse();
    }

    if (looped)
      adjust = checkDrift();

    return adjust;
}

/**
 * There are a any number of algorithms to compensate for drift.
 * Here's a swag...
 *
 * Ultimately when drift exceeds a threshold the pulse or audio rate needs to
 * increase or decreate by a small amount.  The calculations here assume
 * we're adjusting the pulse rate which works for the Transport.
 *
 * If this monitor is being used for Host or MIDI, then negate it to impact
 * the other side.
 *
 * This could be a lot smarter about things by keeping running averages and things.
 * In early testing a .1 error produced rapid drift, so let's start the adjustments
 * at .01.
 *
 * I don't know the proper math term for this, but we can enter a sort of "osscillation"
 * where any correction will overshoot and go in the opposite direction, which tnen
 * needs compensation by a smaller amount, and the effective compensation until
 * the compensation stabilizes and ideally the drift stays at zero.  There ought to
 * be a better way to calculate this by measuring rates of change, but my brain
 * doesn't work well.
 *
 * After the initial adjustment, start monitoring and wait for the drift to reach zero.
 * Then back off on the adjustment by half of the current amount.
 * If the drift stays constant for some number of checkpoints, we can stop adjusting.
 *
 * Note that for Transport, drift really represents a miscalculation in the original tempo
 * and since we are controlling it, these kinds of shenanigans should not be necessary.
 * It will be rare but more likely for Host drift, and very likely for MIDI drift.
 *
 * If the user is deliberately fiddling with the host/midi tempo then drift can become
 * extreme and at some point we need to stop trying to compensate.  If audio drift is
 * implemented by playback rate adjustments, this will start having noticeable changes
 * in pitch.
 */
float DriftMonitor::checkDrift()
{
    float adjust = 0.0f;

    // drift is audioFrame - pulseFrame so if the drift is negative pulses are too
    // fast and the adjust must be negative to slow it down

    Trace(2, "DriftMonitor: Drift %d", drift);
    
    int absdrift = ((drift < 0) ? -drift : drift);
    if (absdrift > 1000) {
        Trace(1, "DriftMonitor: Drift threshold exceeded %d", drift);

        adjust = 0.05f;
#if 0
        if (lastAdjustPulse > 0) {

            // check to see if the last adjustment had any effect
            // simce there is always audio block jitter you usually need to wait
            // a few checkpoints to see a trend, this is where it would be better
            // to keep a running drift average
            int abslast = ((lastAdjustDrift < 0) ? -lastAdjustDrift : lastAdjustDrift);
            if (absdrift != abslast) {
                // changes are happening
            }
        }
#endif        

        if (drift < 0)
          adjust = -adjust;

        Trace(2, "DriftMonitor: Adjust %d", (int)(adjust * 1000));
        lastAdjustPulse = pulseCount;
        lastAdjustDrift = drift;
        lastAdjust = adjust;
    }
    
    return adjust;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
