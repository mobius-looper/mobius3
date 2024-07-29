/**
 * An External represents a linkage between the MSL environment and something in the
 * containing application that can behave in one of two ways:
 *
 *      Function: something that performs an action and returns a value
 *      Variable: something that has a value and may be assigned a value
 *
 * It is conceptually similar to the Symbol in Mobius 3, but abstracts that implementation
 * and allows references to things in the Mobius engine that are not Symbols, notably
 * Variables in the old scripting language.
 *
 * This is still a bit hacky and expected to evolve.
 *
 * Some design goals:
 *
 *     - the ability to associate MslSymbol nodes with implementations that can
 *       behave as functions or variables
 *
 *     - the linkage memory model minimizes the amount of work the containing application
 *       needs to do to integrate existing similar concepts
 *
 * The first obvious approach would be to model this as functions in the MslContext
 * interface like this:
 *
 *      MslValue* value = context->getValue(name)
 *      MslValue* value = context->callFunction(name, arguments...)
 *
 * The application now has a name-to-implementation mapping problem.  Some form of switch
 * or hash table needs to be built to convert the MSL symbol name into something within the
 * application that implements that name.  This table can be large, and lookups by name
 * can be needlessly slow.
 *
 * The next approach would be a Handler/Adapter where the application resolves a name into
 * an object that can provide the implementation:
 *
 *       MslThing* thing = context->getHandlerForName(name);
 *       MslValue* value = thing->getValue()
 *
 * This has the advantage that the name-to-impementation mapping can be done once, cached
 * on the MslSymbol node, and reused every time this symbol is evaluated.  The problem here
 * is that the application needs to create many new MslThing implementation objects for each
 * name, or modify existing "thing" objects to have the MslThing interface.
 *
 * Lambdas can simplify this by eliminating the need for an intermediate object but there
 * is still the need to associate many different lambda functions with MSL symbol names.
 *
 * The approach taken here is kind of in between and fits better with applications that
 * already have some form of complex object model to represent variables and functions.
 * The MslExternal provides the mapping between an MSL symbol name and an arbitrary set of
 * opaque pointers that the application can set to reference the corresponding implementation
 * objects.  Since MslExternals are managed within the MSL environment, the application
 * doesn't need any new mapping structures or intermediate object classes.  It only needs one
 * adapter function to convert the information left in an MslExternal into the corresponding
 * calls within the application.  I'm sure there is a name for this...I don't know maybe
 * "opaque closure" or something.
 *
 * The interface now looks like this:
 *
 *           // put the information necessary to resolve this symbol into the external
 *           // return false if it could not be resolved
 *           bool success = context->resolveSymbol(name, external)
 *
 *           // hang this external on an MslSymbol for later reuse
 * 
 *           // ask the application to do what is necessary with this external and compute a value
 *           MslValue* value = context->getExternalValue(external);
 *
 * The application is not allowed to put anything into the External that is dynamically allocated
 * and would need to be freed.  The application is not allowed to retain a reference to the
 * External and expect that it have a defined lifespan.  The application cannot change
 * the resolution of an External once it has been made.  Once an external name has been resolved
 * it will be used forever.
 *
 * It's not pretty but it does the job, and doesn't require much effort to integrate with
 * the annoyingly large old Mobius "Variables" model and with the new Mobius "Symbols" model.
 *
 */

#pragma once

class MslExternal
{
  public:
    MslExternal() {}
    ~MslExternal() {}

    // the name of this symbol, in case the application still needs to make decisions
    // based on the name
    juce::String name;

    // the behaviora of this external: Function or Variable
    bool isFunction = false;

    // the primary pointer to the application object that implements this symbol
    void *object = nullptr;

    // a value indiciating the type of this pointer if there can be more than one
    // class of object being referenced
    int type = 0;

    // todo: a small number of random arguments that may be necessary
    // to evaluate this external symbol?

};

    
