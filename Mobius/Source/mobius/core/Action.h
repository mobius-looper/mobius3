/*
 * An internal core model for UIAction that includes additional
 * engine specific state.  One of these will be created when
 * receiving a UIAction from the outside, and can also be created
 * on the fly within the engine, especially in scripts.
 *
 * Actions can live for an indefinite period after they have requested
 * if they are scheduled and associated with events or scripts.
 *
 * They are allocated from a pool since they need to be created randomly
 * within the engine.
 *
 * See UIAction.h for more on the general structure of the action members.
 * The terminology changed during the Juce port so some of the legacy
 * access methods are maintained until we can evolve the old code that
 * uses actions.
 *
 * One important difference with UIAction is the notion of "results"
 *
 *   Results
 *
 *    When an action is being processed, several result properties may
 *    be set to let the caller how it was processed.  This is relevant
 *    only for the script interpreter.
 *
 * Old comments:
 * 
 * Actions may be created from scratch at runtime but it is more common
 * to create them once and "register" them so that they may be reused.
 * Using registered actions avoids the overhead of searching for the
 * system objects that define the target, Functions, Parameters, Bindables
 * etc.  Instead, when the action is registered, the target is resolved
 * and saved in the Action.
 *
 * Before you execute a registered action you must make a copy of it.
 * Actions submitted to Mobius are assumed to be autonomous objects
 * that will become owned by Mobius and deleted when the action is complete.
 *
 * New Notes:
 *
 * Mobius has the notion of interning or registering an action for reuse by
 * the old UI.  Retain this for awhile but I think we can get rid of it.
 * 
 */

#pragma once

#include "../../model/SystemConstant.h"
#include "../../model/UIAction.h"
#include "../../model/ActionType.h"

// sigh, need this until we can figure out what to do with ExValue
#include "Expr.h"

/**
 * Maximum length of a target name
 *
 * For most actions, this is relevant only until the target reference
 * is resolved to a pointer to a system constant object.
 *
 * Configurable objects like Presets and Scripts the name identifies
 * the non-constant object and an "ordinal" may be calculated for faster
 * lookup.
 */
#define MAX_TARGET_NAME 128

/**
 * Maximum length of a string argument in an Action.
 * There are four of these so make them relatively short.
 *
 * UPDATE: In theory Function aruments could be arbitrary.  The only one
 * that comes to mind is RunScript which will have the name of the
 * Script to run.
 *
 * todo: think about the difference between targetName and targetArg for
 * configurable objects.  I think in all cases now we have a Function
 * to represent the object (SelectPreset, RunScript) and the argument
 * can be the object name.  In those cases targetName would be "SelectPreset".
 * Currently targetName is the object name.
 */
#define MAX_ARG_LENGTH 128

/**
 * A random string we used to call "name".
 * I think this is only used in OSC bindings, might be the path.
 * Figure out what this is and give it a better name.
 */
#define MAX_EXTENSION 1024

/**
 * Maximum length of an internal buffer used to format a readable
 * description of the action for debugging.
 */
#define MAX_DESCRIPTION 1024

//////////////////////////////////////////////////////////////////////
//
// TargetPointer
//
//////////////////////////////////////////////////////////////////////

/**
 * This corresponds to UIAction::ActionImplementation
 * It has the same meaning except that it points to the internal
 * model for Parameter and Function.
 * 
 * Direct references to what used to be called "Bindable"s and what
 * are now Structures (Preset, Setup, BindingSet) have been removed
 * and those are now referenced with an ordinal number.  Keeping a cached
 * pointer to those causes lots of complications since unlike Function and
 * Parameter the model for those can change which would invalidate
 * the cache we would keep here.  Since actions on structures is rare
 * and there aren't many of them, we just save the name and look
 * it up in the current model by name when necessary.
 *
 * There are still cache invalidation problems for Scripts which are
 * referenced here as dynamically allocated Functions.  Need to rethink
 * how that works, it's by far easiest just to require that scripts
 * can only be changed in a state of Global Reset.
 */
typedef union {

    void* object;
    class Function* function;
    class Parameter* parameter;
    int ordinal;
    
} TargetPointer;

/****************************************************************************
 *                                                                          *
 *                                   ACTION                                 *
 *                                                                          *
 ****************************************************************************/

/**
 * An object containing information about an action that is to 
 * take place within the engine.
 * 
 * These are created in response to trigger events then passed to Mobius
 * for processing.
 */
class Action {

    // actions are normally associated with a pool
    friend class ActionPool;

  public:

	Action();

    // clone the action, who uses this?
	Action(Action* src);

    // must be a shorthand for a common dynamically created Function action
    Action(class Function* func);

    // destructor should normally be called only by the pool
	~Action();

    // application code normally frees it which returns it to the pool
    void free();

    // pool state
    Action* getNext();
    void setNext(Action* a);

    // called by Mobius::doParameter to convert an argument string
    // from a binding model into the actionOperator and numeric argument
    // variables.  This is optional, the caller could have set those
    // directly if known
    void parseBindingArgs();

    // these are things to unpack MIDI message pieces that are held
    // in the "id" value
    // this shoudl NOT be necessary in core code, all we should care about
    // is that the id be unique for sustain and longPress detection
    // keep around temporarily but revisit this and if we really need them
    // UIAction can provide the utilities
    
    int getMidiStatus();
    void setMidiStatus(int i);

    int getMidiChannel();
    void setMidiChannel(int i);

    int getMidiKey();
    void setMidiKey(int i);

    // pretty sure this is only related to how bindings are processed
    // and does not need to be replicated down here
    bool isSpread();

    // for trace logging, and probably the old UI, get a readable
    // representation of what this action does
    void getDisplayName(char* buffer, int max);
    const char* getDisplayName();
    const char* getTypeDisplayName();
    void getGroupName(char* buf);
    void getFullName(char* buffer, int max);

    //////////////////////////////////////////////////////////////////////
    // Trigger
    //////////////////////////////////////////////////////////////////////

    // new: optional request identifier from the originator
    // used in some cases to send completion notification
    int requestId = 0;

    // things that are direct copies from UIAction
	long triggerId;
    void* triggerOwner;
    
	class Trigger* trigger;
    class TriggerMode* triggerMode;
    int triggerValue;
	int triggerOffset;
	bool down;
	bool longPress;

    // UIAction had this but it shouldn't be necessary in core
    //bool passOscArg;

    //////////////////////////////////////////////////////////////////////
    // ActionType (the artist formerly known as Target)
    //////////////////////////////////////////////////////////////////////
    
    ActionType* type;
    char actionName[MAX_TARGET_NAME];

    // These were inside a ResolvedTarget in old code

    // Corresponds to UIAction::actionImplementation
    TargetPointer implementation;

    //////////////////////////////////////////////////////////////////////
    // Scope
    //////////////////////////////////////////////////////////////////////

    // track number we still want to pass down, it will
    // be resolved to a Track poitner
    int scopeTrack;

    // groups should be handled in the UI, but keep it around
    // until we can remove the group tooling in the engine
    int scopeGroup;

    //////////////////////////////////////////////////////////////////////
    // Time
    //////////////////////////////////////////////////////////////////////

	/**
	 * True if quantization is to be disabled.
	 * Used only when rescheduling quantized functions whose
	 * quantization has been "escaped".
	 */
	bool escapeQuantization;

	/**
	 * True if input latency compensation is disabled.
	 * Used when invoking functions from scripts after we've
	 * entered "system time".
	 */
	bool noLatency;

    /**
     * True if the event should not be subject to synchronization
     * as it normally might.
     */
    bool noSynchronization;

    //////////////////////////////////////////////////////////////////////
    // Arguments
    //////////////////////////////////////////////////////////////////////

    char bindingArgs[MAX_ARG_LENGTH];
    class ActionOperator* actionOperator;
    ExValue arg;
    class ExValueList* scriptArgs;

    //////////////////////////////////////////////////////////////////////
    //
    // Runtime
    //
    // Various transient things maintained while the action is 
    // being processed.
    //
    //////////////////////////////////////////////////////////////////////

	/**
	 * True if we're rescheduling this after a previously scheduled
	 * function event has completed.
	 */
	class Event* rescheduling;

    /**
     * When reschedulign is true, this should have the event that 
     * we just finished that caused the rescheduling.
     */
    class Event* reschedulingReason;

    // can we get by without this?
    class Mobius* mobius;

    /**
     * Transient flag to disable focus lock and groups.  Used only
     * for some error handling in scripts.
     */
    bool noGroup;

    /**
     * Don't trace invocation of this function.  
     * A kludge for Speed shift parameters that convert themselves to 
     * many function invocations.   
     */
    bool noTrace;

    // temporary for debugging trigger timing
    long millisecond;
    double streamTime;

    //////////////////////////////////////////////////////////////////////
    // Legacy Accessors
    //////////////////////////////////////////////////////////////////////

    bool isResolved();
    bool isSustainable();
    
    class ActionType* getTarget() {return type;}
    void* getTargetObject() {return implementation.object;}
    class Function* getFunction();
    int getTargetTrack() {return scopeTrack;}
    int getTargetGroup() {return scopeGroup;}

    // probably used with "interning" still need this?
    bool isTargetEqual(Action* other);

    void setTarget(class ActionType* t);
    void setTarget(ActionType* t, void* object);
    void setFunction(class Function* f);
    void setParameter(class Parameter* p);
    void setTargetTrack(int track);
    void setTargetGroup(int group);

    // kludge for long press, make this cleaner
    void setLongFunction(class Function*f);
    Function* getLongFunction();

    // internal use only, not for the UI
    void setResolvedTrack(class Track* t);
    Track* getResolvedTrack();

    class KernelEvent* getKernelEvent();
    void setKernelEvent(class KernelEvent* e);

    class Event* getEvent();

    void setEvent(class Event* e);
    void changeEvent(class Event* e);
    void detachEvent(class Event* e);
    void detachEvent();

    // name is weird, this seems to be something left over from OSC
    // it is not the same as actionName
    const char* getName();
    void setName(const char* name);

    //////////////////////////////////////////////////////////////////////
    //
    // Protected
    //
    //////////////////////////////////////////////////////////////////////

  protected:

    void setPooled(bool b);
    bool isPooled();
    void setPool(class ActionPool* p);

    //////////////////////////////////////////////////////////////////////
    //
    // Private
    //
    //////////////////////////////////////////////////////////////////////

  private:

	void init();
    void reset();
    void clone(Action* src);
    char* advance(char* start, bool stopAtSpace);

    Action* mNext;
    bool mPooled;

    /**
     * The pool we came from.
     * !! I'm hating the notion that we have to keep a pool pointer
     * around.  Caller should just return it to any pool or delete it.
     */
    class ActionPool* mPool;

	/**
	 * Set as a side effect of Function invocation when a track event
     * is scheduled that represents the end of processing for this function.
	 * There may be play jump child events and other similar things
	 * that happen first.  This is only used if the Function was invoked
     * by a ScriptInterpreter and it wants to wait on the event to finish.
	 */
	class Event* mEvent;

	/**
	 * Set as a side effect of Function invocation when a KernelEvent
     * is scheduled to perform the function outside the kernel.
     * This is only used if the Function was invoked by a ScriptInterpreter
     * and it wants to wait on the event to finish.
     *
     * Old code overloaded this for a second purpose, to pass the
     * completed ThreadEvent down in a new Action and tell the ScriptInterpreters
     * to stop waiting.  We no longer do that, it is only a transient value
     * for use by ScriptInterpreter to set up a wait.
	 */
	class KernelEvent* mKernelEvent;

    /**
     * Set during internal processing to the resolved Track
     * in which this action will run.  Overrides whatever is specified
     * in the target.
     */
    class Track* mResolvedTrack;

    /**
     * Allow the client to specify a name, convenient for
     * OSC debugging.
     */
    char* mName;

    /**
     * Alternate function to have the up transition after
     * a long press.  
     * !! get rid of this, we should either just replace Function
     * at invocation time, or have Function test the flags and figure it out.
     */
    Function* mLongFunction;

};

/****************************************************************************
 *                                                                          *
 *                                ACTION POOL                               *
 *                                                                          *
 ****************************************************************************/

class ActionPool {

  public:

    ActionPool();
    ~ActionPool();

    Action* newAction();
    Action* newAction(Action* src);
    void freeAction(Action* a);

    void dump();

  private:

    Action* allocAction(Action* src);

    Action* mActions;
    int mAllocated;


};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
