/**
 * Simple debug output utilities.
 * Used because Juce DBG() seems to have a lot of overhead, not sure why.
 *
 * This is temporary until we get the original Mobius Trace() library ported
 * rewritten.
 */
#pragma once

#include <string>
#include <sstream>

void qtrace(const char* buf);
void qtrace(std::string* str);
void qtrace(std::ostringstream* os);

