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

#include "../model/Symbol.h"

#include "MslScript.h"
#include "MslParser.h"
#include "MslEvaluator.h"

class MslSession
{
    friend class MslEvaluator;
    
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

    MslSession();
    ~MslSession();
    void setListener(Listener* l) {listener = l;}

    // evaluate a scriptlet
    void eval(juce::String source);


    juce::OwnedArray<class MslProc>* getProcs() {
        return &(dynamicScript.procs);
    }
    
  protected:

    bool resolve(MslSymbol* snode);
    void eval(MslSymbol* snode, MslValue& result);
    

    class Symbol* findSymbol(juce::String name);
    void assign(Symbol* s, int value);
    
 
  private:

    // script holding parse state during dynamic sessions (the console)
    MslScript dynamicScript;

    // these two do most of the work
    MslParser parser;
    MslEvaluator evaluator;

    // optional listener to receive trace notifications
    Listener* listener = nullptr;

    juce::StringArray errors;

    // hmm, if we make this symbol oriented which has nice properties
    // don't need procTable and varTable
    SymbolTable symbols;

    //MslNode* assimilate(MslNode* node);
    //void intern(MslProc* proc);
    
    void invoke(Symbol* s, MslValue& result);
    void query(Symbol* s, MslValue& result);
    void assign(MslSymbol* snode, int value);

    void conveyErrors(juce::StringArray* errors);

};


    
