/**
 * State for an interactive MSL session.
 *
 * Need to improve conveyance of errors.  To be useful the messages
 * need to have line/column numbers and it would support a syntax highlighter
 * better if the error would include that in an MslError model rather than
 * embedded in the text.
 */

#include <JuceHeader.h>

#include "../model/UIAction.h"
#include "../model/UIParameter.h"
#include "../model/Query.h"
#include "../model/Symbol.h"
#include "../model/ScriptProperties.h"
#include "../Supervisor.h"

#include "MslSession.h"

MslSession::MslSession()
{
}

MslSession::~MslSession()
{
}

/**
 * Primary entry point for evaluating a script.
 */
void MslSession::start(MslScript* script)
{
    // todo: initialze call stacks
    errors.clear();

    MslEvaluator ev (this);
    MslValue result = ev->start(script->root);

    // todo: where does the value go?
}

/**
 * Called internally to add a runtime error.
 * Using MslError here so we can capture the location in the source
 * of the node having issues, but unfortunately the parser isn't leaving
 * that behind yet.
 */
void MslEvaluator::addError(MslNode* node, const char* details)
{
    // see file comments about why this is bad
    MslError* e = new MslError();
    // todo: capture line/column from the node
    e->token = node->token;
    e->details = juce::String(details);
    errors.add(e);
}

//////////////////////////////////////////////////////////////////////
//
// Symbol Resolution
//
// Unclear whether this should go in MslEvaluator or in Session
// but until it becomes clearer how this should work keep it up here
// and keep teh parser clean.
//
//////////////////////////////////////////////////////////////////////

/**
 * Resolve a symbol reference in a part tree by decorating it with the
 * thing that it references.
 * Returns false if it could not be resolved.
 */
bool MslSession::resolve(MslSymbol* snode)
{
    bool resolved = false;

    // first check for cached symbol
    if (snode->symbol == nullptr && snode->proc == nullptr) {
list        // look for a proc
        snode->proc = dynamicScript.findProc(snode->token);
        if (snode->proc == nullptr) {
            // todo: resolve vars in the current scope

            // else resolve to a global symbol
            snode->symbol = Symbols.find(snode->token);
        }
    }
    
    resolved = (snode->symbol != nullptr || snode->proc != nullptr);
    
    return resolved;
}

/**
 * Evaluate a Symbol node from the the parse tree and leave the result.
 * Not sure I like the handoff between resolve and eval...
 */
void MslSession::eval(MslSymbol* snode, MslValue& result)
{
    result.setNull();

    if (!resolve(snode)) {
        errors.add("Unresolved symbol " + snode->token);
    }
    else {
        if (snode->proc != nullptr) {
            // todo: arguments
            // kludge, result passing sucks, we've bounced from the Evaulator
            // up to the Session, and now back down to Evaluator which is
            // where result came from, and will be set as a side effect

            // sigh
            // evaluating a proc means evaluating it's body
            // proc nodes just have a child list, not a block, or do they?
            MslBlock* body = snode->proc->getBody();
            if (body != nullptr)
              evaluator.mslVisit(body);
        }
        else if (snode->symbol != nullptr) {
            Symbol* s = snode->symbol;
            if (s->function != nullptr) {

                invoke(s, result);
            }
            else if (s->parameter != nullptr) {
        
                query(s, result);
            }
        }
    }
}

void MslSession::invoke(Symbol* s, MslValue& result)
{
    UIAction a;
    a.symbol = s;
    // todo: arguments
    // todo: this needs to take a reference
    Supervisor::Instance->doAction(&a);

    // what is the result of a function?
    result.setNull();
}

void MslSession::query(Symbol* s, MslValue& result)
{
    result.setNull();

    if (s->parameter == nullptr) {
        errors.add("Error: Not a parameter symbol " + s->name);
    }
    else {
        Query q;
        q.symbol = s;
        bool success = Supervisor::Instance->doQuery(&q);
        if (!success) {
            errors.add("Error: Unable to query parameter " + s->name);
        }
        else if (q.async) {
            // not really an error, need a different message/warning list
            errors.add("Asynchronous parameter query " + s->name);
        }
        else {
            // And now we have the issue of whether to return an ordinal
            // or a label.  At runtime you usually want an ordinal, in the
            // interactive console usually a label.
            // will need a syntax for that, maybe ordinal(foo) or foo.ordinal

            UIParameterType ptype = s->parameter->type;
            if (ptype == TypeEnum) {
                // don't use labels since I want scripters to get used to the names
                //result.setString(s->parameter->getEnumLabel(q.value));
                result.setString(s->parameter->getEnumName(q.value));
            }
            else if (ptype == TypeBool) {
                result.setBool(q.value == 1);
            }
            else if (ptype == TypeStructure) {
                // hmm, the understand of LevelUI symbols that live in
                // UIConfig and LevelCore symbols that live in MobiusConfig
                // is in Supervisor right now
                // todo: Need to repackage this
                result.setJString(Supervisor::Instance->getParameterLabel(s, q.value));
            }
            else {
                // should only be here for TypeInt
                // unclear what String would do
                result.setInt(q.value);
            }
        }
    }
}

void MslSession::assign(MslSymbol* snode, int value)
{
    resolve(snode);
    if (snode->symbol != nullptr) {
        // todo: context forwarding
        UIAction a;
        a.symbol = snode->symbol;
        a.value = value;
        Supervisor::Instance->doAction(&a);
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
