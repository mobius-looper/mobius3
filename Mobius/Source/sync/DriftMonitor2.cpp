/**
 * The tricky part here is knowing when to compare the stream times
 * to calculate drift.
 *
 * Each stream is advancing at it's own rate, and either of them may be more than one beat
 * ahead of or beind the other.
 *
 */

#include "DriftMonitor2.h"

void DriftMonitor2::orient()
{
    streamTime = 0;
    sourceBeat = 0;
    sourceBeatTime = 0;
    normalizedBeat = 0;
    normalizedBeatTime = 0;
    drift = 0;
}

void DriftMonitor2::addSourceBeat(int blockOffset)
{
    sourceBeat++;
    sourceBeatTime = streamTime + blockOffset;

    if (sourceBeat <= normalizedBeat) {
        // I was behind calculate drift
        drift = normalizedBeatTime - sourceBeatTime;
    }
}

void DriftMonitor2::addNormalizedBeat(int blockOffset)
{
    normalizedBeat++;
    normalizedBeatTime = streamTime + blockOffset;

    if (normalizedBeat <= sourceBeat) {
        // I was behind calculate drift
        drift = normalizedBeatTime - sourceBeatTime;
    }
}

void DriftMonitor2::advance(int blockSize)
{
    streamTime += blockSize;
}

int DriftMonitor2::getDrift()
{
    return drift;
}
