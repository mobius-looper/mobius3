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
    
    MclScript* parse(juce::String src, class MclResult& result);

  private:

    class Provider* provider = nullptr;
    class MclResult* result = nullptr;
    
    int lineNumber = 1;
    juce::String line;
    
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

