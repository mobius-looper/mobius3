
#include <JuceHeader.h>

#include "../util/Trace.h"

#include "SyncMaster.h"
#include "HostAnalyzer.h"

HostAnalyzer::HostAnalyzer()
{
}

HostAnalyzer::~HostAnalyzer()
{
}

/**
 * If we're standalone, then AP will be nullptr and needs
 * to be checked on advance.
 */
void HostAnalyzer::initialize(juce::AudioProcessor* ap)
{
    audioProcessor = ap;
}

/**
 * Sample rate is expected to be an int, Juce gives
 * us a double, under what conditions would this be fractional?
 */
void HostAnalyzer::setSampleRate(int rate)
{
    sampleRate = rate;
}

/**
 * If we couldn't build one, return nullptr.
 * Caller uses this to know if we're a plugin or not.
 */
HostAudioTime* HostAnalyzer::getAudioTime()
{
    if (audioProcessor != nullptr)
      return &audioTime;
    else
      return nullptr;
}

/**
 * Called at the beginning of each audio block to convert state
 * from the AudioPlayHead into the AudioTime model used
 * by SyncMaster.
 *
 * See HostSyncState.cpp for more on the history of this.
 *
 * We start by feeding the state from the AudioHead through the
 * HostSyncState object which does analysis.  The results of that
 * analysis are then deposited in an AudioTime that will be be used
 * by SyncMaster and Pulsator.
 */
void HostAnalyzer::advance(int blockSize)
{
    if (audioProcessor != nullptr) {
        juce::AudioPlayHead* head = audioProcessor->getPlayHead();

        if (head != nullptr) {
            juce::Optional<juce::AudioPlayHead::PositionInfo> pos = head->getPosition();
            if (pos.hasValue()) {

                bool isPlaying = pos->getIsPlaying();
            
                // haven't cared about getIsLooping in the past but that might be
                // interesting to explore

                // updateTempo needs: sampleRate, tempo, numerator, denominator
                double tempo = 0.0;
                juce::Optional<double> bpm = pos->getBpm();
                if (bpm.hasValue()) tempo = *bpm;
            
                int tsigNumerator = 0;
                int tsigDenominator = 0;
                juce::Optional<juce::AudioPlayHead::TimeSignature> tsig = pos->getTimeSignature();
                if (tsig.hasValue()) {
                    tsigNumerator = tsig->numerator;
                    tsigDenominator = tsig->denominator;
                }

                //syncState.updateTempo(sampleRate, tempo, tsigNumerator, tsigDenominator);
                syncState.updateTempo(sampleRate, tempo, tsigNumerator, tsigDenominator);

                // advance needs: frames, samplePosition, beatPosition,
                // transportChanged, transportPlaying
            
                // transportChanged is an artifact of kVstTransportChanged in VST2
                // and CallHostTransportState in AUv1 that don't seem to exist in
                // VST3 and AU3
                // so we can keep using the old HostSyncState code, derive transportChanged by
                // comparing isPlaying to the last value
                //bool transportChanged = isPlaying != syncState.isPlaying();

                // samplePosition was only used for transport detection in old hosts
                // that didn't support kVstTransportChanged, shouldn't need that any more
                // but we can pass it anyway.  For reasons I don't remember samplePosition
                // was a double, but getTimeInSamples is an int, shouldn't matter
                double samplePosition = 0.0;
                juce::Optional<int64_t> tis = pos->getTimeInSamples();
                if (tis.hasValue()) samplePosition = (double)(*tis);
            
                // beatPosition is what is called "ppq position" everywhere else
                double beatPosition = 0.0;
                juce::Optional<double> ppq = pos->getPpqPosition();
                if (ppq.hasValue()) beatPosition = *ppq;

                if (traceppq) {
                    int lastq = (int)lastppq;
                    int newq = (int)beatPosition;
                    if (lastq != newq)
                      Trace(2, "HostAnalyzer: beat %d", newq);
                    lastppq = beatPosition;
                }

                // HostSyncState never tried to use "bar" information from the host
                // because it was so unreliable as to be useless, things may have
                // changed by now.  It will try to figure that out it's own self,
                // would be good to verify that it matches what Juce says...
            
                //syncState.advance(blockSize, samplePosition, beatPosition,
                //transportChanged, isPlaying);
            
                syncState.advance(blockSize, isPlaying, samplePosition, beatPosition);

                // now dump that mess into an AudioTime for Mobius
                //syncState.transfer(&audioTime);
                syncState.transfer(&audioTime);
            }
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
