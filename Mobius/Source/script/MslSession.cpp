/**
 * State for an interactive MSL session.
 *
 * Need to improve conveyance of errors.  To be useful the messages
 * need to have line/column numbers and it would support a syntax highlighter
 * better if the error would include that in an MslError model rather than
 * embedded in the text.
 */

#include <JuceHeader.h>

#include "../util/Util.h"
#include "../model/UIAction.h"
#include "../model/UIParameter.h"
#include "../model/Query.h"
#include "../model/Symbol.h"
#include "../model/ScriptProperties.h"
#include "../Supervisor.h"

#include "MslError.h"
#include "MslModel.h"
#include "MslScript.h"
#include "MslEnvironment.h"

#include "MslSession.h"

MslSession::MslSession(MslEnvironment* env)
{
    environment = env;
}

MslSession::~MslSession()
{
    // if we error, or cancel while waiting
    // just clean the stack
    while (stack != nullptr) {
        MslStack* prev = stack->parent;
        freeStack(stack);
        stack = prev;
    }

    delete rootResult;
}

/**
 * Primary entry point for evaluating a script.
 */
void MslSession::start(MslScript* argScript)
{
    errors.clear();

    delete rootResult;
    rootResult = new MslValueTree();
    
    script = argScript;
    stack = allocStack();
    stack->node = script->root;

    run();
}

bool MslSession::isWaiting()
{
    return (stack != nullptr && stack->waiting);
}

MslValueTree* MslSession::getResult()
{
    return rootResult;
}

/**
 * Okay, here is where it's getting weird.
 *
 * For some things, like symbol invocations the result of a block
 * needs to be a list, which becomes the argument list for the symbol.
 * The node that consumes the child evaluations then decides what
 * to pass up the stack.
 *
 * Here we are at the root of the stack and will normally have a
 * child block result for the implicit block that surrounds the script root.
 * In this context, the result of the block is the last result in the list.
 *
 * If that result is also a block, then recurse until we hit an atomic.
 * This little dance is going to be necessary as well during inner node
 * value propagation.
 */
MslValue MslSession::getAtomicResult(MslValueTree* t)
{
    MslValue v;
    
    if (t->list.size() > 0)
      v = getAtomicResult(t->list[t->list.size() - 1]);
    else
      v = t->value;

    return v;
}

MslValue MslSession::getAtomicResult()
{
    return getAtomicResult(rootResult);
}

juce::OwnedArray<MslError>* MslSession::getErrors()
{
    return &errors;
}

/**
 * Trace the full results for debugging.
 */
juce::String MslSession::getFullResult()
{
    juce::String s;
    getResultString(rootResult, s);
    return s;
}

void MslSession::getResultString(MslValueTree* vt, juce::String& s)
{
    if (vt->list.size() > 0) {
        s += "[";
        int count = 0; 
        for (auto v : vt->list) {
            if (count > 0) s += ",";
            getResultString(v, s);
            count++;
        }
        s += "]";
    }
    else {
        const char* sval = vt->value.getString();
        if (sval != nullptr)
          s += sval;
        else
          s += "null";
    }
}

//////////////////////////////////////////////////////////////////////
//
// Stack Pool
//
//////////////////////////////////////////////////////////////////////

/**
 * !! This needs to be a proper stack pool with no memory allocation
 * and periodic fluffing.
 */
MslStack* MslSession::allocStack()
{
    MslStack* s = nullptr;
    if (stackPool.size() > 0) {
        s = stackPool.removeAndReturn(0);
        // should have an empty list, if not reset it
        if (s->childResults != nullptr) {
            Trace(1, "MslSession: Lingering child result pooling stack");
            delete s->childResults;
            s->childResults = nullptr;
        }
    }
    else {
        s = new MslStack();
        // make sure this starts clean
        if (s->childResults != nullptr) {
            // this one is more serious, it shouldn't have gotten in here with a result
            Trace(1, "MslSession: Lingering child result in pooled stack");
            delete s->childResults;
            s->childResults = nullptr;
        }
    }
    return s;
}

void MslSession::freeStack(MslStack* s)
{
    stackPool.add(s);
}

//////////////////////////////////////////////////////////////////////
//
// Run Loop
//
//////////////////////////////////////////////////////////////////////

/**
 * The run loop processes the node at the top of the stack.
 * Each node will push a new stack frame for each child node.
 * Once all child nodes have completed, the node is evaluated
 * and the stack is popped.
 *
 * During this process any node may enter a wait state and the session
 * suspends until the wait is cleared.
 */
void MslSession::run()
{
    while (stack != nullptr && !stack->waiting && errors.size() == 0) {
        continueStack();
    }
}

void MslSession::continueStack()
{
    // process the next child
    // here we may have ordering issues, a few nodes need to process their
    // children out of order, e.g. a conditional statement may skip over a child
    // node or select one of them rather than doing them sequentially
    stack->childIndex += 1;
    if (stack->childIndex < stack->node->children.size()) {
        // push the stack
        MslStack* neu = allocStack();
        neu->node = stack->node->children[stack->childIndex];
        neu->parent = stack;
        stack = neu;
    }
    else {
        // all children have finished, determine this node's value
        // if this is a wait node, it may push another frame
        MslStack* starting = stack;
        
        evalStack();

        if (errors.size() == 0) {
        
            if (starting == stack) {
                // still here, can pop now
                MslStack* prev = stack->parent;
                freeStack(stack);
                stack = prev;
            }
            // todo: when a stack is finished, and evaluation pushes
            // another node, we don't want to keep evaluating it on the
            // next pass, so either popping needs to be aware of this
            // and pop twice, or we need a finished flag
        }
    }
}

/**
 * At this point all of the child nodes have been evaluated and the
 * node examines the result and computes it's own value.
 */
void MslSession::evalStack()
{
    // not liking this, would be better if there was always a special "root" parent
    // stack that did not evaluate?
    MslValueTree* result = nullptr;
    if (stack->parent == nullptr) {
        result = rootResult;
    }
    else {
        result = stack->parent->childResults;
        if (result == nullptr) {
            result = new MslValueTree();
            stack->parent->childResults = result;
        }
    }
    
    if (stack->node->isLiteral()) {
        MslLiteral* lit = static_cast<MslLiteral*>(stack->node);
        MslValue v;
        v.setJString(lit->token.value);
        result->add(v);
    }
    else if (stack->node->isBlock()) {
        // transfer the entire result list
        result->add(stack->childResults);
        stack->childResults = nullptr;
    }
    else if (stack->node->isSymbol()) {
        // symbols are evaluated with an argument list determined by the child nodes
        // for initial testing return the name
        MslSymbol* sym = static_cast<MslSymbol*>(stack->node);
        MslValue v;
        v.setJString(sym->token.value);
        result->add(v);
    }
    else if (stack->node->isOperator()) {
        MslOperator* op = static_cast<MslOperator*>(stack->node);
        doOperator(stack, op);
    }
    else {
        addError(stack->node, "Unsupport node evaluation");
        MslValue v;
        result->add(v);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Operators
//
//////////////////////////////////////////////////////////////////////






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
        // look for a proc
        snode->proc = script->findProc(snode->token.value);
        if (snode->proc == nullptr) {
            // todo: resolve vars in the current scope

            // else resolve to a global symbol
            snode->symbol = Symbols.find(snode->token.value);
        }
    }
    
    resolved = (snode->symbol != nullptr || snode->proc != nullptr);
    
    return resolved;
}

/**
 * Evaluate a Symbol node from the the parse tree and leave the result.
 * Not sure I like the handoff between resolve and eval...
 */
// is this interface necessary any more?
// shit, it is, this is terrible

void MslSession::eval(MslSymbol* snode, MslValue& result)
{
    result.setNull();

    if (!resolve(snode)) {
        addError(snode, "Unresolved symbol");
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
            //if (body != nullptr)
            //evaluator->mslVisit(body);
        }
        else if (snode->symbol != nullptr) {
            Symbol* s = snode->symbol;
            if (s->function != nullptr || s->script != nullptr) {

                invoke(s, result);
            }
            else if (s->parameter != nullptr) {
        
                query(snode, s, result);
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

    // only MSL scripts set a result right now
    result.setString(a.result);
}

void MslSession::query(MslSymbol* snode, Symbol* s, MslValue& result)
{
    result.setNull();

    if (s->parameter == nullptr) {
        addError(snode, "Not a parameter symbol");
    }
    else {
        Query q;
        q.symbol = s;
        bool success = Supervisor::Instance->doQuery(&q);
        if (!success) {
            addError(snode, "Unable to query parameter");
        }
        else if (q.async) {
            // not really an error, need a different message/warning list
            addError(snode, "Asynchronous parameter query");
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

/**
 * Called internally to add a runtime error.
 * Using MslError here so we can capture the location in the source
 * of the node having issues, but unfortunately the parser isn't leaving
 * that behind yet.
 */
void MslSession::addError(MslNode* node, const char* details)
{
    // see file comments about why this is bad
    MslError* e = new MslError();
    // okay, this shit happens a lot now, why not just standardize on passing
    // the MslToken by value everywhere
    e->token = node->token.value;
    e->line = node->token.line;
    e->column = node->token.column;
    e->details = juce::String(details);
    errors.add(e);
}

//////////////////////////////////////////////////////////////////////
//
// Expressions
//
//////////////////////////////////////////////////////////////////////

// if we allow async functions in expressions, then this
// will need to be much more complicated and use the stack
// once we add procs, we can't control what the proc will do so need
// to set a flag indicating "inside expression evaluator" to prevent async

MslOperators MslSession::mapOperator(juce::String& s)
{
    MslOperators op = MslUnknown;
    
    if (s == "+") op = MslPlus;
    else if (s == "-") op = MslMinus;
    else if (s == "*") op = MslMult;
    else if (s == "/") op = MslDiv;
    else if (s == "=") op = MslEq;
    else if (s == "==") op = MslDeq;
    else if (s == "!=") op = MslNeq;
    else if (s == ">") op = MslGt;
    else if (s == ">=") op = MslGte;
    else if (s == "<") op = MslLt;
    else if (s == "<=") op = MslLte;
    else if (s == "!") op = MslNot;
    else if (s == "&&") op = MslAnd;
    else if (s == "||") op = MslOr;

    // will they try to use this?
    else if (s == "&") op = MslAmp;

    return op;
}    

/**
 * Be relaxed about this.  The only things we care about really
 * are numeric values and enumeration symbols coerced from/to ordinals.
 * Would be nice to do enum wrapping, but I don't think that belongs here.
 *
 * For boolean comparisons, a bunch of older logic around trying to map
 * enum symbols to their corresponding numeric values.  If we do that there
 * then it should be done for arithmetic operators too.
 */
void MslSession::doOperator(MslStack* s, MslOperator* opnode)
{
    // can't have tree values from operators yet
    MslValue result;
    
    MslOperators op = mapOperator(opnode->token.value);
    MslNode* node1 = (opnode->children.size() > 0) ? opnode->children[0] : nullptr;
    MslNode* node2 = (opnode->children.size() > 1) ? opnode->children[1] : nullptr;
    
    switch (op) {
        case MslUnknown:
            addError(opnode, "Unknown operator");
            break;
        case MslPlus:
            result.setInt(evalInt(s, 0) + evalInt(s, 1));
            break;
        case MslMinus:
            result.setInt(evalInt(s, 0) - evalInt(s, 1));
            break;
        case MslMult:
            result.setInt(evalInt(s, 0) * evalInt(s, 1));
            break;
        case MslDiv: {
            int divisor = evalInt(s, 1);
            if (divisor == 0) {
                // we're obviously not going to throw if they made an error
                result.setInt(0);
                // should be a warning
                Trace(1, "MslSession: divide by zero");
                //addError(opnode, "Divide by zero");
            }
            else {
                result.setInt(evalInt(s, 0) / divisor);
            }
        }
            break;

            // for direct comparison, be smarter about coercion
            // = and == are the same right now, but that probably won't work
        case MslEq:
            result.setBool(compare(s, node1, node2, true));
            break;
        case MslDeq:
            result.setBool(compare(s, node1, node2, true));
            break;
            
        case MslNeq:
            result.setBool(compare(s, node1, node2, false));
            break;
            
        case MslGt:
            result.setInt(evalInt(s, 0) > evalInt(s, 1));
            break;
        case MslGte:
            result.setInt(evalInt(s, 0) >= evalInt(s, 1));
            break;
        case MslLt:
            result.setInt(evalInt(s, 0) < evalInt(s, 1));
            break;
        case MslLte:
            result.setInt(evalInt(s, 0) <= evalInt(s, 1));
            break;
        case MslNot:
            // here we check to make sure the node only has one child
            result.setInt(evalBool(s, 0));
            break;
        case MslAnd:
            result.setInt(evalBool(s, 0) && evalBool(s, 1));
            break;
        case MslOr:
            result.setInt(evalBool(s, 0) || evalBool(s, 1));
            break;
            
            // unclear about this, treat it as and
        case MslAmp:
            result.setInt(evalBool(s, 0) && evalBool(s, 1));
            break;
    }
    
}

int MslSession::evalInt(MslStack* s, int index)
{
    int ival = 0;
    MslValueTree* operands = s->childResults;
    // always a block?
    // messy, need some common tools for this
    if (index < operands->list.size()) {
        MslValueTree* operand = operands->list[index];
        // this is expected to be atomic
        if (operand->list.size() > 0) {
            addError(s->node, "Unexpected non-atomic operand");
        }
        else {
            ival = operand->value.getInt();
        }
    }
    else {
        // could have checked this earlier?
        addError(s->node, "Missing operand");
    }
    
    return ival;
}

bool MslSession::evalBool(MslStack* s, int index)
{
    bool bval = false;
    MslValueTree* operands = s->childResults;
    if (index < operands->list.size()) {
        MslValueTree* operand = operands->list[index];
        if (operand->list.size() > 0) {
            addError(s->node, "Unexpected non-atomic operand");
        }
        else {
            bval = operand->value.getInt();
        }
    }
    else {
        // could have checked this earlier?
        addError(s->node, "Missing operand");
    }
    return bval;
}

/**
 * Semi-smart comparison that deals with strings and symbols.
 * See compareSymbol for why this is complicated.
 */
bool MslSession::compare(MslStack* s, MslNode* node1, MslNode* node2, bool equal)
{
    bool result = false;
    
    if (node1 == nullptr || node2 == nullptr) {
        // ahh, null
        // I suppose two nulls are equal?
        // these will coerce down to numeric zero and be equal
        if (equal)
          result = (evalInt(s, 0) == evalInt(s, 1));
        else
          result = (evalInt(s, 0) != evalInt(s, 1));
    }
    else if (node1->isSymbol() || node2->isSymbol()) {
        // complicated to deal with enumerations
        result = compareSymbol(s, node1, node2, equal);
    }
    else if (isString(node1) || isString(node2)) {
        MslValue val1 = evalString(stack, 0);
        MslValue val2 = evalString(stack, 1);
        if (equal)
          result = (StringEqualNoCase(val1.getString(), val2.getString()));
        else
          result = (!StringEqualNoCase(val1.getString(), val2.getString()));
    }
    return result;
}

// ugly
MslValue MslSession::evalString(MslStack* s, int index)
{
    MslValue v;

    MslValueTree* operands = s->childResults;
    if (index < operands->list.size()) {
        MslValueTree* operand = operands->list[index];
        if (operand->list.size() > 0) {
            addError(s->node, "Unexpected non-atomic operand");
        }
        else {
            v = operand->value;
        }
    }
    else {
        // could have checked this earlier?
        addError(s->node, "Missing operand");
    }
    return v;
}

bool MslSession::isString(MslNode* node)
{
    bool itis = false;
    if (node != nullptr) {
        if (node->isLiteral()) {
            // guess it doesn't really matter if the token was
            // a quoted string or not, visit(literal) will coerce
            // it to a string anyway
            itis = true;
        }
    }
    return itis;
}

/**
 * Comparison of nodes involving Parameter symbols is more complicated due
 * to the normal use of enumeration values in the comparison rather than ordinals.
 *
 * So while the ordinal value of the switchQuantize parameter might be 3 no
 * one ever types that, they say "if switchQuantize == loop"
 *
 * There are various more elegant ways to handle this: we could treat enumeration values
 * in scripts as symbols consistently, intern them, and then do equality on the Symbols.
 * What I'm going to do is sort of like an operator overload on == that looks to see if
 * one side is a parameter symbol and if so arrange to coerce the other side into the
 * ordinal for comparison.  It gets the job done, in this case I care more about the
 * syntax than I do about the cleanliness of the evaluator.
 *
 * "loop" (without quotes) will be treated by the parser as an MslSymbol node that
 * is unresolved.
 *
 * Should do something similar for other likely comparisons, though anything other
 * than == and != don't make much sense since lexical or ordinal ordering isn't obvious.
 *
 * What this can't do is treat Symbols as return values from an expression.
 * So this
 *
 *    if quantize==Loop
 *
 * can work but this
 * 
 *    if (quantize)==Loop
 *
 * won't work since any node surroudng the symbol will hide the symbol.  By the point
 * of comparison it will already have been evaluated to an ordinal and the parameter-ness
 * of it has been lost.
 *
 * Might be nice to pass those around, similar to a Lisp quoted symbol.
 *
 * It would also work to treat parameter values symbolically rather than ordinals if they
 * are enumerations.  This would however result in interning a boatload of Symbols
 * for every enumeration in every parameter, which polutes the namespace.  So
 * "var loop" would hide the symbol representing the "loop" enumeration value of
 * switchQuantize.  And no, no one is going to understand namespace qualfiers and packages.
 *
 * A perhaps nicer alternative to this would be to allow values of enumerated
 * symbols to just always use the string representation but that requires changes
 * to the Query interface, which would be nice, but isn't there yet.
 *
 * And...having done all this shit, that's exactly what we're doing.  Since we control
 * the context and do the Query, we can coerce it to the enumeration.  So we have
 * the opposite problem now, the Query will have a nice symbol, and the unresolved
 * symbol will have been coerced to an ordinal.  Fuck.  This now reduces
 * to a string comparison.
 */


// update: I don't think this needs to be this complex
bool MslSession::compareSymbol(MslStack* s, MslNode* node1, MslNode* node2, bool equal)
{
    bool result = false;
    
    MslSymbol* param = getResolvedParameter(node1, node2);
    MslNode* other = getUnresolved(node1, node2);

    if (param == nullptr || other == nullptr) {
        // this is not a combo we can reason with, revert to numeric
        if (equal)
          result = (evalInt(s, 0) == evalInt(s, 1));
        else
          result = (evalInt(s, 0) != evalInt(s, 1));
    }
    else {
        UIParameter* p = param->symbol->parameter;
        juce::String sval = other->token.value;
        int otherOrdinal = p->getEnumOrdinal(sval.toUTF8());
        int paramOrdinal = -1;
        // now get the parameter symbol ordinal
        // oh, this isn't going to work since we've lost the side we're on
        // !!!!!!!!! rewrite this
        MslValue v = evalString(stack, 0);
        //param->visit(this);
        if (v.type == MslValue::Type::Int)
          paramOrdinal = v.getInt();
        else if (v.type == MslValue::Type::String) {
            paramOrdinal = p->getEnumOrdinal(v.getString());
        }

        if (equal)
          result = (paramOrdinal == otherOrdinal);
        else
          result = (paramOrdinal != otherOrdinal);
    }
    return result;
}

MslSymbol* MslSession::getResolvedParameter(MslNode* node1, MslNode* node2)
{
    MslSymbol* param = getResolvedParameter(node1);
    if (param == nullptr)
      param = getResolvedParameter(node2);
    return param;
}

// need to move MslSymbol resolution up to session so it can deal with vars !!

MslSymbol* MslSession::getResolvedParameter(MslNode* node)
{
    MslSymbol* param = nullptr;
    if (node->isSymbol()) {
        MslSymbol* symnode = static_cast<MslSymbol*>(node);

        resolve(symnode);
        
        if (symnode->symbol != nullptr && symnode->symbol->parameter != nullptr)
          param = symnode;
    }
    return param;
}

MslNode* MslSession::getUnresolved(MslNode* node1, MslNode* node2)
{
    MslNode* unresolved = getUnresolved(node1);
    if (unresolved == nullptr)
      unresolved = getUnresolved(node2);
    return unresolved;
}

MslNode* MslSession::getUnresolved(MslNode* node)
{
    MslNode* unresolved = nullptr;
    if (node->isLiteral()) {
        unresolved = node;
    }
    else if (node->isSymbol()) {
        MslSymbol* symnode = static_cast<MslSymbol*>(node);
        if (symnode->symbol == nullptr)
          unresolved = node;
    }
    return unresolved;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
