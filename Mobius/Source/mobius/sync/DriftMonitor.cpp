/**
 * This doesn't do much, but it's a good place to keep more analysis
 * like source beat length averaging which is effectively the same
 * as tempo smoothing.
 *
 * Also should be measuring the rate of change.  If the drift accumulates
 * slowly, it is a small difference in the two tempos and is suitable
 * for occasional correction of the audio loops.
 *
 * If the drift accumulates repidly then it is more likely a tempo change
 * in the source that should cause recalculation of the unit length.
 * And a disconnect betweeen the following loops and the sync source.
 */

#include <cmath>
#include "../../util/Trace.h"

#include "DriftMonitor.h"

/**
 * This assumes orientation will happen at the beginning of a host beat.
 * If it doesn't the first beat will be quite off and should not factor into
 * drift.  We can either ignore the first beat and start tracking on the next one
 * or somehow calculate where the host actually is in the audio stream and
 * seed the streamTime to compensate for that.
 */
void DriftMonitor::orient(int unitLength)
{
    streamTime = 0;
    normalizedUnit = unitLength;
    lastBeatTime = 0;
    drift = 0;
}

void DriftMonitor::advanceStreamTime(int blockSize)
{
    streamTime += blockSize;
}

/**
 * Record a beat from the sync source and calculate drift
 * away from the normalizedUnit.
 *
 * If the beat length is greater than the normalized unit length
 * the beat came in slower than expected.  The normalied "loop"
 * is playing faster than the source beats and the drift is positive.
 */
void DriftMonitor::addBeat(int blockOffset)
{
    // unit may be zero on startup with hosts that don't give
    // an initial ttransport tempo and before we start deriving the
    // tempo from beat distance
    if (normalizedUnit > 0) {
        int beatTime = streamTime + blockOffset;
        int beatLength = beatTime - lastBeatTime;

        // this can be up to one block size, but FL Studio often bounces around
        // above that
        int delta = beatLength - normalizedUnit;
        if (abs(delta) > 2048)
          Trace(1, "DriftMonitor: Drift starting to get out of hand %d", delta);
        
        drift += delta;

        lastBeatTime = beatTime;
    }
}

int DriftMonitor::getDrift()
{
    return drift;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
