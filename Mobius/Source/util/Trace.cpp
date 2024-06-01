//
// This is very old code that could use a refresh, but it is used EVERYWHERE
// and I needed it early
//
// Replaced CriticalSection with Juce CriticalSection/ScopedLock
// Lingering issues about where to get io.h and windows.h for DebugOutputStream
// Forced it on for now, but when we get to the Mac port this will need
// to be addressed.  Need something in Juce we can test for selective includes.
//
// Soon to be integrated with the DebugWindow utility so we can have an alternative
// to OutputDebugStream on Mac
//

/*
 * Copyright (c) 2024 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Trace utilities.
 * 
 * This is used to send messages to the Windows debug output stream which
 * is necessary since stdout is not accessible when running as a plugin.
 * Stdout is also not visible when running under Visual Studio.
 *
 * Mac doesn't have this, I've been just using stdout/stderr and doing most
 * development on Windows.  With the addition of Juce it would be interesting
 * to explore having a Juce window async window that could be used for this
 * on both platforms.
 *
 * This is called from the audio interrupt and must be fast and not do
 * anything dangerous like allocating dynamic memory.
 *
 * For both of these reasons trace records are normally simply accumulated
 * in a global array which is then sent to the appropriate display method
 * by another thread outside the interrupt.
 *
 * Adding trace records should be synchronized since records can be added
 * by concurrent threads.  In practice there will only be one thread pulling
 * records out of the queue.
 *
 * The records are acumulated as a ring buffer with a head
 * pointer advanced as trace messages are added, and a tail pointer advanced
 * as messages are displayed.  This can be bypassed for testing purposes
 * by setting the TracetoStdout flag.
 *
 */

// CriticalSection/ScopedLocked
#include <JuceHeader.h>

#include <stdio.h>
#include <stdarg.h>

// Had been using #ifdef _WIN32 forever but that's gone under
// Juce and I can't find an equivalent.  There are supposed to be some
// standard symbols for each compiler I use, but unclear how standard
// those are.  __APPLE__ seems to work which we can negate but this
// will break if we ever get to an ANDROID port

#ifndef __APPLE__
#include <io.h>
#include <windows.h>
#endif

#include "Util.h"
#include "TraceClient.h"
#include "TraceFile.h"
#include "Trace.h"

// forward reference to private function 
extern void TraceEmit(const char* msg);

/****************************************************************************
 *                                                                          *
 *   							 SIMPLE TRACE                               *
 *                                                                          *
 ****************************************************************************/

//
// Simple non-buffering trace used in non-critical UI threads
// Added support for juce::String
//
// todo: shoudl this use TraceEmit now to go to the console
// or keep it simple?
//

bool TraceToDebug = true;
bool TraceToStdout = false;

// internal method that deals with a single char array
void traceInternal(const char* buf)
{
	if (TraceToStdout) {
		printf("%s", buf);
		fflush(stdout);
	}

	if (TraceToDebug) {
#ifndef __APPLE__
		OutputDebugString(buf);
#else
		// OSX sadly doesn't seem to have anything equivalent emit to stderr
		// if we're not already emitting to stdout
		if (!TraceToStdout) {
			fprintf(stderr, "%s", buf);
			fflush(stderr);
		}
#endif
	}
}

void vtrace(const char *string, va_list args)
{
	char buf[1024];
	vsnprintf(buf, sizeof(buf), string, args);
    traceInternal(buf);
}

void trace(const char *string, ...)
{
    va_list args;
    va_start(args, string);
	vtrace(string, args);
    va_end(args);
}

void trace(juce::String& s)
{
    // hack, these typically are using String concatenation and don't have newlines
    s += "\n";
    traceInternal(s.toUTF8());
}

/****************************************************************************
 *                                                                          *
 *   							TRACE RECORDS                               *
 *                                                                          *
 ****************************************************************************/
/*
 * Trace mechanism optimized for the gathering of potenitally
 * large amounts of trace data, such as in digital audio processing.
 */

/**
 * Trace records at this level or lower are printed to the console.
 * Disable this and eventually remove it, printf is useless under VStudio
 */
int TracePrintLevel = 0;

/**
 * Trace records at this level or lower are sent to the debug output stream.
 */
int TraceDebugLevel = 1;

/**
 * When set, trace messages for both the print and debug streams
 * are queued and must be periodically flushed.
 */
TraceFlusher* GlobalTraceFlusher = nullptr;

/**
 * When set, rendered trace messages are sent here during flush.
 */
TraceListener* GlobalTraceListener = nullptr;

/**
 * Trace records are accumulated in a global array.
 * In theory there could be thread synchronization problems, 
 * but in practice that would be rare as almost all trace messages
 * come from the interrupt thread.  I don't want to mess with
 * csects here, the only potential problem is loss of a message.
 */
TraceRecord TraceRecords[MAX_TRACE_RECORDS];

/**
 * Csect needed for the head pointer since we can be adding
 * trace from several threads.
 * You must only flush trace from one thread.
 */

// converted to Juce, and also it no longer leaks
//CriticalSection* TraceCsect = new CriticalSection("Trace");
juce::CriticalSection TraceCriticalSection;

/**
 * Index into TraceRecords of the first active record.
 * If this is equal to TraceTail, then the message queue is empty.
 */
int TraceHead = 0;

/**
 * The index into TraceRecords of the next available record.
 */
int TraceTail = 0;

/**
 * A default object that may be registered to provide context and time
 * info for all trace records.
 */
TraceContext* DefaultTraceContext = nullptr;

bool TraceInitialized = false;

void TraceBreakpoint()
{
	int x = 0;
    (void)x;
}

void ResetTrace()
{
    //TraceCsect->enter();
    const juce::ScopedLock lock (TraceCriticalSection);
    TraceHead = 0;
    TraceTail = 0;
    //TraceCsect->leave();
}

/**
 * Fix an argument to it is safe to copy.
 * 
 * Note that we can't tell the difference between NULL and 
 * empty string once we copy, which is important in order to select
 * the right sprintf argument list.  If either of these are 
 * non-null but empty, convert them to a single space so we know
 * that a string is expected at this position.
 */
void SaveArgument(const char* src, char* dest)
{
    dest[0] = 0;
    if (src != nullptr) {
        if (strlen(src) == 0) 
          src = " ";

        // wtf was this trying to do?
        // not reliable with 64-bit
        //else if ((unsigned long)src < 65535)
        //src = "INVALID";
        
        CopyString(src, dest, MAX_ARG);
    }
}

void SaveMessage(const char* src, char* dest)
{
    dest[0] = 0;

    if (src == nullptr || src[0] == 0)
      src = "!!!!!! MISSING TRACE MESSAGE !!!!!!";
    
    if (src != nullptr) {
        CopyString(src, dest, MAX_MSG);
    }
}

/**
 * Add a trace record to the trace array.
 * If we're queueing and we fill the record array, we can either lose
 * old records or new ones.  New ones are more important, but we're not
 * supposed to increment TraceHead if TraceQueued is on.
 * 
 */
void AddTrace(TraceContext* context, int level, 
              const char* msg, 
              const char* string1, 
              const char* string2,
              const char* string3,
              long l1, long l2, long l3, long l4, long l5)
{
	// kludge: trying to track down a problem, make sure the 
	// records are initialized
	if (!TraceInitialized) {
		for (int i = 0 ; i < MAX_TRACE_RECORDS ; i++)
          TraceRecords[i].msg[0] = 0;
		TraceInitialized =  true;
	}

    // trying to detect something weird
    if (msg == nullptr || strlen(msg) == 0) {
        msg = "!!!!!!!!!!! SHOULDN'T BE HERE !!!!!!!!!!!!!!";
    }

	// only queue if it falls within the interesting levels
	if (level <= TracePrintLevel || level <= TraceDebugLevel) {

        // must csect this, the TraceTail can't advance
        // until we've fully initialized the record or else
        // the flush thread can try to render a partially
        // initialized record
        //TraceCsect->enter();
        // note: the new Juce csect will have a longer scope
        // than the old one, we released it early if we had a buffer overflow,
        // shouldn't matter
        const juce::ScopedLock lock (TraceCriticalSection);

		TraceRecord* r = &TraceRecords[TraceTail];

		int nextTail = TraceTail + 1;
		if (nextTail >= MAX_TRACE_RECORDS)
		  nextTail = 0;

		if (nextTail == TraceHead) {
            // overflow
            // originally we bumped the head but that causes
            // problems if the flush thread is active at the moment,
            // we can overwtite the record being flushed causing
            // a sprintf format/argument mismatch.  This can
            // only happen when the refresh thread is bogged down 
            // or excessive trace is being generated

            // used to leave the csect early, but it's easier just to let
            // it extend to the original scope
            // TraceCsect->leave();
            TraceEmit("WARNING: Trace record buffer overflow!!\n");
		}
        else {
            // use the default context if none explictily passedn
            if (context == nullptr)
              context = DefaultTraceContext;

            if (context != nullptr)
              context->getTraceContext(&(r->context), &(r->time));
            else {
                r->context = 0;
                r->time = 0;
            }

            r->level = level;
            r->long1 = l1;
            r->long2 = l2;
            r->long3 = l3;
            r->long4 = l4;
            r->long5 = l5;
            r->string[0] = 0;
            r->string2[0] = 0;
            r->string3[0] = 0;

            try {
                SaveMessage(msg, r->msg);
                SaveArgument(string1, r->string);
                SaveArgument(string2, r->string2);
                SaveArgument(string3, r->string3);
            }
            catch (...) {
                printf("Trace: Unable to copy string arguments!\n");
            }

            // only change the tail after the record is fully initialized
            TraceTail = nextTail;
            //TraceCsect->leave();
        }

		// spot to hang a breakpoint
		if (level <= 1)
		  TraceBreakpoint();
	}
}

/**
 * Variant of the above that just creates a TraceRecord
 * with a pre-formatted string and skips level checking.
 * Used for script Echo statement
 */
void AddTrace(const char* msg) 
{
	// kludge: trying to track down a problem, make sure the 
	// records are initialized
	if (!TraceInitialized) {
		for (int i = 0 ; i < MAX_TRACE_RECORDS ; i++)
          TraceRecords[i].msg[0] = 0;
		TraceInitialized =  true;
	}
    
    const juce::ScopedLock lock (TraceCriticalSection);
    
    TraceRecord* r = &TraceRecords[TraceTail];

    int nextTail = TraceTail + 1;
    if (nextTail >= MAX_TRACE_RECORDS)
      nextTail = 0;

    if (nextTail == TraceHead) {
        TraceRaw("WARNING: Trace record buffer overflow!!\n");
    }
    else {
        r->level = 0;
        r->context = 0;
        r->time = 0;
        r->long1 = 0;
        r->long2 = 0;
        r->long3 = 0;
        r->long4 = 0;
        r->long5 = 0;
        r->string[0] = 0;
        r->string2[0] = 0;
        r->string3[0] = 0;

        try {
            SaveMessage(msg, r->msg);
        }
        catch (...) {
            printf("Trace: Unable to copy string arguments!\n");
        }
        
        // only change the tail after the record is fully initialized
        TraceTail = nextTail;
    }
}

/**
 * Render the contents of a trace record to a character buffer.
 * !! need max handling.
 */
void RenderTrace(TraceRecord* r, char* buffer, size_t size)
{
	if (r->msg[0] == 0)
	  snprintf(buffer, size, "ERROR: Invalid trace message!\n");
	else {
        try {
            // For trace message that didn't have a TraceContext this was leaving
            // usless "0 0" in front of everything.  Suppress that, though if we just
            // happen to be at frame zero in a Track what then?  Maybe -1 would
            // be better for that
            if (r->context > 0 || r->time > 0) {
                snprintf(buffer, size, "%s%d %ld: ", ((r->level == 1) ? "ERROR: " : ""),
                        r->context, r->time);
            }
            else if (r->level == 1) {
                strcpy(buffer, "ERROR: ");
            }
            else {
                strcpy(buffer, "");
            }
            buffer += strlen(buffer);

            if (strlen(r->string3) > 0) {
                snprintf(buffer, size, r->msg, r->string, r->string2, r->string3,
                         r->long1, r->long2, r->long3, r->long4, r->long5);
            }
            else if (strlen(r->string2) > 0) {
                snprintf(buffer, size, r->msg, r->string, r->string2, 
                         r->long1, r->long2, r->long3, r->long4, r->long5);
            }
            else if (strlen(r->string) > 0) {
                snprintf(buffer, size, r->msg, r->string, 
                         r->long1, r->long2, r->long3, r->long4, r->long5);
            }
            else {
                snprintf(buffer, size, r->msg, r->long1, r->long2, r->long3, r->long4, r->long5);
            }
        }
        catch (...) {
            // don't let malformed trace args ruin your day
            // actually this will rarely work because SEGV or misalligned
            // pointers raise signals rather than throw exceptions
            snprintf(buffer, size, "ERROR: Exception rendering trace: %s\n", r->msg);
        }

        // this is so easy to miss
        int len = (int)strlen(buffer);
        if (len > 0) {
            char last = buffer[len-1];
            if (last != '\n') {
                buffer[len] = '\n';
                buffer[len+1] = 0;
            }
        }

		// keep this clear so we can try to detect anomolies in the
		// head/tail iteration
		r->msg[0] = 0;
	}
}

/****************************************************************************
 *                                                                          *
 *   						BUFFERED TRACE OUTPUT                           *
 *                                                                          *
 ****************************************************************************/

#if 0
void WriteTrace(FILE* fp)
{
	char buffer[1024 * 8];

	// guard against mods during the flush, really that safe?
    int tail = TraceHead;
    {
        const juce::ScopedLock lock (TraceCriticalSection);
        tail = TraceTail;
    }

    fprintf(fp, "=========================================================\n");
	while (TraceHead != tail) {
        TraceRecord* r = &TraceRecords[TraceHead];
		RenderTrace(r, buffer);
		fprintf(fp, "%s", buffer);
		TraceHead++;
		if (TraceHead >= MAX_TRACE_RECORDS)
		  TraceHead = 0;
    }
}

void WriteTrace(const char* file)
{
	// guard against mods during the flush, really that safe?
    int tail = TraceHead;
    {
        const juce::ScopedLock lock (TraceCriticalSection);
        tail = TraceTail;
    }

	if (TraceHead != tail) {
		FILE* fp = fopen(file, "w");
		if (fp != nullptr) {
			WriteTrace(fp);
			fclose(fp);
		}
		else
		  printf("Unable to open trace output file %s\n", file);
	}
}

void AppendTrace(const char* file)
{
	// guard against mods during the flush, really that safe?
    int tail = TraceHead;
    {
        const juce::ScopedLock lock (TraceCriticalSection);
        tail = TraceTail;
    }
    
	if (TraceHead != tail) {
		FILE* fp = fopen(file, "a");
		if (fp != nullptr) {
			WriteTrace(fp);
			fclose(fp);
		}
		else
		  printf("Unable to open trace output file %s\n", file);
	}
}

void PrintTrace()
{
    WriteTrace(stdout);
}
#endif

/**
 * Once a global trace listener is installed this
 * can be called from multiple threads so have
 * lock the whole thing.
 */
void FlushTrace()
{
    const juce::ScopedLock lock (TraceCriticalSection);
    
	char buffer[1024 * 8];
	
    // shouldn't be necessary to capture this now that we're
    // in a larger csect
    int tail = TraceTail;

	while (TraceHead != tail) {
        TraceRecord* r = &TraceRecords[TraceHead];
		RenderTrace(r, buffer, sizeof(buffer));

        // not used any more what what the heck
        // note that the way AddTrace works, if you
        // set TracePrintLevel high it will now
        // make TraceEmit unconditional, but you
        // shoudlnt' be using PrintLevel anyway
		if (r->level <= TracePrintLevel) {
			printf("%s", buffer);
			fflush(stdout);
		}

        TraceEmit(buffer);

		TraceHead++;
		if (TraceHead >= MAX_TRACE_RECORDS)
		  TraceHead = 0;
    }
}

/**
 * Flush the messages or notify the listener
 *
 * In old code the listener just called signal() to break
 * it out of the thread loop wait.  It would then
 * be expected to immediately call FlushTrace.
 * So technically traceEvent() doesn't do the flush.
 * We'll be getting FlushTrace calls periodically with or
 * without notifying the listener.
 */
void FlushOrNotify()
{
	if (GlobalTraceFlusher != nullptr)
	  GlobalTraceFlusher->traceEvent();
	else
	  FlushTrace();
}

/****************************************************************************
 *                                                                          *
 *   							TRACE METHODS                               *
 *                                                                          *
 ****************************************************************************/

/**
 * New, more direct trace that skips levels,  formatting, and all the folderol.
 * Used for script Echo, and possibly elsewhere.
 */
void Trace(const char* msg)
{
    if (msg != nullptr) {
        AddTrace(msg);
        FlushOrNotify();
    }
}

// not used for performance critical stuff, mostly optional structure dumps
void Tracej(juce::String msg)
{
    Trace(msg.toUTF8());
}

/**
 * Called for every string argument to make sure that it has a value,
 * otherwise when the trace record is rendered we pick the wrong
 * set of args for the format string.
 */
const char* CheckString(const char* arg)
{
    if (arg == nullptr || strlen(arg) == 0) {
        // originally had "???" and then "null"
        // but this happens in a few places where we just don't
        // want to display anything, so leave empty
        arg = "";
    }
    return arg;
}

void Trace(int level, const char* msg)
{
	Trace(nullptr, level, msg);
}

void Trace(TraceContext* context, int level, const char* msg)
{
    AddTrace(context, level, msg, nullptr, nullptr, nullptr, 0, 0, 0, 0, 0);
	FlushOrNotify();
}

void Trace(int level, const char* msg, const char* arg)
{
	Trace(nullptr, level, msg, arg);
}

void Trace(TraceContext* context, int level, const char* msg, 
				  const char* arg)
{
    arg = CheckString(arg);
	AddTrace(context, level, msg, arg, nullptr, nullptr, 0, 0, 0, 0, 0);
	FlushOrNotify();
}

void Trace(int level, const char* msg, const char* arg,
				  const char* arg2)
{
	Trace(nullptr, level, msg, arg, arg2);
}

void Trace(TraceContext* context, int level, const char* msg, 
				  const char* arg, const char* arg2)
{
    arg = CheckString(arg);
    arg2 = CheckString(arg2);
    AddTrace(context, level, msg, arg, arg2, nullptr, 0, 0, 0, 0, 0);
	FlushOrNotify();
}

void Trace(int level, const char* msg, const char* arg,
				  const char* arg2, const char* arg3)
{
	Trace(nullptr, level, msg, arg, arg2, arg3);
}

void Trace(TraceContext* context, int level, const char* msg, 
				  const char* arg, const char* arg2, const char* arg3)
{
    arg = CheckString(arg);
    arg2 = CheckString(arg2);
    arg3 = CheckString(arg3);
    AddTrace(context, level, msg, arg, arg2, arg3, 0, 0, 0, 0, 0);
	FlushOrNotify();
}

void Trace(int level, const char* msg, const char* arg, long l1)
{
	Trace(nullptr, level, msg, arg, l1);
}

void Trace(TraceContext* context, int level, const char* msg, 
				  const char* arg, long l1)
{
    arg = CheckString(arg);
    AddTrace(context, level, msg, arg, nullptr, nullptr, l1, 0, 0, 0, 0);
	FlushOrNotify();
}

void Trace(int level, const char* msg, 
				  const char* arg, const char* arg2, long l1)
{
	Trace(nullptr, level, msg, arg, arg2, l1);
}

void Trace(int level, const char* msg, 
				  const char* arg, const char* arg2, long l1, long l2)
{
	Trace(nullptr, level, msg, arg, arg2, l1, l2);
}

void Trace(int level, const char* msg, 
				  const char* arg, const char* arg2, long l1, long l2, long l3)
{
	Trace(nullptr, level, msg, arg, arg2, l1, l2, l3);
}

void Trace(TraceContext* context, int level, const char* msg, 
				  const char* arg, const char* arg2, long l1)
{
    arg = CheckString(arg);
    arg2 = CheckString(arg2);
    AddTrace(context, level, msg, arg, arg2, nullptr, l1, 0, 0, 0, 0);
	FlushOrNotify();
}

void Trace(TraceContext* context, int level, const char* msg, 
				  const char* arg, const char* arg2, long l1, long l2)
{
    arg = CheckString(arg);
    arg2 = CheckString(arg2);
    AddTrace(context, level, msg, arg, arg2, nullptr, l1, l2, 0, 0, 0);
	FlushOrNotify();
}

void Trace(TraceContext* context, int level, const char* msg, 
				  const char* arg, const char* arg2, long l1, long l2, long l3)
{
    arg = CheckString(arg);
    arg2 = CheckString(arg2);
    AddTrace(context, level, msg, arg, arg2, nullptr, l1, l2, l3, 0, 0);
	FlushOrNotify();
}

void Trace(int level, const char* msg, 
				  const char* arg, long l1, long l2)
{
	Trace(nullptr, level, msg, arg, l1, l2);
}

void Trace(TraceContext* context, int level, const char* msg, 
				  const char* arg, long l1, long l2)
{
    arg = CheckString(arg);
    AddTrace(context, level, msg, arg, nullptr, nullptr, l1, l2, 0, 0, 0);
	FlushOrNotify();
}

void Trace(int level, const char* msg, long l1)
{
	Trace(nullptr, level, msg, l1);
}

void Trace(TraceContext* context, int level, const char* msg, long l1)
{
    AddTrace(context, level, msg, nullptr, nullptr, nullptr, l1, 0, 0, 0, 0);
	FlushOrNotify();
}

void Trace(int level, const char* msg, long l1, long l2)
{
	Trace(nullptr, level, msg, l1, l2);
}

void Trace(TraceContext* context, int level, const char* msg, 
				  long l1, long l2)
{
    AddTrace(context, level, msg, nullptr, nullptr, nullptr, l1, l2, 0, 0, 0);
	FlushOrNotify();
}

void Trace(int level, const char* msg, 
				  long l1, long l2, long l3)
{
	Trace(nullptr, level, msg, l1, l2, l3);
}

void Trace(TraceContext* context, int level, const char* msg, 
				  long l1, long l2, long l3)
{
    AddTrace(context, level, msg, nullptr, nullptr, nullptr, l1, l2, l3, 0, 0);
	FlushOrNotify();
}

void Trace(int level, const char* msg, const char* arg,
				  long l1, long l2, long l3)
{
	Trace(nullptr, level, msg, arg, l1, l2, l3);
}

void Trace(TraceContext* context, int level, const char* msg, 
				  const char* arg,
				  long l1, long l2, long l3)
{
    arg = CheckString(arg);
    AddTrace(context, level, msg, arg, nullptr, nullptr, l1, l2, l3, 0, 0);
	FlushOrNotify();
}

void Trace(int level, const char* msg, 
				  long l1, long l2, long l3, long l4)
{
	Trace(nullptr, level, msg, l1, l2, l3, l4);
}

void Trace(TraceContext* context, int level, const char* msg, 
				  long l1, long l2, long l3, long l4)
{
    AddTrace(context, level, msg, nullptr, nullptr, nullptr, l1, l2, l3, l4, 0);
	FlushOrNotify();
}

void Trace(TraceContext* context, int level, const char* msg, 
				  const char* arg,
				  long l1, long l2, long l3, long l4)
{
    arg = CheckString(arg);
    AddTrace(context, level, msg, arg, nullptr, nullptr, l1, l2, l3, l4, 0);
	FlushOrNotify();
}

void Trace(int level, const char* msg, 
                  const char* arg,
				  long l1, long l2, long l3, long l4)
{
	Trace(nullptr, level, msg, arg, l1, l2, l3, l4);
}

void Trace(int level, const char* msg, 
				  long l1, long l2, long l3, long l4, long l5)
{
	Trace(nullptr, level, msg, l1, l2, l3, l4, l5);
}

void Trace(int level, const char* msg, const char* arg,
				  long l1, long l2, long l3, long l4, long l5)
{
	Trace(nullptr, level, msg, arg, l1, l2, l3, l4, l5);
}

void Trace(TraceContext* context, int level, const char* msg, 
				  long l1, long l2, long l3, long l4, long l5)
{
    Trace(context, level, msg, nullptr, l1, l2, l3, l4, l5);
}

void Trace(TraceContext* context, int level, const char* msg, 
                  const char* arg,
                  long l1, long l2, long l3, long l4, long l5)
{
    arg = CheckString(arg);
    AddTrace(context, level, msg, arg, nullptr, nullptr, l1, l2, l3, l4, l5);
	FlushOrNotify();
}

//////////////////////////////////////////////////////////////////////.
//
// Trace Emitters
//
// This is new, to support directing trace messages to one of several
// destinations, notably the TraceClient and DebugWindow
//
//////////////////////////////////////////////////////////////////////.

/**
 * All the old code should eventually make it's way down here,
 * either synchronsly if GlobalTraceFlusher isn't set, or
 * asynchronusly if trace is flushed in a maintenance thread.
 *
 * We've passed TraceDebugLevel and have rendered the TraceRecord
 * into a single string.
 *
 * There are now three places this can go: stdout, the debug output stream
 * or the new DebugWindow.
 *
 * I don't really need stdout any more since that's useless under Visual Studio
 * and I don't run from the command line any more.
 *
 * Always send to the debug output stream.
 *
 * Whether or not we send to the debug window will be controlled
 * within that class, but the behavior of that must be extremely robust.
 * If something goes wrong do not throw exceptions or cause any other
 * disruption that would prevent at least the debug output stream from
 * getting fed.
 *
 * As usual Mac's don't have OutputDebugString so send it to stderr which
 * seemed to be reliable than stdout at the time.
 */
void TraceEmit(const char* msg)
{
#ifndef __APPLE__
    OutputDebugString(msg);
#else
    fprintf(stderr, "%s", msg);
    fflush(stderr);
#endif

    if (GlobalTraceListener != nullptr)
      GlobalTraceListener->traceEmit(msg);

    // this never worked, and we don't really need it
    // the file and the listener are good enough and better in some ways
    //TraceClient.send(msg);
    
    // this turns out to be very useful, even if there is a listener
    TraceFile.add(msg);
}

/**
 * So TraceClient can tell us things when it is broken, this
 * is the least we can do.
 *
 * Also used in a few places to trace large things like XML strings
 * that are too large to fit in TraceRecords.
 */
void TraceRaw(const char* msg)
{
#ifndef __APPLE__
    OutputDebugString(msg);
#else
    fprintf(stderr, "%s", msg);
    fflush(stderr);
#endif
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
