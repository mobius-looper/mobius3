/**
 * State related to the compilation and evaluation of an MSL scriptlet.
 *
 */

#include <JuceHeader.h>

#include "MslScript.h"
#include "MslParser.h"
#include "MslSession.h"
#include "MslEnvironment.h"

#include "MslScriptletSession.h"


MslScriptletSession::MslScriptletSession(MslEnvironment* env)
{
    environment = env;
    script.reset(new MslScript());
    // this is how we convey the logging name to the environment
    script->name = name;
}

/**
 * These are not pooled, but they can pool the things they contain.
 */
MslScriptletSession::~MslScriptletSession()
{
    resetLaunchResults();
    // this one isn't pooled which is starting to be awkward
    delete parseResult;
}

void MslScriptletSession::reset()
{
    resetLaunchResults();
    delete parseResult;
    parseResult = nullptr;

    script.reset(new MslScript());
    // this is how we convey the logging name to the environment
    script->name = name;
}

void MslScriptletSession::setName(juce::String s)
{
    name = s;
    if (script != nullptr)
      script->name = s;
}

/**
 * Reset launch state after a previous evaluation.
 */
void MslScriptletSession::resetLaunchResults()
{
    sessionId = 0;
    wasTransitioned = false;
    wasWaiting = false;

    MslPools* pool = environment->getPool();
    
    pool->free(launchResult);
    launchResult = nullptr;

    pool->free(launchErrors);
    launchErrors = nullptr;
}

//////////////////////////////////////////////////////////////////////
//
// Compilation
//
//////////////////////////////////////////////////////////////////////

bool MslScriptletSession::compile(juce::String source)
{
    bool success = false;

    // reclaim the last parser result
    // todo: if we're in kernel context this needs to be pooling
    // but we can't be right now
    delete parseResult;
    parseResult = nullptr;

    // also reset evaluation state now, or wait till eval?
    
    // special parser interface where we manage the MslScript on the outside
    MslParser parser;
    parseResult = parser.parse(script.get(), source);

    // we own the result
    if (parseResult->errors.size() == 0)
      success = true;

    return success;
}

/**
 * Return the parser result for inspection.
 * Note that unlike normal script compilation the result will NOT contain a pointer
 * to an MslScript, just the errors.
 * Could avoid the layer and just capture and return the MslError list instead.
 */
MslParserResult* MslScriptletSession::getCompileErrors()
{
    return parseResult;
}

//////////////////////////////////////////////////////////////////////
//
// Evaluation
//
//////////////////////////////////////////////////////////////////////

/**
 * Evaluate a previously compiled scriptlet.
 */
bool MslScriptletSession::eval(class MslContext* c)
{
    bool success = false;

    resetLaunchResults();
    
    if (script->root == nullptr) {
        // nothing was compiled or the scriptlet source was empty
        // should this be an error condition?
        // what if it contained only directives
        success = true;
    }
    else {
        // ask the environment to launch ourselves, this is a little weird
        // but allows the environment to deposit complex results directly
        // in this object rather than returning something we have to copy
        environment->launch(c, this);

        if (launchErrors == nullptr)
          success = true;
    }

    return success;
}

//////////////////////////////////////////////////////////////////////
//
// Evaluation Results
//
//////////////////////////////////////////////////////////////////////

bool MslScriptletSession::isFinished()
{
    return (launchErrors == nullptr && sessionId == 0);
}

MslError* MslScriptletSession::getErrors()
{
    return launchErrors;
}

bool MslScriptletSession::isTransitioning()
{
    return wasTransitioned;
}

bool MslScriptletSession::isWaiting()
{
    return wasWaiting;
}

int MslScriptletSession::getSessionId()
{
    return sessionId;
}

/**
 * The result of the last launch.
 * Ownership is retained.
 */
MslValue* MslScriptletSession::getResult()
{
    return launchResult;
}

/**
 * Trace the full results for debugging.
 */
juce::String MslScriptletSession::getFullResult()
{
    juce::String s;
    getResultString(launchResult, s);
    return s;
}

/**
 * This should probably be an MslValue utility
 */
void MslScriptletSession::getResultString(MslValue* v, juce::String& s)
{
    if (v == nullptr) {
        s += "null";
    }
    else if (v->list != nullptr) {
        // I'm a list
        s += "[";
        MslValue* ptr = v->list;
        int count = 0;
        while (ptr != nullptr) {
            if (count > 0) s += ",";
            getResultString(ptr, s);
            count++;
            ptr = ptr->next;
        }
        s += "]";
    }
    else {
        // I'm atomic
        const char* sval = v->getString();
        if (sval != nullptr)
          s += sval;
        else
          s += "null";
    }
}

/**
 * Used by the console to show the results of a proc evaluation.
 * Proc definitions accumulate in the script after each evaluation.
 * 
 * !! woah, need to revisit how this is maintained if the inner
 * session goes async.  Maybe not an issue except for the unusual
 * way the console uses scriptlets.
 */
juce::OwnedArray<MslProc>* MslScriptletSession::getProcs()
{
    return &(script->procs);
}

/**
 * Used by the console to show the results of a var evaluation.
 * These accumulate as Bindings on the script.
 * Another ugly hack for the console.
 */
MslBinding* MslScriptletSession::getBindings()
{
    return script->bindings;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

    
