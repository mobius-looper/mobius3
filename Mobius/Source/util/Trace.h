/*
 * Trace utilities.
 * 
 * This is old and due for a rewrite but I use it EVERYWHERE and it's
 * really handy for debugging.
 *
 * Use trace() for simple explicitly requested messages that need to
 * go the debug output stream.
 *
 * Use Trace(1,... for things in the audio thread that always have the
 * potential to queue messages, but may be filtered depending on desired
 * trace level.  Trace queues the messages so they can be printed
 * outside of the audio thread.
 * 
 */

#ifndef TRACE_H
#define TRACE_H

#include <stdarg.h>
// for juce::String
#include <JuceHeader.h>

/****************************************************************************
 *                                                                          *
 *   							 SIMPLE TRACE                               *
 *                                                                          *
 ****************************************************************************/

extern bool TraceToDebug;
extern bool TraceToStdout;

void trace(const char *string, ...);
void vtrace(const char *string, va_list args);

void trace(juce::String& js);

/****************************************************************************
 *                                                                          *
 *   							 TRACE LEVELS                               *
 *                                                                          *
 ****************************************************************************/

/**
 * Trace records at this level or lower are printed to the console.
 */
extern int TracePrintLevel;

/**
 * Trace records at this level or lower are sent to the debug output stream.
 */
extern int TraceDebugLevel;

/**
 * The interace of an object that may be registered to enable
 * buffered trace.  The callback method is called whenever a new
 * record is added, which may wake the flusher out of a deep slumber.
 */
class TraceFlusher {

  public:
    virtual ~TraceFlusher() {}
	virtual void traceEvent() = 0;

};

/**
 * When set, trace messages will be buffered.
 * This object is notified whenever a trace record is added and
 * must call FlushTrace at regular intervals.
 */
extern TraceFlusher* GlobalTraceFlusher;

/**
 * Interface of an object to receive trace messages as they
 * are being processed by TraceEmit during a flush.
 * Added for the unit test log panel.
 */
class TraceListener {
  public:
    virtual ~TraceListener() {}
    virtual void traceEmit(const char* msg) = 0;
};    

/**
 * The one global listener.
 * If we allowed more than one of these it would take the place
 * of the hard wired calls to TraceClient and FileTrace in TraceEmit.
 */
extern TraceListener* GlobalTraceListener;

/****************************************************************************
 *                                                                          *
 *   							 TRACE RECORD                               *
 *                                                                          *
 ****************************************************************************/

#define MAX_TRACE_RECORDS 1000

#define MAX_ARG 64
#define MAX_MSG 256

/**
 * Encapsulates the information necessary to format a trace message.
 * Formatting is deferred so that trace records can be captured
 * in high volume time sensitive environments like digitial audio processing.
 *
 * This isn't very flexible but it gets the job done.  We allow
 * one string argument and four long arguments.  If the string argument
 * is non-NULL it is expected to be the first argument in the message.
 */
class TraceRecord {

  public:

	/* Message level */
	int level;

	/**
	 * A number printed at the beginning of the rendered message indiciating
	 * the "context" of the record.  This will be application specific,
	 * for Mobius it will be the Loop number.
	 */
	int context;

	/**
	 * A number representing "time" within the application which will
	 * generally be a monotonically increasing number in an arbitrary
	 * time base.  For Mobius this will be the frame counter within
	 * the current loop.
	 */
	long time;

    // an sprintf format string
    char msg[MAX_MSG];

    // optional string arguments
    char string[MAX_ARG];
	char string2[MAX_ARG];
	char string3[MAX_ARG];

    // optional long arguments
    long long1;
    long long2;
    long long3;
    long long4;
    long long5;
};

/****************************************************************************
 *                                                                          *
 *   							TRACE CONTEXT                               *
 *                                                                          *
 ****************************************************************************/

/**
 * The interface of an object that may be registered to provide
 * application specific context for the trace records.
 *
 * This seems to exist to let some levels of the engine include
 * track/loop/frame information, which is especially useful for events
 * on the track.  Mobius implemented but only returned 0/0.
 * 
 * Track includes:
 *
 *    *context = (getDisplayNumber() * 100) + mLoop->getNumber();
 *    *time = mLoop->getFrame();
 *
 * Why the "*100" is there is a mystery.
 *
 * Loop does:
 * 
 * 	*context = (mTrack->getDisplayNumber() * 100) + mNumber;
 * 	*time = mFrame;
 *
 * which is the same as Track, but reaches up, it could just be calling
 * mTrack->getTraceContext
 *
 * Layer does this;
 * 
 * 	if (mLoop != NULL)
 * 	  mLoop->getTraceContext(context, time);
 * 
 * So all three do the same thing.
 */
class TraceContext {

  public:
    virtual ~TraceContext() {}
	virtual void getTraceContext(int* context, long* time) = 0;

};

/**
 * An object that may be registered to provide context and time
 * info for all trace records.
 */
extern TraceContext* DefaultTraceContext;

/****************************************************************************
 *                                                                          *
 *   							BUFFERED TRACE                              *
 *                                                                          *
 ****************************************************************************/

// old functions that aren't used

void ResetTrace();
void WriteTrace(const char* file);
void AppendTrace(const char* file);
void PrintTrace();
void FlushTrace();

/****************************************************************************
 *                                                                          *
 *   						   TRACE FUNCTIONS                              *
 *                                                                          *
 ****************************************************************************/

// We have a bazillion signatures to avoid va_list overhead though I never
// measured that to see how bad it actually was.
// 
// The main thing to keep in mind here is that Trace calls are scattered
// all over extremely time senensitive audio thread code so unless the trace level
// is going to pay attention to the call, it needs to have minimal overhead and
// avoid memory allocation.
//
// So no, I'm not using fucking cout, operator overloading, String classes
// or anything else that adds overhead where it isn't needed.
// If we pass the trace level, it would be good to use something more type
// safe for formatting, but that's for another day.

void Trace(int level, const char* msg);
void Trace(TraceContext* c, int level, const char* msg);

void Trace(int level, const char* msg, const char* arg);
void Trace(TraceContext* c, int level, const char* msg, const char* arg);

void Trace(int level, const char* msg, const char* arg,
				  const char* arg2);
void Trace(TraceContext* c, int level, const char* msg, const char* arg,
				  const char* arg2);

void Trace(int level, const char* msg, const char* arg,
				  const char* arg2, const char* arg3);
void Trace(TraceContext* c, int level, const char* msg, const char* arg,
				  const char* arg2, const char* arg3);

void Trace(int level, const char* msg, const char* arg, long l1);
void Trace(TraceContext* c, int level, const char* msg, 
				  const char* arg, long l1);

void Trace(int level, const char* msg, 
				  const char* arg, const char* arg2, long l1);
void Trace(TraceContext* c, int level, const char* msg, 
				  const char* arg, const char* arg2, long l1);

void Trace(int level, const char* msg, 
				  const char* arg, const char* arg2, long l1, long l2);
void Trace(TraceContext* c, int level, const char* msg, 
				  const char* arg, const char* arg2, long l1, long l2);

void Trace(int level, const char* msg, 
				  const char* arg, const char* arg2, long l1, long l2, long l3);
void Trace(TraceContext* c, int level, const char* msg, 
				  const char* arg, const char* arg2, long l1, long l2, long l3);

void Trace(int level, const char* msg, const char* arg, 
				  long l1, long l2);
void Trace(TraceContext* c, int level, const char* msg, 
				  const char* arg, long l1, long l2);

void Trace(int level, const char* msg, long l1);
void Trace(TraceContext* c, int level, const char* msg, long l1);

void Trace(int level, const char* msg, long l1, long l2);
void Trace(TraceContext* c, int level, const char* msg, 
				  long l1, long l2);

void Trace(int level, const char* msg, long l1, long l2, long l3);
void Trace(TraceContext* c, int level, const char* msg, 
				  long l1, long l2, long l3);

void Trace(int level, const char* msg, const char* str,
				  long l1, long l2, long l3);
void Trace(TraceContext* c, int level, const char* msg, const char* str,
				  long l1, long l2, long l3);

void Trace(int level, const char* msg, 
				  long l1, long l2, long l3, long l4);
void Trace(TraceContext* c, int level, const char* msg, 
				  long l1, long l2, long l3, long l4);

void Trace(int level, const char* msg, const char* str,
				  long l1, long l2, long l3, long l4);
void Trace(TraceContext* c, int level, const char* msg, const char* str,
				  long l1, long l2, long l3, long l4);

void Trace(int level, const char* msg,
				  long l1, long l2, long l3, long l4, long l5);

void Trace(int level, const char* msg, const char* str,
				  long l1, long l2, long l3, long l4, long l5);

void Trace(TraceContext* c, int level, const char* msg, 
				  long l1, long l2, long l3, long l4, long l5);

void Trace(TraceContext* c, int level, const char* msg, const char* str,
				  long l1, long l2, long l3, long l4, long l5);

// new, trace without checking level on a pre-formatted string
// but unlike trace() support buffering
void Trace(const char* msg);

// sigh, get "ambiguous call to overloaded function"
// if it has the same name as above, this isn't used much, sort it out later
void Tracej(juce::String msg);

// only for use by TraceClient
void TraceRaw(const char* msg);

#endif
