/**
 * Implementation related to Symbol nodes.
 *
 * These have the most complicated evaluation code so it was broken out
 * of MslSession to make it easier to manage.  
 */

#include <JuceHeader.h>

#include "../util/Trace.h"

#include "MslModel.h"
#include "MslSymbol.h"
#include "MslContext.h"
#include "MslEnvironment.h"
#include "MslSession.h"
#include "MslStack.h"
#include "MslExternal.h"
#include "MslVariable.h"

//////////////////////////////////////////////////////////////////////
//
// Evaluation
//
//////////////////////////////////////////////////////////////////////

/**
 * Now it gets interesting.
 *
 * Symbols can return the value of these things:
 *    a dynamic variable binding on the stack
 *    an exported variable value from another script
 *    the result of an local function call
 *    the result of a function call exported from another another script
 *    an external variable accessed with a query
 *    an external function call accessed with an action
 *    just the name literal of an unresolved symbol
 *
 * The only one that requires thread transition is the external action
 * though we might want to do this for some external queries as well.
 *
 * Unresolved symbols are usually an error, but there are a few cases where
 * we allow that for the names of external parameter values that are enumerations.
 * todo: this is messy and needs thought.
 *
 * For functions, the decisions about what to call and what the arguments should be
 * were made during the link() phase and left on the MslSymbol object.
 *
 * For variables, we first look for a dynamcic binding on the stack, if not bound
 * then query the other script or external context for the value.
 *
 * Name collisions are not expected in well written scripts but can happen.
 * If the symbol has call arguments, it must become a function call.  If the
 * link phase did not resolve to a function, it is an error.
 *
 * If the symbol has no arguments, it may either be a variable reference or a function
 * call.  This is particularly common for external functions like "Record".
 * If there is a variable binding on the stack for this name, it is unclear what to do:
 *
 *     {var Record=1 Record}
 *
 * For externals this makes no sense.  For things within the script environment it might
 * since there is less control over the names of things.  For example: Script A exports
 * a function named "ImportantValue" that takes no arguments, it just calculates a value
 * and returns it.
 *
 * Script B was written by someone else and wants to define a Variable with the same name.
 * Within Script B, the value of the local variable should be preferred over a function
 * in a different script.  A similar argument could be made for externals, over time we may add
 * new externals that conflict with names in older scripts.
 *
 * So the rule is: For un-argumented symbols, if there is a dynamic binding on the stack, use it.
 *
 * For functions and variables that have definitions in both the script environment and
 * as externals, the script environment is preferred.  Again this allows new externals to
 * be added over time without breaking old scripts.  If the script author wants to use
 * the new externals, they must change the script.
 *
 * For variable references that have a local definition and a different one exported
 * from another script, the local definition is preferred.
 *
 * It normally should not happen but if there is a name collisions with a function and a variable
 * either locally or exported, the function is preferred.
 * todo: this seems rather arbitrary, it probably should be an error:
 *
 *    var a=1
 *    func a {2}
 *    print a
 *
 * We don't have a syntax like "funcall" to prefer one over the other.
 *
 * Ugh, another weird case.  If there is a local variable declaration that is not initialized,
 * there will be no binding on the stack.  If another script exports a variable with the same name
 * do you 1) treat the reference as unbound or 2) use the exported variable.
 * I think 1.
 * 
 * Evaluation Phases
 * 
 * Phase 0: First time here, figure out what to do
 * Phase 1: Back from the evaluation of the function call arguments
 * Phase 2: Back from the evaluation of the function body
 *
 */
void MslSession::mslVisit(MslSymbol* snode)
{
    logVisit(snode);
    // for safety check later
    MslStack* startStack = stack;

    if (stack->phase == 2) {
        // back from a function call, we're done
        popStack();
    }
    else if (stack->phase == 1) {
        // back from arguments, call the function
        pushCall(snode);
    }
    else if (snode->arguments.size() > 0) {
        // set up a function call
        // linker should already have verified this resolved to a function
        if (!snode->resolution.isFunction())
          addError(snode, "Call syntax for a symbol that was not resolved to a function");
        else
          pushArguments(snode);
    }
    else {
        // a variable reference or a function with no arguments
        // always prefer a dynamic binding on the stack
        // !! I don't think this is what the new MslLinker expects and it should
        // have errored at this point
        // also this will override the use of "in all" if you bind "all" to a variable
        // which is not intended
        MslBinding* binding = findBinding(snode->token.value.toUTF8());
        if (binding != nullptr) {

            if (snode->resolution.isResolved() && snode->resolution.isFunction())
              addError(snode, "Conflict between variable binding and resolved function");
            
            returnBinding(binding);    
        }
        else if (!snode->resolution.isResolved()) {
            
            returnUnresolved(snode);
        }
        else if (!snode->resolution.isFunction()) {
            // must be a variable
            if (snode->resolution.keyword)
              returnKeyword(snode);
            
            else if (snode->resolution.external != nullptr)
              returnQuery(snode);
            
            else if (snode->resolution.linkage != nullptr)
              returnLinkedVariable(snode);

            else if (snode->resolution.staticVariable != nullptr)
              returnStaticVariable(snode);

            else {
                // it's either a functionArgument or a localVariable
                // should have found a binding from findBindings above, if not
                // it's a missing argument which should have been caught by now
                // or something else I missed
                addError(snode, "Bindings failed us");
            }
        }
        else {
            // it's a function call with no arguments
            pushCall(snode);
        }
    }

    // sanity check
    // at this point we should have returned a value and popped the stack,
    // or pushed a new frame and are waiting for the result
    // if neither happened it is a logic error, and the evaluator will
    // hang if you don't catch this
    if (errors == nullptr && startStack == stack && !transitioning) {
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
 *
 * update: now that we have keyword symbols, it isn't unreasonable to reequire
 * the use if switchQuantize == :loop and we can treat unresolved symbols as
 * an error.  This is better for diagnostics because otherewise they don't know
 * nothing happened.
 */
void MslSession::returnUnresolved(MslSymbol* snode)
{
    // lenient way
/*    
    MslValue* v = pool->allocValue();
    v->setJString(snode->token.value);
    // todo, might want an unresolved flag in the MslValue
    popStack(v);
*/
    Trace(1, "MslSession: Reference to unresolved symbol %s\n", snode->token.value.toUTF8());
    addError(snode, "Unresolved symbol");
}

/**
 * The value of a keyword is it's name
 */
void MslSession::returnKeyword(MslSymbol* snode)
{
    MslValue* v = pool->allocValue();
    v->setJString(snode->token.value);
    popStack(v);
}

/**
 * Here for a symbol that resolved to a top-level variable that
 * was declared global or exported and is referenced within the
 * script that defined it.  In this case there may be no MslLinkage
 * but the variable is still static for the compilation unit where
 * it was defined.
 */
void MslSession::returnStaticVariable(MslSymbol* snode)
{
    MslVariable* rv = snode->resolution.staticVariable;
    MslValue* v = pool->allocValue();

    // !! needs to be csect protected
    rv->getValue(getEffectiveScope(), v);
    
    popStack(v);
}

/**
 * Here for a symbol that resolved to a public or exported static
 * variable defined in another script.  These must indirece
 * through an MslLinkage so the defining script can be reloaded.
 */
void MslSession::returnLinkedVariable(MslSymbol* snode)
{
    MslLinkage* link = snode->resolution.linkage;
    MslVariable* var = link->variable;
    MslValue* value = pool->allocValue();
    
    if (var == nullptr) {
        Trace(1, "MslSession: Unresolved variable link %s",
              snode->getName());
    }
    else {
        // !! this needs to be csect protected
        var->getValue(getEffectiveScope(), value);
    }
    popStack(value);
}

/**
 * Here we've got a function call that might have an argument block.
 * If it does push them and set the phase to 1.
 *
 * This relies on link() resolving any name ambiguities and leaving only
 * the appropriate function reference behind on the symbol, which must
 * match the compiled argument list.  So while it looks we might be dealing
 * with more than one possibility here, that decision has already been made.
 *
 */
void MslSession::pushArguments(MslSymbol* snode)
{
    if (snode->arguments.size() == 0) {
        // no arguments, just call it
        pushCall(snode);
    }
    else {
        stack->phase = 1;
        pushStack(&(snode->arguments));
        stack->accumulator = true;
    }
}

/**
 * Here we're back from evaluation the function call arguments and are
 * ready to call the function.
 *
 * This relies on link() resolving any name ambiguities and leaving only
 * the appropriate function reference behind on the symbol, which must
 * match the compiled argument list.  So while it looks we might be dealing
 * with more than one possibility here, that decision has already been made.
 */
void MslSession::pushCall(MslSymbol* snode)
{
    if (snode->resolution.external != nullptr) {
        callExternal(snode);
    }
    else {
        MslBlock* body = snode->resolution.getBody();
        if (body != nullptr)
          pushBody(snode, body);
        else
          addError(snode, "Call with nowhere to go");
    }
}

/**
 * Push either a local or exported function body.
 *
 * If the function has no body, what is its value?
 * This could be an error, or the user may just have wanted to comment it out.
 * We don't currently have the notion of a "void" function so return nil for now.
 * Could have avoided evaluating the argument list in this case.
 */
void MslSession::pushBody(MslSymbol* snode, MslBlock* body)
{
    if (body == nullptr) {
        // nothing to do, nil
        popStack();
    }
    else {
        bindArguments(snode);
        stack->phase = 2;
        pushStack(body);
    }
}

/**
 * Convert the previously evaluated argument list for a function call
 * into MslBindings on the stack frame.
 * The values for the binding are in the stack->childResults.
 */
void MslSession::bindArguments(MslSymbol* snode)
{
    int position = 1;
    
    for (auto node : snode->arguments.children) {

        if (!node->isArgument()) {
            addError(snode, "WTF did you put in the argument list?");
            break;
        }
        MslArgumentNode* arg = static_cast<MslArgumentNode*>(node);

        MslBinding* b = pool->allocBinding();
        b->setName(arg->name.toUTF8());
        // ownership of the value transfers
        if (stack->childResults == nullptr) {
            // this should only happen for :optional arguments
            if (!arg->optional) {
                // did not evaluate enough arguments, should not happen
                // should this fail or move on?
                addError(snode, "Not enough arguments to function call");
            }
            else {
                // should this bind anything?  if we leave it on the stack
                // it will shadow arguments bound above
                // todo: unclear, feels like both are useful
                // leave it null
            }
        }
        else {
            b->value = stack->childResults;
            stack->childResults = stack->childResults->next;
            b->value->next = nullptr;
        }

        // also give it a position for $n references
        // this was left in the MslArgumentNode but we don't
        // really need it there, it's always the same as the list position
        // right?
        if (position != arg->position) {
            Trace(1, "MslSession::bindArguments Mismatched argument position, wtf?");
        }
        b->position = position;
        position++;

        b->next = stack->bindings;
        stack->bindings = b;
    }

    if (stack->childResults != nullptr) {
        // more results than expected, should not happen
        // even random dynamcic bindings or extra call args should have been given an argument node
        addError(snode, "Extra arguments to function call");
    }
}        

//////////////////////////////////////////////////////////////////////
//
// ArgumentNode
//
//////////////////////////////////////////////////////////////////////

/**
 * These nodes are not parsed they are manufactured during symbol linking.
 * It simply passes along evaluation to the resolved argument value node.
 */
void MslSession::mslVisit(MslArgumentNode* node)
{
    logVisit(node);
    if (node->node != nullptr) {
        if (stack->phase == 0) {
            stack->phase = 1;
            pushStack(node->node);
        }
        else {
            MslValue* cresult = stack->childResults;
            stack->childResults = nullptr;
            popStack(cresult);
        }
    }
    else {
        // argument with no initializer
        // this should only happen for optional arguments with
        // nothing passed in the call
        // leave a null here or just nothing?
        popStack();
    }
}
 
//////////////////////////////////////////////////////////////////////
//
// Externals
//
//////////////////////////////////////////////////////////////////////

/**
 * The symbol references an external variable.
 * 
 * Build an MslQuery and submit it to the container.
 * These always run synchronously right now and don't care which
 * thread they're on, though that may change.
 *
 * The complication here is enumerations.
 * The Mobius engine always uses ordinal numbers where possible, but script users
 * don't think that way, they want enumeration names.  Use the weird
 * Type::Enum to return both so either can be used.
 */
void MslSession::returnQuery(MslSymbol* snode)
{
    MslExternal* external = snode->resolution.external;
    
    // error checks that should have been done by now
    if (external == nullptr) {
        addError(snode, "Attempting to query on something that isn't an external");
    }
    else if (external->isFunction) {
        addError(snode, "Attempting to query on an external function");
    }
    else {
        MslQuery query;

        query.external = external;

        // todo: unclear if this should send our internal scope ids and expect
        // the container to map that back to track numbers, or if we should do that
        // mapping here...it's realky the same we ask container to do the mapping now
        // or later.  Actually MslIn is broken because it is taking a user-spece
        // scope identifier and assuming that is an internal scope id which it isn't
        // but works for now as long as scopeId==trackNumber
        query.scope = getTrackScope();

        if (!context->mslQuery(&query)) {
            // need both messages?
            addError(stack->node, "Error retrieving external variable");
            if (strlen(query.error.error) > 0)
              addError(stack->node, query.error.error);
        }
        else {
            MslValue* v = pool->allocValue();
        
            // and now we have the ordinal vs. enum symbol problem
            // with the introduction of MslExternal that mess was pushed into the MslContext
            // and it is supposed return a value with TypeEnum

            v->copy(&(query.value));

            popStack(v);
        }
    }
}

/**
 * The symbol references an external function.
 * 
 * Build an MslAction and submit it to the container.
 * This may need a thread transition.
 *
 * The complication here is enumerations.
 * The Mobius engine always uses ordinal numbers where possible, but script users
 * don't think that way, they want enumeration names.  Use the weird
 * Type::Enum to return both so either can be used.
 */
void MslSession::callExternal(MslSymbol* snode)
{
    MslExternal* external = snode->resolution.external;
    
    // error checks that should have been done by now
    if (external == nullptr) {
        addError(snode, "Attempting to call something that isn't an external");
    }
    else if (!external->isFunction) {
        addError(snode, "Attempting to call an external variable");
    }
    else if (external->context != MslContextNone &&
             external->context != context->mslGetContextId()) {
        // ask for a transition
        // if this didn't happen, it will still usually work through
        // an asynchronous action but those take time and you can't wait on it
        // need to get the transition right
        transitioning = true;
    }
    else {
        MslAction action;

        action.session = this;
        action.external = external;
        // see mslQuery for questions around what this scope number should be
        action.scope = getTrackScope();

        // external functions normally expect at most one argument
        // but we don't have signatures for those yet so pass whatever was
        // in the call list
        action.arguments = stack->childResults;

        // reset async action state before calling
        asyncAction.init();

        if (!context->mslAction(&action)) {
            // need both messages?
            addError(stack->node, "Error calling external function");
            if (strlen(action.error.error) > 0)
              addError(stack->node, action.error.error);
        }
        else {
            // the action handler was allowed to fill in a single static MslResult
            MslValue* v = pool->allocValue();
            // !! this won't handle lists
            v->copy(&(action.result));

            // if the action returned async event state, save it
            if (action.event != nullptr) {
                asyncAction.event = action.event;
                asyncAction.eventFrame = action.eventFrame;
            }

            // what a long strange trip it's been
            popStack(v);
        }
    }
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
 * in the node as is done for MslFunctionNode and MslVariableNode.  But this does open up potentially
 * useful behavior where the LHS could be any expression that produces a name
 * literal string:    "x"=y  or foo()=y.  While possible and relatively easy
 * that's hard to explain.
 *
 * Like non-assignment symbols, the link() phase will have resolved this to a locally
 * declared Variable, an exported variable from another script, or an external.
 *
 * Also like non-assignment symbols, if there is a binding on the stack at runtime that
 * takes precedence over where the assignment goes.
 *
 * Evaluation Phases
 *
 * Phase 0: figure out what to do
 *
 * Phase 1: evaluating the RHS value to assign
 *
 */
void MslSession::mslVisit(MslAssignment* ass)
{
    logVisit(ass);
    if (stack->phase == 1) {
        // back from the initializer expression
        if (stack->childResults == nullptr) {
            // something weird like "x=var foo;" that that the parser
            // could have caught
            addError(ass, "Malformed assignment, initializer had no value");
        }
        else {
            doAssignment(ass);
        }
    }
    else {
        // verify we have an assignment target symbol before we bother
        // evaluating the initializer
        MslSymbol* namesym = getAssignmentSymbol(ass);
        if (namesym != nullptr) {
            
            MslNode* initializer = ass->get(1);
            if (initializer == nullptr) {
                addError(ass, "Malformed assignment, missing initializer");
            }
            else {
                // push the initializer
                stack->phase = 1;
                pushStack(initializer);
            }
        }
    }
}

/**
 * Derive the target symbol for the assignment.
 * Could have done this at parse time and just left it in the MslAssignment
 */
MslSymbol* MslSession::getAssignmentSymbol(MslAssignment* ass)
{
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
    return namesym;
}

/**
 * At this point, we've evaluated what we need, and are ready to make the assignment.
 * A thread transition may need to be made depending on the target symbol.
 * The stack childResults has the value to assign.
 *
 * If we have to do thread transition, we're going to end up looking for bindings twice,
 * could skip that with another stack phase but it shouldn't be too expensive.
 */
void MslSession::doAssignment(MslAssignment* ass)
{
    MslSymbol* namesym = getAssignmentSymbol(ass);
    if (namesym != nullptr) {
    
        // if there is a dyanmic binding on the stack, it always gets it first
        MslBinding* binding = findBinding(namesym->token.value.toUTF8());
        if (binding != nullptr) {
            // transfer the value
            pool->free(binding->value);
            binding->value = stack->childResults;
            stack->childResults = nullptr;

            // and we are done, assignments do not have values though I suppose
            // we could allow the initializer value to be the assignment node value
            // as well, Lisp does that, not sure what c++ does
            popStack(nullptr);
        }
        else if (namesym->resolution.staticVariable != nullptr) {
            assignStaticVariable(namesym->resolution.staticVariable, stack->childResults);
            popStack(nullptr);
        }
        else if (namesym->resolution.linkage != nullptr) {
            // assignment of public variable in another script
            MslVariable* var = namesym->resolution.linkage->variable;
            if (var == nullptr)
              addError(ass, "Missing variable in linkage");
            else {
                assignStaticVariable(var, stack->childResults);
                popStack(nullptr);
            }
        }
        else if (namesym->resolution.external != nullptr) {
            // assignments are currently expected to be synchronous and won't do transitions
            // but that probably needs to change
        
            MslAction action;
            action.external = namesym->resolution.external;
            action.scope = getTrackScope();

            if (stack->childResults == nullptr) {
                // assignment with no value, I suppose this could mean "set to null"
                // but it's most likely a syntax error
                addError(stack->node, "Assignment with no value");
            }
            else {
                // assignments are currently only to atomic values
                // though the MslAction model says that the value can be a list
                // chained with the next pointer
                // if we ever get to the point where lists become usable data types
                // then there will be ambiguity here between the child list as the list
                // we want to pass vs. an element of the child list HAVING a list value
                action.arguments = stack->childResults;

                // here is the magic bean
                // assignments are not async so don't need to reset asyncState
                context->mslAction(&action);

                // assignment actions are not expected to have return value
                // though they can have errors
                // the MslContextError wrapper doesn't provide any purpose beyond the message
                if (strlen(action.error.error) > 0) {
                    // this will copy the string to the MslError
                    addError(stack->node, action.error.error);
                }
            
                popStack(nullptr);
            }
        }
        else {
            // unlike references, the name symbol of an assignment must resolve
            addError(namesym, "Unresolved symbol");
        }
    }
}

/**
 * Look up the stack for a binding for "scope" which will be taken as the track
 * number to use when referencing externals.
 * One of these is created automatically by "in" but as a side effect of the way
 * that works you could also do this:
 *
 *    {var scope = 1 Record}
 *
 * Interesting...if we keep that might want a better name.
 *
 * update: there is now getEffectiveScope() which saves the MslIn scope number
 * on the stack rather than as a binding.  That is what static track variable
 * referencing uses, so should be consistent about this.  I'm not sure I like
 * using bindings to control this.  Feels better to build it into the stack, but
 * might want to support a user defined binding as an option?
 * 
 */
int MslSession::getTrackScope()
{
    int scope = 0;

    // if you want to supporr this then the traversal has to be done
    // in getEffectiveScope because it is the NEAREST of either the stack
    // sccope or the binding that wins
    bool oldWay = false;
    if (oldWay) {
        MslBinding* b = findBinding("scope");
        if (b != nullptr && b->value != nullptr) {
            // need some sanity checks on the range
            scope = b->value->getInt();
        }
    }

    if (scope == 0)
      scope = getEffectiveScope();
        
    return scope;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
