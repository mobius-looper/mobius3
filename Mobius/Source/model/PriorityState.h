/**
 * An optimized variant of SystemState that only contains things that
 * need to be refreshed at a much higher rate than the full SystemState.
 */

#pragma once

class PriorityState
{
  public:

    bool transportBeat = false;
    bool transportBar = false;
    
    // todo: other thigns that could go here
    // the focused track loopFrame, the focused track outputLevel
    // in theory need those for all tracks if you want a "meter strip"

};
