/**
 * Simple debug output utilities.
 * Used because Juce DBG() seems to have a lot of overhead, not sure why.
 * OutputDebugString isn't that fast either, will need to port over the
 * original queued output trace eventually.
 */

// necessary for OutputDebugString
// debugapi.h alone didn't work, get some kind of missing architecture error
// windows.h seems to cure that

#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>

#ifndef __APPLE__
#include <windows.h>
#endif

#include <string>
#include <sstream>

// this was taken from the original Mobius Trace file, don't need all of these yet
/*
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdarg.h>

// only works on Windows, Mac port eventually
#include <io.h>
#include <windows.h>
*/

void qtrace(const char* str)
{
#ifndef __APPLE__
    OutputDebugString(str);
#else
    printf("%s", str);
    fflush(stdout);
#endif    
}

void qtrace(std::string* s)
{
    qtrace(s->c_str());
}

void qtrace(std::ostringstream* os)
{
    qtrace(os->str().c_str());
}
