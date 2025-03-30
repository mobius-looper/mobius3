/**
 * Representation of changes made in the session editor, used
 * by the engine when an edited session is received to seletively
 * reset parameters that had been modified dynamically outside the
 * session editor.
 */

#pragma once

#include "SymbolId.h"

class SessionDiff
{
  public:

    // track number, zero for the default parameters
    int track = 0;

    // parameter identifier
    SymbolId parameter;
};

class SessionDiffs
{
  public:

    juce::Array<SessionDiff> diffs;
};

    
