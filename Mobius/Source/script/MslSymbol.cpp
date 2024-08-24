/**
 * Implementation related to Symbol nodes.
 *
 */

#include <JuceHeader.h>

#include "../util/Trace.h"

#include "MslModel.h"
#include "MslSymbol.h"
#include "MslScript.h"
#include "MslContext.h"
#include "MslEnvironment.h"
#include "MslSession.h"
#include "MslStack.h"
#include "MslExternal.h"

//////////////////////////////////////////////////////////////////////
//
// Linking
//
//////////////////////////////////////////////////////////////////////

/**
 * Linking
 *
 * Linking for variable/parameter references can be done to a degree, but we don't
 * error if a symbol is unresolved at link time.  There are two reasons for this, first
 * scripts can eventually reference variables defined in other scripts, and lacking any
 * "extern" declaration at the moment, we have to wait for the other script to be loaded
 * before it can be resolved.
 *
 * Because of dynamic binding, the values of variable references don't  "live" on a symbol
 * or an MslNode, they are found in MslBindings that live on the stack at runtime.
 * 
 * The actual variable to use may be defined by the execution context rather than the lexical context.
 *
 * proc foo(a) {
 *    print a b
 * }
 *
 * "b" is a free or "special" reference that may be bound at runtime like this:
 *
 * {var b = 1 foo(2)}  --> 2 1
 *
 * This is not used often but is allowed.
 * Actually it isn't the variable that's unresolved it's the binding.  If a script file
 * wants to reference a variable that is not defined in that unit, it needs to declare
 * it:
 *
 *     extern b
 *
 * External parameter symbols are implicitly declared extern.
 *
 * A more common use for unresolved symbols is in comparison of enumerated parameters
 *
 *     if switchQuantize == loop
 *
 * Here the value of the symbol is expected to be the name of the symbol.  In Lisp this
 * would normally be done by quoting the symbol 'loop but we don't have quote syntax and
 * I'm not convinced we need it here.  It will be confusing to explain.
 * How we deal nicely with enumerations is complex, and I won't belabor it here.  See design notes...
 *
 * Linking for function references can be done at compile time.  We do not support dynamic
 * binding of functions.  You can however establish a variable binding on the call stack with
 * the same name as a function.  Some Lisp dialects allow a symbol to have both a value and
 * a function binding, and you select between them with an explicit function call syntax.
 * I would like to avoid that.  If a symbol parses with call syntax:
 *
 *      foo(x)
 *
 * then it must resolve to a function.  But it is extremely common not to use an arguemnt list:
 *
 *      Record
 *
 * For a language with this limited execution environment, I don't think we need to mess
 * with defining variables with the same name as a function.  If you define a Var and the name
 * resoles to a proc within the same compilation unit, it is an error.  At minimum during
 * linking, if there is both a Var/Extern and a Proc with the same name, the Proc is preferred.
 *
 * The most complex part of symbol linking is the construction of the function call argument
 * expressions.  This is formed by combining several things.
 * 
 *      - default argument values defined in the proc definition
 *      - positional arguments passed in the call
 *      - keyword arguments passed in the call
 *
 * Because of flexibility in ordering and simplicity of syntax compared to most languages,
 * Evaluating the argument values to a function call isn't simply a matter of eveluating the
 * child block in the call.   The nuances around this are larger than can go in a file comment
 * see the design docs for more.
 */
void MslSymbol::link(MslContext* context, MslEnvironment* env, MslScript* script)
{
    // clear prior resolution
    function = nullptr;
    variable = nullptr;
    linkage = nullptr;
    external = nullptr;

    // first resolve to functions
    
    // look for local procs, these could be cached on the MslSymbol
    MslProc* proc = script->findProc(token.value);
    if (proc != nullptr) {
        function = proc;
    }
    else {
        // todo: look for exported procs defined in the environment
        // precedence question: should script exports be able to mess up
        // external symbol resolution?
        MslLinkage* link = env->findLinkage(token.value);
        if (link != nullptr && link->proc != nullptr)
          linkage = link;
        else {
            // look for an external
            // do we need to attempt dynamic resolution?
            external = env->getExternal(token.value);
            if (external == nullptr) {
                MslExternal retval;
                if (context->mslResolve(token.value, &retval)) {
                    // make one we can intern
                    external = new MslExternal(retval);
                    external->name = token.value;
                    env->intern(external);
                }
            }
        }
    }

    // if we couldn't find a function, look for variables
    // externals can't have overlap between variables and functions
    // so we'll already have found that

    if (function == nullptr && linkage == nullptr && external == nullptr) {
        
        MslVar* var = script->findVar(token.value);
        if (var != nullptr) {
            variable = var;
        }
        else {
            MslLinkage* link = env->findLinkage(token.value);
            if (link != nullptr && link->var != nullptr)
              linkage = link;
        }
    }
    
    // compile the call arguments from the function decclaration
    MslProc* procToLink = function;
    if (procToLink == nullptr) {
        // didn't have a local function
        if (linkage != nullptr) {
            // found an exported function in another acript
            procToLink = linkage->proc;
        }
        else if (external != nullptr && external->isFunction) {

            // todo: here we need a way for the context to give us the signature
            // for each external function, the easiest would be to provide
            // a signature that resembles a proc declaration
            // "(a b (global1 global2))"
            // and then parse that into an MslProc node, punt for now
        }
    }
    if (procToLink != nullptr)
      linkCall(procToLink);
}

/**
 * Construct the call argument block for an MSL function
 *
 * The basic algorithm is:
 *
 * for each argument defined in the proc
 *    is there an assignment for it in the call
 *        use the call assignment
 *    else is there an available non-assignment arg in the call
 *        use the call arg
 *    else is there an assignment in the proc (a default)
 *        use the proc
 *
 */
void MslSymbol::linkCall(MslProc* proc)
{
    juce::String error;
    arguments.clear();

    // copy the call args and whittle away at them
    juce::Array<MslNode*> callargs;
    for (auto child : children)
      callargs.add(child);

    // remember the position of each argument added to the list
    int position = 0;
    
    MslBlock* decl = proc->getDeclaration();
    if (decl != nullptr) {
        for (auto arg : decl->children) {
            MslSymbol* argsym = nullptr;
            MslNode* initializer = nullptr;
            if (arg->isSymbol()) {
                // simple named argument
                argsym = static_cast<MslSymbol*>(arg);
            }
            else if (arg->isAssignment()) {
                MslAssignment* argass = static_cast<MslAssignment*>(arg);
                // assignments have two children, LHS is the symbol to assign
                // and RHS is the value expression
                // would be nicer if the parser could simplify this
                if (argass->children.size() > 0) {
                    MslNode* node = argass->children[0];
                    if (node->isSymbol()) {
                        argsym = static_cast<MslSymbol*>(node);
                        if (argass->children.size() > 1)
                          initializer = argass->children[1];
                    }
                }
            }

            if (argsym == nullptr) {
                // not a symbol or well-formed assignment in the delcaration
                error = "Unable to determine function argument name";
            }
            else {
                // add a argument for this name
                juce::String name = argsym->token.value;
                MslArgument* argref = new MslArgument();
                argref->name = name;
                argref->position = position;
                position++;
                arguments.add(argref);

                // is there a keyword argument for this in the call?
                MslAssignment* callass = findCallKeyword(callargs, name);
                if (callass != nullptr) {
                    if (callass->children.size() > 1)
                      argref->node = callass->children[1];
                    else {
                        // no rhs on the assignment, something like this
                        // foo(... x =, ...) or foo(x=)
                        // this is most likely an error, but it could also be used to indiciate
                        // overriding a default from the function declaration with null
                    }
                }
                else {
                    // use the next available positional argument
                    MslNode* positional = findCallPositional(callargs);
                    if (positional != nullptr) {
                        argref->node = positional;
                    }
                    else if (initializer != nullptr) {
                        // use the default initializer from the declaration
                        argref->node = initializer;
                    }
                    else {
                        // no initializer and ran out of positionals, something is missing
                        error = "Missing function argument " + name;
                    }
                }
            }

            if (error.length() > 0)
              break;
        }
    }

    // anything left over are not in the function declaration
    // go ahead and pass them, they may be referenced with positional
    // references $1 in the function body, or may just represent temporary
    // symbol bindings
    if (error.length() == 0) {
        while (callargs.size() > 0) {
            MslNode* extra = callargs.removeAndReturn(0);
            MslArgument* argref = nullptr;
        
            if (extra->isAssignment()) {
                MslAssignment* argass = static_cast<MslAssignment*>(extra);
                // foo(...x=y)  becomes a local binding for this symbol
                if (argass->children.size() > 0) {
                    MslNode* node = argass->children[0];
                    if (node->isSymbol()) {
                        MslSymbol* argsym = static_cast<MslSymbol*>(node);
                        argref = new MslArgument();
                        argref->name = argsym->token.value;
                        if (argass->children.size() > 1)
                          argref->node = argass->children[1];
                    }
                }
            }
            else {
                // unnamed positional argument
                argref = new MslArgument();
                argref->node = extra;
            }

            if (argref != nullptr) {
                argref->extra = true;
                argref->position = position;
                position++;
                arguments.add(argref);
            }
        }
    }
    
    // !! todo: If we detected an error, need to convey this back to
    // the compiler or environment
    if (error.length() > 0) {
        Trace(1, "MslSymbol: Link failure %s", error.toUTF8());
        arguments.clear();
    }
}

/**
 * Find the call argument with a matching assignment name and remove
 * it from the consideration list.
 */
MslAssignment* MslSymbol::findCallKeyword(juce::Array<MslNode*>& callargs, juce::String name)
{
    MslAssignment* found = nullptr;

    for (auto arg : callargs) {
        if (arg->isAssignment()) {
            MslAssignment* argass = static_cast<MslAssignment*>(arg);
            if (argass->children.size() > 0) {
                MslNode* node = argass->children[0];
                if (node->isSymbol()) {
                    MslSymbol* argsym = static_cast<MslSymbol*>(node);
                    if (argsym->token.value == name) {
                        found = argass;
                        // why the fuck is it whining about this, are the docs wrong?
                        //callargs.remove(arg);
                        callargs.remove(callargs.indexOf(arg));
                        break;
                    }
                }
            }
        }
    }

    return found;
}

/**
 * Find the next positional non-keyword argument and remove it
 * from the consideration list.
 */
MslNode* MslSymbol::findCallPositional(juce::Array<MslNode*>& callargs)
{
    MslNode* found = nullptr;

    for (auto arg : callargs) {
        if (!arg->isAssignment()) {
            found = arg;
            // again, wtf??
            //callargs.remove(arg);
            callargs.remove(callargs.indexOf(arg));
            break;
        }
    }

    return found;
}

//////////////////////////////////////////////////////////////////////
//
// Evaluation
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
    else if (stack->external != nullptr) {
        // we resolved to an external model/Symbol and have completed
        // calculating the argument list
        // this may cause a thread transition
        returnExternal();
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
                    // this is old when we did symbol resolution at runtime
                    // keep it around but it should find the MslExternal and stop
                    // aw shit, the whole purpose of dynamic resolution originally
                    // was to allow "switchQuantize == loop" which everyone expects
                    // so if MslEnvironment bitches about that during linking it won't work
                    // it has to be allowed to go through to evaluation so we can treat
                    // it like a string
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
        stack->proc == nullptr && stack->external == nullptr) {
        
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
// Argument
//
//////////////////////////////////////////////////////////////////////

/**
 * These nodes are not parsed they are manufactured during symbol linking.
 * It simply passes along evaluation to the resolved argument value node.
 */
void MslSession::mslVisit(MslArgument* node)
{
    if (node->node != nullptr)
      node->node->visit(this);
}
 
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
 * There are two ways this can work: pre-linking and dynanmic linking.
 * MslEnvironment will attempt (if you ask it) to pre link symbols
 * to MslExternals and it will whine if they are unresolved.  This is nice
 * for the user so they can see the errors immediately when the script is
 * compiled in the script console.  Unfortunately it prevents a very common
 * use of symbols for enumerations:
 *
 *      if switchQuantize=loop
 *
 * "loop" in this case is not a bound symbol, it's the name of an enumeration element.
 *
 * If we prevent the script from compiling when that happens then they'll have to
 * use if switchQuantize="loop"   which isn't bad but annoying for them.
 *
 * An alternative here would be something like operator overloading casting where
 * if the LHS can be resolved to an enumeration, then the RHS is coerced into a value
 * suitable for that enumeration.  That would be nicer since we could find misspellings
 * in enumeration names.  This would be possible if we resolved switchQuantize first but
 * it's more work for the linker.
 *
 * Here we'll use pre-resolved MslExternals if they were left there, otherwise do dynamic
 * linking.
 *
 * If the user calls a function however foo(x) then it MUST resolve to an external.
 * This will be handled by returnUnresolved above.
 *
 */
bool MslSession::doExternal(MslSymbol* snode)
{
    bool resolved = false;

    MslExternal* external = resolveExternal(snode);
    if (external != nullptr) {
        resolved = true;

        // does this have arguments?
        if (snode->children.size() > 0 && stack->phase == 0) {
            // yes, push them
            // !! this is interesting, if this is a function that requires a transition
            // in which context should the arguments be evaluated?
            // probably the one it ends up in but not necessarily and we'll bounce around
            stack->phase = 1;
            pushStack(snode->children[0]);
        }
        else {
            // no arguments, or back from arguments, do the thing
            // useful to have a frame for this?
            stack->external = external;
            returnExternal();
        }
    }
    return resolved;
}

/**
 * Resolve an MslExternal for a symbol node.
 * The intent is that this has already been done by MslEnvironment
 * during a linking phase, but sometimes it forgets.
 */
MslExternal* MslSession::resolveExternal(MslSymbol* snode)
{
    MslExternal* external = snode->external;
    if (external == nullptr) {
        juce::String name = snode->token.value;
        // do we need to attempt dynamic resolution?
        external = environment->getExternal(name);
        if (external != nullptr) {
            snode->external = external;
        }
        else {
            MslExternal retval;
            if (context->mslResolve(name, &retval)) {
                // make one we can intern
                external = new MslExternal(retval);
                external->name = name;
                environment->intern(external);
                snode->external = external;
            }
        }
    }

    return external;
}
    
/**
 * Here after evaluating the argument block for an external symbol
 * which is expected to be a function.  Or returning from a transition.
 */
void MslSession::returnExternal()
{
    MslExternal* external = stack->external;
    
    // check transitions for functions
    // queries have so far not required transitions
    // wait, should we transition now or way
    if (external->isFunction &&
        external->context != MslContextNone &&
        external->context != context->mslGetContextId()) {
        transitioning = true;
    }
    else if (external->isFunction) {
        doExternalAction(external);
    }
    else {
        doExternalQuery(external);
    }
}

/**
 * Build an action to perform a function.
 * Thread transition should have already been performed, if not
 * this will be asynchronous and go through the non-script way of
 * doing thread transitions.  It should work but isn't immediate
 * and you can't wait on it.
 */
void MslSession::doExternalAction(MslExternal* ext)
{
    MslAction action;

    action.external = ext;
    action.arguments = stack->childResults;
    action.scope = getTrackScope();

    if (!context->mslAction(&action)) {
        // need both messages?
        addError(stack->node, "Error calling external function");
        if (strlen(action.error.error) > 0)
          addError(stack->node, action.error.error);
    }
    else {
        // the action handler was allowed to full in a single static MslResult
        MslValue* v = pool->allocValue();
        // !! this won't handle lists
        v->copy(&(action.result));

        // ah...now we have the "wait last" problem.
        // the action may have scheduled an event and the next
        // statement is often "wait last" to wait for it
        // but it isn't necessarily the next immediate statement so
        // in the mean time, where do we store the event pointer that
        // was returned in this action?

        if (action.event != nullptr)
          Trace(1, "MslSession: External action returned an event with no place to go");

        // what a long strange trip it's been
        popStack(v);
    }
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
void MslSession::doExternalQuery(MslExternal* ext)
{
    MslQuery query;

    query.external = ext;
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

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
