/*
 * Initial attempt at using the Juce thread library to manage the
 * single Mobius master thread.
 *
 * Chatter says that c++11 std:;thread is nicer and Jules said he's like
 * to use it, that was from 2013 so maybe it does by now.  Start here and
 * look at std:: later, my needs are few beyond millisecond accuracy.
 *
 * Focusing on UI refresh needs until we need more resolution for
 * MIDI output clocks.
 */

#include <JuceHeader.h>

#include "util/Trace.h"
#include "util/TraceFile.h"
#include "Supervisor.h"
#include "MainThread.h"

MainThread::MainThread(Supervisor* super) :
    Thread(juce::String("Mobius"))  // second arg is threadStackSize
{
    supervisor = super;
}

MainThread::~MainThread()
{
    if (GlobalTraceFlusher == this) {
        FlushTrace();
        GlobalTraceFlusher = nullptr;
    }
}

void MainThread::start()
{
    juce::Thread::RealtimeOptions options;

    // not sure how much all of this is necessary
    // going to have to be careful about this in the context
    // of a plugin host
    // most comments indicate that this only works for Mac or Posix
    // so start with just priority
    (void)options.withPriority(10);
    (void)options.withPeriodMs(1);

    if (!startRealtimeThread(options)) {
        Trace(1, "MainThread: Unable to start thread\n");
    }
}

void MainThread::stop()
{
    // example says: allow 2 seconds to stop cleanly - should be plenty of time
    if (!stopThread(2000)) {
        Trace(1, "MainThread: Unable to stop thread\n");
    }
}

/**
 * Looks like this is similar to Java threads, in that run() is
 * called once and if you have any timing to do you have to handle
 * that yourself.   
 */
void MainThread::run()
{
    // set this only if you want to start buffering Trace
    // leave it null during testing period where you don't want
    // trace buffering for some reason
    
    GlobalTraceFlusher = this;

    // threadShouldExit returns true when the stopThread method is called
    while (!threadShouldExit()) {

        wait(100);

        // from the Juce example
        // because this is a background thread, we mustn't do any UI work without
        // first grabbing a MessageManagerLock..
        const juce::MessageManagerLock mml (Thread::getCurrentThread());
        if (!mml.lockWasGained()) {
            // if something is trying to kill this job the lock will fail
            // in which case we better return
            return;
        }

        // flush any accumulated trace messages
        // had to move this under MessageManagerLock once UnitTestPanel started
        // intercepting messages
        if (GlobalTraceFlusher == this)
          FlushTrace();

        // hmm, not liking the double buffering
        // Should FlushTrace do this or are they independent?
        // gak, what a mess
        TraceFile.flush();

        // thread is locked, we can mess with components
        // I don't think we're going to need events to process, just
        // notify Supervisor
        supervisor->advance();
    }

    FlushTrace();
    GlobalTraceFlusher = nullptr;
}

/**
 * TraceFlusher callback indicating  a trace record
 * has been added.  The old MobiusThread used this to call signal() to
 * break the thread out of the wait state so it could call FlushTrace
 * immediately rather than waiting the full 1/10th second timeout so
 * that messages would have less lag.
 *
 * Unclear what the Juce equivalent of that is, but I'm sure there's something.
 * This doesn't really matter as long as the thread calls FlushTrace
 * regularly, it just might have lag.
 */
void MainThread::traceEvent()
{
    // signal();
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/









    
