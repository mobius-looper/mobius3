/**
 * Runtime state maintained during the evaluation of a script.
 *
 * This is conceptually similar to ScriptInterpreter in the old language.
 */

#pragma once

class MslSession
{
  public:
    
    MslSession() {}
    MslSession(Script* s) {
        script = s;
    }
    ~MslSession() {}

    void start();
    void start(Script* s);

    /**
     * A listener may be registered to receive notifications of things
     * that happen during evaluation.  Currently this is only trace messages.
     */
    class Listener {
      public:
        virtual ~Listener() {}
        void MslTrace(const char* msg) = 0;
    }

  private:

    // the evaluator that does most of the work
    MslEvaluator evaluator;
    
    // the script being run
    // the session does not own the script, it is expected to be held
    // in an MslEnvironment or a random scriptlet container
    Script* script = nullptr;

    // errors that accumulate while the script is being evaluated
    // the compiled MslNode model isn't capturing source information so
    // while we use MslError, it won't currently have line/column set
    // todo: when running in the audio thread it's best to avoid dynamic memory
    // allocation for the array and juce::String, but if the script is getting
    // errors, an audio glitch is probably not that significant
    juce::OwnedArray<MslError> errors;

    // true if evaluation trace is enabled
    bool trace = false;

    // optional listener to receive trace notifications
    Listener* listener = nullptr;

};


    
