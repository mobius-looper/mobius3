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
 * See commentary in MslSession::visit(MslSymbol) below for more relevant discussion
 * on precedence if a function and a variable have the same names.
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
 * func foo(a) {
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
 * resoles to a function within the same compilation unit, it is an error.  At minimum during
 * linking, if there is both a Var/Extern and a Function with the same name, the Function is preferred.
 *
 * The most complex part of symbol linking is the construction of the function call argument
 * expressions.  This is formed by combining several things.
 * 
 *      - default argument values defined in the function definition
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
    arguments.clear();

    // determining whether something is a function or not is annoying
    // with the three different models
    bool isFunction = false;

    // special case for symbols used in assignments
    if (parent->isAssignment() && parent->children.size() > 1 &&
        parent->children[0] == this) {
        // we're the LHS of an assignment, special resolution applies
        linkAssignment(context, env, script);
        return;
    }

    // first resolve to functions
    
    // look for local functions, these could be cached on the MslSymbol
    MslFunction* func = script->findFunction(token.value);
    if (func != nullptr) {
        function = func;
        isFunction = true;
    }
    else {
        // todo: look for exported functions defined in the environment
        // precedence question: should script exports be able to mess up
        // external symbol resolution?
        MslLinkage* link = env->find(token.value);
        if (link != nullptr && (link->function != nullptr || link->script != nullptr)) {
            linkage = link;
            isFunction = true;
        }
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

                    isFunction = external->isFunction;
                }
            }
        }
    }

    // if we couldn't find a function, look for variables
    // externals can't have overlap between variables and functions
    // so we'll already have found that

    if (function == nullptr && linkage == nullptr && external == nullptr) {
        
        MslVariable* var = script->findVariable(token.value);
        if (var != nullptr) {
            variable = var;
        }
        else {
            MslLinkage* link = env->find(token.value);
            if (link != nullptr && link->variable != nullptr)
              linkage = link;
        }
    }

    // compile the call argument initializers by merging the
    // call arguments with the function signature
    if (isFunction) {
        MslBlock* signature = nullptr;
        if (function != nullptr) {
            signature = function->getDeclaration();
        }
        else if (linkage != nullptr) {
            if (linkage->function != nullptr)
              signature = linkage->function->getDeclaration();
            else if (linkage->script != nullptr)
              signature = linkage->script->getDeclaration();
        }
        else if (external != nullptr) {
            // todo: eventually externals need signatures too
        }
        
        linkCall(script, signature);
    }
    else {
        // todo: this resolved to a variable, if we have call arguments
        // should probably error now?
        if (children.size() > 0) {
            MslError* errobj = new MslError(this, "Variable symbol does not accept arguments");
            script->errors.add(errobj);
        }
    }
}

/**
 * Construct the call argument block for an MSL function
 *
 * The basic algorithm is:
 *
 * for each argument defined in the function
 *    is there an assignment for it in the call
 *        use the call assignment
 *    else is there an available non-assignment arg in the call
 *        use the call arg
 *    else is there an assignment in the function (a default)
 *        use the function
 *
 */
void MslSymbol::linkCall(MslScript* script, MslBlock* signature)
{
    juce::String error;
    
    // this isn't parsed so it won't start out with a parent pointer,
    // make sure it always has one
    arguments.parent = this;
    arguments.clear();

    // copy the call args and whittle away at them
    // the child list of a symbol is expected to be a single () block
    juce::Array<MslNode*> callargs;
    if (children.size() > 0) {
        MslNode* first = children[0];
        if (first->isBlock()) {
            for (auto child : first->children) {
                callargs.add(child);
            }
        }
    }

    // remember the position of each argument added to the list
    // these are $x reference positions starting from 1
    int position = 1;
    bool optional = false;
    
    if (signature != nullptr) {
        for (auto arg : signature->children) {

            // deal with keywords for :optional and :include
            if (arg->isKeyword()) {
                MslKeyword* key = static_cast<MslKeyword*>(arg);
                if (key->name == "optional") {
                    optional = true;
                }
                else {
                    error = "Invalid keyword";
                }
            }
            else {
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
                else {
                    // this is probably an error, what else would it be?
                }

                if (argsym == nullptr) {
                    // not a symbol or well-formed assignment in the delcaration
                    error = "Unable to determine function argument name";
                }
                else {
                    // add a argument for this name
                    juce::String name = argsym->token.value;
                    MslArgumentNode* argref = new MslArgumentNode();
                    argref->name = name;
                    argref->position = position;
                    // remember this for later when binding the results
                    argref->optional = optional;
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
                        else if (!optional) {
                            // no initializer and ran out of positionals, something is missing
                            error = juce::String("Missing function argument: ") + name;
                        }
                        else {
                            // optional arg with no initializer, I guess leave it
                            // in place with a null node, but could just keep it off
                            // the list entirely so it doesn't make a gratuituous
                            // binding.  Or perhaps that's what you want?
                        }
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
            MslArgumentNode* argref = nullptr;
        
            if (extra->isAssignment()) {
                MslAssignment* argass = static_cast<MslAssignment*>(extra);
                // foo(...x=y)  becomes a local binding for this symbol
                if (argass->children.size() > 0) {
                    MslNode* node = argass->children[0];
                    if (node->isSymbol()) {
                        MslSymbol* argsym = static_cast<MslSymbol*>(node);
                        argref = new MslArgumentNode();
                        argref->name = argsym->token.value;
                        if (argass->children.size() > 1)
                          argref->node = argass->children[1];
                    }
                }
            }
            else {
                // unnamed positional argument
                argref = new MslArgumentNode();
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
    
    if (error.length() > 0) {
        MslError* errobj = new MslError(this, error);
        script->errors.add(errobj);
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

/**
 * Here for a symbol used in the LHS of an assignment.
 * These can only resolve to a script variable or an external variable.
 *
 * Since we're not evaluating these, it doesn't really matter if we have both
 * a function and a variable with the same name because it only makes sense to
 * use the variable.  Still could check for that and raise an error since it is
 * likely to get an error later when you try to use the symbol being assigned.
 *
 * Like non-assignment symbol references, if there is a binding for this name
 * on the stack at runtime it will always be used.  Here we look for what
 * it could be if it were not bound.  The precedence is:
 *
 *    - locally declared Variables
 *    - exported Varialbes from other scripts
 *    - externals
 *
 * Like functions we prefer local definitions over externals so the addition
 * of new externals in the future doesn't break older scripts.
 *
 */
void MslSymbol::linkAssignment(MslContext* context, MslEnvironment* env, MslScript* script)
{
    // look for an export
    MslLinkage* link = env->find(token.value);
    if (link != nullptr && link->variable != nullptr) {
        // assign the exported variable
        linkage = link;
    }
    else {
        // look for a local declaration in the "closure"
        // !! is this really necessary?  should this always shadown an external?
        MslVariable* var = script->findVariable(token.value);
        if (var != nullptr) {
            variable = var;
        }
        else {
            // look for an external
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
        pushArguments(snode);
    }
    else {
        // a variable reference or a function with no arguments
        // always prefer a dynamic binding on the stack
        MslBinding* binding = findBinding(snode->token.value.toUTF8());
        if (binding != nullptr) {
            returnBinding(binding);    
        }
        else {
            // for function calls and exported variables the link() phase has already decided
            // the precidence on these, it looks like we're doing precidence here but only
            // one of these snode values will be set
            
            // look locally
            if (snode->function != nullptr) {
                // function call with no arguments, push the call
                pushCall(snode);
            }
            else if (snode->variable != nullptr) {
                // there is a local variable with no binding, meaning it is uninitialized
                // see code comments about why we don't fall back to exports here
                returnUnresolved(snode);
            }
            else if (snode->linkage != nullptr) {
                // then globally
                // same preference for functions over variables
                if (snode->linkage->function != nullptr || snode->linkage->script != nullptr) {
                    pushArguments(snode);
                }
                else if (snode->linkage->variable != nullptr) {
                    returnVariable(snode);
                }
                else {
                    // linker didn't do it's job
                    addError(snode, "Linkage with no sunshine");
                }
            }
            else if (snode->external != nullptr) {
                if (snode->external->isFunction) {
                    pushCall(snode);
                }
                else {
                    returnQuery(snode);
                }
            }
            else {
                // unresolved
                // see annoying commentary about enumeration symbols
                returnUnresolved(snode);
            }
        }
    }

    // sanity check
    // at this point we should have returned a value and popped the stack,
    // or pushed a new frame and are waiting for the result
    // if neither happened it is a logic error, and the evaluator will
    // hang if you don't catch this
    if (errors == nullptr && startStack == stack) {
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
    MslValue* v = pool->allocValue();
    v->setJString(snode->token.value);
    // todo, might want an unresolved flag in the MslValue
    popStack(v);
}

/**
 * Here for a symbol that resolved to a variable exported from another script.
 *
 * todo: Not implemented yet
 * Here we have what might loosly be called a "lexical closure" or a "member variable".
 *
 * Scripts need to be allowed to define things that behave like Mobius parameters.
 * Global variables that hold a value that doesn't need to be bound on the stack.
 *
 * You can think of a script as a class with static member variables in it.  Those need
 * to be stored somwehere, probably in a MslBinding list hanging off the MslScript.
 */
void MslSession::returnVariable(MslSymbol* snode)
{
    Trace(1, "MslSession: Reference to exported variable not implemented %s",
          snode->getName());
    
    // don't have a way to save values for these yet,
    // needs to either be in the MslLinkage or in a list of value containers
    // inside the referenced script

    // return null
    popStack(pool->allocValue());
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
    if (snode->function != nullptr) {
        
        pushBody(snode, snode->function->getBody());
    }
    else if (snode->linkage != nullptr) {

        if (snode->linkage->function != nullptr) {
            pushBody(snode, snode->linkage->function->getBody());
        }
        else if (snode->linkage->script != nullptr) {
            pushBody(snode, snode->linkage->script->root);
        }
        else {
            addError(snode, "Call with invalid linkage");
        }
    }
    else if (snode->external != nullptr) {
        callExternal(snode);
    }
    else {
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
    if (node->node != nullptr) {
        node->node->visit(this);
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
    // error checks that should have been done by now
    if (snode->external == nullptr) {
        addError(snode, "Attempting to query on something that isn't an external");
    }
    else if (snode->external->isFunction) {
        addError(snode, "Attempting to query on an external function");
    }
    else {
        MslQuery query;

        query.external = snode->external;
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
    MslExternal* external = snode->external;
    
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

        action.external = external;
        action.scope = getTrackScope();

        // external functions normally expect at most one argument
        // but we don't have signatures for those yet so pass whatever was
        // in the call list
        action.arguments = stack->childResults;

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
 * in the node as is done for MslFunction and MslVariable.  But this does open up potentially
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
        else if (namesym->linkage != nullptr) {
            // assignment of exported variable in another script
            // not implemented
            addError(ass, "Exported variable assignment not implemented");
        }
        else if (namesym->external != nullptr) {
            // assignments are currently expected to be synchronous and won't do transitions
            // but that probably needs to change
        
            MslAction action;
            action.external = namesym->external;
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
 * number to use when referenccingn externals.
 * One of these is created automatically by "in" but as a side effect of the way
 * that works you could also do this:
 *
 *    {var scope = 1 Record}
 *
 * Interesting...if we keep that might want a better name.
 */
int MslSession::getTrackScope()
{
    int scope = 0;

    MslBinding* b = findBinding("scope");
    if (b != nullptr && b->value != nullptr) {
        // need some sanity checks on the range
        scope = b->value->getInt();
    }
    return scope;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
