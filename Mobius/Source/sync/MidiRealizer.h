/**
 * Generator of MIDI realtime events.
 * Used under the Transport when clock generation is enabled.
 *
 * What used to be called SyncMode=Out is now more like track control
 * over the system transport with the option to make it generate MIDI clocks.
 *
 * The Realizer is independent of Transport and maintains it's own tempo,
 * beatsPerBar and other control parameters.  In current usage however, it will
 * always be a slave to the Transport and track the corresponding Transport parameters
 * exactly.
 * Not seeing a reason to generate clocks at a tempo independent of the Transport
 * but it's possible.
 */

#pragma once

#include <JuceHeader.h>

#include "../MidiManager.h"

#include "MidiQueue.h"

/**
 * High resolution thread used when generating MIDI clocks and sending
 * transport messages to a device.
 */
class MidiClockThread : public juce::Thread
{
  public:
    
    MidiClockThread(class MidiRealizer* gen);
    ~MidiClockThread();

    bool start();
    void stop();

    void run() override;

  private:

    class MidiRealizer* realizer;
    
};

class MidiRealizer
{
    friend class MidiClockThread;
    
  public:

    MidiRealizer();
    ~MidiRealizer();

    void initialize(class SyncMaster* sm, class MidiManager* mm);
    void setSampleRate(int rate);
    void shutdown();

    void enableEvents();
    void disableEvents();
    void flushEvents();
    
    void startThread();
    void stopThread();
    
    // Transport control
    
    void start();
    void startClocks();
    void stop();
    void stopSelective(bool sendStop, bool stopClocks);
    void midiContinue();
    void setTempo(float tempo);

    // State

    float getTempo();
    int getBeat();
    bool isSending();
    bool isStarted();
    int getStarts();
    void incStarts();
    int getSongClock();

    // Events

    MidiSyncEvent* nextEvent();
    // new non-destrictive iterator
    // since the consumer of this, Pulsator, was moved up here, we could
    // just expose the MidiQueue and be done with it
    void iterateStart();
    MidiSyncEvent* iterateNext();
    
  protected:

    // this is called from the clock thread NOT the SyncMaster on audio blocks
    void advance();
    void setTempoNow(float newTempo);
    
  private:
    
    void startClocksInternal();

    class SyncMaster* syncMaster = nullptr;
    class MidiManager* midiManager = nullptr;
    
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

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
