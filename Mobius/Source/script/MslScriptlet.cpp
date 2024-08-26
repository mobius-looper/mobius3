/**
 * State related to the compilation and evaluation of an MSL scriptlet.
 *
 */

#include <JuceHeader.h>

#include "MslScript.h"
#include "MslParser.h"
#include "MslSession.h"
#include "MslEnvironment.h"

#include "MslScriptlet.h"


MslScriptlet::MslScriptlet(MslEnvironment* env)
{
    environment = env;
    script.reset(new MslScript());
    // this is how we convey the logging name to the environment
    script->name = name;
}

/**
 * These are not pooled, but they can pool the things they contain.
 */
MslScriptlet::~MslScriptlet()
{
    resetLaunchResults();
}

void MslScriptlet::reset()
{
    resetLaunchResults();

    script.reset(new MslScript());
    // this is how we convey the logging name to the environment
    script->name = name;
}

void MslScriptlet::setName(juce::String s)
{
    name = s;
    if (script != nullptr)
      script->name = s;
}

/**
 * Reset launch state after a previous evaluation.
 */
void MslScriptlet::resetLaunchResults()
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

/**
 * Recompile the scriptlet with new source code.
 *
 * The interface is a little weird, but at least it's encapsulated.
 * First the MslParser is used to parse the source and install the root block
 * into an existing MslScript object that the scriptlet owns.
 *
 * If parsing succeeds, the MslEnvironment is then used to link references
 * from the parsed script to the outside world.  If there are link errors, they
 * are added to the MslScript error list so they can be conveyed back to the
 * application as if parsing and linking were one operation.
 */
bool MslScriptlet::compile(MslContext* context, juce::String source)
{
    bool success = false;

    // also reset evaluation state now, or wait till eval?
    
    // special parser interface where we manage the MslScript on the outside
    MslParser parser;
    if (parser.parse(script.get(), source)) {
        // passed parsing, now link it
        
        // special environment interface to link just this one script
        success = environment->linkScriptlet(context, script.get());
    }
    
    return success;
}

/**
 * Return errors encountered during parsing and linking.
 */
juce::OwnedArray<MslError>* MslScriptlet::getCompileErrors()
{
    return &(script->errors);
}

//////////////////////////////////////////////////////////////////////
//
// Evaluation
//
//////////////////////////////////////////////////////////////////////

/**
 * Evaluate a previously compiled scriptlet.
 */
bool MslScriptlet::eval(class MslContext* c)
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

bool MslScriptlet::isFinished()
{
    return (launchErrors == nullptr && sessionId == 0);
}

MslError* MslScriptlet::getErrors()
{
    return launchErrors;
}

bool MslScriptlet::isTransitioning()
{
    return wasTransitioned;
}

bool MslScriptlet::isWaiting()
{
    return wasWaiting;
}

int MslScriptlet::getSessionId()
{
    return sessionId;
}

/**
 * The result of the last launch.
 * Ownership is retained.
 */
MslValue* MslScriptlet::getResult()
{
    return launchResult;
}

/**
 * Trace the full results for debugging.
 */
juce::String MslScriptlet::getFullResult()
{
    juce::String s;
    getResultString(launchResult, s);
    return s;
}

/**
 * This should probably be an MslValue utility
 */
void MslScriptlet::getResultString(MslValue* v, juce::String& s)
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
 * Function definitions accumulate in the script after each evaluation.
 * 
 * !! woah, need to revisit how this is maintained if the inner
 * session goes async.  Maybe not an issue except for the unusual
 * way the console uses scriptlets.
 */
juce::OwnedArray<MslFunction>* MslScriptlet::getFunctions()
{
    return &(script->functions);
}

/**
 * Used by the console to show the results of a var evaluation.
 * These accumulate as Bindings on the script.
 * Another ugly hack for the console.
 */
MslBinding* MslScriptlet::getBindings()
{
    return script->bindings;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

    
