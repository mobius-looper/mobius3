/**
 * A model for actions that can be sent through the application to cause
 * something to happen.
 *
 * Most actions will be processed by the Mobius engine, but a few will can
 * be targeted for the UI.
 *
 * An action always has a Symbol which defines what the action will do.  The
 * most common are execute a function, set a parameter, and start a script.
 *
 * The action may have optional flags that influence how it is to be performed.
 * This includes options related to the "sustainable" behavior of the trigger
 * that caused the action, and various runtime options maintained as the
 * action passes through it's lifetime.
 *
 * An action is usually created to handle a Binding to process a Trigger event
 * from an external device such a a MIDI controller.  A few parts of the system
 * create and execute actions as a side effect of something else.
 *
 * Sustain
 * 
 * A sustained action is one that has a start a start and an end.
 * An action that is sustained for a certain period of time is considered
 * a "long" action and may trigger additional behavior.
 * The most common examples are MIDI notes and computer keyboards which
 * send an action when a button is pressed, and another when it is released.
 * What exactly triggered an action is not relevant to the processing of
 * the action, only that it will or will not have sustain behavior.
 *
 * The start of a sustained action is indiciating by sending a UIAction
 * with the "sustain" flag set.  The sender of the action must then send
 * a second action with the "sustainEnd" flag set when the trigger is released.
 * Both actions must have a "sustainId" which is unique identifier for the trigger
 * that caused this action, examples include the MIDI note number or keyboard
 * scan code.  The system must not make any assumptions about what this id
 * means, only that it is unique among action triggers and can be used for
 * tracking the start/end transitions.
 *
 * TODO: !! Currently this is using MIDI note numbers, keyboard scan codes
 * and ActionButton numbers for the sustainId.  What got lost in the UIAction
 * redesign is the "trigger type" such as TriggerMidi, TriggerKey, etc.
 * This was important in theory becausee one MIDI byte can have the same integer
 * value as a key scan code or a button index.  It was the combination of the
 * TriggerType and the triggerValue that was unique.  For MIDI triggers we're
 * combining the status byte (action and channel) and the first data byte
 * (note number, controller number) to produce a relatively large integer.
 * Key codes tend to be small integers though with Juce I'm seeing some high
 * bits set for some keys.  ActionButton ids are small numbers from 1 to the
 * number of buttons being displayed.  The likelyhood of there being overlap
 * with MIDI is low, higher for ActionButton and Key.  But it is possible.
 * I don't want to reintroduce TriggerType so if we have problems add some
 * high bits to give each of those a unique number space.
 * 
 */

#pragma once

#include "ObjectPool.h"

/**
 * Sustainable triggers need to generate a unique "sustain id" that does
 * not conflict with any other trigger types.  The sustain id must be
 * greater than zero in the engine.  Code that builds actions should
 * use these id offsets for the various trigger types.  Could have a more
 * flexible registration of id bases, but this gets the job done.
 *
 * todo: Forming ids with arbitrary numbers like key scan codes is
 * awkward due to unpredictable ranges.  Would be better if Binderator
 * used a simpler numbering, like just the index of the Binding in the
 * BindingSet
 */

// Id base for UI buttons, we normally won't have very many of these
// The id is formed from this base plus the index of the button in
// the ActionButtons list
const int UIActionSustainBaseButton = 1;

// Id base for MIDI notes and controllers
// The id is formed from this base plus the MIDI note or CC number
// there needs to be at least 128 between them
const int UIActionSustainBaseNote = 100;
const int UIActionSustainBaseControl = 300;

// Id base for host parameters
// the upper bound on these is unclear, but probably 128, it
// would depend on the host and the amount of time the user wants to
// spend configuring them
const int UIActionSustainBaseHost = 500;

// Id base for keyboard keys
// Id is formed from this base plus the key code
// key codes are usually relatively small ascii codes, but can be large
// for some function keys.  Keep these at the end since the range
// is unpredictable
const int UIActionSustainBaseKey = 1000;

/**
 * Maximum length of a string argument in an Action.
 * This receives a copy of the argument string from a Binding.
 */
const int UIActionArgMax = 128;

/**
 * Maximum length of a scope (group) name
 */
const int UIActionScopeMax = 32;

class UIAction : public PooledObject {

  public:

	UIAction();
    UIAction(UIAction* src);
	~UIAction();

    void poolInit() override;
    void init(class Binding* b);
    void copy(UIAction* src);
    void reset();

    /**
     * Optional numeric action identifier.  When non-zero, the engine
     * will, under some conditions, send notification back to the MobiusListener
     * when the action completes.
     *
     * This only works for script actions, and is only intended for use by
     * the TestDriver.  Think about generalizing this into something that
     * might be generally useful.  The engine does not care what the value of the
     * identifier is.  It will be sent back in the MobiusListener::mobiusActionComplete
     * callback method.
     */
    int requestId = 0;

    //////////////////////////////////////////////////////////////////////
    // Target/Symbol
    //////////////////////////////////////////////////////////////////////

    /**
     * Symbol representing the action to perform.
     * In various code generations this was the same as the "target"
     * or "operation" of an action.  Symbols represent the things an
     * action can do such set set a parameter, execute a function,
     * start a script, or activate a preset.
     */
    class Symbol* symbol = nullptr;

    //////////////////////////////////////////////////////////////////////
    // Arguments
    //////////////////////////////////////////////////////////////////////

    /**
     * Optional integer value of this action.
     * The meaning of these will depend on the target Symbol.
     * For Parameter actions this would be the ordinal value of the
     * parameter to set.  For Function actions, this is usually left zero
     * but many functions accept a numeric argument to qualify what the
     * function does.  For example the LoopSelect function will use
     * the action value as the number of the loop to select.
     *
     * There can be more compelx action arguments, but most only need a
     * single integer.
     */
    int value = 0;

    /**
     * A copy of the argument string from the Binding.
     * The need for this is unclear, normally Binding arguments would be
     * processed before the action is created and used to determine the
     * action value.  Scripts could use this to pass complex information.
     * Unused at the moment.
     */
    char arguments[UIActionArgMax];
        
    //////////////////////////////////////////////////////////////////////
    //
    // Scope
    //
    // This limits where the action can be performed.  Typically actions
    // are sent to the currently active track or to all tracks with
    // "focus".  Setting a scope can override this.  Values are expected to
    // be a two digit track number or the name of a GroupDefinition.
    //
    //////////////////////////////////////////////////////////////////////

    const char* getScope();
    void setScope(const char* s);
    bool hasScope();
    
    void setScopeTrack(int i);
    int getScopeTrack();
    
    //////////////////////////////////////////////////////////////////////
    // Sustain
    //////////////////////////////////////////////////////////////////////

    /**
     * True if this action will behave as a sustained action.
     */
    bool sustain = false;

    /**
     * Unique id used to correlate the start and end of a sustained action.
     */
    int sustainId = 0;

    /**
     * True if this represents the end of a sustained action.
     */
    bool sustainEnd = false;

    /**
     * True if this action response to release bindings.
     * Kludge because we can't set the sustain flag to true without confusing
     * the engine, but Binderator needs to know that up transitions are allowed.
     * Transient field set only in Binderator.
     */
    bool release = false;

    /**
     * True if this action represents a point in a sustained action's
     * lifetime that is considered a long time to be sustaining an action.
     * This theshold is configurable but is usually around 1 second.
     * This may trigger additional behavior detetmined by the target Symbol.
     * This is usually set only by core code as it tracks sustained actions
     * and will auto-generate additional actions during the sustain period.
     */
    bool longPress = false;

    /**
     * If the long press detector is configured to support it,
     * and the trigger continues to be held, the counter will reset
     * and the long press action may fire more than once.  This
     * is the number of times it has been fired.
     */
    int longPressCount = 0;

    //////////////////////////////////////////////////////////////////////
    // Processing Options
    // These are normally set only by scripts.
    //////////////////////////////////////////////////////////////////////

	/**
	 * True if quantization is to be disabled.
	 * Used only when rescheduling quantized functions whose
	 * quantization has been "escaped".
	 */
	bool noQuantize = false;

	/**
	 * True if input latency compensation is disabled.
	 * Used when invoking functions from scripts after we've
	 * entered "system time".
	 */
	bool noLatency = false;

    /**
     * True if the event should not be subject to synchronization
     * as it normally might.
     */
    bool noSynchronization = false;

    //////////////////////////////////////////////////////////////////////
    // Execution State
    //////////////////////////////////////////////////////////////////////
    
    /**
     * A chain pointer for a few (one?) place that need to queue multiple
     * actions.  In particular MobiusKernel needs to do this when processing
     * incomming actions at the start of each audio block.  std::vector or
     * juce::Array ara problematic because they can dynamically grow and we're not
     * allowed to allocate memory in the audio thread.  Use a good old-fashioned
     * linked list.  Not that unlike other old objects with a chain pointer
     * we do not cascade delete objects on the list when the action is deleted.
     * Since this is also a PooledObject we also have the pool chain that could
     * be used for this, but I like keeping the usage clean.
     */
    UIAction* next = nullptr;

    /**
     * Optional pointer to an object that is considered to be the owner
     * or originator of an action.  This is only set by the engine and
     * UI level code should not make any assumptions about it.
     * In current practice, it will be a pointer to the Script that started
     * the action, and used to resume the script when the action completes.
     */
    void* owner = nullptr;

    /**
     * Optional pointer to an internal Track object once the action
     * begins processing and has been replicated to more than one track
     * based on the action scope.
     */
    void* track = nullptr;

    /**
     * Obscure flag set in Scripts to disable focus/group handling
     * for this action.
     */
    bool noGroup = false;

    //////////////////////////////////////////////////////////////////////
    // Result
    //////////////////////////////////////////////////////////////////////

    /**
     * Kind of a hack for testing MSL scripts that can return values.
     * Needs more thought, any action should be able to have a synchronous value
     * and a way to return some sort of async request id for polling.
     */
    char result[UIActionArgMax];

    /**
     * If the core action scheduled an event, this is a pointer
     * to it.  Necessary for MSL script waits.
     */
    void* coreEvent = nullptr;
    int coreEventFrame = 0;
    
  private:

    /**
     * Symbolic scope name.  Replaces the older scopeTrack and scopeGroup.
     * Force this through the get/set methods for better buffer security
     */
    char scope[UIActionScopeMax];

};

/**
 * Pool for UIActions
 * Doesn't belong here, but it's the first and only one.
 * Move later.
 */
class UIActionPool : public ObjectPool
{
  public:

    UIActionPool();
    virtual ~UIActionPool();

    class UIAction* newAction();

    virtual PooledObject* alloc() override;
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
