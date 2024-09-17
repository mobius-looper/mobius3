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
 */

#pragma once

class ScriptProperties
{
  public:

    ScriptProperties();
    ~ScriptProperties();

    /**
     * True if this script uses sustailable features
     * Indiciates to Binderator that both down/up actions can be sent
     */
    bool sustainable = false;

    /**
     * True if this script can act as a continuous control.
     * Indiciates to Binderator that this won't behave like a Function
     * with on/off sustain behavior.
     */
    bool continuous = false;

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
     * todo: woah, this is kludgey, revisit this
     */
    juce::StringArray testProcs;

    /**
     * Handle to the internal object that implements this script.
     */
    void* coreScript = nullptr;

    /**
     * Handle to the internal object that implements a proc
     * todo: should be obsolete with MslLinkage
     */
    void* proc = nullptr;

    /**
     * For new MSL scripts, a pointer into the MslEnvironment for the
     * script or proc to be called.
     */
    class MslLinkage *mslLinkage = nullptr;

    
};

