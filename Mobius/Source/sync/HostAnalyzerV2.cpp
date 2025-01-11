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
 * host, not a partial sliced block segment that is used for track scheduling
 * around sync pulses.
 */
void HostAnalyzerV2::advance(int blockSize)
{
    int initialUnit = unitLength;
    bool tsigChanged = false;
    
    result.reset();

    if (audioProcessor != nullptr) {
        juce::AudioPlayHead* head = audioProcessor->getPlayHead();

        if (head != nullptr) {
            juce::Optional<juce::AudioPlayHead::PositionInfo> pos = head->getPosition();
            if (pos.hasValue()) {

                // If the host doesn't give us PPQ, then everything falls apart
                juce::Optional<double> ppq = pos->getPpqPosition();
                if (ppq.hasValue()) {

                    double beatPosition = *ppq;

                    // Track changes to the time signature
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

                    // Determine whether we started or stopped in this block
                    // in the olden days samplePosition was used to detect whether or not
                    // the transport was playing for a few hosts that didn't set some of the VST2
                    // flags correctly, assuming that is no longer an issue, but here's
                    // how to get it
                    //double samplePosition = 0.0;
                    //juce::Optional<int64_t> tis = pos->getTimeInSamples();
                    //if (tis.hasValue()) samplePosition = (double)(*tis);
                    
                    detectStart(pos->getIsPlaying(), beatPosition);
                
                    // haven't cared about getIsLooping in the past but that might be
                    // interesting to explore

                    // Adapt to a tempo change if the host provides one
                    juce::Optional<double> bpm = pos->getBpm();
                    if (bpm.hasValue())
                      ponderTempo(*bpm);

                    // Watch for host beat changes and detect tempo
                    ponderPpq(beatPosition, blockSize);

                    // old code never tried to use "bar" information from the host
                    // because it was so unreliable as to be useless, things may have
                    // changed by now.  Though Juce forum chatter suggests ProTools still doesn't
                    // provide it.  Unlike beats, bars are more abstract and while we can
                    // default to what the host provides, it is still necessary to allow the
                    // user to define their own time signature independent of the host.
                }
            }
        }
    }

    if (initialUnit != unitLength) {
        // the tempo was adjusted, this will have side
        // effects if application recordings were following this source
        // more to do here...
    }

    if (playing)
      advanceAudioStream(blockSize);

    // do this last, deriveTempo and DriftMonitor need to know what it is at the start
    // of the block, not the end
    audioStreamTime += blockSize;
}

//////////////////////////////////////////////////////////////////////
//
// Start and Stop
//
//////////////////////////////////////////////////////////////////////

/**
 * Called first during block analysis to determine when the host transport
 * starts and stops.
 *
 * The isPlaying flag comes from the juce::AudioStream PlayHead.
 */
void HostAnalyzerV2::detectStart(bool newPlaying, double beatPosition)
{
    if (newPlaying != playing) {
        
        playing = newPlaying;
        
        if (playing) {
            Trace(2, "HostAnalyzer: Start");
            result.started = true;

            drifter.orient();

            // some people use floor() any better?
            hostBeat = (int)beatPosition;
            
            double remainder = beatPosition - (double)hostBeat;

            if (remainder > 0) {
                // need to deal with this and set the unitPlayHead accordinaly
                Trace(1, "HostAnalyzer: Starting in the middle of a beat");
            }

            unitPlayHead = 0;
            normalizedBeat = hostBeat;

            // just start this over, if we're not following host
            // time signature, then this could get weird
            normalizedBar = 0;

            // this doesn't really matter, it's only for debugging
            normalizedLoop = 0;

            elapsedUnits = 0;
            unitCounter = 0;
            
            resetTempoMonitor();
            
            // temporary: trace the next 10 blocks
            traceppqFine = true;
            ppqCount = 0;
        }
        else {
            Trace(2, "HostAnalyzer: Stop");
            result.stopped = true;
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Tempo Analysis
//
//////////////////////////////////////////////////////////////////////

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

            setUnitLength(newUnit);
            
            // whenever the tempo changes the last data point for the
            // monitor will be invalid, so reset it so it starts seeing
            // the new tempo ppq width
            resetTempoMonitor();

            result.tempoChanged = true;
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
 * The host has not given us a tempo and we've started receiving ppqs.
 * Try to guess the tempo by watching a few of them.
 */
double HostAnalyzerV2::deriveTempo(double beatPosition, int blockSize)
{
    double beatsPerSample = 0.0f;
    
    if (lastPpq > 0.0f) {

        double ppqAdvance = beatPosition - lastPpq;
        int sampleAdvance = audioStreamTime - lastPpqStreamTime;

        // normally the block size
        if (sampleAdvance != blockSize) {
            Trace(2, "HostAnalyzer: Host is giving us random blocks");
        }

        // return this for ponderPpq
        beatsPerSample = ppqAdvance / (double)sampleAdvance;
        double samplesPerBeat = 1 / beatsPerSample;
        double beatsPerSecond = (double)sampleRate / samplesPerBeat;
        double bpm = beatsPerSecond * 60.0f;
        
        if (tempo == 0.0f) {
            // never had a tempo
            traceFloat("HostAnalyzer: Derived tempo %f", bpm);
            tempo = bpm;
            setUnitLength(tempoToUnit(tempo));
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
                    tempo = bpm;
                    setUnitLength(derivedUnitLength);
                    result.tempoChanged = true;
                    result.newUnitLength = unitLength;
                }
                
                // todo: if the length exceeds some threshold, resync
            }
        }
    }
    return beatsPerSample;
}

/**
 * If the unit length changes, the unit play position may need to wrap.
 */
void HostAnalyzerV2::setUnitLength(int newLength)
{
    if (newLength != unitLength) {

        Trace(2, "HostAnalyzer: Changing unit length %d", newLength);
        unitLength = newLength;

        // !! there is more to do here
        // if this wraps is that a "beat", what about bar boundary adjustments
        unitPlayHead = (int)(unitPlayHead / unitLength);
    }
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

//////////////////////////////////////////////////////////////////////
//
// PPQ Analysis
//
//////////////////////////////////////////////////////////////////////

/**
 * Examine the PPQ position on each block.
 *
 * This is where we detect host beat changes, determine their offset
 * into the current audio block, and advance the host beat stream in
 * the DriftMonitor.
 *
 */
void HostAnalyzerV2::ponderPpq(double beatPosition, int blockSize)
{
    // if the transport is stopped, then the ppqPosition won't be advancing
    if (playing) {
        int startingBeat = hostBeat;

        // monitor tempo changes, and return a useful number
        double beatsPerSample = deriveTempo(beatPosition, blockSize);
    
        // now the meat
        // attempt to find the location of the next beat start within this block
        // since ppqPosition doesn't roll it's integral part until after it happens
        int newBeat = (int)beatPosition;
        if (newBeat != hostBeat) {
            // not expecting to get here with early detection
            Trace(1, "HostAnalyer: Missed a beat detection");
            hostBeat = newBeat;
            drifter.addSourceBeat(0);
        }
        else {
            // several ways to detect this, this is one
            double nextPpqPosition = beatPosition + (beatsPerSample * blockSize);
            int nextBeat = (int)nextPpqPosition;
            if (nextBeat != hostBeat) {
                // the beat happened in this block, try to locate the location it changed

                // the number of ppq units between the next beat integral and where we are now
                double ppqDelta = (double)nextBeat - beatPosition;

                if (lastPpq > 0.0f) {
                    // calculate the number of ppq units each frame in the prior block represented
                    // then use that to calculate the offset in this block
                    int lastBlockSize = audioStreamTime - lastPpqStreamTime;
                    double ppqFrameUnit = (beatPosition - lastPpq) / (double)lastBlockSize;
                    int blockOffset = (int)(ppqFrameUnit * ppqDelta);
                    
                    if (blockOffset < blockSize) {
                        hostBeat = nextBeat;
                        drifter.addSourceBeat(blockOffset);
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
                    //
                    // there are other ways to do this, but just let it get picked up
                    // late on the next block
                }
            }
        }

        // if we found a beat, optional trace
        if (startingBeat != hostBeat) {
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
    lastPpqStreamTime = audioStreamTime;
}

//////////////////////////////////////////////////////////////////////
//
// Normalized Beat Generation
//
//////////////////////////////////////////////////////////////////////

/**
 * This is what actually generates sync pulses for the outside world.
 *
 * As blocks in the audio stream come in, a "play head" within the
 * syncronization unit is advanced as if it were a short loop.
 * When the play head crosses the loop boundary, a beat is generated,
 * and this cascades into advancing bar and loop counters.
 *
 * The determination of bar boundaries needs more options, at the moment
 * it just counts beats from the beginning of the Start Point.
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

        unitPlayHead = unitPlayHead + blockFrames;
        if (unitPlayHead >= unitLength) {

            // a unit has transpired
            int blockOffset = unitPlayHead - unitLength;
            if (blockOffset > blockFrames || blockOffset < 0)
              Trace(1, "Transport: You suck at math");

            // effectively a frame wrap too
            unitPlayHead = blockOffset;

            elapsedUnits++;
            unitCounter++;

            if (unitCounter >= unitsPerBeat) {

                result.beatDetected = true;
                result.blockOffset = blockOffset;

                unitCounter = 0;
                normalizedBeat++;
                
                if (normalizedBeat >= beatsPerBar) {

                    normalizedBeat = 0;
                    normalizedBar++;
                    result.bar = true;

                    if (normalizedBar >= barsPerLoop) {

                        normalizedBar = 0;
                        normalizedLoop++;
                        
                        result.loop = true;
                    }
                }

            }
        }
    }

    // if we found a beat, tell the drift monitor
    if (result.beatDetected)
      drifter.addNormalizedBeat(result.blockOffset);

    // when the stream tracking loop reaches the loop point
    // that's as good a place as any to check drift
    if (result.loop) {

        int drift = drifter.getDrift();
        
        Trace(2, "HostAnalyzer: Drift %d", drift);
    }

    drifter.advance(blockFrames);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
