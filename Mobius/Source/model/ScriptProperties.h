/**
 * Structure that is attached to a Symbol associated with
 * a Script.  Defines various properties of the script that can
 * only be known after it is parsed.
 *
 * todo: work on display names for scripts, and other objects in general.
 *
 * Symbol has a display name but it is not well defined what it is.
 * Scripts need a unique symbol name which is by default the leaf file name minus
 * the .mos extension.  You can specify an alternate display name in the script
 * source with the !name directive.  MobiusShell uses this alternate name for
 * the symbol.  That's probably enough but might want to allow the display name
 * to just be used for action buttons and change it without disrupting bindings.
 *
 * Similar issues for Samples.
 *
 * Functions and Parameters have display names too, and these have been left on
 * the Symbol.displayName.
 *
 * Reallly, FunctionDefinition and UIParameter serve
 * the same purpose, to provide a set of behavioral properties for something
 * defined in core code without exposing internall classes that implement them.
 * The UI uses those properties to interact with the core code: creating
 * bindings, editing saved values, building actions.
 *
 * This feels like a good thing to start generalizing.  Scripts will start becomming
 * more like functions with sustainability options and argument lists.
 *
 * Also like this as a way for the core to publish behavior.  If we did that for
 * Functions we wouldn't need FunctionDefinition with static constant objects at all
 * in the model.  Just let Mobius publish symbols for each internal Function and hang
 * a FunctionProperties on the symbol that describes what it does.
 *
 * Parameter could be done much the same way except that we still need a class to
 * implement the get/set code for the configuration editor.  
 *
 * Ownership of properties objects is annoying.
 * Symbol could own them which is nice since we have to dispose of the Symbols anyway.
 * The thing that installed them (MobiusShell/Supervisor) can also have an OwnedArray
 * of them, that's what DisplayParameter is doing.  But you have to duplicate this
 * OwnedArray in several places.
 *
 * For Scripts/Symbols start letting Symbol manage it and see how this shakes out.
 */

#pragma once

class ScriptProperties
{
  public:

    ScriptProperties();
    ~ScriptProperties();

    /**
     * True if this script should be automatically given an action
     * button in the main display.  This was just a test hack I liked
     * but may be of general interest.
     */
    bool button = false;

    /**
     * True if this script is a test script that will
     * be managed in a special way by the Test Panel UI.
     * This should also keep these off the binding lists since
     * you won't want to bind triggers to them.
     */
    bool test = false;

    /**
     * For scripts with test=true, the names of the test Procs
     * in the script for tests that may be run independently.
     */
    juce::StringArray testProcs;

    /**
     * Handle to the internal object that implements this script.
     */
    void* coreScript = nullptr;

    
};

