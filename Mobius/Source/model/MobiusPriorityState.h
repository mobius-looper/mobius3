/**
 * An optimized variant of MobiusState that only contains things that
 * need to be refreshed at a much higher rate than the full MobiusState.
 *
 * One of these will be maintained by the engine and refreshed upon request
 * by the UI.  It remains stable for the lifetime of the engine.  
 */

#pragma once

class MobiusPriorityState
{
  public:

    MobiusPriorityState() {}
    ~MobiusPriorityState() {}
    
    bool metronomeBeat = false;
    bool metronomeBar = false;

    // todo: other thigns that could go here
    // the focused track loopFrame, the focused track outputLevel
    // in theory need those for all tracks if you want a "meter strip"

};
