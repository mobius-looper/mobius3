/**
 * Dig information out of the Juce model for passing host transport
 * status, and distill it into beats and a "unit length".
 *
 * In this domain, the ultimate goal of any time-based sync analyzer
 * is the derivation of the "unit length".  This is a length is samples
 * (frames) that reprents the smallest unit of audio content upon which
 * synchronized recordings are built.  All recordings made from the same
 * source will have the unit as a common factor.
 *
 * Minor fluctuations in tempo don't really matter as long as the unit length
 * derived from it remains the same.  This may cause "drift" which will be
 * compensated but the unit length remains constant until the tempo deviates
 * beyond a threshold that requires recalculation of a new unit length.
 
 * Tempo and ppqPosition drive everything.
 *
 * Tempo is usually specified by the host but is is not a hard requirement.
 *
 * ppqPosition is also technically optional, but I've never seen a host that doesn't
 * provide it, and don't care about those that don't.
 *
 * If tempo is provided, that will be used to derive the unit length.
 * ppqPosition will be verified to see if it is advancing at the same rate as the tempo
 * but it will be ignored.
 *
 * If tempo is not provided the the ppqPosition is used to measure the distance betwee
 * quarter note "beats" which then determines the unit length.
 *
 * ppqPosition is a floating point number that represents "the current play position in
 * units of quarter notes".  There is some ambiguity over how hosts implement the concepts
 * of "beats" and "quarter notes" and they are not always the same.  In 6/8 time, there are
 * six beats per measure and the eighth note gets one beat.  Unclear whether ppq means
 * "pulses per beat" which would be pulses per eights, or whether that would be adjusted
 * for quarter notes.   Will have to experiment with differnent hosts to see what they do.
 *
 * ppqPosition normally starts at 0.0f when the transport starts and increase
 * on each block.  We know a beat happens when the non-fractional part of this
 * number changes, for example going from 1.xxxxx on the last block to 2.xxxxxx
 * But note that the beat actually happened in the PREVIOUIS block, not the
 * block being received.  It is possible to use the sample rate to determine
 * whether the next beat MIGHT occur in the previous block and calculate a more
 * accurate buffer offset to where the beat actually is.
 *
 * The notion of where a "bar" is is not well defined.  Some hosts provide a user
 * specified time signature, and some don't.  Even when they do there are times
 * when Mobius users may want different bar lengths than what the host is advertising.
 * So determine of where bars are is left to higher levels.
 *
 * Although the unit length can be smaller than a "beat", in current practice they
 * are always the same thing.
 */
 
#include <JuceHeader.h>

#include "../util/Trace.h"

#include "SyncMaster.h"
#include "HostAnalyzerV2.h"

HostAnalyzerV2::HostAnalyzerV2()
{
}

HostAnalyzerV2::~HostAnalyzerV2()
{
    traceppq = true;
}

/**
 * If we're standalone, then AP will be nullptr and needs
 * to be checked on advance.
 */
void HostAnalyzerV2::initialize(juce::AudioProcessor* ap)
{
    audioProcessor = ap;
}

/**
 * Sample rate is expected to be an int, Juce gives
 * us a double, under what conditions would this be fractional?
 */
void HostAnalyzerV2::setSampleRate(int rate)
{
    sampleRate = rate;
}

/**
 * This must be called at the beginning of every audio block.
 * Though most internal code deals with MobiusAudioStream, we need more
 * than that exposes, so go directly to the juce::AudioProcessor and
 * don't you dare pass go.
 *
 * It is important that the blockSize be the full block size provided by the
 * host, not a partial sliced block segment that is used for pulse processing.
 *
 */
void HostAnalyzerV2::advance(int blockSize)
{
    bool transportChanged = false;
    bool tsigChanged = false;
    int initialUnit = unitLength;
    
    result.reset();

    if (audioProcessor != nullptr) {
        juce::AudioPlayHead* head = audioProcessor->getPlayHead();

        if (head != nullptr) {
            juce::Optional<juce::AudioPlayHead::PositionInfo> pos = head->getPosition();
            if (pos.hasValue()) {

                // Determine whether we started or stopped in this block
                bool newPlaying = pos->getIsPlaying();
                if (newPlaying != playing) {
                    playing = newPlaying;

                    if (playing) {
                        Trace(2, "HostAnalyzer: Start");
                        // assume start is always on a beat, valid?
                        // no, need to use the initial ppqPosition to orient the
                        // play frame
                        beat = -1;

                        // temporary: trace the next 10 blocks
                        traceppqFine = true;
                        ppqCount = 0;

                        // reset the derived tempo monitor
                        resetTempoMonitor();
                        result.started = true;
                    }
                    else {
                        Trace(2, "HostAnalyzer: Stop");
                        result.stopped = true;
                    }
                }
                
                // haven't cared about getIsLooping in the past but that might be
                // interesting to explore

                juce::Optional<double> bpm = pos->getBpm();
                if (bpm.hasValue()) {
                    ponderTempo(*bpm);
                }
                
                juce::Optional<juce::AudioPlayHead::TimeSignature> tsig = pos->getTimeSignature();
                if (tsig.hasValue()) {
                    if (tsig->numerator != timeSignatureNumerator ||
                        tsig->denominator != timeSignatureDenominator) {

                        timeSignatureNumerator = tsig->numerator;
                        timeSignatureDenominator = tsig->denominator;
                        tsigChanged = true;

                        Trace(2, "HostAnalyzer: Time signature %d / %d",
                              timeSignatureNumerator, timeSignatureDenominator);
                    }
                }

                // in the olden days samplePosition was used to detect whether or not
                // the transport was playing for a few hosts that didn't set some of the VST2
                // flags correctly, assuming that is no longer an issue, but here's
                // how to get it
                //double samplePosition = 0.0;
                //juce::Optional<int64_t> tis = pos->getTimeInSamples();
                //if (tis.hasValue()) samplePosition = (double)(*tis);

                // While ppqpos is technically optional, every host has it, and I'm
                // not seeing the need to adapt to those that don't

                juce::Optional<double> ppq = pos->getPpqPosition();
                if (ppq.hasValue()) {
                    beatEncountered = ponderPpq(*ppq, blockSize);
                }

                // old code never tried to use "bar" information from the host
                // because it was so unreliable as to be useless, things may have
                // changed by now.  Though Juce forum chatter suggests ProTools still doesn't
                // provide it.  Unlike beats, bars are more abstract and while we can
                // default to what the host provides, it is still necessary to allow the
                // user to define their own time signature independent of the host.
            }
        }
    }

    if (initialUnit != unitLength) {
        // the tempo was adjusted, this will have side
        // effects if application recordings were following this source
        // more to do here...
    }

    if (playing) {
        if (transportChanged)
          resync();
        advanceAudioStream(blockSize);
    }

    // do this last, deriveTempo needs to know what it is at the start
    // of the block, not the end
    streamTime += blockSize;
}

/**
 * When the transport starts after having been stopped, the last
 * captured stream and ppq position won't be valid, so begin again.
 */
void HostAnalyzerV2::resetTempoMonitor()
{
    lastPpq = 0;
    lastPpqStreamTime = 0;
}

void HostAnalyzerV2::traceFloat(const char* format, double value)
{
    char buf[128];
    snprintf(buf, sizeof(buf), format, value);
    Trace(2, buf);
}

/**
 * The host has given us an explicit tempo.
 */
void HostAnalyzerV2::ponderTempo(double newTempo)
{
    if (tempo != newTempo) {

        // tempo is allowed to fluctuate as long as it does not
        // change unit length which effectively rounds off the tempo
        // to a smaller resolution than a double float
        
        tempo = newTempo;

        int newUnit = tempoToUnit(tempo);

        if (newUnit != unitLength) {
            
            // the tempo changed enough to change the unit
            // here we could reuqire it change above a small threshold
            traceFloat("HostAnalyzer: New host tempo %f", tempo);
            Trace(2, "HostAnalyzer: Unit length %d", newUnit);
            unitLength = newUnit;
            // whenever the tempo changes the last data point for the
            // monitor will be invalid, so reset it so it starts seeing
            // the new tempo ppq width
            resetTempoMonitor();

            result.tempoChange = true;
            result.newUnitLength = newUnit;
        }
    }

    // from this point forward, the tempo is considered specified by the host
    // and jitter in the ppq advance won't override it
    tempoSpecified = true;
}

/**
 * Convert a tempo into a unit length.
 *
 * For drift correction it is better if the follower loop is a little
 * slower than the sync source so that the correction jumps it forward
 * rather than backward.  So when the float length has a fraction round
 * it up, making the unit longer, and hence the playback rate slower.
 *
 * There are a lot of calculations that work better if the unit length is
 * even, so if the initial calculation results in an odd number, add one.
 * Might be able to relax this part.
 */
int HostAnalyzerV2::tempoToUnit(double newTempo)
{
    int unit = 0;
    
    // the sample/frame length of one "beat" becomes the unit length
    // sampleRate / (bpm / 60)

    double rawLength = (double)sampleRate / (newTempo / 60.0);

    // it is generally better to round up rather than down
    // so that any drift corrections make the audio jump forward
    // rather than backward
    unit = (int)ceil(rawLength);
    if ((unit % 2) > 0) {
        //Trace(2, "HostAnalyzer: Evening up");
        unit++;
    }
    
    return unit;
}

/**
 * Examine the PPQ position on each block.
 * Returns true if there was a beat in this block and
 * sets the blockOffset.
 */
void HostAnalyzerV2::ponderPpq(double beatPosition, int blockSize)
{
    // if the transport is stopped, then the ppqPosition won't be advancing
    if (playing) {

        // monitor tempo changes, and return a useful number
        double beatsPerSample = deriveTempo(beatPosition, blockSize);
    
        // now the meat
        // attempt to find the location of the next beat start within this block
        // since ppqPosition doesn't roll it's integral part until after it happens
        int newBeat = (int)beatPosition;
        if (newBeat != beat) {
            // not expecting to get here with early detection
            Trace(1, "HostAnalyer: Missed a beat detection");
            result.sourceBeatDetected = true;
            result.sourceBeatOffset = 0;
            beat = newBeat;
        }
        else {
            // several ways to detect this, this is one
            double nextPpqPosition = beatPosition + (beatsPerSample * blockSize);
            int nextBeat = (int)nextPpqPosition;
            if (nextBeat != beat) {

                // the beat hanged in this block, try to locate the location it changed

                // the number of ppq units between the next beat integral and where we are now
                double ppqDelta = (double)nextBeat - beatPosition;

                if (lastPpq > 0.0f) {
                    // calculate the number of ppq units each frame in the prior block represented
                    // then use that to calculate the offset in this block
                    int lastBlockSize = streamTime - lastPpqSTreamTime;
                    double ppqFrameUnit = (beatPosition - lastPPq) / (double)lastBlockSize;
                    int blockOffset = (int)(ppqFrameUnit * ppqDelta);
                    if (blockOffset < blockSize) {
                        result.sourceBeatDetected = true;
                        result.sourceBeatOffset = blockOffset;
                    }
                    else {
                        // host is sending odd sized blocks, and the pulse will actually
                        // happen in the next one
                    }
                }
                else {
                    // don't have a reference point to calculate ppqFrameUnit
                    // this is very unusual, would have to come out of Stop one block
                    // before the start of the beat
                    // these are other ways to calculate this, but just let it be picked
                    // up on the next block which will be late, but drift will tend
                    // to even out
                        
                    // there are other ways to do this, but just let it get picked up
                }
            }
        }

        // if we found a beat, optional trace
        if (result.sourceBeatDetected) {
            if (traceppq) {
                char buf[128];
                snprintf(buf, sizeof(buf), "HostAnalyzer: Beat %f", (float)beatPosition);
                Trace(2, buf);
            }
        }
        else {
            if (traceppqFine && ppqCount < 10) {
                traceFloat("HostAnalyzer: PPQ %f", beatPosition);
                ppqCount++;
            }
        }
    }

    lastPpq = beatPosition;
    lastPpqStreamTime = streamTime;
}

/**
 * The host has not given us a tempo and we've started receiving ppqs.
 * Try to guess the tempo by watching a few of them.
 */
void HostAnalyzerV2::deriveTempo(double beatPosition, int blockSize)
{
    if (lastPpq > 0.0f) {

        double ppqAdvance = beatPosition - lastPpq;
        int sampleAdvance = streamTime - lastPpqStreamTime;

        // normally the block size
        if (sampleAdvance != blockSize) {
            Trace(2, "HostAnalyzer: Host is giving us random blocks");
        }
        
        double beatsPerSample = ppqAdvance / (double)sampleAdvance;
        double samplesPerBeat = 1 / beatsPerSample;
        double beatsPerSecond = (double)sampleRate / samplesPerBeat;
        double bpm = beatsPerSecond * 60.0f;

        if (tempo == 0.0f) {
            // never had a tempo
            traceFloat("HostAnalyzer: Derived tempo %f", bpm);
            tempo = bpm;
            unitLength = tempoToUnit(tempo);
            Trace(2, "HostAnalyzer: Unit length %d", unitLength);

            result.tempoChanged = true;
            result.newUnitLength = unitLength;
        }
        else if (tempoSpecified) {
            // We had a host provided tempo
            // Monitoring the beat width shouldn't be necessary since it's
            // up to the host to make them match, but for some it might be useful
            // to verify the ppq advance is happening as we expect.  The two tempos probably
            // won't be exact after a large number of fractional digits, but should be the
            // same out to around 4.  Since the end result is the unit length, this
            // is a reasonable amount of rounding.
            int derivedUnitLength = tempoToUnit(bpm);
            if (derivedUnitLength != unitLength) {
                // measuing the tempo over a single block has a small amount of jitter
                // which in testing resulted in an off by one on the unit length
                // e.g. 119.9999999999....  instead of 120.0 and with ceil() and evening
                // result in a unit length of 22051 instead of 22050
                // occassionlly see off by 2, 4 should suppress the warnings
                // it would be better to average the ppq advance out over several blocks
                // but can also just filter out small errors here
                int delta = abs(derivedUnitLength - unitLength);
                if (delta > 4) {
                    Trace(1, "HostAnalyzer: Host tempo does not match derived tempo");
                    traceFloat("Host: %f", tempo);
                    traceFloat("Derived: %f", bpm);
                }
                // since this is likely to happen frequently, need a governor on the
                // number of times we trace this and decide what if anything we do about it
            }
        }
        else {
            // we had previously derived a tempo
            // minor fluctuations are expected on each block, need to be ignoring
            // very minor changes after a few digits of precision
            
            // can use the same unit length rounding here
            int derivedUnitLength = tempoToUnit(bpm);
            if (derivedUnitLength != unitLength) {

                // similar jitter suppression, may want a higher threshold here though?
                // !! this really needs smoothing because that initial guess can be wrong
                int delta = abs(derivedUnitLength - unitLength);
                if (delta > 2) {
                    traceFloat("HostAnalyzer: New derived tempo %f", bpm);
                    Trace(2, "HostAnalyzer: Unit length %d", derivedUnitLength);
                    tempo = bpm;
                    unitLength = derivedUnitLength;
                    result.tempoChanged = true;
                    result.newUnitLength = unitLength;
                }
                
                // todo: if the length exceeds some threshold, resync
            }
        }
    }

}

//////////////////////////////////////////////////////////////////////
//
// Tracking Loop
//
//////////////////////////////////////////////////////////////////////

/**
 * Once the unit has been set, we need to flesh out a "loop" that
 * plays as the audio stream advances.  And compare the audio stream location
 * within that loop to another imaginary loop that is playing in time with
 * beat signals.  Due to rounding when the unit is calculated, they
 * can be very slightly off and may require periodic but rare drift adjustments.
 * 
 */
void HostAnalyzerV2::resync(double beatPosition, bool starting)
{
    // here is where it gets magic
    // you should not be assuming that when we start the transport
    // is exactly on a beat.  Ableton seems to do this but others might now
    streamFrame = 0;
    sourceFrame = 0;
    streamBeat = 0;
    streamBar = 0;
    streamLoop = 0;
    units = 0;
    unitCounter = 0;
}

/**
 * This is what actually generates sync pulses for the outside world.
 */
void HostAnalyzerV2::advanceAudioStream(int blockFrames)
{
    // start with the loop length being one "bar"
    int beatsPerBar = timeSignatureNumerator;
    if (beatsPerBar == 0)
      beatsPerBar = 4;

    int barsPerLoop = 1;

    // almost identical logic here to Transport

    if (playing) {

        streamFrame = streamFrame + blockFrames;
        if (streamFrame >= unitLength) {

            // a unit has transpired
            int blockOffset = streamFrame - unitLength;
            if (blockOffset > blockFrames || blockOffset < 0)
              Trace(1, "Transport: You suck at math");

            // effectively a frame wrap too
            streamFrame = blockOffset;

            units++;
            unitCounter++;

            if (unitCounter >= unitsPerBeat) {

                result.beatDetected = true;
                result.blockOffset = blockOffset;

                unitCounter = 0;
                streamBeat++;
                
                if (streamBeat >= beatsPerBar) {

                    streamBeat = 0;
                    streamBar++;

                    if (streamBar >= barsPerLoop) {

                        streamBar = 0;
                        streamLoop++;
                        
                        result.loop = true;
                    }
                    else {
                        result.bar = true;
                    }
                }
                else {
                    result.beat = true;
                }

            }
        }

        // do a similar advance into the imaginary loop that the
        // source beats are playing
        sourceFrame = sourceFrame + blockFrames;
        if (sourceFrame >= unitLength) {
            // "loop" 
                sourceFrame = sourceFrame - unitLength;
        }
    }

    // when the stream tracking loop reaches the loop point
    // that's as good a place as any to check drift
    if (result.loop) {

        // result.blockOffset has the location of the tracking loop
        // frame where the loop occurred
        // there will normally be a source beat near the same location
        drift = sourceFrame - streamFrame;
        // here is where need to look at which end of the unit we're on and calculate
        // the minimum length from each side
        Trace(2, "HostAnalyzer: Drift %d", drift);
    }
}

/**
 * At this point both the audio stream and the host stream have advanced.
 * If the audio stream hit a "loop" compare the loop start frame offset with
 * the expected beat offset from the host.
 */
void HostAnalyzerV2::checkDrift()
{
    
    
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
