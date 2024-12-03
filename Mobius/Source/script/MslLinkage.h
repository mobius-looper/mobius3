/**
 * Represents a uniquely named object that can be referenced within the
 * script environment and/or the outside application.
 *
 * Maintained within an MslResolutionContext.
 * This is conceptually similar to the Symbols table in the Mobius application.
 *
 */

#pragma once

#include <JuceHeader.h>

class MslLinkage
{
  public:

    MslLinkage() {}
    ~MslLinkage() {}

    // the name that can be referenced by an MslSymbol
    juce::String name;

    // the compilation unit this came from
    class MslCompilation* unit = nullptr;

    // true if the function or variable is to be exported to the containing application
    bool isExport = false;

    // Behavior characteristics for use by the applicaion when this
    // is passed to mslExport
    bool isFunction = false;
    bool isSustainable = false;
    bool isContinuous = false;

    // From here down these should be accessible only within the MslEnvironment
    // try to protect them
    
    // the resolved target of the link
    class MslFunction* function =  nullptr;
    class MslVariableExport* variable = nullptr;

    // the number of times the function has been called with a Request
    // todo: need more interesting stats, like internal function calls
    // varialbe updates, etc.
    int runCount = 0;

    // used when installing a previous link
    void reset() {
        // do NOT reset the name, a Linkage with that name still exists
        // it just may not do anything
        unit = nullptr;
        function = nullptr;
        variable = nullptr;
        isFunction = false;
        isSustainable = false;
        isContinuous = false;
        runCount = 0;
    }
    
};

