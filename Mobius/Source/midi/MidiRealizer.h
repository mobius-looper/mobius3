/**
 * An implementation of MobiusMidiTransport that provides MIDI synchronization
 * services to the Mobius engine.
 *
 * todo: generalize this so that it can be packaged as a standalone MIDI services
 * utility for other plugins.  Part of a synchronization library that also pulls
 * in HostSyncState and possibly parts of what is now mobius/core/SyncTaracker
 */

#pragma once

#include <JuceHeader.h>


#include "../mobius/MobiusMidiTransport.h"
#include "../MidiManager.h"

#include "MidiQueue.h"
#include "TempoMonitor.h"

/**
 * High resolution thread used when generating MIDI clocks and sending
 * transport messages to a device.
 */
class MidiClockThread : public juce::Thread
{
  public:
    
    MidiClockThread(class MidiRealizer* mr);
    ~MidiClockThread();

    bool start();
    void stop();

    void run() override;

  private:

    class MidiRealizer* realizer;
    
};

/**
 * Class encapsulating all MIDI realtime message processing.
 * 
 * Implements MobiusMidiTransport so it can be handed
 * to the engine's Synchronizer.
 */
class MidiRealizer : public MobiusMidiTransport, public MidiManager::RealtimeListener
{
    friend class MidiClockThread;
    
  public:

    MidiRealizer(class Supervisor* s);
    ~MidiRealizer();

    void initialize();
    void shutdown();
    void startThread();
    void stopThread();
    
    // message accumulation can be turned on and off for testing
    void enableEvents();
    void disableEvents();
    void flushEvents();
    
    // check for termination of MIDI clocks without warning
    void checkClocks();

    // MidiManager::RealtimeListener
    void midiRealtime(const juce::MidiMessage& msg, juce::String& source) override;

    // Output Sync
    
    void start() override;
    void startClocks() override;
    void stop() override;
    void stopSelective(bool sendStop, bool stopClocks) override;
    void midiContinue() override;
    void setTempo(float tempo) override;

    float getTempo() override;
    int getRawBeat() override;
    bool isSending() override;
    bool isStarted() override;
    int getStarts() override;
    void incStarts() override;
    int getSongClock() override;

    MidiSyncEvent* nextOutputEvent() override;
    // new non-destrictive iterator
    void iterateOutput(MidiQueue::Iterator& it);
    
    // Input Sync

    int getMilliseconds() override;
    float getInputTempo() override;
    int getInputSmoothTempo() override;
    int getInputRawBeat() override;
    int getInputSongClock() override;
    bool isInputReceiving() override;
    bool isInputStarted() override;
    
    MidiSyncEvent* nextInputEvent() override;
    void iterateInput(MidiQueue::Iterator& it) override;
    
  protected:

    void advance();
    void setTempoNow(float newTempo);
    
  private:
    
    void startClocksInternal();

    class Supervisor* supervisor = nullptr;
    class MidiManager* midiManager = nullptr;
    
    //////////////////////////////////////////////////////////////////////
    // Output sync state
    //////////////////////////////////////////////////////////////////////

    MidiClockThread* thread = nullptr;

    MidiQueue outputQueue;

    /**
     * The system millisecond counter on the last advance.
     * Used to calculate how much time elapses between advances.
     */
    int lastMillisecondCounter = 0;

    // flags indicating transport events should be sent on the next advance
    // these are normally set in the audio or UI thread and cleared in the clock thread
    bool pendingStart = false;
    bool pendingContinue = false;
    bool pendingStop = false;

    // true when pendingStart or pendingContinue has been processed
    // and we're waiting 1 cycle to send the first clock
    bool pendingStartClock = false;

    // true if we're supposed to stop sending clocks after processing
    // a pendingStop
    bool pendingStopClocks = false;

    // current tempo
    float tempo = 0.0f;
    
    // pending tempo to be set on the next advance
    float pendingTempo = 0.0f;

    // number of milliseconds in each MIDI clock
    float msecsPerPulse = 0.0f;

    // amount of time to wait until the next MIDI clock
    float pulseWait = 0.0f;

    // true if we're allowing advance to send clocks
    bool running = false;

    // old stuff

    /**
     * Audio sample rate.  Used in a few cases to align MIDI events
     * with their logical locations in the audio stream.
     * Really?  Where?
     */
    int mSampleRate = 0;

    /**
     * Increments each time we send MS_START, 
     * cleared after MS_STOP.
     */
	int mStarts = 0;

    /**
     * Set at the start of each interrupt, used for timing adjusments.  
     */
	long mInterruptMsec = 0;

    //////////////////////////////////////////////////////////////////////
    // Input sync state
    //////////////////////////////////////////////////////////////////////

    MidiQueue inputQueue;
    TempoMonitor tempoMonitor;
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
