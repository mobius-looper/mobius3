/**
 * Dig information out of the Juce model for passing host transport
 * status, and distill it into Pulses.
 *
 * ppqPosition drives everything.  It is a floating point number that represents
 * "the current play position in units of quarter notes".  There is some ambiguity
 * over how hosts implement the concepts of "beats" and "quarter notes" and they
 * are not always the same.  In 6/8 time, there are six beats per measure and the
 * eighth note gets one beat.  Unclear whether ppq means "pulses per beat" which
 * would be pulses per eights, or whether that would be adjusted for quarter notes.
 * Will have to experiment with differnent hosts to see what they do.
 *
 * ppqPosition normally starts at 0.0f when the transport starts and increase
 * on each block.  We know a beat happens when the non-fractional part of this
 * number changes, for example going from 1.xxxxx on the last block to 2.xxxxxx
 * But note that the beat actually happened in the PREVIOUIS block, not the
 * block being received.  It would be possible to use the sample rate to determine
 * whether the next beat MIGHT occur in the previous block and calculate a more
 * accurate buffer offset to where the beat actually is.  But this is fraught with
 * round off errors and edge conditions.
 *
 * The simplest thing is to do beat detection at the beginning of every block
 * when the integral value ppqPosition changes.  This in effect quantizes beats
 * to block boundaries, and makes Mobius a little late relative to the host.
 * With small buffer sizes, this difference is not usually noticeable audibly.
 
 *
 */
 
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
 * This must be called at the beginning of every audio block.
 * Though most internal code deals with MobiusAudioStream, we need more
 * than that exposes, so go directly to the juce::AudioProcessor and
 * don't you dare pass go.
 */
void HostAnalyzer::advance(int blockSize)
{
    pulse.reset();
    
    bool transportChanged = false;
    bool tempoChanged = false;
    bool tsigChanged = false;
    bool beatChanged = false;
    bool beatOffset = 0;
    int newBeat = 0;
    
    if (audioProcessor != nullptr) {
        juce::AudioPlayHead* head = audioProcessor->getPlayHead();

        if (head != nullptr) {
            juce::Optional<juce::AudioPlayHead::PositionInfo> pos = head->getPosition();
            if (pos.hasValue()) {

                bool newPlaying = pos->getIsPlaying();
                if (newPlaying != playing) {
                    playing = newPlaying;
                    transportChanged = true;
                }
                
                // haven't cared about getIsLooping in the past but that might be
                // interesting to explore

                // Juce gives us a double here, everything else has been using floats
                // shouldn't be important
                juce::Optional<double> bpm = pos->getBpm();
                if (bpm.hasValue()) {
                    float newTempo = (float)(*bpm);
                    if (tempo != newTempo) {
                        tempo = newTempo;
                        tempoChanged = true;
                    }
                }

                juce::Optional<juce::AudioPlayHead::TimeSignature> tsig = pos->getTimeSignature();
                if (tsig.hasValue()) {
                    if (tsig->numerator != timeSignatureNumerator ||
                        tsig->denominator != timeSignatureDenminator) {

                        timeSignatureNumerator = tsig->numerator;
                        timeSignatureDenominator = tsig->denominator;
                        tsigChanged = true;
                    }
                }

                // in the olden days samplePosition was used to detect whether or not
                // the transport was playing for a few hosts that didn't set some of the VST2
                // flags correctly, assuming that is no longer an issue, but here's
                // how to get it
                //double samplePosition = 0.0;
                //juce::Optional<int64_t> tis = pos->getTimeInSamples();
                //if (tis.hasValue()) samplePosition = (double)(*tis);
            
                // beatPosition is what is called "ppq position" everywhere else
                // this could also be used to detect transport play/stop
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

                // old code never tried to use "bar" information from the host
                // because it was so unreliable as to be useless, things may have
                // changed by now.  It will try to figure that out it's own self,
                // would be good to verify that it matches what Juce says...

                // now the meat
                // experiment with two methods, the "detect late" "detect early"
                bool detectLate = true;
                if (detectLate) {
                    int newBeat = (int)beatPosition;
                    if (lastBeat != beat) {
                        beat = newBeat;
                        beatChanged = true;
                    }
                    // it was behind us so process it at the beginning
                    beatOffset = 0;
                }
                else {
                    
                        

                        
                


                
            
            }
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
