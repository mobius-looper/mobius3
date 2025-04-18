/**
 * Structure that is attached to a Symbol associated with a function
 * to describe how it behaves.
 *
 * This might be able to take the place of BehaviorFunction.  If a symbol has one
 * of these it must have function behavior?
 */

#pragma once

#include "Symbol.h"

class FunctionProperties
{
  public:

    FunctionProperties() {}
    ~FunctionProperties() {}

    /**
     * True if this function is relevant only in MIDI tracks.
     */
    bool midiOnly = false;

    /**
     * When true, this is a global function, meaning it is not specific
     * to any one track scope.  Became necessary with MIDI tracks to get things
     * like GlobalReset targeted to both track sets.  Core Function also has a global
     * flag but it is not reliable, and some things that are global for Mobius core
     * are not relevant for MIDI tracks.  This must be set in symbols.xml
     */
    bool global = false;

    /**
     * When true, this function may respond to a sustained action.
     */
    bool sustainable = false;

    /**
     * When true, this function may obey focus lock.
     * Whether it does or not is user configurable and the focus flag will be set.
     *
     * NEW: I think this is backwards, everything shold default to focus lockable and
     * have to be turned off, mayFocus just means it is configurable.
     */
    bool mayFocus = false;

    /**
     * When true, this function will never be used with focus lock.
     * Assumed when global=true, is this really necessary?
     * What are the functions that can't do focusLock but are not global?
     */
    bool noFocus = false;
    
    /**
     * When true, this function may act as a switch confirmation function.
     * Whether it does or not is user configurable and the confirmation flag will be set.
     */
    bool mayConfirm = false;
    
    /**
     * When true, this function may act as a mute mode cancel function.
     * Whether it does or not is user configurable and the muteCancel flag will be set.
     */
    bool mayCancelMute = false;

    /**
     * When true, this function may response to QuantizeMode.
     * This is mostly to control the quantize enable UI for MIDI tracks, audio tracks
     * do not pay attention to this.  The Function::quantized flag is hard coded.
     */
    bool mayQuantize = false;

    /**
     * Handle to a core object that implements this function.
     */
    void* coreFunction = nullptr;

    /**
     * Text describing the arguments supported by this function in the binding panels.
     */
    juce::String argumentHelp;

    /**
     * Text describing what sustaining a trigger bound to this function does.
     */
    juce::String sustainHelp;

    /**
     * Set when this function has long press behavior.
     * Implies sustainable if that is not set.
     * The core Function definition has a pointer to the associated
     * Function a long press becomes, in new code, that's left up
     * to the action handler.
     */
    bool longPressable = false;

    /**
     * Text describing what a long-press of a trigger does.
     */
    juce::String longHelp;

    /**
     * Flag indiciating this should not appear in binding windows
     */
    bool noBinding = false;

    // Options used by the binding editor
    bool hasArguments = false;
    juce::String argumentLabel;
    juce::String argumentValue;
    juce::String argumentNone;

    //
    // User configurable properties
    //

    bool xxxfocus = false;
    bool confirmation = false;
    bool muteCancel = false;
    bool quantized = false;
    
  private:
    
};


