
#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../midi/MidiByte.h"

// so we can have a pseudo-loop for tracking progress
// migth want to put that in a wrapper to keep this focused?
#include "../model/Session.h"
#include "../model/SessionHelper.h"
#include "../model/SyncState.h"
#include "../model/PriorityState.h"

#include "MidiQueue.h"
#include "MidiSyncEvent.h"
#include "SyncMaster.h"
#include "MidiAnalyzer.h"

//////////////////////////////////////////////////////////////////////
//
// Configuration
//
//////////////////////////////////////////////////////////////////////

MidiAnalyzer::MidiAnalyzer()
{
	inputQueue.setName("external");
}

MidiAnalyzer::~MidiAnalyzer()
{
}

void MidiAnalyzer::initialize(SyncMaster* sm, MidiManager* mm)
{
    syncMaster = sm;
    midiManager = mm;
    mm->addRealtimeListener(this);

    // so we can pull things from the Session
    sessionHelper.setSymbols(sm->getSymbols());
}

void MidiAnalyzer::setSampleRate(int rate)
{
    sampleRate = rate;
}

void MidiAnalyzer::shutdown()
{
    midiManager->removeRealtimeListener(this);
}

void MidiAnalyzer::loadSession(Session* s)
{
    sessionHelper.setSession(s);
    beatsPerBar = sessionHelper.getInt(ParamMidiBeatsPerBar);
    barsPerLoop = sessionHelper.getInt(ParamMidiBarsPerLoop);
}

//////////////////////////////////////////////////////////////////////
//
// State
//
//////////////////////////////////////////////////////////////////////

void MidiAnalyzer::refreshState(SyncState* state)
{
    state->midiReceiving = isReceiving();
    state->midiStarted = isRunning();
    state->midiTempo = getTempo();

    state->midiBeat = inputQueue.beat;
    // need a virtual loop to play like Transport
    state->midiBar = 1; 
    state->midiLoop = 1;
    
    // need to be capturing these from the session
    state->midiBeatsPerBar = beatsPerBar;
    state->midiBarsPerLoop = barsPerLoop;
    state->midiUnitLength = unitLength;
    state->midiPlayHead = unitPlayHead;
}

/**
 * Capture the priority state from the transport.
 */
void MidiAnalyzer::refreshPriorityState(PriorityState* state)
{
    state->midiBeat = inputQueue.beat;
    state->midiBar = 1;
    state->midiLoop = 1;
}

//////////////////////////////////////////////////////////////////////
//
// SyncAnalyzer Interface
//
//////////////////////////////////////////////////////////////////////

SyncAnalyzerResult* MidiAnalyzer::getResult()
{
    return &result;
}

/**
 * True if we have received a MIDI start or continue message.
 */
bool MidiAnalyzer::isRunning()
{
    return inputQueue.started;
}

/**
 * !! Is this really elapsed or did MidiQueue orient it for SongPosition?
 */
int MidiAnalyzer::getElapsedBeats()
{
    return inputQueue.beat;
}

float MidiAnalyzer::getTempo()
{
    //return tempoMonitor.getTempo();
    return (float)(tempoMonitor.getSmoothTempo()) / 10.0f;
}

/**
 * !! Not doing units and drift yet.
 */
int MidiAnalyzer::getUnitLength()
{
    return 0;
}

int MidiAnalyzer::getDrift()
{
    return 0;
}

//////////////////////////////////////////////////////////////////////
//
// Extended Public Interface
//
//////////////////////////////////////////////////////////////////////

bool MidiAnalyzer::isReceiving()
{
    return inputQueue.receivingClocks;
}

int MidiAnalyzer::getSmoothTempo()
{
    return tempoMonitor.getSmoothTempo();
}

int MidiAnalyzer::getSongClock()
{
    return inputQueue.songClock;
}

/**
 * Allow enabling and disabling of MidiSyncEvents in cases where
 * Mobius may not be responding and we don't want to overflow the event buffer.
 */
void MidiAnalyzer::disableEvents()
{
    inputQueue.setEnableEvents(false);
}

void MidiAnalyzer::enableEvents()
{
    inputQueue.setEnableEvents(true);
}

void MidiAnalyzer::flushEvents()
{
    inputQueue.flushEvents();
}

/**
 * Expected to be called periodically to check whather clocks are still
 * being received.
 */
void MidiAnalyzer::checkClocks()
{
    int now = juce::Time::getMillisecondCounter();
    inputQueue.checkClocks(now);

    midiMonitorClockCheck();
}

//////////////////////////////////////////////////////////////////////
//
// MidiManager::RealtimeListener
//
//////////////////////////////////////////////////////////////////////

/**
 * Given a MIDI Realtime message received from a MIDI device, add the
 * interesting ones to the input queue.
 *
 * We'll get SystemCommon messages as well as Realtime messages which
 * we need for SongPosition.  Everything else ignore.
 */
void MidiAnalyzer::midiRealtime(const juce::MidiMessage& msg, juce::String& source)
{
    (void)source;

    midiMonitor(msg);
    
    const juce::uint8* data = msg.getRawData();
    const juce::uint8 status = *data;
    int now = juce::Time::getMillisecondCounter();
    
	switch (status) {
		case MS_QTRFRAME: {
			// not sure what this is, ignore
		}
		break;
		case MS_SONGPOSITION: {
			// only considered actionable if a MS_CONTINUE is received later
            // does not generate a MidiSyncEvent, just save it
            // I'm not sure what Juce does with this value, assume it's
            // the usual combination of message bytes
            inputQueue.setSongPosition(msg.getSongPositionPointerMidiBeat());
		}
		break;
		case MS_SONGSELECT: {
			// nothing meaningful for Mobius?
			// could use it to select loops?
		}
		break;
		case MS_CLOCK: {
            inputQueue.add(status, now);
            tempoMonitor.clock(now, msg.getTimeStamp());
		}
		break;
		case MS_START: {
			inputQueue.add(status, now);
		}
		break;
		case MS_STOP: {
			inputQueue.add(status, now);
		}
		break;
		case MS_CONTINUE: {
			inputQueue.add(status, now);
		}
		break;
	}
}

//////////////////////////////////////////////////////////////////////
//
// Analysis
//
//////////////////////////////////////////////////////////////////////

/**
 * Consume any queued events at the beginning of an audio block
 * and prepare the SyncAnalyzerResult
 */
void MidiAnalyzer::analyze(int blockFrames)
{
    (void)blockFrames;
    
    result.reset();

    bool useMidiQueue = false;
    if (useMidiQueue) {
        inputQueue.iterateStart();
        MidiSyncEvent* mse = inputQueue.iterateNext();
        while (mse != nullptr) {
            if (inputQueue.isStarted())
              detectBeat(mse);
            mse = inputQueue.iterateNext();
        }
        inputQueue.flushEvents();
    }
    else {
        // look at flags left behind by the clock monitor
        if (playing != inStarted) {
            if (inStarted) {
                // since we're always behind, start the beat at frame zero
                result.started = true;
                //drifter.orient(unitLength);
                unitPlayHead = 0;
                // shold always be zero
                elapsedBeats = inBeat;
                playing = true;
                unitLength = 0;

                // todo: deal with continue
                inContinuePending = false;
            }
            else {
                result.stopped = true;
                unitLength = 0;
            }
        }
        else if (playing) {
            if (elapsedBeats != inBeat) {
                drifter.addBeat(0);
                if (unitLength == 0) {
                    // the first beat after starting
                    double secsPerBeat = (60.0f / inSmoothTempo);
                    double samplesPerBeat = (float)sampleRate * secsPerBeat;
                    int smoothUnitLength = (int)samplesPerBeat;
                    if ((smoothUnitLength % 2) == 1)
                      smoothUnitLength++;
                    unitLength = smoothUnitLength;
                    Trace(2, "MidiAnalyzer: Locking unit length %d", unitLength);
                    result.beatDetected = true;
                }
                else {
                    // once the unit length is set, beats are determined by the
                    // virtual loop
                }
                elapsedBeats = inBeat;
            }
        }
    }
    advance(blockFrames);
}

/**
 * Convert a queued MidiSyncEvent into fields in the SyncAnalyzerResult
 * for later consumption by Pulsator.
 * 
 * todo: this is place where we should try to offset the event into the buffer
 * to make it align more accurately with real time.
 *
 * This still queues queues MidiSyncEvents for each clock although only
 * one of them should have the beat flag set within one audio block.
 */
void MidiAnalyzer::detectBeat(MidiSyncEvent* mse)
{
    bool detected = false;
    
    if (mse->isStop) {
        stopDetected();
    }
    else if (mse->isStart) {
        // MidiRealizer deferred this until the first clock
        // after the start message, so it is a true beat
        detected = true;
        startDetected();
    }
    else if (mse->isContinue) {
        // note: There is no actual "continue" in MIDI
        // there is 0xF2 "song position" followed by 0xFA Start
        
        // !! this needs a shit ton of work
        // only pay attention to this if this is also a beat pulse
        // not sure if this will work, but I don't want to fuck with continue right now
        // treat it like a Start and ignore song position
        if (mse->isBeat) {
            detected = true;
            continueDetected(mse->songClock);

            // this is how older code adjusted the Pulse
            //pulse->mcontinue = true;
            //pulse->continuePulse = mse->songClock;
        }
    }
    else {
        // ordinary clock
        // ignore if this isn't also a beat
        detected = (mse->isBeat);
    }

    if (detected && result.beatDetected) {
        // more than one beat in this block, bad
        Trace(1, "MidiAnalyzer: Multiple beats detected in block");
    }
    
    result.beatDetected = detected;
}

void MidiAnalyzer::startDetected()
{
    Trace(2, "MidiAnalyzer: Start");
    result.started = true;
    drifter.orient(unitLength);

    unitPlayHead = 0;
    elapsedBeats = 0;
    //lastBeatTime = 0;
}

void MidiAnalyzer::stopDetected()
{
    Trace(2, "MidiAnalyzer: Stop");
    result.stopped = true;
}

void MidiAnalyzer::continueDetected(int songClock)
{
    Trace(2, "MidiAnalyzer: Continue %d", songClock);
    result.started = true;
}

/**
 * Advance the pseudo loop and keep track of beat bar boundaries
 *
 * This one is weirder than transport because we detect beats based on
 * events actually received, so it's more like HostAnalyzer
 */
void MidiAnalyzer::advance(int frames)
{
#if 0    
    if (inputQueue.started && unitLength > 0) {

        unitPlayHead = unitPlayHead + frames;
        if (unitPlayHead >= unitLength) {

            // a unit has transpired
            int blockOffset = unitPlayHead - unitLength;
            if (blockOffset > frames || blockOffset < 0)
              Trace(1, "Transport: You suck at math");

            // effectively a frame wrap too
            unitPlayHead = blockOffset;

            elapsedBeats++;
            beat++;
            
            //result.beatDetected = true;
            //result.blockOffset = blockOffset;

            if (!result.beatDetected)
              Trace(1, "MidiAnalyzer: Beat not detected where we thought");

            if (beat >= beatsPerBar) {

                beat = 0;
                bar++;
                result.barDetected = true;

                if (bar >= barsPerLoop) {

                    bar = 0;
                    loop++;
                    result.loopDetected = true;
                }
            }
        }

        // todo: also advance the drift monitor
        //drifter.advanceStreamTime(frames);
    }

    if (result.loopDetected)
      checkDrift();
#endif    
}

void MidiAnalyzer::checkDrift()
{
    //int drift = drifter.getDrift();
    //if (drift > 256)
    //Trace(2, "Transport: Drift %d", drift);
}

//////////////////////////////////////////////////////////////////////
//
// Midi Stream Monitoring
//
// This is a simplification of and eventual replacemenet for MidiQueue
//
//////////////////////////////////////////////////////////////////////

void MidiAnalyzer::midiMonitor(const juce::MidiMessage& msg)
{
    const juce::uint8* data = msg.getRawData();
    const juce::uint8 status = *data;
    
	switch (status) {
		case MS_START: {
            if (!inStarted) {
                inStartPending = true;
                inContinuePending = false;
                inSongPosition = 0;
                inClock = 0;
            }
            else {
                Trace(1, "MA: Redundant Start");
            }
		}
            break;
		case MS_CONTINUE: {
            if (!inStarted) {
                inStartPending = true;
                inContinuePending = true;
            }
            else {
                Trace(1, "MA: Redundant Continue");
            }
		}
            break;
		case MS_STOP: {
            if (inStarted) {
                inStarted = false;
                inStartPending = false;
                inContinuePending = false;
                inBeatPending = false;
                Trace(2, "MA: Stop");
            }
            else {
                Trace(1, "MA: Redundant Stop");
            }
		}
            break;
		case MS_SONGPOSITION: {
            if (!inStarted) {
                inSongPosition = msg.getSongPositionPointerMidiBeat();
            }
            else
              Trace(1, "MA: Unexpected SongPosition");
		}
            break;
		case MS_CLOCK: {
            if (!inClocksReceiving) {
                Trace(2, "MA: Clocks starting");
                inClocksReceiving = true;
            }
            if (inStartPending) {
                inStarted = true;
                inStartPending = false;
                inBeatPending = true;
                inStreamTime = 0;
                // no this isn't accurate, we can start on a sixteenth with song position
                inClock = 0;
                inBeatClock = 0;
                inBeat = 0;
                if (inContinuePending)
                  Trace(2, "MA: Continue");
                else
                  Trace(2, "MA: Start");
            }
            else if (inStarted) {
                inClock++;
                inBeatClock++;
                if (inBeatClock == 24) {
                    inBeat++;
                    inBeatClock = 0;
                    Trace(2, "MA: Beat");
                    if (inBeatPending) {
                        // don't need this here, let the audio thread handle it
                        //midiMonitorFirstBeat();
                        inBeatPending = false;
                    }
                }
            }
            tempoMonitorAdvance(msg.getTimeStamp());
        }
		break;
	}
}

/**
 * The juce::MidiMessage timestamp is "milliseconds / 1000.0f"
 * in other words a "seconds" timestamp.  The delta
 * between clocks is then "seconds per clock".
 *
 * There are 24 clocks per quarter note so tempo is:
 *
 *      60 / secsPerClock * 24
 *
 * There is jitter even when using a direct path to the MidiInput
 * callback.  Trace clouds this to dump the results after capture.
 *
 *  90.016129, 89.714100, 91.415700, 89.285714, 89.285714, 89.836927
 *  90.074510, 90.446661, 89.819500, 90.219159, 89.572989, 90.302910
 *  90.456154, 89.710558, 89.877946. 90.039149, 89.891842, 90.047257
 *  89.885053
 *
 * The MC-101 tempo for those measurements was 90.0.
 *
 * 
 */
void MidiAnalyzer::tempoMonitorAdvance(double clockTime)
{
    if (inClockTime > 0.0f) {
        double delta = clockTime - inClockTime;
        if (delta < 0.0f) {
            Trace(2, "MA: Clock went back in time");
            tempoMonitorReset();
        }
        else {
            // we now have the milliseconds between two clocks
            // calculate the tempo in BPM
            // juce timestamp is milliseconds / 1000

            // this captures the tempo
#if 0            
            double secsPerQuarter = delta * 24.0f;
            double tempo = 60.0f / secsPerQuarter;
            if (inTraceCount < TempoTraceMax) {
                inTraceCapture[inTraceCount] = tempo;
                inTraceCount++;
            }
#endif
            // this just captures the delta and computes tempo later
            if (inTraceCount < TraceMax) {
                inTraceCapture[inTraceCount] = delta;
                inTraceCount++;
            }
            
            double newTempo = 60.0f / (delta * 24.0f);

            if (inSampleCount == inSampleLimit) {
                // we have gathered enough samples, drop the first one
                int firstIndex = inSampleIndex - inSampleLimit;
                if (firstIndex < 0)
                  firstIndex += SmoothSampleMax;
                double firstSample = inTempoSamples[firstIndex];
                inSampleTotal -= firstSample;
            }
            else {
                inSampleCount++;
            }
            
            // deposit the new sample
            inSampleTotal += newTempo;
            inTempoSamples[inSampleIndex] = newTempo;
            inSampleIndex++;
            if (inSampleIndex == SmoothSampleMax)
              inSampleIndex = 0;

            if (inSampleCount == inSampleLimit)
              inSmoothTempo = inSampleTotal / (float)inSampleLimit;
        }
    }
    else {
        Trace(2, "MidiMonitor: Clocks starting");
    }
    inClockTime = clockTime;
}

void MidiAnalyzer::tempoMonitorReset()
{
    inSampleCount = 0;
    inSampleIndex = 0;
    inSampleTotal = 0.0f;
}

void MidiAnalyzer::midiMonitorClockCheck()
{
    double now = juce::Time::getMillisecondCounterHiRes();
    
    if (inClocksReceiving && inClockTime > 0.0f) {
        // juce timestamp is divided by 1000 for some reason
        double nowSeconds = now / 1000.0f;
        double delta = nowSeconds - inClockTime;
        if (delta > 1000.0f) {
            // clocks dead for more than a second
            Trace(2, "MA: Clocks stopped");
            inClocksReceiving = false;
            tempoMonitorReset();
        }
    }

    if (!inTraceDumped && inTraceCount >= TraceMax) {
        char buf[64];
        for (int i = 0 ; i < TraceMax ; i++) {

            // this assumes tempo was pre calculated
            //sprintf(buf, "%f", inTraceCapture[i]);

            // this doesn't
            double tempo = 60.0f / (inTraceCapture[i] * 24.0f);
            sprintf(buf, "%f", tempo);
            Trace(2, buf);
        }
        inTraceDumped = true;
    }

    double smoothTraceDelta = now - lastSmoothTraceTime;
    if (lastSmoothTraceTime == 0.0f || smoothTraceDelta > 1000.0f) {

        double secsPerBeat = (60.0f / inSmoothTempo);
        double samplesPerBeat = (float)sampleRate * secsPerBeat;
        int smoothUnitLength = (int)samplesPerBeat;
        if ((smoothUnitLength % 2) == 1)
          smoothUnitLength++;
        
        char buf[64];
        sprintf(buf, "Smooth tempo: %f unit %d", inSmoothTempo, smoothUnitLength);
        Trace(2, buf);
        lastSmoothTraceTime = now;
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
