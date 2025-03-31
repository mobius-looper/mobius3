/**
 * Representation of changes made in the session editor, used
 * by the engine when an edited session is received to seletively
 * reset parameters that had been modified dynamically outside the
 * session editor.
 */

#pragma once

class SessionDiff
{
  public:

    // track number, zero for the default parameters
    int track = 0;

    // parameter identifier
    class Symbol* symbol = nullptr;
};

class SessionDiffs
{
  public:

    juce::Array<SessionDiff> diffs;
};

    
