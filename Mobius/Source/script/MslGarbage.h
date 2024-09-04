/**
 * Holder for objects that may be referenced by compilations and
 * still in use by running sessions.
 *
 * As definitions of functions change, the prior definition must be placed
 * in garbage and reclaimed when it is safe.
 */

#pragma once

#include <JuceHeader.h>

class MslGarbage
{
  public:

    MslGarbage();
    ~MslGarbage();

    void add(class MslFunction* f) {
        functions.add(f);
    }

    void add(class MslVariableExport* v) {
        variables.add(v);
    }

    void add(class MslResolutionContext* c) {
        contexts.add(c);
    }

  protected:

    juce::OwnedArray<MslFunction> functions;
    // do we need to gc these?
    juce::OwnedArray<MslVariable> variables;

    juce::OwnedArray<MslResolutionContext> contexts;

};


    
