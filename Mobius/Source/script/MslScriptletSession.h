/**
 * An experiment to allow fragments of the MSL language to be
 * parsed and evaluated outside of the usual flow where MSL is contained
 * in files, parsed, and saved in the MslEnvironment for use in bindings.
 *
 * This was developmed primary for the MobiusConsole but keep other
 * uses in mind.  In particular this would be good for bindings, where the
 * "target" of a binding is a script fragment rather than needing to be
 * a file of MSL script.
 *
 */

#pragma once

class MslScriptletSession {
  public:

    MslScriptletSession(MslEnvironment* env);
    ~MslScriptletSession();

    // reset any state accumulated in this session
    // used only for interactive sessions like the MobiusConsole
    void reset();

    // parse one line of MSL text and evaluate it
    void eval(juce::String source);

    // todo: results and errors

  private:

    // dynamic script maintained for this session
    std::unique_ptr<class MslScript> script = nullptr;

    // active session created if the script needs to suspend
    //class MslSession* session = nullptr;

};


    
