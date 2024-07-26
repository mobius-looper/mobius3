/**
 * An experiment to allow fragments of the MSL language to be
 * parsed and evaluated outside of the usual flow where MSL is contained
 * in files, and exported for use in bindings.
 *
 * This was developmed primary for the MobiusConsole but keep other
 * uses in mind.  In particular this would be good for bindings, where the
 * "target" of a binding is a script fragment rather than needing to be
 * a file of MSL script.
 *
 */

#pragma once

#include "MslValue.h"

class MslScriptletSession {
  public:

    // should only be allocated by MslEnvironment, protect this...
    MslScriptletSession(class MslEnvironment* env);
    ~MslScriptletSession();

    // evaluate a fragment of MSL text
    // returns false if there were compilation or runtime errors
    // the Context here is the initial context used until the internal
    // session needs to transition to another level
    bool eval(class MslContext* c, juce::String source);

    // true if the last script given to eval() has suspended on a wait
    bool isWaiting();

    // return information about what the session is waiting on
    class MslWait* getWait();

    // resume the suspended scriptlet
    // this is only for testing in the console, normally waits are handled
    // automatically
    void resume(class MslContext* c, class MslWait* w);

    // reset any state accumulated in this session
    void reset();

    // errors from the last failed eval()
    juce::OwnedArray<class MslError>* getErrors() {
        return &errors;
    }

    // result from the last sucessful eval()
    MslValue* getResult() {
        return scriptletResult;
    }

    // debugging interface that dumps details about the return value
    juce::String getFullResult() {
        return fullResult;
    }

    // bindings representing the top-level script variables encountered during
    // evaluation
    // also for console testing
    // these accumulate across calls to eval()
    MslBinding* getBindings() {
        return script->bindings;
    }

    // top-level procs encountered in the last eval()
    // these accumulate across calls to eval()
    juce::OwnedArray<MslProc>* getProcs();
    
  private:

    class MslEnvironment* environment = nullptr;

    // dynamic script maintained for this session
    std::unique_ptr<class MslScript> script = nullptr;
    std::unique_ptr<class MslSession> waitingSession = nullptr;

    juce::OwnedArray<class MslError> errors;
    MslValue* scriptletResult = nullptr;
    MslBinding* scriptletBindings = nullptr;
    juce::String fullResult;
    
};


    
