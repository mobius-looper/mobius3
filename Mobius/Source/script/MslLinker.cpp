/**
 * The linking phase after compilation.
 *
 * This does three things:
 *
 *    - resolve the symbol name to something
 *    - check for collisions on that name
 *    - compile function call arguments
 *
 * Symbols may resolve to three different levels in this order of priority.
 *
 *     local definitions defined lexically within the compilation unit
 *     exported definitions from another unit
 *     external definitions defined by the containing application
 *
 * There normally should not be name collisions between levels but this can't
 * be enforced as scripts written by different people can be combined and externals
 * can be added over time that collide with old scripts.
 *
 * Analysis of name collisions is handled elsewhere.
 *
 * Locally, functions and variables are usually defined at the top level (root block)
 * of the unit.  It is permissible however to define them in inner blocks which
 * then "shadow" the definition of the outer blocks.  This is often done for variables,
 * but rarely for functions.
 *
 * In general, within a compilation unit, having a function and a variable with the same
 * name should be avoided, but cannot be guaranteed because users gonna user.  Definitions
 * at inner levels always shadow those at higher levels.  Within one level/block if there
 * is a function and a variable defined with the same name the function is prefered.
 * This should be raised as an error elsewhere.  Here we could add a warning.
 * 
 * If there is an argument block on the symbol it must resolve to a function.  If
 * it resolves to a variable it is an error.
 *
 * Definitions exported from other scripts are more constrained and name collisions
 * are prevented.  There will be only one MslLinkage associcated with a name, and
 * the linkage can only reference one thing.
 *
 * It is required by the containing application that Externals have no name collisions.
 *
 * Linking for variable/parameter references can be done to a degree, but we don't
 * error if a symbol is unresolved at link time.  There are two reasons for this, first
 * scripts can eventually reference variables defined in other scripts, and lacking any
 * "extern" declaration at the moment, we have to wait for the other script to be loaded
 * before it can be resolved.
 *
 * External parameter symbols are implicitly declared extern.
 *
 * A more common use for unresolved symbols is in comparison of enumerated parameters
 *
 *     if switchQuantize == loop
 *
 * Here the value of the symbol is expected to be the name of the symbol.  In Lisp this
 * would normally be done by quoting the symbol 'loop but we don't have quote syntax.
 * I did introduce the concept of keyword symbols so it is supposed to be written ":loop".
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
     
#include <JuceHeader.h>

#include "../util/Trace.h"

#include "MslContext.h"
#include "MslEnvironment.h"
#include "MslCompilation.h"
#include "MslModel.h"
#include "MslSymbol.h"
#include "MslError.h"

#include "MslLinker.h"

/**
 * Link everything in a compilation unit.
 */
void MslLinker::link(MslContext* c, MslEnvironment* e, MslCompilation* u)
{
    context = c;
    environment = e;
    unit = u;

    // reset results in the unit
    unit->errors.clear();
    unit->warnings.clear();
    unit->collisions.clear();
    unit->unresolved.clear();

    link(unit->getInitFunction());
    link(unit->getBodyFunction());

    for (auto func : unit->functions)
      link(func);

    // todo: figure out how to do variable iniitalizers
    // they either need to be part of the inititialization block
    // or we need to link them in place

    // if symbol resolution was succesful check for name collisions
    checkCollisions();
}

void MslLinker::link(MslFunction* f)
{
    if (f != nullptr) 
      link(f->getBody());
}

void MslLinker::link(MslNode* node)
{
    if (node != nullptr) {
        // first link any chiildren
        for (auto child : node->children) {
            link(child);
            // todo: break on errors or keep going?
        }

        // now the hard part
        // only symbols need special processing right now
        if (node->isSymbol()) {
            MslSymbol* sym = static_cast<MslSymbol*>(node);
            link(sym);
        }
    }
}

void MslLinker::addError(MslNode* node, juce::String msg)
{
    MslError* errobj = new MslError(node, msg);
    unit->errors.add(errobj);
    Trace(1, "MslLInker: Link failure %s", msg.toUTF8());
}

void MslLinker::addWarning(MslNode* node, juce::String msg)
{
    MslError* errobj = new MslError(node, msg);
    unit->warnings.add(errobj);
    Trace(2, "MslLInker: Link warning %s", msg.toUTF8());
}

//////////////////////////////////////////////////////////////////////
//
// Collision Detection
//
//////////////////////////////////////////////////////////////////////

/**
 * Collision detection involves looking for every symbol this compilation
 * unit wants to export to the MslLinkage table.  If there is already
 * a linkage defined for a different unit, there is a collision.
 *
 * todo: We don't have the notion of exported functions and variable yet
 * so just for testing purposes, assume that any function found in the unit
 * will be exported.  This is temporary.
 *
 * What IS important is that if the unit has an executable body, the name
 * of the unit (aka the "script" from the user's perspective) must not collide.
 */
void MslLinker::checkCollisions()
{
    // first the script body function
    MslFunction* f = unit->getBodyFunction();
    if (f != nullptr) {
        checkCollision(f->name);
    }
    
    // then the exported functions
    for (auto function : unit->functions) {
        // todo: if function->isExported
        checkCollision(function->name);
    }

    // todo: check exported variables
}

void MslLinker::checkCollision(juce::String name)
{
    MslLinkage* link = environment->find(name);
    if (link != nullptr &&
        link->unit != nullptr &&
        // note that it isn't enough to compare unit pointers, when
        // we're replacing or extending will have a new unit that will
        // overwrite the old one
        link->unit->id != unit->id) {

        MslCollision* col = new MslCollision();
        col->name = name;
        col->fromPath = unit->id;
        col->otherPath = link->unit->id;

        unit->collisions.add(col);
    }
}

/**
 * This interface is used to do post-installation collision checking
 * only.
 */
void MslLinker::checkCollisions(MslEnvironment* env, MslCompilation* comp)
{
    context = nullptr;
    environment = env;
    unit = comp;

    unit->collisions.clear();

    checkCollisions();
}

//////////////////////////////////////////////////////////////////////
//
// Symbol Resolution
//
//////////////////////////////////////////////////////////////////////

/**
 * Resolving a symbol involves two things: finding the thing it
 * references, and the compilation of function call arguments.
 *
 * Variable references are fairly straightforward, we just look for
 * a variable in various places and leave a pointer to it.  At runtime
 * the value is fetched and returned to the interpreter.
 *
 * Function references are far more complex due to the way argument
 * passing works.  See file header comments for gory details on the flexibility
 * of arguments.
 *
 * If a symbol does not resolve, the name is left in the "unresolved" list
 * of the compiation unit.  This can be treated as an error or not depending
 * on the context of the linking.
 *
 * Symbols on the LHS of an assignment "x = y" can only resolve to a variable.
 *
 * A symbol can only resolve to one thing, but I suppose we could leave all possible
 * resolutions behind and allow some kind of scoping syntax. e.g.
 *
 *      foo vs scriptname:foo vs external:foo
 *
 * This relates to the "packages" concept that needs more thought down the road.
 *
 * Functions and variables can come from three places:
 *
 *     local definitions made lexically within this compilation unit
 *     links to definitions exported from other units
 *     externals defined by the containing application
 *
 * If an object with the same name exists in several places, the one in the
 * order above is preferred.
 *
 * If a function and variable in local scope have the same name, it
 * is an error.
 *     
 */
void MslLinker::link(MslSymbol* sym)
{
    resolve(sym);

    if (sym->isResolved()) {

        if (sym->parent->isAssignment() &&
            sym->parent->children.size() > 1 &&
            sym->parent->children[0] == sym) {
            // we're the LHS of an assignment, this can only resolve to a variable
            if (sym->resolution.isFunction()) {
                addError(sym, juce::String("Assignment target resolved to a function"));
                sym->resolution.reset();
            }
        }
        else if (!sym->resolution.isFunction() && sym->children.size() > 0) {
            // this resolved to a variable but there is an argument
            // block meaning the user thought it was a function call
            // we could force resolution to a function if there are arguments
            // but I think this would be confusing if there is ambiguity
            addError(sym, juce::String("Symbol with arguments resolved to a variable"));
        }
        else if (sym->resolution.isFunction()) {
            compileArguments(sym);
        }
    }
}

/**
 * Resolve a reference through the various levels.
 * Leave a warning if it was unresolved.
 */
void MslLinker::resolve(MslSymbol* sym)
{
    // this helps maintain state while we look for things
    // todo: it would be useful to have a utility that examines
    // the entire space of definitions in references in the environment
    // and the externals to look for name overlaps that could be confusing
    sym->resolution.reset();
        
    // first look locally
    resolveLocal(sym);
    if (!sym->isResolved()) {
        // then within other units in the environment
        resolveEnvironment(sym);
        if (!sym->isResolved()) {
            // then externals
            resolveExternal(sym);
        }
    }
        
    if (!sym->isResolved()) {
        // todo: here is where we could try to be smart about
        // the "if switchQuantize == loop" problem and issue a more helpful
        // warning about using keyword symbols
        addWarning(sym, juce::String("Unresolved symbol ") + sym->token.value);
        unit->unresolved.add(sym->token.value);
    }
}

/**
 * Attempt to resolve the symbol locally within the compilation unit.
 */
void MslLinker::resolveLocal(MslSymbol* sym)
{
    // start looking up the stack for a function or variable definition
    resolveLocal(sym, sym->parent);
}

/**
 * Recurse up the parse tree looking for something, anything.
 * This is assumign that definitions haven't been moved into any
 * special locations outside the node's child list.  This may have
 * to change...
 *
 * There may be some nodes that need special thought about the semantics
 * of their child blocks, but I don't think so.  "var" for example
 * might have special meaning if it is inside the argument declaration
 * block of a function definition?
 */
void MslLinker::resolveLocal(MslSymbol* sym, MslNode* node)
{
    if (node->isFunction()) {
        // we're inside a function definition, function signature symbols
        // will have bindings at runtime
        MslFunctionNode* def = static_cast<MslFunctionNode*>(node);
        resolveFunctionArgument(sym, def);
    }
    else {
        for (auto child : node->children) {
        
            MslFunctionNode* func = nullptr;
            MslVariable *var = nullptr;

            if (child->isFunction())
              func = static_cast<MslFunctionNode*>(child);
        
            else if (child->isVariable())
              var = static_cast<MslVariable*>(child);

            if (func != nullptr && var != nullptr) {
                // a block had both a function and a variable with the same name
                // I think we can consider this an error rather than picking one at random
                // in this case the node with the location should be one of the two
                // things causing the conflict, not the symbol node
                addError(func, juce::String("Ambiguous definitions: " + sym->token.value));
                break;
            }
            else if (func != nullptr) {
                sym->resolution.innerFunction = func;
                break;
            }
            else if (var != nullptr) {
                sym->resolution.localVariable = var;
                break;
            }
        }
    }
    
    if (!sym->isResolved()) {
        // recurse up
        MslNode* parent = node->parent;
        if (parent != nullptr)
          resolveLocal(sym, parent);
    }
}

/**
 * Attempt to resolve the symbol to the argument of a conatining function definition.
 * This can get quite complicated if there are keyword arguments and special
 * keyword symbols.  Might be nice to compile this and leave it on the MslFunction
 */
void MslLinker::resolveFunctionArgument(MslSymbol* sym, MslFunctionNode* def)
{
    MslBlock* decl = def->getDeclaration();
    if (decl != nullptr) {
        for (auto arg : decl->children) {
            MslSymbol* argsym = nullptr;
            if (arg->isSymbol()) {
                argsym = static_cast<MslSymbol*>(arg);
            }
            else if (arg->isAssignment()) {
                // it's a default argument, LHS must be a symbol
                if (arg->children.size() > 0) {
                    MslNode* first = arg->children[0];
                    if (first->isSymbol())
                      argsym = static_cast<MslSymbol*>(first);
                }
            }

            if (argsym != nullptr && argsym->token.value == sym->token.value) {
                // okay it resolves to an argument in the declaration, these
                // are just raw symbols, they don't have MslVariableNodes around them
                // just remember that fact that it did resolve, and look for a binding
                // at runtime
                sym->resolution.functionArgument = true;
                break;
            }
        }
    }
}

/**
 * Resolving exports is relatively easy as name collisions have already been
 * dealt with.
 */
void MslLinker::resolveEnvironment(MslSymbol* sym)
{
    sym->resolution.linkage = environment->find(sym->token.value);
}

/**
 * If we got here, then there was nothing within the environment that matched,
 * ask the application.
 *
 * Once this has been resolved the external is interned in a table in the
 * environment for future lookups.  Might be overkill but we don't control
 * the external resolution process and it has to resolve to the same
 * thing every time.
 */
void MslLinker::resolveExternal(MslSymbol* sym)
{
    juce::String refname = sym->token.value;
    
    MslExternal* external = environment->getExternal(refname);
    if (external == nullptr) {

        // haven't seen this one before, ask the container
        MslExternal retval;
        if (context->mslResolve(refname, &retval)) {
            // make one we can intern
            external = new MslExternal(retval);
            external->name = refname;
            environment->intern(external);
        }
        else {
            // todo: so we don't keep going back here, could intern
            // a special "null" external?
        }
    }

    sym->resolution.external = external;
}

//////////////////////////////////////////////////////////////////////
//
// Call Arguments
//
//////////////////////////////////////////////////////////////////////

/**
 * After resolving to a function, compile the argument block
 * for the call and hang it on the symbol.
 *
 * This is where all the magic happens for keyword arguments,
 * default arguments, optional arguments, positionals, etc.
 * The complexity here is why it can't be done at runtime, though
 * that could change with some effort.
 */
void MslLinker::compileArguments(MslSymbol* sym)
{
    // first determine the signature of the function we're calling
    // this could be simplified into a model that isn't a raw node
    // block like MslFunction, but it only happens here so it isn't important
    MslBlock* signature = nullptr;
    
    if (sym->resolution.innerFunction != nullptr) {
        // someday these might be MslFunctions too
        signature = sym->resolution.innerFunction->getDeclaration();
    }
    else if (sym->resolution.localFunction != nullptr) {
        signature = sym->resolution.localFunction->getDeclaration();
    }
    else if (sym->resolution.linkage != nullptr) {
        if (sym->resolution.linkage->function != nullptr)
          // whew, how many levels does it take to get to a signature
          signature = sym->resolution.linkage->function->getDeclaration();
    }
    else if (sym->resolution.external != nullptr) {
        // todo: eventually externals need signatures too
    }
        
    compileArguments(sym, signature);
}

/**
 * Construct the argument block for a function call.
 * This block will be pushed on the stack prior to the call and
 * the results will be passed to the function.
 *
 * The basic algorithm is:
 *
 * for each argument defined in the function signature
 *    is there an assignment for it in the call
 *        use the call assignment
 *    else is there an available non-assignment arg in the call
 *        use the call arg
 *    else is there an assignment in the function (a default)
 *        use the function
 *
 */
void MslLinker::compileArguments(MslSymbol* sym, MslBlock* signature)
{
    bool error = false;
    
    // this isn't parsed so it won't start out with a parent pointer,
    // make sure it always has one
    sym->arguments.parent = sym;
    sym->arguments.clear();

    // copy the call args and whittle away at them
    // the child list of a symbol is expected to be a single () block
    juce::Array<MslNode*> callargs;
    if (sym->children.size() > 0) {
        MslNode* first = sym->children[0];
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
                    // errors here are problematic because the error lies in
                    // the signature which could be outside this compilation unit
                    // you would really like to know where that is, not the symbol
                    // that is referencing it, but while we have that node it isn't
                    // in the same file for syntax highlighting
                    addError(sym, juce::String("Invalid keyword in function signature: ") +
                             key->name);
                    error = true;
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
                    Trace(2, "MslLinker: Unexpected situation 42");
                }

                if (argsym == nullptr) {
                    // not a symbol or well-formed assignment in the delcaration
                    addError(sym, juce::String("Unable to determine function argument name"));
                    error = true;
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
                    sym->arguments.add(argref);

                    // is there a keyword argument for this in the call?
                    MslAssignment* callass = findCallKeyword(callargs, name);
                    if (callass != nullptr) {
                        if (callass->children.size() > 1)
                          argref->node = callass->children[1];
                        else {
                            // no rhs on the assignment, something like this
                            // foo(... x =, ...) or foo(x=)
                            // this is most likely an error, but it could also be used
                            // to indiciate overriding a default from the function
                            // declaration with null
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
                            addError(sym, juce::String("Missing function argument: ") + name);
                            error = true;
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
            
            if (error)
              break;
        }
    }

    // anything left over are not in the function signature
    // go ahead and pass them, they may be referenced with positional
    // references $1 in the function body, or may just represent temporary
    // symbol bindings
    if (!error) {
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
                sym->arguments.add(argref);
            }
        }
    }

    // if errors were encountered, don't leave a partially constructured
    // argument list behind
    if (error)
      sym->arguments.clear();
}

/**
 * Find the call argument with a matching assignment name and remove
 * it from the consideration list.
 */
MslAssignment* MslLinker::findCallKeyword(juce::Array<MslNode*>& callargs, juce::String name)
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
MslNode* MslLinker::findCallPositional(juce::Array<MslNode*>& callargs)
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

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
