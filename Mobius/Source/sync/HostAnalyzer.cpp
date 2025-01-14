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
#include "../model/SyncState.h"

#include "HostAnalyzer.h"

//////////////////////////////////////////////////////////////////////
//
// Configuration
//
//////////////////////////////////////////////////////////////////////

HostAnalyzer::HostAnalyzer()
{
}

HostAnalyzer::~HostAnalyzer()
{
    traceppq = true;
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
 * Only refresh the things we control, Pulsator adds the rest
 */
void HostAnalyzer::refreshState(SyncState& state)
{
    state.receiving = playing;
    
    // this will normally be overridden by Pulsator
    state.beat = elapsedBeats;
}

//////////////////////////////////////////////////////////////////////
//
// SyncAnalyzer Properties
//
//////////////////////////////////////////////////////////////////////

bool HostAnalyzer::isRunning()
{
    return playing;
}

/**
 * hasNativeBeat returns true in the .h
 */
int HostAnalyzer::getNativeBeat()
{
    return hostBeat;
}

/**
 * Punt for now, I suppose if an interesting combination of Ableton, Logic,
 * Mainstage, or Bitwig support bars, we could provide this.
 */
bool HostAnalyzer::hasNativeBar()
{
    return false;
}
    
int HostAnalyzer::getNativeBar()
{
    return 0;
}
    
int HostAnalyzer::getElapsedBeats()
{
    return elapsedBeats;
}
    
bool HostAnalyzer::hasNativeTimeSignature()
{
    return timeSignatureSpecified;
}
    
int HostAnalyzer::getNativeBeatsPerBar()
{
    return timeSignatureDenominator;
}
    
float HostAnalyzer::getTempo()
{
    return (float)tempo;
}
    
int HostAnalyzer::getUnitLength()
{
    return unitLength;
}

int HostAnalyzer::getDrift()
{
    return drifter.getDrift();
}

//////////////////////////////////////////////////////////////////////
//
// Analysis
//
//////////////////////////////////////////////////////////////////////

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
void HostAnalyzer::analyze(int blockSize)
{
    int initialUnit = unitLength;
    
    result.reset();

    // null if we're not a plugin
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
                        timeSignatureSpecified = true;
                        if (tsig->numerator != timeSignatureNumerator ||
                            tsig->denominator != timeSignatureDenominator) {

                            timeSignatureNumerator = tsig->numerator;
                            timeSignatureDenominator = tsig->denominator;

                            result.timeSignatureChanged = true;

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
                    if (bpm.hasValue()) {
                        ponderTempo(*bpm);
                    }

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
        result.tempoChanged = true;
    }

    if (playing) {
        drifter.advanceStreamTime(blockSize);
        advanceAudioStream(blockSize);
    }

    // do this last, deriveTempo and DriftMonitor need to know what it is at the start
    // of the block, not the end
    lastAudioStreamTime = audioStreamTime;
    audioStreamTime += blockSize;
}

SyncSourceResult* HostAnalyzer::getResult()
{
    return &result;
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
void HostAnalyzer::detectStart(bool newPlaying, double beatPosition)
{
    if (newPlaying != playing) {
        
        playing = newPlaying;
        
        if (playing) {
            Trace(2, "HostAnalyzer: Start");
            result.started = true;

            drifter.orient(unitLength);

            // some people use floor() any better?
            hostBeat = (int)beatPosition;
            
            double remainder = beatPosition - (double)hostBeat;

            if (remainder > 0) {
                // need to deal with this and set the unitPlayHead accordingly
                Trace(1, "HostAnalyzer: Starting in the middle of a beat");
            }
            else {
                // this can be considered a beat
                result.beatDetected = true;
                result.blockOffset = 0;
            }

            unitPlayHead = 0;
            elapsedUnits = 0;
            unitCounter = 0;
            elapsedBeats = 0;
            lastBeatTime = 0;
            
            resetTempoMonitor();
            
            // temporary: trace the next 10 blocks
            traceppqFine = true;
            ppqCount = 0;
        }
        else {
            Trace(2, "HostAnalyzer: Stop");
            result.stopped = true;

            // Stop is not considered a beat, application needs
            // to decide what to do in this case if a recording is in progress
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Tempo
//
//////////////////////////////////////////////////////////////////////

/**
 * The host has given us an explicit tempo.
 */
void HostAnalyzer::ponderTempo(double newTempo)
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
int HostAnalyzer::tempoToUnit(double newTempo)
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
        // hmm, evening after ceil results in a higher drift
        // so even though the unit is technically a little smaller that feels
        // better than being more larger
        
        //Trace(2, "HostAnalyzer: Evening up");
        //unit++;
        unit = (int)rawLength;

        // unlikely this would still be odd but could be if rawLength had no fraction
        if ((unit % 2) > 0)
          unit++;
    }
     
    return unit;
}

/**
 * If the unit length changes, the unit play position may need to wrap.
 */
void HostAnalyzer::setUnitLength(int newLength)
{
    if (newLength != unitLength) {

        Trace(2, "HostAnalyzer: Changing unit length %d", newLength);
        unitLength = newLength;

        // !! there is more to do here
        // if this wraps is that a "beat", what about bar boundary adjustments
        unitPlayHead = (int)(unitPlayHead / unitLength);

        // !! drift monitor needs to know this
        // orient assumes we're exactly on a beat, which is the case if
        // we're doing tempo derivation by watching beats, but not necessarily
        // if the user is changing the host tempo while it plays
        // more to do here
        drifter.orient(unitLength);
    }
}

/**
 * When the transport starts after having been stopped, the last
 * captured stream and ppq position won't be valid, so begin again.
 */
void HostAnalyzer::resetTempoMonitor()
{
    lastPpq = 0;
}

void HostAnalyzer::traceFloat(const char* format, double value)
{
    char buf[128];
    snprintf(buf, sizeof(buf), format, value);
    Trace(2, buf);
}

//////////////////////////////////////////////////////////////////////
//
// Beats
//
//////////////////////////////////////////////////////////////////////

/**
 * For reasons I've never understood, ppqPosition at the start of each block has
 * the integral beat number plus the portion of the beat that occurred after the beat.
 * The sample position of the beat actually happened in the previous block.
 *
 * When attempting to locate beats in the audio stream, it's best if you can try to
 * locate them in the block they actually happened.  The only way I see to do that
 * is to guess by adding some amount to the ppqPosition at the start of each block
 * and seeing if the integral beat number increaeses.
 *
 * To do that we need the amount of ppqPosition that will be added for each sample.
 * In other words beatsPerSample, which is typically a very small floating poitn
 * fraction.
 *
 * This number is dependent on the tempo which usually does not change, but since it can
 * it's always a guess.  I suppose if the host is providing the tempo we could use
 * that instead and work down from there rather than assuming the beatsPerSample
 * in the last block will remain the same.   Any errors would be very slight.
 */
double HostAnalyzer::getBeatsPerSample(double currentPpq, int currentBlockSize)
{
    double beatsPerSample = 0.0f;

    // must know both the change in ppqPosition since the last block
    // and the length of the last block
    // if we're just starting, have to wait at least one block
    
    if (lastPpq > 0.0f) {

        double ppqAdvance = currentPpq - lastPpq;
        int sampleAdvance = audioStreamTime - lastAudioStreamTime;

        // normally the block size
        // this isn't that important but I'd like to know
        if (sampleAdvance != currentBlockSize) {
            Trace(2, "HostAnalyzer: Host is giving us random blocks");
        }

        beatsPerSample = ppqAdvance / (double)sampleAdvance;

        // alternate method working from the current tempo
        // assumes the host is giving us an accurate tempo and is advancing
        // ppq correctly
        if (tempoSpecified) {

            double samplesPerBeat = (double)sampleRate / (tempo / 60.0f);

            double tbps = 1.0f / samplesPerBeat;

            // this hit every time with trace
            // HostAnalyzer: PPQ advance method 0.000045
            // HostAnalyzer: Tempo method 0.000045
            //
            // so the difference must be way down in the noise
#if 0
            if (tbps != beatsPerSample) {
                // this is another one that is likely to happen often,
                // so put a constraint on it
                Trace(2, "HostAnalyzer: beatsPerSample mismatch");
                traceFloat("HostAnalyzer: PPQ advance method %f", beatsPerSample);
                traceFloat("HostAnalyzer: Tempo method %f", tbps);
            }
#endif
            (void)samplesPerBeat;
            (void)tbps;
        }
    }
    
    return beatsPerSample;
}

/**
 * Examine the PPQ position on each block.
 *
 * This is where we detect host beat changes, determine their offset
 * into the current audio block, and advance the host beat stream in
 * the DriftMonitor.
 *
 * !! What this will not do right is handle hosts that "loop" like FL Studio.
 * There the next beat number may be less than the current beat, and this
 * is often expected to become a "bar" for synchronization.  Once we've sent the
 * beat pulse out though we can't retroactively change the beat number so the host
 * can check it.  This may require a new result flag "transportLooped" or something
 * that can be used as a deferred indicator to signal a bar boundary.  The tricky part
 * is that we've already consumed (blocksize - blockOffset) samples of time that would
 * need to be added to the end of the synchronized recording to make it match the unitLength.
 * Or I suppose we could just round up the ending to compensate.
 */
void HostAnalyzer::ponderPpq(double beatPosition, int blockSize)
{
    // if the transport is stopped, then the ppqPosition won't be advancing
    if (playing) {
        int startingBeat = hostBeat;
        int blockOffset = 0;
        
        // important number for beat position and tempo guessing
        double beatsPerSample = getBeatsPerSample(beatPosition, blockSize);

        // monitor tempo changes
        deriveTempo(beatsPerSample);
    
        // now the meat
        // attempt to find the location of the next beat start within this block
        // since ppqPosition doesn't roll it's integral part until after it happens
        int newBeat = (int)beatPosition;
        if (newBeat != hostBeat) {
            // not expecting to get here with early detection
            Trace(1, "HostAnalyer: Missed a beat detection");
            hostBeat = newBeat;
            drifter.addBeat(0);
            lastBeatTime = audioStreamTime;
        }
        else if (beatsPerSample > 0.0f) {
            // several ways to detect this, this is one
            
            double nextPpqPosition = beatPosition + (beatsPerSample * (double)blockSize);
            int nextBeat = (int)nextPpqPosition;
            if (nextBeat != hostBeat) {
                // the beat happened in this block, try to locate the location it changed

                // method 1: take the remainder and work backward from the end
                double nextPpqFraction = nextPpqPosition - (double)nextBeat;
                double samplesPerBeat = 1.0f / beatsPerSample;
                int samplesAfterBeat = (int)(nextPpqFraction * samplesPerBeat);
                int method1Location = blockSize - 1 - samplesAfterBeat;

                // method 2: working from the front
                // the number of ppq units between the next beat integral and where we are now
                double ppqDelta = (double)nextBeat - beatPosition;
                int method2Location = (int)(ppqDelta * samplesPerBeat);
                
                if (method1Location != method2Location) {
                    Trace(2, "HostAnalyzer: Beat location mismatch.  Method 1 %d method 2 %d",
                          method1Location, method2Location);
                }

                // for vague reasons, method 2 feels better because the ppq delta is usually
                // larger
                blockOffset = method2Location;
                if (blockOffset < blockSize) {
                    hostBeat = nextBeat;
                    drifter.addBeat(blockOffset);
                    lastBeatTime = audioStreamTime + blockOffset;
                }
                else {
                    // this should not happen, let it be picked up on the next block
                    // in the first clause after deriveTempo
                }
            }
        }
        else {
            // without beatsPerSample we can't guess yet
            // will have to resync on the next beat
        }

        // if we found a beat, optional trace
        if (startingBeat != hostBeat) {
            if (traceppq) {
                traceFloat("HostAnalyzer: Beat %f", beatPosition);
                Trace(2, "HostAnalyzer: Block offset %d", blockOffset);
            }
        }
        else {
            // no beat, but may trace the first few ppqs after start
            if (traceppqFine && ppqCount < 10) {
                traceFloat("HostAnalyzer: PPQ %f", beatPosition);
                ppqCount++;
            }
        }
    }

    lastPpq = beatPosition;
}

/**
 * The host has not given us a tempo and we've started receiving ppqs.
 * Try to guess the tempo by watching a few of them.
 */
void HostAnalyzer::deriveTempo(double beatsPerSample)
{
    // ignore if we haven't received enough blocks to calculate this
    if (beatsPerSample > 0.0f) {

        double samplesPerBeat = 1.0f / beatsPerSample;
        double beatsPerSecond = (double)sampleRate / samplesPerBeat;
        double bpm = beatsPerSecond * 60.0f;
        
        if (tempo == 0.0f) {
            // never had a tempo
            traceFloat("HostAnalyzer: Derived tempo %f", bpm);
            tempo = bpm;
            int tempoUnit = tempoToUnit(tempo);

            // alreadh have samplesPerBeat, trust but verify
            checkUnitMath(tempoUnit, samplesPerBeat);
            
            setUnitLength(tempoUnit);
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
            checkUnitMath(derivedUnitLength, samplesPerBeat);
            
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
                }
                
                // todo: if the length exceeds some threshold, resync
            }
        }
    }
}

/**
 * Temporary diagnostics to check some math with the same
 * number arrived at different ways.
 */
void HostAnalyzer::checkUnitMath(int tempoUnit, double samplesPerBeat)
{
    // this won't get round up or evening...
    // commonlly see these at 44100 and 120bpm
    // HostAnalyzer: Unit math anomoly.  Tempo method 22050 ppq method 22049
    // HostAnalyzer: Unit math anomoly.  Tempo method 22052 ppq method 22050
    //
    // 49 to 50 is normal, 50 to 52 isn't
    // adjusted the unit round up so it doesn't do both evening and ceil
    //
    // still too much noise to leave in
    // revisit
#if 0
    int otherUnit = (int)samplesPerBeat;
    if (tempoUnit != otherUnit) {
        Trace(2, "HostAnalyzer: Unit math anomoly.  Tempo method %d ppq method %d",
              tempoUnit, otherUnit);
    }
#endif
    (void)tempoUnit;
    (void)samplesPerBeat;
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
 * When the play head crosses the loop boundary, a beat is generated.
 *
 * The determination of where "bars" are is deferred to the BarTender
 * managed by Pulsator for each Follower.
 *
 * The elapsedBeat count normally parallels the hostBeat advance unless
 * the host tempo and the unit length are way out of alignment.
 *
 */
void HostAnalyzer::advanceAudioStream(int blockFrames)
{
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
                elapsedBeats++;
            }
        }
    }

    // this used to rely on bar/loop tracking to trace drift
    // do it at a higher level now
#if 0    
    if (result.onLoop) {

        int drift = drifter.getDrift();
        
        Trace(2, "HostAnalyzer: Drift %d", drift);
    }
#endif
    
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
