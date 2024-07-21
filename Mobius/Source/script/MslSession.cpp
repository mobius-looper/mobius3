/**
 * The MSL interupreter.
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
        // todo: the node token always has just the token string but
        // we remembered the type the tokenizer thought it was with the flags isInt etc.
        MslValue v;
        if (lit->isInt) {
            v.setInt(atoi(lit->token.value.toUTF8()));
        }
        else if (lit->isBool) {
            // could be a bit more relaxed here but this is what the tokanizer left
            v.setBool(lit->token.value == "true");
        }
        else {
            v.setJString(lit->token.value);
        }
        result->add(v);
    }
    else if (stack->node->isBlock()) {
        // transfer the entire result list
        if (stack->childResults != nullptr) {
            result->add(stack->childResults);
            stack->childResults = nullptr;
        }
        else {
            // block with no value, i see this when entering procs in the console
            // any other conditions
            // don't want to mess with nullptr in value trees so leave a null node
            MslValue v;
            result->add(v);
        }
    }
    else if (stack->node->isSymbol()) {
        // symbols are evaluated with an argument list determined by the child nodes
        // for initial testing return the name
        MslSymbol* sym = static_cast<MslSymbol*>(stack->node);
        doSymbol(stack, sym, result);
    }
    else if (stack->node->isOperator()) {
        MslOperator* op = static_cast<MslOperator*>(stack->node);
        doOperator(stack, op, result);
    }
    else {
        addError(stack->node, "Unsupport node evaluation");
        MslValue v;
        result->add(v);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Symbol Resolution
//
//////////////////////////////////////////////////////////////////////

/**
 * Evaluate a MslSymbol node, leave the result in the parent's value list.
 */
void MslSession::doSymbol(MslStack* s, MslSymbol* snode, MslValueTree* dest)
{
    MslValue result;

    if (s->childResults == nullptr) {
        // first time here
        // resolve the symbol, may have been reaolved on a previous pass
        if (snode->symbol == nullptr && snode->proc == nullptr) {
            // procs override global symbols
            snode->proc = script->findProc(snode->token.value);
            if (snode->proc == nullptr) {
                // else resolve to a global symbol
                snode->symbol = Symbols.find(snode->token.value);
            }
        }

        if (snode->proc != nullptr) {
            MslBlock* body = snode->proc->getBody();
            if (body != nullptr) {
                // todo: dig out the arguments and set up a binding context
                // push the stack
                MslStack* neu = allocStack();
                neu->node = body;
                neu->parent = stack;
                stack = neu;
            }
        }
        else if (snode->symbol != nullptr) {
            doSymbol(s, snode->symbol, result);
        }
        else {
            // unresolved symbol
            // it is important for "if switchQuantize == loop" that we allow
            // unresolved symbols to be used instead of quoted string literals
            result.setJString(snode->token.value);
            // todo, might want an unresolved flag in the MslValue
        }

        if (errors.size() == 0)
          dest->add(result);
    }
    else {
        // back from a proc
        // transfer the function result
        // todo: does it make sense to collapse surrounding blockage?
        dest->add(s->childResults);
        s->childResults = nullptr;
    }
}

void MslSession::doSymbol(MslStack* s, Symbol* sym, MslValue& result)
{
    if (sym->function != nullptr || sym->script != nullptr) {
        invoke(s, sym, result);
    }
    else if (sym->parameter != nullptr) {
        query(s, sym, result);
    }
}

void MslSession::invoke(MslStack* s, Symbol* sym, MslValue& result)
{
    (void)s;
    UIAction a;
    a.symbol = sym;

    // todo: arguments
    // this is where the MslContext needs to take over so we can do the
    // function either async or sync, and possibly transition threads
    Supervisor::Instance->doAction(&a);

    // only MSL scripts set a result right now
    result.setString(a.result);
}

void MslSession::query(MslStack* s, Symbol* sym, MslValue& result)
{
    (void)s;
    result.setNull();

    if (sym->parameter == nullptr) {
        addError(stack->node, "Not a parameter symbol");
    }
    else {
        Query q;
        q.symbol = sym;
        bool success = Supervisor::Instance->doQuery(&q);
        if (!success) {
            addError(stack->node, "Unable to query parameter");
        }
        else if (q.async) {
            // not really an error, need a different message/warning list
            addError(stack->node, "Asynchronous parameter query");
        }
        else {
            // and now we have the ordinal vs. enum symbol problem
            UIParameterType ptype = sym->parameter->type;
            if (ptype == TypeEnum) {
                // don't use labels since I want scripters to get used to the names
                const char* ename = sym->parameter->getEnumName(q.value);
                result.setEnum(ename, q.value);
            }
            else if (ptype == TypeBool) {
                result.setBool(q.value == 1);
            }
            else if (ptype == TypeStructure) {
                // hmm, the understand of LevelUI symbols that live in
                // UIConfig and LevelCore symbols that live in MobiusConfig
                // is in Supervisor right now
                // todo: Need to repackage this
                // todo: this should be treated like an Enum and also return
                // the ordinal!
                result.setJString(Supervisor::Instance->getParameterLabel(sym, q.value));
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
    if (snode->symbol == nullptr) {

        // todo: look for local vars on the block stack
        //snode->var = script->findProc(snode->token.value);
        if (snode->var == nullptr) {
            // else resolve to a global symbol
            snode->symbol = Symbols.find(snode->token.value);
        }
    }

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
// Arguments
//
//////////////////////////////////////////////////////////////////////

/**
 * Locate the value of an operand.
 * Normally they will be atomic values under the stack's value list.
 * But in the case of blocks, may (always?) be contained in a list wrapper
 * that should only have one eleemnt.
 *
 * e.g.
 *
 *     1 + 2 will be [1,2]
 * but
 *     (1) + 2 will be [[1],2]
 *
 * verify this.  Either way it shouldn't matter.  If we do encounter
 * a multi-value block result, i suppose take the last value?  hmm,
 * run some tests.  Blocks always return lists now and it is up to the
 * parent node to decide whether to use all of the values or just the last one.
 */
MslValue* MslSession::getArgument(MslStack* s, int index)
{
    MslValue* value = nullptr;

    MslValueTree* arguments = s->childResults;

    if (arguments == nullptr) {
        // this is probably a bug, node must not have been evaluated
        addError(s->node, "Missing arguments");
    }
    else if (arguments->list.size() == 0) {
        // not sure if this is necessary, could this ever collapse to
        // a single value?
        if (index == 0)
          value = &(arguments->value);
        else
          addError(s->node, "Missing argument");
    }
    else if (index < arguments->list.size()) {
        MslValueTree* arg = arguments->list[index];
        value = getLeafValue(arg);
    }

    return value;
}

/**
 * Recurse down a value tree to get to the bottom-right-most value.
 * Common in cases with () blocks in expressions but ((x)) should be allowed
 * so really any level of gratuitous paren blocks.
 */
MslValue* MslSession::getLeafValue(MslValueTree* arg)
{
    MslValue* value = nullptr;
    if (arg->list.size() == 0) {
        value = &(arg->value);
    }
    else {
        MslValueTree* nested = arg->list[arg->list.size() - 1];
        value = getLeafValue(nested);
    }
    return value;
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
 * Be relaxed about types here.  The only things we care about really
 * are numeric values and enumeration symbols coerced from/to ordinals.
 * Would be nice to do enum wrapping, but I don't think that belongs here.
 *
 * For boolean comparisons, try to be smart about enumerated values from
 * UIParameters.  Those will be saved in the MslValue as an Enum with both
 * the ordinal and the symbolic name.
 *
 * Null is treated as zero numerically which might be bad.
 */
void MslSession::doOperator(MslStack* s, MslOperator* opnode, MslValueTree* dest)
{
    // can't have tree values from operators yet
    MslValue result;
    
    MslOperators op = mapOperator(opnode->token.value);

    if (op == MslUnknown) {
        addError(opnode, "Unknown operator");
    }
    else {
        // everything needs two operands except for !
        MslValue* value1 = getArgument(s, 0);
        MslValue* value2 = nullptr;
        if (op != MslNot)
          value2 = getArgument(s, 1);

        if (errors.size() == 0) {
    
            switch (op) {
                case MslUnknown:
                    // already added an error
                    break;
                case MslPlus:
                    result.setInt(value1->getInt() + value2->getInt());
                    break;
                case MslMinus:
                    result.setInt(value1->getInt() - value2->getInt());
                    break;
                case MslMult:
                    result.setInt(value1->getInt() * value2->getInt());
                    break;
                case MslDiv: {
                    int divisor = value2->getInt();
                    if (divisor == 0) {
                        // we're obviously not going to throw if they made an error
                        result.setInt(0);
                        // should be a warning
                        Trace(1, "MslSession: divide by zero");
                        //addError(opnode, "Divide by zero");
                    }
                    else {
                        result.setInt(value1->getInt() / divisor);
                    }
                }
                    break;

                    // for direct comparison, be smarter about coercion
                    // = and == are the same right now, but that probably won't work
                case MslEq:
                    result.setBool(compare(s, value1, value2, true));
                    break;
                case MslDeq:
                    result.setBool(compare(s, value1, value2, true));
                    break;
            
                case MslNeq:
                    result.setBool(compare(s, value2, value2, false));
                    break;
            
                case MslGt:
                    result.setInt(value1->getInt() > value2->getInt());
                    break;
                case MslGte:
                    result.setBool(value1->getInt() >= value2->getInt());
                    break;
                case MslLt:
                    result.setBool(value1->getInt() < value2->getInt());
                    break;
                case MslLte:
                    result.setBool(value1->getInt() <= value2->getInt());
                    break;
                case MslNot:
                    // here we check to make sure the node only has one child
                    result.setBool(!(value1->getBool()));
                    break;
                case MslAnd:
                    // c++ won't evaluate the second arg if the first one is false
                    // msl doesn't do deferred evaluation so no, we don't
                    result.setBool(value1->getBool() && value2->getBool());
                    break;
                case MslOr:
                    // c++ won't evaluate the second arg if the first one is true
                    result.setBool(value1->getBool() || value2->getBool());
                    break;
            
                    // unclear about this, treat it as and
                case MslAmp:
                    result.setInt(value1->getBool() && value2->getBool());
                    break;
            }
        }
    }
    
    if (errors.size() == 0) {
        // put the result in the parent's value list
        dest->add(result);
    }
}

/**
 * Semi-smart comparison that deals with strings and symbols.
 * For UIParameter symbols, this is a little complicated and relies on the
 * value of the parameter being stored in the MslValue as an Enum containing
 * both the ordinal integer and the symbolic name string.
 *
 * Nullness isn't really a thing yet, which could lead to some weird
 * comparisons with string coercion.
 *
 * Symbols that resolve to UIParameters is a little lose right now, but the main
 * thing that needs to be supported is this:
 *
 *      if quantize == loop
 *
 * While the true value of most parameters is an ordinal number, no one thinks of it
 * that way, they want to compare against a symbolic name.  In the above example
 * the lhs will be an Enum and the RHS will be a String holding the name of an
 * unresolved symbol.
 *
 * Use of unresolved symbols is a little weird, and prevents checking for errors
 * at the point of evaluation.  But the alternative is to intern a bunch of Symbols.
 */
bool MslSession::compare(MslStack* s, MslValue* value1, MslValue* value2, bool equal)
{
    (void)s;
    bool result = false;

    if (value1->type == MslValue::String || value2->type == MslValue::String) {
        // it doesn't really matter if one side is a symbol Enum, they
        // both compare as strings
        // numeric coersion to strings is a little weird, does that cause trouble?
        if (equal)
          result = StringEqualNoCase(value1->getString(), value2->getString());
        else
          result = !StringEqualNoCase(value1->getString(), value2->getString());
    }
    else {
        // simple integer comparison
        if (equal)
          result = (value1->getInt() == value2->getInt());
        else
          result = (value1->getInt() != value2->getInt());
    }
    
    return result;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
