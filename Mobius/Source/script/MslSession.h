/**
 * Runtime state maintained during the evaluation of a script.
 *
 * This is conceptually similar to ScriptInterpreter in the old language.
 *
 * Hmm, for the console I'd like the notiion of a session to have a longer lifespan
 * than just one script invocation, so sessions can have indefinite lifespan and can
 * run anything in the environment.
 *
 * This is interesting...the envionment is shared by an app, the environment needs
 * to have multiple threads doing independent things.  But it would be nice
 * if thigns created in one session were visible to another?  Or can sessions "isntall"
 * things into the environment to make them accessible?
 *
 * Until this fleshes out, consider this to always be an interactive session that can do
 * things that might not be allowed in the audio thread.  It always "runs" at UI level.
 *  
 */

#pragma once

#include <JuceHeader.h>

#include "MslParser.h"
#include "MslEvaluator.h"

class MslSession
{
  public:
    
    /**
     * A listener may be registered to receive notifications of things
     * that happen during parsing and evaluation.
     */
    class Listener {
      public:
        virtual ~Listener() {}
        virtual void mslTrace(const char* msg) = 0;
        virtual void mslError(const char* msg) = 0;
        virtual void mslResult(const char* msg) = 0;
    };

    // normally on only in inactive sessions
    bool trace = false;

    MslSession() {}
    ~MslSession() {}
    void setListener(Listener* l) {listener = l;}

    // evaluate a scriptlet
    void eval(juce::String source);
 
  private:

    // these two do most of the work
    MslParser parser;
    MslEvaluator evaluator;

    // optional listener to receive trace notifications
    Listener* listener = nullptr;

    // registry of global procs defined within this session
    juce::OwnedArray<class MslProc> procs;
    // name lookup index for procs
    juce::HashMap<juce::String, class MslProc*> procIndex;

    // registry of global variables
    juce::OwnedArray<class MslVar> vars;
    // name lookup index for procs
    juce::HashMap<juce::String, class MslVar*> varIndex;

    

};


    
