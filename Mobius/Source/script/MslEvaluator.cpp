
#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../model/UIAction.h"
#include "../model/Query.h"
#include "../ui/JuceUtil.h"
#include "../Supervisor.h"

#include "MslModel.h"
#include "MslEvaluator.h"

juce::String MslEvaluator::start(MslNode* node)
{
    juce::String result;
    
    // initialize stacks etc...
    errors.clear();
    
    // will need to work out suspend, and deferred results
    result = node->eval(this);

    // check errors etc...

    return result;
}

juce::StringArray MslEvaluator::getErrors()
{
    return errors;
}

//////////////////////////////////////////////////////////////////////
//
// MslNode eval targets
//
//////////////////////////////////////////////////////////////////////

juce::String MslEvaluator::evalBlock(MslBlock* block)
{
    juce::String result;

    for (auto child : block->children) {
        result = child->eval(this);
    }

    return result;
}

juce::String MslEvaluator::evalSymbol(MslSymbol* node)
{
    juce::String result;

    // will want to cache this in a "link" phase after parsing
    Symbol* s = Symbols.find(node->name);
    if (s != nullptr)
      result = eval(s);
    else
      errors.add("Unknown symbol: " + node->name);

    return result;
}

juce::String MslEvaluator::evalLiteral(MslLiteral* lit)
{
    // will need a typed value container at runtime
    return lit->value;
}

juce::String MslEvaluator::evalOperator(MslOperator* op)
{
    (void)op;
    return "operator punt";
}

//////////////////////////////////////////////////////////////////////
//
// Engine Touchpoints
//
//////////////////////////////////////////////////////////////////////

juce::String MslEvaluator::eval(Symbol* s)
{
    juce::String result;
    
    if (s->function != nullptr) {
        result = invoke(s);
    }
    else if (s->parameter != nullptr) {
        result = query(s);
    }
    return result;
}

juce::String MslEvaluator::invoke(Symbol* s)
{
    UIAction a;
    a.symbol = s;
    // todo: arguments
    // todo: this needs to take a reference
    Supervisor::Instance->doAction(&a);

    // what is the result of a function?
    return juce::String("");
}

// will need more flexibility on value types
juce::String MslEvaluator::query(Symbol* s)
{
    juce::String result;

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
                result = juce::String(s->parameter->getEnumLabel(q.value));
            }
            else if (ptype == TypeBool) {
                result = (q.value == 1) ? "false" : "true";
            }
            else if (ptype == TypeStructure) {
                // hmm, the understand of LevelUI symbols that live in
                // UIConfig and LevelCore symbols that live in MobiusConfig
                // is in Supervisor right now
                // todo: Need to repackage this
                result = Supervisor::Instance->getParameterLabel(s, q.value);
            }
            else {
                // should only be here for TypeInt
                // unclear what String would do
                result = juce::String(q.value);
            }
        }
    }
    
    return result;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
