/**
 * The MSL interupreter.
 *
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
#include "MslStack.h"
#include "MslBinding.h"
#include "MslEnvironment.h"

#include "MslSession.h"

MslSession::MslSession(MslEnvironment* env)
{
    environment = env;
    // need this all the time so cache it here
    pool = env->getPool();
}

/**
 * These are normally only deleted when the MslPool destructs and
 * should be clean.  
 * 
 * But if we land in an error condition trying to clean up, delete things
 * rather than returning them to the pool.  The pool should still be valid
 * at this point, but it is sensitive to static initialization order
 * in supervisor and I'd like to know if we get here because it means something
 * isn't pooling properly.
 */
MslSession::~MslSession()
{
    if (stack != nullptr) {
        Trace(1, "MslSession: You're deleting a loaded session without freeing it to the pool");
        while (stack != nullptr) {
            MslStack* prev = stack->parent;
            delete stack;
            stack = prev;
        }
    }

    if (rootValue != nullptr) {
        // this one does cascade
        Trace(1, "MslSession: Lingering rootValue on delete");
        delete rootValue;
    }
    
    if (errors != nullptr) {
        // this one does cascade
        Trace(1, "MslSession: Lingering errors on delete");
        delete errors;
    }
}

void MslSession::init()
{
    next = nullptr;
    result = nullptr;
    sessionId = 0;
    script = nullptr;
    context = nullptr;
    stack = nullptr;
    transitioning = false;
    errors = nullptr;
    rootValue = nullptr;
}

/**
 * Primary entry point for evaluating a script.
 */
void MslSession::start(MslContext* argContext, MslScript* argScript)
{
    // should be clean out of the pool but make sure
    pool->free(errors);
    errors = nullptr;
    pool->free(rootValue);
    rootValue = nullptr;
    pool->freeList(stack);
    stack = nullptr;
    
    script = argScript;
    stack = pool->allocStack();
    stack->node = script->root;

    // put the saved static bindings in the root block stack frame
    // !! todo: potential thread issues
    // it is rare to have concurrent evaluations of the same script and still
    // rarer to be doing it from both the UI thread and the audio thread at the same time
    // but it can happen
    // if static bindings are maintained on the MslScript and copied to the MslStack,
    // any new values won't be "seen" by other threads until this session ends
    // also, the last thread to execute overwrite the values of the previous ones
    // todo this the Right Way, static variable bindings need to be more like exported
    // bindings and managed in a single shared location with csects around access
    stack->bindings = script->bindings;
    script->bindings = nullptr;

    context = argContext;
    run();
}

/**
 * Name to use in the MslResult and for logging.
 */
const char* MslSession::getName()
{
    const char* s = nullptr;
    if (script != nullptr)
      s = script->name.toUTF8();
    return s;
}

/**
 * Resume a script after transitioning or to check wait states.
 * If we transitioned, it will just continue from the previous node.
 * If waiting, we'll immediately wait again unless the MslWait was
 * modified.
 *
 * If this was transitioning, the transition has happened, and that
 * state can be cleared.
 * 
 * Everything else is left untouched, the error list may be non-empty
 * if we're transitioning from the kernel back to the shell to show results.
 */
void MslSession::resume(MslContext* argContext)
{
    transitioning = false;

    // stack and bindings remain in place
    context = argContext;

    // run may just immediately return if there were errors
    // or if the MslWait hasn't been satisified
    run();
}

/**
 * Return true if we're transitioning.
 * This is just a flag that will toss the session to the other side.
 * It is important that the session logic does not flag a transition that doesn't
 * make sense, or else it will just keep bouncing between them.
 */
bool MslSession::isTransitioning()
{
    return (transitioning == true);
}

/**
 * Return true if we're waiting on something.
 * This is not normally true if transitioning is also true since
 * we shold do the transition before we enter the wait state.
 *
 * When we get to the point where sessions can have multiple threads of
 * execution will have to check all threads.
 *
 * Currently a wait state is indiciated by the top of the stack having
 * an active MslWait 
 */
bool MslSession::isWaiting()
{
    bool waiting = (stack != nullptr && stack->wait.active);
    
    if (waiting && transitioning)
      Trace(1, "MslSession: I'm both transitioning and waiting, can this happen?");
    
    return waiting;
}

/**
 * Only for MslScriptletSession and the console
 */
MslWait* MslSession::getWait()
{
    MslWait* wait = nullptr;
    if (stack != nullptr && stack->wait.active)
      wait = &(stack->wait);
    return wait;
}

/**
 * Return true if the script has finished.
 * This is indicated by an empty stack or the presence of errors.
 * The error list is what distinguishes this from simply testing
 * !transitioning and !waiting.  We can be in either of those states
 * but are still fished because of errors.
 */
bool MslSession::isFinished()
{
    return (stack == nullptr || hasErrors());
}

bool MslSession::hasErrors()
{
    return (errors != nullptr);
}

//////////////////////////////////////////////////////////////////////
//
// Results
//
//////////////////////////////////////////////////////////////////////

/**
 * Ownership of the result value DOES NOT TRANSFER to the caller.
 * Who uses this?  Anything that cares should be using captureValue
 */
MslValue* MslSession::getValue()
{
    return rootValue;
}

/**
 * Transfer ownership of the final result.
 */
MslValue* MslSession::captureValue()
{
    MslValue* retval = rootValue;
    rootValue = nullptr;
    return retval;
}

MslError* MslSession::getErrors()
{
    return errors;
}

MslError* MslSession::captureErrors()
{
    MslError* retval = errors;
    errors = nullptr;
    return retval;
}

/**
 * Called internally to add a runtime error.
 * Using MslError here so we can capture the location in the source
 * of the node having issues, but unfortunately the parser isn't leaving
 * that behind yet.
 */
void MslSession::addError(MslNode* node, const char* details)
{
    MslError* e = pool->allocError();
    e->init(node, details);
    e->next = errors;
    errors = e;
}

//////////////////////////////////////////////////////////////////////
//
// Run Loop
//
//////////////////////////////////////////////////////////////////////

/**
 * The run loop processes the node at the top of the stack until
 * all stack frames have been consumed, a wait state has been reached,
 * or an unrecoverable runtime error has been encountered.
 */
void MslSession::run()
{
    //if (stack != nullptr)
    //aTrace(2, "Run: %s", debugNode(stack->node).toUTF8());
    
    while (stack != nullptr && errors == nullptr && !transitioning && !isWaitActive()) {
        advanceStack();
    }
}

/**
 * This is called by the run loop to determine if the stack is active and not
 * finished wait.  This differs from isWaiting for non-obvious reasons, I think
 * because isWaiting is used for initial results, and here we need to adapt to
 * the completion of a wait asynchronously.
 *
 * I dislike having the logic duplicated here and in the MslWaitNode visitor but
 * we've got a control flow issue.  If we use only wait.active in the run loop then
 * MslWaitNode will never get processed, but if we don't check wait.active we'll never
 * stop.  Might be better to have a "newWait" flag that is used like "transitioning" and can
 * be cleared every time we're resumed, then set again if WaitNode decides it wasn't finished.
 * Will have to do that eventually when you get to multiple threads.  Advance all threads
 * then test of isAnyWaiting or something.
 */
bool MslSession::isWaitActive()
{
    return (stack->wait.active && !stack->wait.finished);
}

/**
 * During an advance, a handler for the node associated with the frame is called.
 * This dispatching is done through a "visitor" pattern using overloaded class methods
 * to handle the dispatch and downcasting.
 *
 * The node handler performs actions and calculates a result value.  Then
 * the stack is popped and the result value is added to the child results list
 * of the previous stack frame.
 *
 * Note node handlers will usually push additional frames onto the stack.
 * These frames are then evaluated, results accumulated, and a final
 * aggregate result is calculated and returned.  In this process a stack frame
 * may be advanced several times, each time accumulating more information from
 * the child nodes until a completion state is reached.
 */
void MslSession::advanceStack()
{
    if (stack->node != nullptr) {
        // warp to a node-specific handler
        stack->node->visit(this);
    }
    else {
        // here is where we might add special stack frames that are not
        // associated with language nodes?
    }
}

/**
 * Called by node handlers to push a new frame onto the stack.
 * The stack frame is returned and in some cases may be further initialized
 * by the node handler.
 */
MslStack* MslSession::pushStack(MslNode* node)
{
    MslStack* neu = pool->allocStack();

    neu->node = node;
    neu->parent = stack;
    
    stack = neu;

    //Trace(2, "Pushed: %s", debugNode(stack->node).toUTF8());
    return neu;
}

/**
 * Called by node handlers to push the next child node onto the stack.
 * Most nodes do simple iteration of their child nodes in order.
 * If we run out of children, nullptr is returned.
 */
MslStack* MslSession::pushNextChild()
{
    MslStack* neu = nullptr;

    // todo: some nodes will want more control over this list
    juce::OwnedArray<MslNode>* childNodes = &(stack->node->children);

    // this starts at -1 
    stack->childIndex++;
    if (stack->childIndex < childNodes->size()) {
        MslNode* nextNode = (*childNodes)[stack->childIndex];
        neu = pushStack(nextNode);
    }
    return neu;
}

/**
 * When a frame has completed, transfer a calculated value to the parent frame,
 * remove the current frame from the stack, and return control to the parent frame.
 *
 * There are two ways values can be returned from a child frame to the parent.
 * Simple nodes have single-valued results, prior results if any are discarded and
 * the new result is saved.
 *
 * A frame may also be designated as an "accumulator" frame.  Here, the values from
 * every child frame are accumulated on a list where they can be processed in bulk
 * by the parent frame.
 *
 * There is a bit of ugliness here when dealing with the results for the root frame.
 * Since there is no parent frame, the results to in the rootValue list managed
 * by the session.  Root results are always accumulated.  It would simplify some things
 * if we always had a dummy block at the top of the stack to accumulate root results.
 */
void MslSession::popStack(MslValue* v)
{
    // it is permissible to pop without a value, if the child wants nullness to have
    // meaning, it must return an empty MslValue
    MslStack* parent = stack->parent;
    if (parent == nullptr) {
        // we're at the root frame
        if (rootValue == nullptr)
          rootValue = v;
        else {
            MslValue* last = rootValue->getLast();
            last->next = v;
        }

        // see start() for more on the thread issues here...
        // save the final root block bindings back into the script
        // technically should only be doing this for static variables
        // and not locals
        // or we could have the binding reset when the MslVar that defines
        // them is encountered during evaluation
        script->bindings = stack->bindings;
        stack->bindings = nullptr;
    }
    else if (!parent->accumulator) {
        // replace the last value
        pool->free(parent->childResults);
        parent->childResults = v;
    }
    else if (parent->childResults == nullptr) {
        // we're an accumulator, and this is the first one
        parent->childResults = v;
    }
    else {
        // add the result to the end of the list
        MslValue* last = parent->childResults->getLast();
        last->next = v;
    }

    // now do the popping part
    MslStack* prev = stack->parent;
    pool->free(stack);
    stack = prev;
}

/**
 * Pop the stack and return whatever child results remain on the
 * current stack frame.
 */
void MslSession::popStack()
{
    // ownership transfers so must clear this before pooling
    MslValue* cresult = stack->childResults;
    stack->childResults = nullptr;
    
    popStack(cresult);
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
// Bindings
//
//////////////////////////////////////////////////////////////////////

/**
 * Walk up the stack looking for a binding, and finally into
 * the static binging list in the script.
 */
MslBinding* MslSession::findBinding(const char* name)
{
    MslBinding* found = nullptr;

    MslStack* level = stack;

    while (found == nullptr && level != nullptr) {

        if (level->bindings != nullptr)
          found = level->bindings->find(name);

        if (found == nullptr)
          level = level->parent;
    }
    
    // if we make it to the top of the stack, look in the static bindings
    // !! potential thread issues here
    if (found == nullptr) {
        if (script->bindings != nullptr)
          found = script->bindings->find(name);
    }

    return found;
}

MslBinding* MslSession::findBinding(int position)
{
    MslBinding* found = nullptr;

    MslStack* level = stack;
    while (found == nullptr && level != nullptr) {
        if (level->bindings != nullptr)
          found = level->bindings->find(position);

        if (found == nullptr)
          level = level->parent;
    }
    
    // if we make it to the top of the stack, look in the static bindings
    // !! potential thread issues here
    if (found == nullptr) {
        if (script->bindings != nullptr)
          found = script->bindings->find(position);
    }

    return found;
}

//////////////////////////////////////////////////////////////////////
//
// Literal
//
//////////////////////////////////////////////////////////////////////

/**
 * Stack handler for a literal.
 * Literals do not have child nodes or computation, they simply return
 * their value to the parent frame.
 */
void MslSession::mslVisit(MslLiteral* lit)
{
    MslValue* v = pool->allocValue();
    
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

    popStack(v);
}

//////////////////////////////////////////////////////////////////////
//
// Block
//
//////////////////////////////////////////////////////////////////////

/**
 * Block frames evaluate each child in order and may or may not accumulate
 * results.  The accumulation flag will have been set by the parent frame
 * that pushed this one.
 */
void MslSession::mslVisit(MslBlock* block)
{
    (void)block;
    
    MslStack* nextStack = pushNextChild();
    if (nextStack == nullptr) {
        // ran out of children, at this point we return the aggregate result
        // to the parent
        // todo: this is where we may want to make the distinction between
        // concatenating a list onto the parent or returning a single
        // value that is itself a list, will become interesting if we ever
        // support arrays as values
        popStack();
    }
}

//////////////////////////////////////////////////////////////////////
//
// Var
//
//////////////////////////////////////////////////////////////////////

/**
 * When a var is encounterd, push the optional child node to calculate
 * an initial value.
 *
 * When the initializer has finished, place a Binding for this var and
 * it's value into the parent frame
 */
void MslSession::mslVisit(MslVar* var)
{
    // the parser should have only allowed one child, if there is more than
    // one we take the last value
    MslStack* nextStack = pushNextChild();
    if (nextStack == nullptr) {
        // initializer finished
    
        MslStack* parent = stack->parent;
        if (parent == nullptr) {
            // this shouldn't happen, there can be vars at the top level of a script
            // but the script body should have been surrounded by a containing block
            addError(var, "Var encountered above root block");
        }
        else if (!parent->node->isBlock()) {
            // the var was inside something other than a {} block
            // the parser allows this but it's unclear what that should mean, what use
            // would a var be inside an expression for example?  I suppose we could allow
            // this, but flag it for now until we find a compelling use case
            addError(var, "Var encountered in non-block container");
        }
        else {
            MslBinding* b = pool->allocBinding();
            b->setName(var->name.toUTF8());
            // value ownership transfers
            b->value = stack->childResults;
            stack->childResults = nullptr;
            
            parent->addBinding(b);

            // vars do not have values themselves
            popStack(nullptr);
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Proc
//
//////////////////////////////////////////////////////////////////////

/**
 * This is the DECLARATION of a proc not a call.
 * Currently, procs are "sifted" by the parser and left on a special list
 * in the MslScript so we should not encounter them during evaluation.
 * If that ever changes, then you'll have to deal with them here.
 * In theory we could have scoped proc definitions like we do for vars.
 */
void MslSession::mslVisit(MslProc* proc)
{
    addError(proc, "Encountered unsifted Proc");
}

//////////////////////////////////////////////////////////////////////
//
// Assignment
//
//////////////////////////////////////////////////////////////////////

/**
 * Assignments result from a statement of this form:
 *
 *     x=y
 *
 * Unlike Operator, the LHS is required to be a Symbol and the
 * RHS can be any expression.  The LHS symbol is NOT evaluated, it is
 * simply used as the name of the thing to be assigned.  It may be better
 * to have the parser consume the Symbol token and just leave the name behind
 * in the node as is done for proc and var.  But this does open up potentially
 * useful behavior where the LHS could be any expression that produces a name
 * literal string:    "x"=y  or foo()=y.  While possible and relatively easy
 * that's hard to explain.
 */
void MslSession::mslVisit(MslAssignment* ass)
{
    // verify we have a name symbol, only really need this on phase 0 but it doesn't
    // hurt to look again
    MslSymbol* namesym = nullptr;
    MslNode* first = ass->get(0);
    if (first == nullptr) {
        addError(ass, "Malformed assignment, missing assignment symbol");
    }
    else if (!first->isSymbol()) {
        addError(ass, "Malformed assignment, assignment to non-symbol");
    }
    else {
        namesym = static_cast<MslSymbol*>(first);
    }

    // asssignment of this symbol may require thread transition,
    // but so may the initializer so we have to do that first

    if (namesym != nullptr) {
        if (stack->phase == 0) {
            // first time here, push the initializer
            MslNode* second = ass->get(1);
            if (second == nullptr) {
                addError(ass, "Malformed assignment, missing initializer");
            }
            else {
                // push the initializer
                stack->phase++;
                pushStack(second);
            }
        }
        else {
            // back from the initializer
            if (stack->childResults == nullptr) {
                // something weird like "x=var foo;" that that the parser
                // could have caught
                addError(ass, "Malformed assignment, initializer had no value");
            }
            else {
                doAssignment(namesym);
            }
        }
    }
}

/**
 * At this point, we've evaluated what we need, and are ready to make the assignment.
 * A thread transition may need to be made depending on the target symbol.
 * The stack childResults has the value to assign.
 *
 * If we have to do thread transition, we're going to end up looking for bindings twice,
 * could skip that with another stack phase but it shouldn't be too expensive.
 */
void MslSession::doAssignment(MslSymbol* namesym)
{
    // resolve the target symbol, first look for var bindings
    MslBinding* binding = findBinding(namesym->token.value.toUTF8());
    if (binding != nullptr) {
        // transfer the value
        pool->free(binding->value);
        binding->value = stack->childResults;
        stack->childResults = nullptr;

        // and we are done, assignments do not have values though I suppose
        // we could allow the initializer value to be the assignment node value
        // as well, what does c++ do?
        popStack(nullptr);
    }
    else {
        // symbol did not resolve internally, look outside
        // !! this is where we need to factor out awareness of what
        // model/Symbol is to the MslContainer
        // currently the model provides a cache pointer to a previously
        // resolved model/Symbol.  Should be using a cache, but until
        // we factor out what model/Symbol is, I'd like to re-resolve it
        // every time

        // !! juce::String passing, need to weed out all of this in the evaluator
        Symbol* extsym = Symbols.find(namesym->token.value);
        if (extsym == nullptr) {
            // here we could have the notion of an auto-var
            // pretend that a var existed in the parent block and give
            // it a binding...not sure if useful
            addError(stack->node, "Unresolved symbol in assignment");
        }
        else if (stack->childResults == nullptr) {
            // should have caught this in the previous phase
            addError(stack->node, "Assignment with no value");
        }
        else {
            // this is where we may need to do thread transition
            
            // !! and also factor out awareness of what a UIAction is
            UIAction a;
            a.symbol = extsym;
            // UIActions can only have numeric values right now
            // if the initializer didn't produce one, it coerce to zero
            // probably want to raise an error instead
            a.value = stack->childResults->getInt();

            // Supervisor::Instance->doAction(&a);
            context->mslAction(&a);
            
            popStack(nullptr);
        }
    }
}
        
//////////////////////////////////////////////////////////////////////
//
// Reference
//
//////////////////////////////////////////////////////////////////////

/**
 * A $x reference.
 * These aren't used much now that we have named references but could
 * still be useful for special symbol handling.
 *
 * If one of these is unresolved it's a little more serious than
 * an unresolved symbol since you can't use them for just the name.
 */
void MslSession::mslVisit(MslReference* ref)
{
    MslBinding* binding = nullptr;
    
    int position = atoi(ref->name.toUTF8());
    if (position > 0) {
        binding = findBinding(position);
    }
    else {
        // it's not uncommon to do $foo to reference a named binding
        // though it isn't necessary
        binding = findBinding(ref->name.toUTF8());
    }

    if (binding != nullptr) {
        returnBinding(binding);
    }
    else {
        // return null or error?
        //MslValue* v = pool->allocValue();
        //popStack(v);
        // the token in the error message will just say $ rather than
        // the name which is inconvenient
        addError(ref, "Unresolved reference");
        Trace(1, "Unresolved reference %s", ref->name.toUTF8());
    }
}

//////////////////////////////////////////////////////////////////////
//
// Symbol
//
//////////////////////////////////////////////////////////////////////

/**
 * Now it gets interesting.
 *
 * Symbols can return the value of these things:
 *    an internal var Binding
 *    an exported var value from another script
 *    an internal proc call
 *    an exported proc call
 *    an external model/Symbol Query
 *    an external model/Symbol UIAction
 *    just the name literal of an unresolved symbol
 *
 * The only one that requires thread transition is the UIAction,
 * though we might want to do this for some Query's as well
 *
 * Unresolved symbols are generally an error, but there are a few cases where
 * we allow that just for names.  Think more about this, in those cases the parser
 * should have consumed it and take the node out.
 *
 * Since there are so many things this can do, it saves extra state beyond the phase
 * number on the stack frame so we don't have to keep running through this analysis 
 * every time a child frame finishes.
 *
 * For procs and vars, if there are name collisions the code logic has an implied
 * priority that is subtle.  If we don't allow procs and vars to have the same name
 * it isn't an issue, but if they do, the search order is:
 *     local var
 *     local proc
 *     exported var linkage
 *     exported proc linkage
 *
 */
void MslSession::mslVisit(MslSymbol* snode)
{
    // for safety check later
    MslStack* startStack = stack;
    
    if (stack->proc != nullptr) {
        // we resolved to an internal or exported proc and are
        // back from the argument block or the body block
        returnProc();
    }
    else if (stack->symbol != nullptr) {
        // we resolved to an external model/Symbol and have completed
        // calculating the argument list
        // this may cause a thread transition
        returnSymbol();
    }
    else {
        // first time here

        // look for local vars
        MslBinding* binding = findBinding(snode->token.value.toUTF8());
        if (binding != nullptr) {
            returnBinding(binding);    
        }
        else {
            // look for local procs, these could be cached on the MslSymbol
            MslProc* proc = script->findProc(snode->token.value);
            if (proc != nullptr) {
                pushProc(proc);
            }
            else {
                // look for exports
                MslLinkage* link = environment->findLinkage(snode->token.value);
                if (link != nullptr) {
                    if (link->var != nullptr) {
                        returnVar(link);
                    }
                    else if (link->proc != nullptr) {
                        pushProc(link);
                    }
                }
                else {
                    // look for external symbols
                    if (!doExternal(snode)) {

                        // unresolved
                        returnUnresolved(snode);
                    }
                }
            }
        }
    }

    // sanity check
    // at this point we should have returned a value and popped the stack,
    // or pushed a new frame and are waiting for the result
    // if neither happened it is a logic error, and the evaluator will
    // hang if you don't catch this
    if (errors == nullptr && startStack == stack &&
        stack->proc == nullptr && stack->symbol == nullptr) {
        
        addError(snode, "Like your dreams, the symbol evaluator is broken");
    }
}

/**
 * Here if despite our best efforts, the symbol could go nowhere.
 * Normally this would be an error, but in this silly language it is
 * expected to be able to write things like this:
 *
 *    if switchQuantize == loop
 *
 * rather than
 *
 *    if switchQuantize == "loop"
 *
 * I'd really rather not introduce syntax complications that non-programmers
 * are going to stumble over all the time.  Which then means the rest of the
 * language needs to treat the evaluation of a symbol as it's name consistently.
 * And if it doesn't like that in a certain context, then the error
 * is raised at runtime.
 *
 * Ideally what should happen in this case is more language awareness of what
 * enumerations are so if that's mispelled as looop we can raise an error rather
 * than just consider the comparison unequal.
 * Needs thought...
 */
void MslSession::returnUnresolved(MslSymbol* snode)
{
    // what we CAN do here is raise an error if there is an argument list
    // on the symbol, that is always a spelling error
    if (snode->children.size() > 0) {
        addError(snode, "Unresolved function symbol");
    }
    else {
        MslValue* v = pool->allocValue();
        v->setJString(snode->token.value);
        // todo, might want an unresolved flag in the MslValue
        popStack(v);
    }
}

/**
 * Here for a symbol that resolved to an internal binding.
 * The binding holds the value to be returned, but it must be copied.
 */
void MslSession::returnBinding(MslBinding* binding)
{
    MslValue* value = binding->value;
    MslValue* copy = nullptr;
    if (value == nullptr) {
        // binding without a value, should this be ignored or does
        // it hide other things?
        copy = pool->allocValue();
    }
    else {
        // bindings can be referenced multiple times, so need to copy
        copy = pool->allocValue();
        copy->copy(value);
    }
    popStack(copy);
}

/**
 * Here for a symbol that resolved to a var exported from another script.
 */
void MslSession::returnVar(MslLinkage* link)
{
    (void)link;
    
    // don't have a way to save values for these yet,
    // needs to either be in the MslLinkage or in a list of value containers
    // inside the referenced script

    // return null
    popStack(pool->allocValue());
}

/**
 * Calling a proc.
 * Push the argument list if we have one, if not the body
 */
void MslSession::pushProc(MslProc* proc)
{
    stack->proc = proc;
    
    // does the call have arguments?
    // note that we're dealing with the calling symbol node and not the proc node
    if (stack->node->children.size() > 0) {
        // yes, push them
        // use the phase number to indiciate which block we're waiting for
        stack->phase = 1;
        pushStack(stack->node->children[0]);
    }
    else {
        // no arguments, push the body
        pushCall();
    }
}

/**
 * Calling a proc in another script.
 * This is similar to calling a local proc except the environment
 * for resolving references could be different.
 *
 * Needs thought...
 * If an exported proc references a var defined in the other script we would
 * need to look inside the other script which requires that it be saved on the stack
 * so we know to look there.  Then again, when procs are brought "inside" another script
 * should they be referencing things from within the calling script's scope?
 *
 * The former would be a way to encapsulate things like constant vars that influence
 * the exported proc, but are not visible from the outside.  The later would behave
 * more like an "include" where the proc body wakes up inside another in-progress script
 * and resolves vars there.  That has uses too, and would be an alternative to passing
 * arguments, rather than an argument list, the proc has unresolved references that are
 * resolved within the caller.
 *
 * I'm going with the second because it's easier and procs really should be self-contained.
 * Having dueling resolution scopes raises all sorts of issues with name collision and
 * where those extern var values are stored.  They could be exported as MslLinkages or just
 * saved inside the MslScript.  It's basically like a function with a closure around it.
 * Or calling a method on an object.  Hmm, scripts as "objects with methods" could be interesting
 * but you can't have more than one instance.
 */
void MslSession::pushProc(MslLinkage* link)
{
    // for now, same as calling a local proc, we just get it through
    // a level of indirection
    MslProc* proc = link->proc;
    if (proc != nullptr) {
        pushProc(proc);
    }
    else {
        // should have caught this by now
        addError(stack->node, "A Linkage with no Proc is like a day without sunshine");
    }
}

/**
 * Attempt to return a proc result after the completion of a child frame.
 * There are two phases to this.  If a proc call has arguments, then the
 * previous child frame was the argument list and we have to push another
 * frame to handle the body.  If this was the body frame we can return
 * from the call.
 */
void MslSession::returnProc()
{
    if (stack->phase == 1) {
        // we were waiting for arguments
        pushCall();
    }
    else if (stack->phase == 2) {
        // we were waiting for the body
        popStack();
    }
    else {
        // we were waiting for Godot
        addError(stack->node, "Invalid symbol stack phase");
    }
}

/**
 * Build a call frame with arguments.
 * childResult has the result of the evaluation of the argument block.
 * Turn that into something nice and pass it to the proc.
 */
void MslSession::pushCall()
{
    bindArguments();
    stack->phase = 2;
    MslBlock* body = stack->proc->getBody();
    if (body != nullptr) {
        pushStack(body);
    }
    // corner case, if the proc has no body, could force a null return
    // now, or just use the phase handling normally
}

/**
 * Convert the arguments for a proc to a list of MslBindings
 * on the symbol node.  The names of the bindings come from
 * the proc definition.
 *
 * The values for the binding are in the stack->childResults.
 */
void MslSession::bindArguments()
{
    // these define the binding names
    MslBlock* names = stack->proc->getDeclaration();

    int position = 1;
    if (names != nullptr) {
        for (auto namenode : names->children) {
            MslBinding* b = makeArgBinding(namenode);
            addArgValue(b, position, true);
            position++;
        }
    }
    
    // if we have any left over, give them bindings with positional names
    while (stack->childResults != nullptr) {
        MslBinding* b = pool->allocBinding();
        snprintf(b->name, sizeof(b->name), "$%d", position);
        addArgValue(b, position, false);
        position++;
    }
}        

/**
 * Start building a binding with a name from a proc declaration.
 * These are supposed to all be MslSymbols but the parser may be
 * letting other things through.
 */
MslBinding* MslSession::makeArgBinding(MslNode* namenode)
{
    // return something on error so the caller doesn't crash till the next
    // error check
    MslBinding* b = pool->allocBinding();
    if (!namenode->isSymbol()) {
        // what else would it be?
        // I suppose we could allow literal strings but why
        // could also allow this and just reference by number?
        addError(namenode, "Proc declaration not a symbol");
    }
    else {
        b->setName(namenode->token.value.toUTF8());
    }
    return b;
}

/**
 * Add the next value to an argument binding and install it on the symbol.
 */
void MslSession::addArgValue(MslBinding* b, int position, bool required)
{
    if (stack->childResults != nullptr) {
        b->value = stack->childResults;
        stack->childResults = stack->childResults->next;
    }
    else if (required) {
        // missing arg value
        // still create a binding so it resolves but has null
        Trace(2, "Not enough argument values for call");
    }
    b->position = position;
    b->next = stack->bindings;
    stack->bindings = b;
}

//////////////////////////////////////////////////////////////////////
//
// External Symbols
//
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
//
// External Symbols
//
//////////////////////////////////////////////////////////////////////

/**
 * Here we've encountered a symbol node that did not resolve into
 * anything within the script environment.  Look into the container.
 * This is the usual case since scripts are almost always used to set Mobius
 * parameters and call functions.
 *
 * This is where thread transitioning may need to happen, moving the session
 * between the shell and the kernel.
 *
 * Currently makes direct use of model/Symbol and model/UIAction but this
 * needs to abstracted out so the interpreter can be used within other
 * things besides Mobius.
 *
 * If this resolves to a parameter, the value is returned through a Query.
 * If this resolves to a function, the function is invoked with a UIAction.
 *
 * Functions may have arguments, so this may turn into a push for the argument list.
 *
 */
bool MslSession::doExternal(MslSymbol* snode)
{
    bool resolved = false;

    // todo: cache this on the node so we don't have to keep going back
    // to the symbol table
    Symbol* sym = Symbols.find(snode->token.value);
    if (sym != nullptr) {
        resolved = true;

        // todo: check for thread transitions
        // could do this either before or after the argument list evaluation
        // it doesn't really matter but we might as well go there early?

        stack->symbol = sym;
        
        // does this have arguments?
        if (snode->children.size() > 0) {
            // yes, push them
            // don't need to use phases here since there are no additional child blocks
            stack->phase = 1;
            pushStack(snode->children[0]);
        }
        else {
            // no arguments, do the thing
            // useful to have a frame for this?
            doSymbol(sym);
        }
    }
    return resolved;
}

/**
 * Here after evaluating the argument block for an external symbol
 * which is expected to be a function
 */
void MslSession::returnSymbol()
{
    doSymbol(stack->symbol);
}

/**
 * Here when we're finally ready to do the external thing.
 * If there was an argument block on the symbol node it will have
 * been evaluated and left on the stack
 */
void MslSession::doSymbol(Symbol* sym)
{
    if (sym->parameter != nullptr) {
        query(sym);
    }
    else if (sym->function != nullptr || sym->script != nullptr) {
        invoke(sym);
    }
    else {
        // I suppose we could support others like BehaviorSample here
        // but you can also get to that with a function
        addError(stack->node, "Symbol is not a function or parameter");
    }
}

/**
 * Build an action to perform a function.
 * Thread transition should have already been performed, if not
 * this will be asynchronous and go through the non-script way of
 * doing thread transitions.  It should work but isn't immediate
 * and you can't wait on it.
 */
void MslSession::invoke(Symbol* sym)
{
    UIAction a;
    a.symbol = sym;

    // actions don't do much in the way of arguments right now
    // only a few functions take arguments and they're all integers
    // but a handful take strings
    // needs a lot more work
    if (stack->childResults != nullptr) {
        // if there is more than one result, could warn since they're not going anywhere
        MslValue *arg = stack->childResults;

        // who gets to win the coersion wars?
        // if the user typed in a literal int, that's what they wanted
        if (arg->type == MslValue::Int)
          a.value = arg->getInt();
        else
          CopyString(arg->getString(), a.arguments, sizeof(a.arguments));
    }
    
    //Supervisor::Instance->doAction(&a);
    context->mslAction(&a);

    // only MSL scripts set a result right now
    MslValue* v = pool->allocValue();
    v->setString(a.result);

    // what a long strange trip it's been
    popStack(v);
}

/**
 * Build a Query to dig out the value of an engine parameter.
 * These always run synchronously right now and don't care which
 * thread they're on, though that may change.
 *
 * The complication here is enumerations.
 * The engine always uses ordinal numbers where possible, but script users
 * don't think that way, they want enumeration names.  Use the weird
 * Type::Enum to return both so either can be used.
 */
void MslSession::query(Symbol* sym)
{
    if (sym->parameter == nullptr) {
        // should have checked this by now
        addError(stack->node, "Not a parameter symbol");
    }
    else {
        Query q;
        q.symbol = sym;
        
        // bool success = Supervisor::Instance->doQuery(&q);
        bool success = context->mslQuery(&q);
        
        if (!success) {
            addError(stack->node, "Unable to query parameter");
        }
        else if (q.async) {
            // not really an error, need a different message/warning list
            addError(stack->node, "Asynchronous parameter query");
        }
        else {
            MslValue* v = pool->allocValue();
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
                // hmm, the understanding of LevelUI symbols that live in
                // UIConfig and LevelCore symbols that live in MobiusConfig
                // is in Supervisor right now
                // todo: Need to repackage this
                // todo: this could also be Type::Enum in the value but I don't
                // think anything cares?
                v->setJString(Supervisor::Instance->getParameterLabel(sym, q.value));
            }
            else {
                // should only be here for TypeInt
                // unclear what String would do
                v->setInt(q.value);
            }

            popStack(v);
        }
    }
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
// Operators
//
//////////////////////////////////////////////////////////////////////

/**
 * An operator node will normally have two children, one for unary.
 *
 * Kind of forgot about pushNextChild while my mind was melting with MslSymbol,
 * revisit the complicated ones and see if that could be used there instead
 * of phase markers?
 */
void MslSession::mslVisit(MslOperator* opnode)
{
    if (opnode->children.size() == 0) {
        addError(opnode, "Missing operands");
    } 
    else {
        // tell popStack that we want all child values
        stack->accumulator = true;
        
        MslStack* nextStack = pushNextChild();
        if (nextStack == nullptr) {
            // ran out of children, apply the operator
            doOperator(opnode);
        }
    }
}


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
    MslValue* v = pool->allocValue();
    
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

        if (errors == nullptr) {
    
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
    
    popStack(v);
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
    bool bresult = false;

    if (value1->type == MslValue::String || value2->type == MslValue::String) {
        // it doesn't really matter if one side is a symbol Enum, they
        // both compare as strings
        // numeric coersion to strings is a little weird, does that cause trouble?
        if (equal)
          bresult = StringEqualNoCase(value1->getString(), value2->getString());
        else
          bresult = !StringEqualNoCase(value1->getString(), value2->getString());
    }
    else {
        // simple integer comparison
        if (equal)
          bresult = (value1->getInt() == value2->getInt());
        else
          bresult = (value1->getInt() != value2->getInt());
    }
    
    return bresult;
}

//////////////////////////////////////////////////////////////////////
//
// Conditionals
//
//////////////////////////////////////////////////////////////////////

/**
 * If nodes have at least two child nodes: a condition and a truth block
 * They may have an optional else block.  There will only be two phases.
 * First the condition block is pushed and evaluated and based on that
 * either the truth or false block is pushed.
 */
void MslSession::mslVisit(MslIf* node)
{
    if (stack->phase == 0) {
        // need the condition
        int nodes = node->children.size();
        if (nodes == 0) {
            // malformed no condition, ignore or error?
            addError(node, "If with no condition");
        }
        else if (nodes == 1) {
            addError(node, "If with no consequence");
        }
        else {
            stack->phase = 1;
            pushStack(node->children[0]);
        }
    }
    else if (stack->phase == 1) {
        // back from the conditional
        // what is truth?
        bool truth = false;
        if (stack->childResults != nullptr)
          truth = stack->childResults->getBool();

        stack->phase = 2;
        if (truth && node->children.size() > 1) {
            pushStack(node->children[1]);
        }
        else if (!truth && node->children.size() > 2) {
            pushStack(node->children[2]);
        }
        else {
            // if truth falls in the forest, does it make a return value?
            // probably should return null for accumulators
            popStack(pool->allocValue());
        }
    }
    else if (stack->phase == 2) {
        // back from the consequence
        popStack();
    }
}

/**
 * These could be collapsed by the parser since they do nothing but just act
 * as a placeholder node.
 */
void MslSession::mslVisit(MslElse* node)
{
    (void)node;
    MslStack* nextStack = pushNextChild();
    if (nextStack == nullptr)
      popStack();
}

//////////////////////////////////////////////////////////////////////
//
// Wait
//
//////////////////////////////////////////////////////////////////////

/**
 * Wait nodes control an extension of the MslStack object that has
 * information about what we are waiting on and when it has finished.
 * It is necessary to put this on the stack rather than inside the node
 * because the node can be shared by multiple sessions.
 * This is in the MslWait object embedded within the MslStack.
 *
 * Since ALL MslStacks will have an MslWait, I suppose that could be used
 * for other nodes besides MslWaitNode, but it isn't right now.
 *
 * When an MslStack has an active MslWait, the session (or thread within the session)
 * suspends until the wait state is cleared by the outside, or
 * it times out.  Timeout is not yet implemented.
 *
 * For MslWaitNode it is important to know if this is the first time this
 * is visited or the second time after the MslWait has been satisfied
 * so control must return here order to cancel the wait.  This makes MslWait
 * not usable for other nodes without additional work.
 *
 */
void MslSession::mslVisit(MslWaitNode* wait)
{
    if (stack->wait.active) {
        // we've been here before
        if (stack->wait.finished) {
            // back here after MslEnvironment::resume was called and the context
            // decided the wait was over
            // might want to have an interesting return value here

            // the assumption is that the session can't be resumed unless it is
            // in a context that is prepared to deal with it, I don't think we
            // can resume from the other side
            if (context->mslGetContextId() != MslContextKernel)
              Trace(1, "MslSession: Wait resumed outside of the kernel context");
            
            stack->wait.reset();
            popStack();
        }
        else {
            // still waiting, leave the stack as it is
        }
    }
    else {
        // starting a wait for the first time
        if (wait->type == WaitTypeNone) {
            addError(wait, "Missing wait type");
        }
        else if (wait->type == WaitTypeEvent && wait->event == WaitEventNone) {
            addError(wait, "Missing or invalid event name");
        }
        else if (wait->type == WaitTypeDuration && wait->duration == WaitDurationNone) {
            addError(wait, "Missing or invalid duration name");
        }
        else if (wait->type == WaitTypeLocation && wait->location == WaitLocationNone) {
            addError(wait, "Missing or invalid location name");
        }
        else {
            // evaluate the count/number/duration
            MslStack* nextStack = pushNextChild();
            if (nextStack == nullptr) {
                // transition if we're not in kernel before setting up the wait event
                // but after we've checked for syntax errors and evaluated the
                // wait value
                if (context->mslGetContextId() != MslContextKernel)
                  transitioning = true;
                else
                  setupWait(wait);
            }
        }
    }
}

/**
 * Set up the MslWait on this stack frame to start the wait.
 */
void MslSession::setupWait(MslWaitNode* node)
{
    MslWait* wait = &(stack->wait);
    
    wait->type = node->type;
    wait->event = node->event;
    wait->duration = node->duration;
    wait->location = node->location;

    // This is always just a number right not, it just means different things
    // depending on the wait type
    wait->value = 0;
    if (stack->childResults != nullptr)
      wait->value = stack->childResults->getInt();

    // ask the context to schedule something suitable to end the wait
    // the context is allowed to retain a pointer to the wait object, and
    // expected to set the finished flag when the wait is over
    // todo: would really like to be able to have the context include more
    // details about what was wrong with the wait request
    // since we're almost always in the kernel now, have memory issues
    if (!context->mslWait(wait))
      addError(node, "Unable to schedule wait state");

    // save where it came from, the session is necessary so MslEnvironment
    // knows which one to resume, I don't think the stack is, it just advances the session
    // and the wait will normally be the top of the stack
    wait->session = this;
    wait->stack = stack;

    // make it go, or rather stop
    wait->finished = false;
    wait->active = true;
}

//////////////////////////////////////////////////////////////////////
//
// Thread control, random
//
//////////////////////////////////////////////////////////////////////

/**
 * context "shell" | "kernel"
 *
 * This transitions the session to one side of the great divide.
 * Normally this happens automatcially as symbols or waits are reached
 * that must be handled in a specific context, but it can also be
 * forced for testing.
 *
 * Note that once the transitioning flag is set in this session it will
 * move to whatever the other side is relative to the current MslContext.
 * So don't set this unless the requested context is actually different
 * from where we already are, or else it may bounce back and forth
 * endlessly.
 */
void MslSession::mslVisit(MslContextNode* con)
{
    if (con->shell) {
        // the shell was requested
        if (context->mslGetContextId() == MslContextKernel) {
            // and it is different from where we are
            transitioning = true;
        }
        // else ignore
    }
    else {
        // the kernel was requested
        if (context->mslGetContextId() == MslContextShell)
          transitioning = true;
    }
    
    popStack(nullptr);
}

//////////////////////////////////////////////////////////////////////
//
// End
//
//////////////////////////////////////////////////////////////////////

/**
 * Todo: might be nice to have end return a value for the script?
 */
void MslSession::mslVisit(MslEnd* end)
{
    (void)end;
    MslValue* v = pool->allocValue();
    v->setString("end");
    popStack(v);
    while (stack != nullptr) popStack();
}

void MslSession::mslVisit(MslEcho* echo)
{
    (void)echo;
    MslStack* nextStack = pushNextChild();
    if (nextStack == nullptr) {
        if (stack->childResults != nullptr)
          context->mslEcho(stack->childResults->getString());
        // echo has no return value so we don't clutter up the console displaying it
        popStack(nullptr);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Debug
//
//////////////////////////////////////////////////////////////////////

juce::String MslSession::debugNode(MslNode* n)
{
    juce::String s;
    debugNode(n, s);
    return s;
}

void MslSession::debugNode(MslNode* n, juce::String& s)
{
    if (n == nullptr)
      s += "null ";
    else if (n->isBlock())
      s += "block ";
    else if (n->isSymbol())
      s += "symbol ";
    else if (n->isVar())
      s += "var ";
    else
      s += "??? ";

    if (n->children.size() > 0) {
        s += "[";
        for (auto child : n->children) {
            debugNode(child, s);
        }
        s += "]";
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
