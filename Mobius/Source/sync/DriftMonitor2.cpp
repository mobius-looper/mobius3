/**
 * The tricky part here is knowing when to compare the stream times
 * to calculate drift.
 *
 * Each stream is advancing at it's own rate, and either of them may be more than one beat
 * ahead of or beind the other.
 *
 */

#include "DriftMonitor.h"

void DriftMonitor2::orient()
{
    streamTime = 0;
    hostBeat = 0;
    hostBeatTime = 0;
    normalizedBeat = 0;
    normalizedBeatTime = 0;
    drift = 0;
}

void DriftMonitor2::sourceBeat(int blockOffset)
{
    sourceBeat++;
    sourceBeatTime = streamTime + blockOffset;

    if (sourceBeat <= normalizedBeat) {
        // I was behind calculate drift
        drift = normalizedBeatTime - sourceBeatTime;
    }
}

void DriftMonitor2::normalizedBeat(int blockOffset)
{
    int normalizedBeat++;
    int normalizedBeatTime = streamTime + blockOffset;

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
