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
    valuePool = env->getValuePool();
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

    // if we're shutting down, it may not make
    // sense to return these to the pool, the "static initializer" ordering problem
    //delete rootResult;
    valuePool->free(rootResult);
}

/**
 * Primary entry point for evaluating a script.
 */
void MslSession::start(MslScript* argScript)
{
    errors.clear();

    valuePool->free(rootResult);
    rootResult = nullptr;
    
    script = argScript;
    stack = allocStack();
    stack->node = script->root;

    run();
}

bool MslSession::isWaiting()
{
    return (stack != nullptr && stack->waiting);
}

/**
 * Ownership of the result DOES NOT TRANSFER to the caller.
 * Needs thought about what the lifespan of this needs to be.
 */
MslValue* MslSession::getResult()
{
    return rootResult;
}

/**
 * Okay, here is where it's getting weird.
 *
 * In the first implementation with MslValueTree I had to do a tree
 * walk to find the last leaf value in a bunch of nested trees because
 * every block created it's own list.
 *
 * I don't think that's necessary any more?
 *
 * What is different about this is that it returns ownership
 * of the rootResult.
 */
MslValue* MslSession::captureResult()
{
    MslValue* result = rootResult;
    rootResult = nullptr;
    return result;
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

/**
 * This should probably be an MslValue utility
 */
void MslSession::getResultString(MslValue* v, juce::String& s)
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
            valuePool->free(s->childResults);
            s->childResults = nullptr;
        }
    }
    else {
        // ordinarilly won't be here unless something is haywire
        // so what's a little more hay
        s = new MslStack();
    }
    return s;
}

void MslSession::freeStack(MslStack* s)
{
    if (s != nullptr) {
        valuePool->free(s->childResults);
        s->childResults = nullptr;
        stackPool.add(s);
    }
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
        
        // don't like the way we detect popping, have evalStack return
        // a bool and let it decide?
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
    if (stack->node->isLiteral()) {
        MslLiteral* lit = static_cast<MslLiteral*>(stack->node);
        MslValue* v = valuePool->alloc();
        if (lit->isInt) {
            v->setInt(atoi(lit->token.value.toUTF8()));
        }
        else if (lit->isBool) {
            // could be a bit more relaxed here but this is what the tokanizer left
            v->setBool(lit->token.value == "true");
        }
        else {
            v->setJString(lit->token.value);
        }
        addStackResult(v);
    }
    else if (stack->node->isBlock()) {
        addBlockResult();
    }
    else if (stack->node->isSymbol()) {
        // symbols are evaluated with an argument list determined by the child nodes
        // for initial testing return the name
        MslSymbol* sym = static_cast<MslSymbol*>(stack->node);
        doSymbol(sym);
    }
    else if (stack->node->isOperator()) {
        MslOperator* op = static_cast<MslOperator*>(stack->node);
        doOperator(op);
    }
    else {
        addError(stack->node, "Unsupport node evaluation");
        // need to push a null for this?
    }
}

/**
 * Blocks return a list and we don't usually want that, needs work...
 */
void MslSession::addBlockResult()
{
    // this is where we might want to be smarter about value
    // accumulation and list expansion so we don't end up with a bunch
    // of nested lists
    // !! yes, almost all blocks are just evaluated for side effect
    bool entireList = false;
    if (entireList) {
        // transfer the entire result list
        MslValue* v = valuePool->alloc();
        v->list = stack->childResults;
        stack->childResults = nullptr;
        addStackResult(v);
    }
    else {
        // take the last value in the list
        // this little dance needs to be a static util
        if (stack->childResults == nullptr) {
            // empty block, return a null placeholder?
            addStackResult(valuePool->alloc());
        }
        else {
            // remove the last one
            MslValue* prev = nullptr;
            MslValue* last = stack->childResults;
            while (last->next != nullptr) {
                prev = last;
                last = last->next;
            }
            if (prev != nullptr)
              prev->next = nullptr;
            else
              stack->childResults = nullptr;
            // free what remains ahead of this one
            valuePool->free(stack->childResults);
            stack->childResults = nullptr;
            // and return the last one
            addStackResult(last);
        }
    }
}

void MslSession::addStackResult(MslValue* v)
{
    MslStack* parent = stack->parent;
    if (parent == nullptr) {
        if (rootResult == nullptr)
          rootResult = v;
        else {
            MslValue* last = rootResult->getLast();
            last->next = v;
            checkCycles(rootResult);
        }
    }
    else if (parent->childResults == nullptr) {
        parent->childResults = v;
    }
    else {
        MslValue* last = parent->childResults->getLast();
        last->next = v;

        if (last == v)
          Trace(1, "WTF is going on here");
        
        checkCycles(parent->childResults);
    }
}

// need to beef this up
void MslSession::checkCycles(MslValue* v)
{
    if (found(v, v->next))
      Trace(1, "Cycle in next list");
    else if (found(v, v->list))
      Trace(1, "Cycle in value list");
}

bool MslSession::found(MslValue* node, MslValue* list)
 {
    bool found = false;
    MslValue* ptr = list;
    while (ptr != nullptr) {
        if (ptr == node) {
            found = true;
            break;
        }
        ptr = ptr->next;
    }
    return found;
}
    
//////////////////////////////////////////////////////////////////////
//
// Symbol Resolution
//
//////////////////////////////////////////////////////////////////////

/**
 * Evaluate a MslSymbol node, leave the result in the parent's value list.
 */
void MslSession::doSymbol(MslSymbol* snode)
{
    if (stack->childResults == nullptr) {
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
                // will add the result later when the block returns
            }
            else {
                // empty proc, null result?
                addStackResult(valuePool->alloc());
            }
        }
        else if (snode->symbol != nullptr) {
            doSymbol(snode->symbol);
        }
        else {
            // unresolved symbol
            // it is important for "if switchQuantize == loop" that we allow
            // unresolved symbols to be used instead of quoted string literals
            MslValue* v = valuePool->alloc();
            v->setJString(snode->token.value);
            // todo, might want an unresolved flag in the MslValue
            addStackResult(v);
        }
    }
    else {
        // back from a proc
        // transfer the function result
        // todo: does it make sense to collapse surrounding blockage?
        // here we have the "blocks retuning a list" problem again
        // need to pull out the last one
        addBlockResult();
    }
}

void MslSession::doSymbol(Symbol* sym)
{
    if (sym->function != nullptr || sym->script != nullptr) {
        invoke(sym);
    }
    else if (sym->parameter != nullptr) {
        query(sym);
    }
}

void MslSession::invoke(Symbol* sym)
{
    UIAction a;
    a.symbol = sym;

    // todo: arguments
    // this is where the MslContext needs to take over so we can do the
    // function either async or sync, and possibly transition threads
    Supervisor::Instance->doAction(&a);

    // only MSL scripts set a result right now
    MslValue* v = valuePool->alloc();
    v->setString(a.result);
    addStackResult(v);
}

void MslSession::query(Symbol* sym)
{
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
            MslValue* v = valuePool->alloc();
            // and now we have the ordinal vs. enum symbol problem
            UIParameterType ptype = sym->parameter->type;
            if (ptype == TypeEnum) {
                // don't use labels since I want scripters to get used to the names
                const char* ename = sym->parameter->getEnumName(q.value);
                v->setEnum(ename, q.value);
            }
            else if (ptype == TypeBool) {
                v->setBool(q.value == 1);
            }
            else if (ptype == TypeStructure) {
                // hmm, the understand of LevelUI symbols that live in
                // UIConfig and LevelCore symbols that live in MobiusConfig
                // is in Supervisor right now
                // todo: Need to repackage this
                // todo: this should be treated like an Enum and also return
                // the ordinal!
                v->setJString(Supervisor::Instance->getParameterLabel(sym, q.value));
            }
            else {
                // should only be here for TypeInt
                // unclear what String would do
                v->setInt(q.value);
            }

            addStackResult(v);
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
MslValue* MslSession::getArgument(int index)
{
    MslValue* value = nullptr;

    MslValue* arguments = stack->childResults;

    if (arguments == nullptr) {
        // this is probably a bug, node must not have been evaluated
        addError(stack->node, "Missing arguments");
    }
    else {
        value = arguments->get(index);
        if (value != nullptr) {
            if (value->list != nullptr) {
                value = value->list->getLast();
            }
        }
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
void MslSession::doOperator(MslOperator* opnode)
{
    MslValue* v = valuePool->alloc();
    
    MslOperators op = mapOperator(opnode->token.value);

    if (op == MslUnknown) {
        addError(opnode, "Unknown operator");
    }
    else {
        // everything needs two operands except for !
        MslValue* value1 = getArgument(0);
        MslValue* value2 = nullptr;
        if (op != MslNot)
          value2 = getArgument(1);

        if (errors.size() == 0) {
    
            switch (op) {
                case MslUnknown:
                    // already added an error
                    break;
                case MslPlus:
                    v->setInt(value1->getInt() + value2->getInt());
                    break;
                case MslMinus:
                    v->setInt(value1->getInt() - value2->getInt());
                    break;
                case MslMult:
                    v->setInt(value1->getInt() * value2->getInt());
                    break;
                case MslDiv: {
                    int divisor = value2->getInt();
                    if (divisor == 0) {
                        // we're obviously not going to throw if they made an error
                        v->setInt(0);
                        // should be a warning
                        Trace(1, "MslSession: divide by zero");
                        //addError(opnode, "Divide by zero");
                    }
                    else {
                        v->setInt(value1->getInt() / divisor);
                    }
                }
                    break;

                    // for direct comparison, be smarter about coercion
                    // = and == are the same right now, but that probably won't work
                case MslEq:
                    v->setBool(compare(value1, value2, true));
                    break;
                case MslDeq:
                    v->setBool(compare(value1, value2, true));
                    break;
            
                case MslNeq:
                    v->setBool(compare(value2, value2, false));
                    break;
            
                case MslGt:
                    v->setInt(value1->getInt() > value2->getInt());
                    break;
                case MslGte:
                    v->setBool(value1->getInt() >= value2->getInt());
                    break;
                case MslLt:
                    v->setBool(value1->getInt() < value2->getInt());
                    break;
                case MslLte:
                    v->setBool(value1->getInt() <= value2->getInt());
                    break;
                case MslNot:
                    // here we check to make sure the node only has one child
                    v->setBool(!(value1->getBool()));
                    break;
                case MslAnd:
                    // c++ won't evaluate the second arg if the first one is false
                    // msl doesn't do deferred evaluation so no, we don't
                    v->setBool(value1->getBool() && value2->getBool());
                    break;
                case MslOr:
                    // c++ won't evaluate the second arg if the first one is true
                    v->setBool(value1->getBool() || value2->getBool());
                    break;
            
                    // unclear about this, treat it as and
                case MslAmp:
                    v->setInt(value1->getBool() && value2->getBool());
                    break;
            }
        }
    }
    
    addStackResult(v);
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
bool MslSession::compare(MslValue* value1, MslValue* value2, bool equal)
{
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
