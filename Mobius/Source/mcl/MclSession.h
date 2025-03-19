// NOT USED
// This was an aborted attempt at making an interactive MCL interpreter
// It requires some major distruption to the current batch-oriented parser/evaluator
// and in retrospect offers very little

/**
 * Parse state for batch and interfactive MCL evaluation.
 */

#pragma once

#include <JuceHeader.h>

class MclSession
{
  public:

    MclSession(class Provider* p);
    ~MclSession();
    
    bool eval(juce::String src);

  private:

    class Provider* provider = nullptr;
    class MclResult* result = nullptr;
    
    std::unique_ptr<class MclSection> currentSection;

    // parse state for sessions and overlays
    std::unique_ptr<class MclScope> currentScope;

    // parse state for bindings

    // defaults
    class Trigger* bindingTrigger = nullptr;
    int bindingChannel = 0;
    juce::String bindingScope;

    // positional columns
    int typeColumn = 0;
    int channelColumn = 0;
    int valueColumn = 0;
    int symbolColumn = 0;
    int scopeColumn = 0;
    
};

