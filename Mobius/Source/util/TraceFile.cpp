/*
 * Experimental tool to direct Trace messages to a log file.
 * Hacked together when TraceClient/DebugWindow was broken.
 *
 * Not terribly concerned with efficiency here, just get something
 * functional for the unit tests.
 *
 * If you decide that file-base logging is a good thing to take forward
 * then it would be good to get something like this out from under the
 * old Trace system.
 *
 * setFile() must be called to give it a proper directory location for the
 * file, the file will be created if it doesn't exist, but not the
 * folder hierarchy.
 *
 * Might be convenient to have this auto-generate the log file location
 * using RootLocator but that's not in the util layer yet and it looks
 * for mobius.xml.                  
 *
 */

#include <JuceHeader.h>

#include "Trace.h"
#include "TraceFile.h"

// the global singleton
class TraceFile TraceFile;

TraceFile::TraceFile()
{
}


TraceFile::~TraceFile()
{
    flush();
}

void TraceFile::setEnabled(bool b)
{
    if (b != enabled) {
        if (!b)
          flush();
        enabled = b ;
    }
}


void TraceFile::add(const char* msg)
{
    if (enabled && msg != nullptr) {
        buffer += msg;
        lines++;
        if (lines >= linesPerFlush) {
            flush();
        }
    }
}

/**
 * Set the file used for the trace log.
 * Expected to be called with a valid file path.
 * The file will be created if it does not exist.
 *
 * !! File.create will auto-create paraent directories which
 * may not be what you want.
 *
 * If creation fails, we don't keep retrying it on every flush.
 * logging will be disabled until you give it a good file.
 */
bool TraceFile::setFile(juce::File file)
{
    bool success = false;

    if (file.exists()) {
        logfile = file;
        TraceRaw("TraceFile: Opened log file ");
        TraceRaw(file.getFullPathName().toUTF8());
        TraceRaw("\n");
    }
    else {
        // !! this will create parent directories, might want to control that
        juce::Result result = file.create();
        if (result.wasOk()) {
            logfile = file;
            success = true;
            TraceRaw("TraceFile: Created log file ");
            TraceRaw(file.getFullPathName().toUTF8());
            TraceRaw("\n");
        }
        else {
            TraceRaw("TraceFile: Unable to create log file ");
            TraceRaw(file.getFullPathName().toUTF8());
            TraceRaw("\n");
        }
    }
    return success;
}
    

void TraceFile::flush()
{
    // assuming this test is relatively fast
    if (logfile.exists()) {
        logfile.appendText(buffer, false, false, nullptr);
    }

    // so the buffer doesn't grow without bounds if the
    // file was messed up, always clear it
    buffer.clear();
    lines = 0;
}

void TraceFile::clear()
{
    if (logfile.exists()) {
        logfile.replaceWithText("", false, false, nullptr);
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
