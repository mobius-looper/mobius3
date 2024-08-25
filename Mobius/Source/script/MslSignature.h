/**
 * Model to describe the arguments to a function.
 * Built manually for MslExternal and MslExport.
 * Built automatically for MslProc
 */

#pragma once

#include <JuceHeader.h>

class MslArgument
{
  public:

    MslArgument() {}
    ~MslArgument() {}

    juce::String name;

    // for script functions only, a node that evaluates to the
    // default value for this argument
    class MslNode* initializer;
};

class MslSignature
{
  public:

    MslSignature() {}
    ~MslSignature() {}

    juce::OwnedArray<MslArgument> arguments;

    // todo: don't have return values atm

};

    
