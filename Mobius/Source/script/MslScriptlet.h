/**
 * A scriptlet session is used to evaluate a fragment of MSL text that
 * is not contained in a script file and loaded into the environment.
 *
 * Scriptlet text can appear anywhere in the application and used to inject
 * user defined computations, both in the UI (shell context) or within the
 * audio block processing thread (kernel).   While their use is more general
 * it is currently confined to the MobiusConsole which provides an interactive
 * command line console for executing script statements, and Binderator which
 * may use them to associate script fragments with binding triggers.
 *
 * File based scripts are normally run in the Mobius application by building
 * and executing a UIAction object associated with a Symbol bound to a script.
 * The session in this case not directly accessed by the application.  Scriptlets
 * differ in that they do not require UIAction processing to run.  They may be
 * created and stored directly on whatever application objects need to run scripts
 * and evaluated at their discression.
 *
 * To use a scriptlet you start by allocating an MslScriptlet which is created
 * and owned by the MslEnvironment.  The scriptlet session will remain alive until
 * the application shuts down and the MslEnvironment is destructructed, or may be released
 * manually by the application when no longer needed.
 *
 * Once an MslScriptSession has been obtained, you give it the fragment of text you
 * want to evaluate with the compile() method.  This may result in compiliation errors
 * which should be conveyed to the user.  Once sucessfully compiled, the script may
 * be run with the eval() method which may accept outside arguments.  What this does
 * internally is build and compile a script object to contain the script fragment much
 * like what would happen if the script were loaded from a file.  It differs in that the
 * resulting script is not installed in the environment and it may not be called by
 * other scripts or used in bindings other than scriptlets managed by the Binderator itself.
 *
 * When a scriptlet is evaluated it will either synchronously run to completion, or enter
 * one of these states:
 *
 *      transitioning
 *      waiting
 *
 * When a scriptlet is transitioning, the evaluation of the code moves to another thread, usually
 * from the UI thread that started the scriptlet and the audio thread that will run it.
 * Once in the audio thread the scriptlet may enter a wait state just like file-based scripts do.
 * Eventually it will complete and leave results in the environment for later inspection.
 *
 * While simple scripts may evaluate immediately and be done, it is important to keep
 * in mind that a script can always launch an asynchronous internal session or "thread" from
 * the perspective of the application.  If a scriptlet becomes asynchronous and you evaluate it
 * again, a second thread of execution may be created.  Concurrency can be controlled with
 * options on the scriptlet session.
 *
 * Special options are provided in the scriptlet session for use by the MobiusConsole.
 * While normally script execution threads are autonomous the console has extra requirements
 * around how the internal state is managed.
 *
 */

#pragma once

#include "MslValue.h"

class MslScriptlet {

    friend class MslEnvironment;
    
  public:

    void setName(juce::String s);

    // reset any state accumulated in this session
    void reset();

    // compile a fragment of MSL text
    // returns false if there were compilation errors
    // todo: will this need a MslContext, or should that only be required for evaluation?
    bool compile(class MslContext* context, juce::String source);
    
    // information about errors encountered during compilation
    // this object is owned by the session and will be reclained on the next compile
    juce::OwnedArray<MslError>* getCompileErrors();

    // evaluate a script after it has been compiled
    // the context passed here is normally Supervisor or MobiusKernel
    // but it can be anything else that implements the MslContext interface
    // ...well not really, MslConductor isn't equiped yet to deal with random contexts
    //
    // true is returned if the script ran synchronously to completion without error,
    // OR if the script transitioned to an asynchronous session
    bool eval(class MslContext* c);

    // errors from the last failed eval() that returned false
    class MslError* getErrors();

    // True if the last evaluation of the scriptlet ran to completion without errors
    // and the inner session was not made asynchronous
    bool isFinished();

    // true if the last evaluation had to transition
    // sessionId is valid
    bool isTransitioning();
    
    // true if the last evaluation had to wait
    // sessionId is valid
    bool isWaiting();

    // when a script became asynchronous, this may be used to obtain a handle
    // to the internal session that was launched to process the scriptlet
    // this can be used to poll for results
    // at any time this handle may become invalid and result polling will fail
    int getSessionId();

    //
    // Synchronous results
    // These are only relevant after eval() is called and isFinished() is true
    //

    // result from the last sucessful eval()
    MslValue* getResult();

    // debugging interface that dumps details about the return value
    juce::String getFullResult();
    void getResultString(MslValue* v, juce::String& s);

    //
    // Accumulated results for the console
    //

    // top-level functions encountered in the last eval()
    // these accumulate across calls to eval()
    juce::OwnedArray<MslFunction>* getFunctions();

    // bindings representing the top-level script variables encountered during
    // evaluation
    // also for console testing
    // these accumulate across calls to eval()
    MslBinding* getBindings();

    // couldn't make this protected for some reason due to the OwnedArray in MslEnvironment
    ~MslScriptlet();

    // public so that the console can see directives that have been parsed
    MslScript* getScript() {
        return script.get();
    }
    
  protected:

    // only for use by MslEnvironment

    MslScriptlet(class MslEnvironment* env);
    void resetLaunchResults();
    
    // the internal session id if one it had to be launched async
    int sessionId = 0;

    // synchronous errors encountered on the last launch
    class MslError* launchErrors = nullptr;

    // flags indicating why it was launched async
    bool wasTransitioned = false;
    bool wasWaiting = false;
    MslValue* launchResult = nullptr;
    juce::String fullResult;


  private:

    class MslEnvironment* environment = nullptr;
    // optional name for logging
    juce::String name;

    // dynamic script maintained for this session
    std::unique_ptr<class MslScript> script = nullptr;
    
};


    
