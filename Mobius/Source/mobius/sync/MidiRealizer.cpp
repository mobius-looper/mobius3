/**
 * Maintenance of the MidiQueue and the generation of sync pulses is no longer necessary
 * now that clock generation is within the Transport.  Transport will generate pulses
 * not the clock generator.
 *
 * old comments, need a lot of weeding here...
 *
 * An implementation of MobiusMidiTransport that provides MIDI synchronization
 * services to the Mobius engine.
 *
 * When acting as the MIDI master clock for external devices, this class uses
 * a high resolution thread and millisecond counter to send MIDI beat clocks
 * and transport messages (Start, Stop, Continue) to a configured MIDI output device.
 *
 * When acting as a slave to an external MIDI clock, this class monitors incomming
 * transport and clock messages, and keeps them in a queue for later processing
 * by the engine during an audio block interrupt.
 *
 * In both cases a MidiQueue is used as a simplified model of the messages
 * sent or received.  It handles the semantics of the message
 * stream including wether we are started or stopped, the song position, and when
 * we've received enough clocks to make a MIDI "beat".
 *
 * These queues are consumed by Synchronizer during each audio interrupt to
 * generate control Events for the tracks that use MIDI synchronization.
 *
 * The code here is relatively general and should not contain dependencies on
 * the Mobius engine so that it could be used by other plugins.
 *
 * Comparing it to old Mobius code, it contains parts of the MidiInterface
 * library, parts of Synchronizer and MidiTransport, and has been substantially
 * simplified.
 *
 * THREADS
 *
 * There are several threads that can touch this code.  In general you should
 * always consider this code to be running in a realtime thread and should run
 * as fast as possible without calling complex system services like memory allocation
 * file access, etc.
 *
 * Midi Device Thread
 *   A thread managed by Juce to receive events from a MIDI device.
 *   This will call the midiReceived() method which adds the event to the MidiQueue
 *   and analyzes the timing of clock pulses to derive the tempo and
 *   beat boundaries.
 *
 * Clock Thread
 *   A thread managed by Mobius to send events to a MIDI device.
 *   This will call the advance() method which monitors a millisecond counter
 *   and decides when to send clock pulses and transport messages.
 *
 * Audio Thread
 *   A thread managed by Juce or the plugin host that provides blocks of
 *   audio data from the audio interface to the Mobius engine.  Most of the
 *   code in this class will be called from the Audio Thread.
 *
 * UI Thread
 *   When the MIDITransportPanel is visible, it may send start/stop/continue
 *   commands for MIDI clock generation when the Mobius engine is not running.
 *   This is only used for testing.
 *
 *   When UI elements are visible that show MIDI clock status, this may call
 *   methods like getTempo and getRawBeat to paint the UI.
 *
 * Maintenance Thread
 *   Again, when UI elements are visible that show MIDI status, this may call
 *   methods like getTempo to decide whether the UI needs to be repainted
 *   in the UI Thread.
 *
 * For the most part, the code is thread safe and only gets/sets the values of
 * atomic integer or floating point numbers.  The only tricky one is MidiQueue which
 * is modified by the Device Thread for output, the Clock Thread for input,
 * and the Audio Thread to decide what to do with events in the queue.  There is
 * only one producer/consumer for each of these queues so a simple ring buffer
 * is used.
 *
 * CLOCKS
 *
 * There are two common approaches to sending MIDI clocks.  Devices may begin sending
 * clocks as soon as a user defined tempo is known and then send Start and Stop
 * messages to indiciate the start/stop state of the sequencer.  Receivers are expected
 * to track the clocks and prepare for that tempo, but must not do anything until
 * a Start message is received.  Others may stop sending clocks when the sequencer
 * is in a stopped state, and resume sending clocks when it starts.
 *
 * Both approaches have consequences, it is sometimes better to keep clocks going
 * so the receiver has time to smooth out jittery clock signals and start more
 * accurately.  Some, mostly older ones may consider the receipt of a clock to indiciate
 * that the sequencer should start, I'll not consider those any more.  Web chatter
 * suggests that some modular hardware prefers not to be bothered with clocks if
 * they aren't in a started state.
 *
 * For a looper with arbitrary loop length, sending clocks early doesn't accomplish
 * anything because the tempo may change dramatically when the loop is closed.  I suppose
 * some devices might respond better with a continuous clock stream if the tempo
 * is changing in relatively small amounts, such as different takes of an initial loop.
 *
 * We'll support both styles with configuration options.  But initially clocks
 * will not be sent when the transport is in a stopped state.
 *
 * How the stream of clock messages interleave with start/stop messages can be
 * significant for some devices.  The 4.2.1 MIDI specification on page 30 says that
 * devices are not supposed to respond to a Start message until the first clock
 * is received after the start, and that there should be at least 1ms between the
 * start and the first clock "so the receiver has time to respond".  It is unclear how
 * strict modern devices are about that 1ms delay.  But the spec is pretty clear that
 * just receiving a Start or Continue is not enough, the device is supposed to wait
 * until the following clock.  Stop message are to be handled immediately.
 *
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../midi/MidiByte.h"
#include "../../MidiManager.h"

#include "SyncTrace.h"
#include "MidiQueue.h"
#include "MidiSyncEvent.h"
#include "SyncMaster.h"

#include "MidiRealizer.h"

MidiRealizer::MidiRealizer()
{
	outputQueue.setName("internal");
    setTempoNow(120.0f);
}

MidiRealizer::~MidiRealizer()
{
    stopThread();
}

void MidiRealizer::initialize(SyncMaster* sm, MidiManager* mm)
{
    syncMaster = sm;
    midiManager = mm;
}

void MidiRealizer::setSampleRate(int rate)
{
    mSampleRate = rate;
}

/**
 * Activate the clock thread.
 * The thread always starts if it may be needed, whether sync actually
 * happens depends on whether there is an output device configured which is checked later.
 */
void MidiRealizer::startThread()
{
    if (thread == nullptr) {
        Trace(2, "MidiRealizer: Starting clock thread\n");
        thread = new MidiClockThread(this);
        if (!thread->start()) {
            // what now? safe to delete?
            delete thread;
            thread = nullptr;

            syncMaster->sendAlert("Unable to start MIDI timer thread");
        }
    }
}

/**
 * Deactivate the clock thread.
 */
void MidiRealizer::stopThread()
{
    if (thread != nullptr) {
        Trace(2, "MidiRealizer: Stopping clock thread\n");
        thread->stop();
        // stop() is assumed to have waited for it
        delete thread;
        thread = nullptr;
    }
}

void MidiRealizer::shutdown()
{
    stopThread();
}

//////////////////////////////////////////////////////////////////////
//
// MIDI Clock Thread
//
//////////////////////////////////////////////////////////////////////

/**
 * A very simple thread that does nothing but call the
 * MidiRealizer::advance method every millisecond.
 *
 * We should try to make the thread period as close as possible
 * to 1ms, but timing isn't crucial as we also follow a system
 * millisecond counter which must be assumed to be accurate.
 *
 * The period just needs to be small enough to allow generation
 * of 24 pulsesPerBeat MIDI clocks with low jitter.
 *
 * Because the work is done by MidiRealizer::advance and
 * uses the system millisecond counter for calculations, we don't
 * necessarily need this thread, it could be anything that is capable
 * of calling advance() at regular intervals.  The audio thread
 * block interrupt might be fine enough for this.  At a sample rate of
 * 44100 and a block size of 256, each block would be received approximately
 * every 5.8 milliseconds.  So a 1ms timer thread would result in less
 * clock jitter.  It is unclear how important this is these days since
 * any modern clock receiver should be doing smoothing of some form.
 *
 * It is unclear what the lifespan of the juce::Thread object should be.
 * MainThread is a member object in Supervisor so it's there all the time,
 * but we don't always need a MIDI clock thread.  Could create it dynamically
 * when necessary and delete it when it's not.  Right now it's a member object
 * in Supervisor and will be destructed after Supervisor destructs so Supervisor
 * stop the thread during the shutdown process.
 * 
 */
MidiClockThread::MidiClockThread(MidiRealizer* mr) :
    juce::Thread(juce::String("MobiusMidiClock"))  // second arg is threadStackSize

{
    realizer = mr;
}

MidiClockThread::~MidiClockThread()
{
    Trace(2, "MidiClockThread: destructing\n");
}

/**
 * MainThread currently asks to be a RealtimeThread though it probably doesn't need to be.
 * MIDI is fairly coarse grained so 1ms should be more than enough, but jitter is
 * more significant here so ask for realtime.
 */
bool MidiClockThread::start()
{
    bool started = false;
    
    juce::Thread::RealtimeOptions options;

    (void)options.withPriority(10);
    (void)options.withPeriodMs(1);

    started = startRealtimeThread(options);
    if (!started) {
        Trace(1, "MidiClockThread: Unable to start thread\n");
    }
    return started;
}

void MidiClockThread::stop()
{
    // example says: allow 2 seconds to stop cleanly - should be plenty of time
    if (!stopThread(2000)) {
        Trace(1, "MidiClockThread: Unable to stop thread\n");
    }
}

void MidiClockThread::run()
{
    // threadShouldExit returns true when the stopThread method is called
    while (!threadShouldExit()) {
        // this seems to be innacurate, in testing my delta was frequently
        // 2 and as high as 5 comparing getMilliseondCounter
        wait(1);
        realizer->clockThreadAdvance();
    }
}

//////////////////////////////////////////////////////////////////////
//
// Advance
//
//////////////////////////////////////////////////////////////////////

/**
 * Advance the state of the MIDI clock generator.
 * This is where all of the interesting work gets done for output sync.
 * Normally called by MidiClockThread every 1ms but don't depend on that.
 * Use juce::Time to get the system millisecond counter to know where exactly
 * we are in space and time.
 *
 * This is a redesign of what MidiTimer::interrupt did in the old code, with
 * the "signal clock" concept removed.
 *
 * See comments at the top of the file about the relationship between Start/Continue
 * and clocks.  Start/Continue signal the "intent" to start/continue but devices are
 * not supposed to actually start/continue until a following clock is received,
 * and it is suggested that there be a 1ms delay between the start and the clock.
 * Old code did not wait 1ms between the Start and the Clock, I'm going to try
 * doing it the suggested way and see how that shakes out.  Need to closely
 * monitor jitter and drift tolerance...
 */
void MidiRealizer::clockThreadAdvance()
{
    if (running) {
        // I starated using this, but web chatter suggests that the HiRes variant
        // can be more accurate.  It returns a float however which complicates things.
        // Explore this someday
        int now = juce::Time::getMillisecondCounter();
        if (lastMillisecondCounter == 0) {
            // first time here, one of the pending flags needs to have been set
            // to force it through the pulse reset logic
            if (!pendingStartClock && !pendingStart && !pendingContinue && !pendingStop) {
                Trace(1, "MidiRealizer: Forcing pendingStartClock");
                pendingStartClock = true;
            }
            // in this case delta is 0 since we're just starting the tracking
            // but we won't use delta since pendingStartClock is on
            lastMillisecondCounter = now;
        }
        int delta = now - lastMillisecondCounter;
        lastMillisecondCounter = now;

        // sanity check on tempo, should always be initialized and constrained
        // but don't let clocks go haywire
        // todo: more boundaries on msecsPerPulse would be safer
        if (msecsPerPulse == 0 && pendingTempo == 0.0f)
          pendingTempo = 120.0f;

        // adjust to a new tempo and reset the msecsPerPulse
        if (pendingTempo > 0.0f) {
            if (SyncTraceEnabled)
              Trace(2, "Sync: Setting pending tempo");
            setTempoNow(pendingTempo);
            pendingTempo = 0.0f;
            // if we've been actively sending clocks and pluseWait is in
            // the middle of it's decay, should we reset that too, or let it
            // continue it's decay with the old tempo?
            pulseWait = msecsPerPulse;
        }

        if (pendingStartClock) {
            if (SyncTraceEnabled)
              Trace(2, "Sync: Sending pending start clock msec %d pulseWait %d",
                    now, (int)(msecsPerPulse * 100));
            // we sent Start or Continue on the last cycle and now
            // send the first clock which officially starts things running
            // in the external device
            // also here when sending the first clock after starting the timer
            // and ManualStart was on, in that case we won't have xxx
            
            midiManager->sendSync(juce::MidiMessage::midiClock());
            outputQueue.add(MS_CLOCK, now);
            pulseWait = msecsPerPulse;
            // todo: if we had pendingContinue old code
            // did stuff with song position which is why we didn't clear
            // pendingStart and pendingContinue so we know the difference here
            pendingStart = false;
            pendingContinue = false;
            pendingStartClock = false;
            
            // todo: process pending tempo changes like old code?
        }
        else if (pendingStart) {
            if (SyncTraceEnabled)
              Trace(2, "Sync: Sending pending start msec %d", now);
            midiManager->sendSync(juce::MidiMessage::midiStart());
            outputQueue.add(MS_START, now);
            pendingStartClock = true;

            // todo: process pending tempo changes like old code?
            // todo: spec says there needs to be a 1ms gap between Start
            // and the first clock, we're not doing that exactly, just waiting
            // until the next block.  This could result in minor jitter in some
            // devices, unclear if we need to send the first clock immediately
            // in some cases
        }
        else if (pendingContinue) {
            if (SyncTraceEnabled)
              Trace(2, "Sync: Sending pending continue msec %d", now);
            midiManager->sendSync(juce::MidiMessage::midiContinue());
            outputQueue.add(MS_CONTINUE, now);
            // todo: this is where old code would look at mPendingSongPosition
            pendingStartClock = true;

            // todo: process pending tempo changes like old code?
        }
        else if (pendingStop) {
            if (SyncTraceEnabled)
              Trace(2, "Sync: Sending pending stop msec %d", now);
            // these we don't have to wait on
            midiManager->sendSync(juce::MidiMessage::midiStop());
            outputQueue.add(MS_STOP, now);
            pendingStop =  false;
            // optionally stop sending clocks
            if (pendingStopClocks) {
                if (SyncTraceEnabled)
                  Trace(2, "Sync: Stopping clocks msec %d", now);
                running = false;
                pendingStopClocks = false;
            }
        }
        else {
            // decrement the clock wait counter and see if we crossed the threshold
            pulseWait -= (float)delta;
            if (pulseWait <= 0.0f) {
                // we've waited long enough, send a clock

                // adjust pulseWait early for trace
                // due to jitter, pulseWait may be less than zero so
                // accumulate the fraction for the next pulse
                // todo: does it make sense to be proactive and send a clock
                // when we're really close to zero instead of always counting down
                // all the way?  Could result in less jitter
                pulseWait += msecsPerPulse;
                
                if (SyncTraceEnabled)
                  Trace(2, "Sync: Sending clock msec %d pulseWait %d",
                        now, (int)(pulseWait * 100));
                
                midiManager->sendSync(juce::MidiMessage::midiClock());
                outputQueue.add(MS_CLOCK, now);

                // todo: here is where old code would check for overage in the
                // "tick" counter and drop clocks, can this really happen?
                // I think it was due to the tempo pulse width changing out from under
                // the interrupt which we can prevent if we queue tempo changes
                if (pulseWait <= 0.0f && !pulseWaitWarning) {
                    Trace(1, "MidiRealizer: pulseWait overflow!\n");

                    // this commonly happens during debugging, would be nice
                    // to be able to detect this and suppress it
                    pulseWaitWarning = true;
                }
            }
        }
    }    
}

//////////////////////////////////////////////////////////////////////
//
// Transport Control
//
//////////////////////////////////////////////////////////////////////

//
// If there are no MIDI output devices configured, we have two options
// We can ignore any attempt to start/stop/continue, or we can continue
// on our merry way and pretend.
//
// There is a lot of complicated state logic built around where we think we are
// in the MIDI clock stream and it is risky to make all of that understand
// a new state of "i tried but nothing will happen".  
//
// Instead, send an alert every time you try to send start, but otherwise
// continue normally.
//

/**
 * Start sending clocks at the current tempo without sending MIDI Start.
 */
void MidiRealizer::startClocks()
{
    if (SyncTraceEnabled)
      Trace(2, "MidiRealizer::startClocks");

    if (!running) {
        // crucial that you set this too so advance() knows to send the
        // first clock and reset the pulseWidth tracking state
        pendingStartClock = true;
        startClocksInternal();
    }
}

/**
 * Once running is set true, advance() will start doing it's thing and it is
 * crucial that all the little state flags be set up properly.  
 */
void MidiRealizer::startClocksInternal()
{
    if (!running) {
        // once the thread starts, it won't stop unless asked, but
        // "running" controls whether we send clocks
        startThread();
        
        // sanity check on the last tempo set and make
        // sure the msecsPerPulse is calculated properly
        // only do this if we aren't running
        setTempoNow(tempo);

        running = true;
    }
}

/**
 * Send a MIDI Start message and resume sending clocks if we aren't already.
 * 
 * todo: Ignore this if we're already in a started state?  I don't think so,
 * you can mash the start button on a sequencer while it is running and some
 * will just restart.
 */
void MidiRealizer::start()
{
    if (SyncTraceEnabled)
      Trace(2, "MidiRealizer::start Set pendingStart");
    
    if (!midiManager->hasOutputDevice(MidiManager::OutputSync)) {
        Trace(1, "MidiRealizer: No MIDI Output device\n");
        // note that if you call Supervisor::alert here it will
        // try to show the AlertPanel which we can't do without
        // a Juce runtime assertion since we're usually in the audio thread
        // at this point.  Instead, set a pending alert and let Synchronizer do this
        // on the next update.
        // update: no longer calling Supervisor directly here
        // supervisor->addAlert(...)
        syncMaster->sendAlert("No MIDI Output device is open.  Unable to send Start");
    }
    
    // what to do about overlaps?
    // this would only happen if there were bugs in Synchronizer or scripts or
    // the clock thread is stuck due to extreme load
    // I suppose it is okay to have start/stop pairs close to each other 
    if (pendingStart || pendingContinue || pendingStop) {
        Trace(1, "MidiRealizer: Start request overflow!\n");
    }
    else {
        pendingStart = true;
        startClocksInternal();
    }
}

/**
 * TODO: Old code supported passing songPosition with the continue.
 * I don't think Mobius needs this but might be nice for other things.
 */
void MidiRealizer::midiContinue()
{
    if (SyncTraceEnabled)
      Trace(2, "MidiRealizer::continue Set pendingContinue");
    
    if (pendingStart || pendingContinue || pendingStop) {
        Trace(1, "MidiRealizer: Continue request overflow!\n");
    }
    else {
        pendingContinue = true;
        startClocksInternal();
    }
}

void MidiRealizer::stop()
{
    stopSelective(true, true);
}

/**
 * Old code supported stopping clocks without sending a Stop message,
 * or any other combination.
 * 
 * "After entering Mute or Pause modes, decide whether to send
 * MIDI transport commands and stop clocks.  This is controlled
 * by an obscure option MuteSyncMode.  This is for dumb devices
 * that don't understand STOP/START/CONTINUE messages."
 *
 * I think we can safely always send a Stop message so ignore
 * the sendStop argument.
 */
void MidiRealizer::stopSelective(bool sendStop, bool stopClocks)
{
    (void)sendStop;

    if (SyncTraceEnabled)
      Trace(2, "MidiRealizer::stopSelective sendStop %d stopClocks %d",
            sendStop, stopClocks);
    
    
    if (pendingStart || pendingContinue || pendingStop) {
        Trace(1, "MidiRealizer: Stop request overflow!\n");
    }
    else if (!running) {
        // we weren't doing anything, why not just leave us alone?
        if (SyncTraceEnabled)
          Trace(2, "MidiRealizer::stopSelective stop when not running");
    }
    else {
        // old code I think allowed you to stop clocks without
        // also sending a Stop message, might have been significant
        // for old devices, why would we want that now?
        pendingStop = true;
        pendingStopClocks = stopClocks;
    }
}   

/**
 * Set the tempo of the output clock pulses.
 * If we're actively running, this is deferred until the next
 * advance so we don't have to deal with unstable pulseWait math.
 */
void MidiRealizer::setTempo(float newTempo)
{
    if (running) {
        if (SyncTraceEnabled)
          Trace(2, "MidiRealizer: Set pendingTempo");
        
        // if they're twisting a control knob we might have these
        // come in rapidly so just overwrite the last one if advance()
        // hasn't consumed it yet
        pendingTempo = newTempo;
    }
    else {
        setTempoNow(newTempo);
    }
}

/**
 * Internal method to change the clock tempo.
 */
void MidiRealizer::setTempoNow(float newTempo)
{
    if (newTempo < 10.0f)
      newTempo = 10.0f;
    else if (newTempo > 300.0f)
      newTempo = 300.0f;

    tempo = newTempo;
    msecsPerPulse = 60000.0f / tempo / 24.0f;

    Trace(2, "MidiRealizer: tempo %d msecsPerPulse %d\n",
          (int)(tempo * 100.0f), (int)(msecsPerPulse * 100.0f));

    pendingTempo = 0.0f;
}

// Just for information pusposes, here is the old setTempo method
// that did more work than it needed to
#if 0
void MidiRealizer::setTempo(float tempo)
{
	if (tempo < 0) {
		Trace(1, "MidiRealizer: Invalid negative tempo!\n");
	}
	else if (tempo == 0.0f) {
		// should we ignore this?
		Trace(1, "MidiRealizer: Tempo changed from %ld (x100) to zero, sync disabled\n",
			  (long)(mTempo * 100));
		mTempo = 0;
	}
	else {
        // don't actually use any of these calculations besides trace
		float clocksPerSecond = (tempo / 60.0f) * 24.0f;
		float framesPerClock = mSampleRate / clocksPerSecond;
		float millisPerClock = 1000.0f / clocksPerSecond;
		Trace(2, "MidiRealizer: tempo changed from %ld to %ld (x100) millis/clock %ld frames/clock %ld\n",
			  (long)(mTempo * 100),
			  (long)(tempo * 100),
			  (long)millisPerClock,
			  (long)framesPerClock);

		mTempo = tempo;

        // todo: what did this do?
		//mMidi->setOutputTempo(mTempo);
	}
}
#endif

//////////////////////////////////////////////////////////////////////
//
// Output sync status
//
//////////////////////////////////////////////////////////////////////

bool MidiRealizer::isSending()
{
    return running;
}

bool MidiRealizer::isStarted()
{
	return outputQueue.started;
}

float MidiRealizer::getTempo()
{
    return tempo;
}

int MidiRealizer::getBeat()
{
	return outputQueue.beat;
}

/**
 * Not keeping track of these yet, only necessary for some
 * old test scripts.
 */
int MidiRealizer::getStarts()
{
    return 0;
}

void MidiRealizer::incStarts()
{
}

int MidiRealizer::getSongClock()
{
	return outputQueue.songClock;
}

//////////////////////////////////////////////////////////////////////
//
// Event Consumption
//
//////////////////////////////////////////////////////////////////////

void MidiRealizer::setTraceEnabled(bool b)
{
    outputQueue.setTraceEnabled(b);
}

/**
 * Allow enabling and disabling of MidiSyncEvents in both
 * queues in cases where Mobius/Synchronizer may not be responding
 * and we don't want to overflow the event buffer.
 */
void MidiRealizer::enableEvents()
{
    outputQueue.setEnableEvents(true);
}

void MidiRealizer::disableEvents()
{
    outputQueue.setEnableEvents(false);
}

void MidiRealizer::flushEvents()
{
    outputQueue.flushEvents();
}

//////////////////////////////////////////////////////////////////////
//
// New SyncMaster/Transport Interaction
//
//////////////////////////////////////////////////////////////////////

/**
 * Consume any queued events at the beginning of an audio block
 * and prepare the SyncAnalyzerResult
 *
 * !! This is basically identical to what MidiAnalyzer does.
 * Could factor out something in common that could be shared, but
 * in current usage, Transport doesn't really care about beat detection
 * so most of this can go away.
 */
void MidiRealizer::advance(int blockFrames)
{
    (void)blockFrames;
    
    result.reset();

    outputQueue.iterateStart();
    MidiSyncEvent* mse = outputQueue.iterateNext();
    while (mse != nullptr) {
        detectBeat(mse);
        mse = outputQueue.iterateNext();
    }
    outputQueue.flushEvents();
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
void MidiRealizer::detectBeat(MidiSyncEvent* mse)
{
    bool detected = false;
    
    if (mse->isStop) {
        result.stopped = true;
    }
    else if (mse->isStart) {
        // MidiRealizer deferred this until the first clock
        // after the start message, so it is a true beat
        detected = true;
        result.started = true;
    }
    else if (mse->isContinue) {
        // !! this needs a shit ton of work
        // only pay attention to this if this is also a beat pulse
        // not sure if this will work, but I don't want to fuck with continue right now
        // treat it like a Start and ignore song position
        if (mse->isBeat) {
            detected = true;
            result.started = true;
        }
    }
    else {
        // ordinary clock
        // ignore if this isn't also a beat
        detected = (mse->isBeat);
    }

    if (detected && result.beatDetected) {
        // more than one beat in this block, bad
        Trace(1, "MidiRealizer: Multiple beats detected in block");
    }
    
    result.beatDetected = detected;
}

SyncAnalyzerResult* MidiRealizer::getResult()
{
    return &result;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
