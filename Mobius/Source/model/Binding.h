/*
 * Names are being changed again...Operation is now ActionType
 *
 * A model for defining associations between external stimuli like MIDI,
 * or keyboard events, internal responses like calling a function or changing
 * a parameter.
 *
 * See model-final.txt for more complete notes.
 *
 * Logically a binding consnts of three things: trigger, operation, destination
 *
 * Triggers are things that cause something to happen.
 * There are several fundamantal trigger types defined by a set of system constant
 * objects.
 *
 *    TriggerMidi   - a MIDI event
 *    TriggerKey    - a computer keyboard key
 *    TriggerUI     - something originating in the user interface
 *    TriggerHost   - a VST or AU plugin parameter
 *    TriggerOSC    - the Open Sound Control interface
 *    TriggerScript - a Mobius script
 *
 * Each trigger type may store additional information about the trigger in the binding
 * for example the MIDI event type (note, program, control) and event value (key number,
 * program number, controller number).
 *
 * Operations are what you want the trigger to do.  They are defined
 * by these system constant objects.
 *
 *    OpFunction       - call a built-in function
 *    OpScript         - call a user defined function (script)
 *    OpParameter      - set the value of a parameter
 *    OpActivation     - apply a collection of parameters (preset, setup, binding set)
 *
 * Destinations are where you want the operation to go.
 *
 * These are more abstract than triggers and operations but fundamentally think
 * of them as "paths" that identify something within the system
 * that can receive a request to perform an operation.  Some examples:
 *
 *    the active track
 *    the specific track 4
 *    the collection of tracks 1,2,3
 *    a group of gracks named "Background"
 *    loop number 2 in track 3
 *    a graphical item in the user interface
 *
 * There are two broad categories of destinations, those in the user interface and
 * those in the looping engine.  Most destinations are in the engine.  
 *
 * Scopes are a simplified representation of a destination that are represented
 * in the binding model as a string of text.
 * 
 * Because most destinations are in the engine which has a fixed internal structure.
 * We define a set of value conventions for the scope property.
 *
 *    digit - a track number
 *    name  - a track group name
 *    "ui"  - the user interface
 *
 * If a scope is not defined the operation is assumed to be targted at the
 * active track in the engine.
 * Scope specifiers may become more flexible than this in the future.
 */

#pragma once

// temporary for displayName
#include <JuceHeader.h>

#include "Trigger.h"
#include "ActionType.h"
#include "Structure.h"
#include "SystemConstant.h"

//////////////////////////////////////////////////////////////////////
//
// Binding
//
//////////////////////////////////////////////////////////////////////

/**
 * Defines a binding between a trigger, operation, and destination.
 * A list of these is contained in a BindingSet
 *
 * Formerly had OSC support in here but I removed it and we can revisit
 * the design later to keep things clean.
 *
 * OSC bindings were added later, need to revisit the design and consider
 * using "paths" consistently everywhere rather than breaking it down
 * into discrete target/name/scope/args, at least not in the persistent model.
 *
 * Scope started out as a pair of track and group numbers but now
 * it is string whose value is a scope name with these conventions:
 *
 *    null - a global binding (current track, focused tracks, group tracks)
 *    digit - track number
 *    letter - group identifier (A,B,C...)
 *
 * In theory we can add other scopes like "muted", simliar to the
 * track identifiers we have with the "for" statement in scripts.
 * 
 * There is usually a number associated with the trigger.
 *   Note - note number
 *   Program - program number 
 *   Control - control number
 *   Key - key code
 *   Wheel - NA
 *   Mouse - ?
 *   Host - host parameter number
 *
 * MIDI triggers may have an additional channel number.
 *
 * Some operations may have an additional numeric argument.
 *   - only functions like Track ?
 *
 * OSC bindings use targetPath instead of discrete target, name, scope, 
 * and arguments values.  This will be parsed into the discrete values.
 * OSC bindings also have a string value (the source method) rather than
 * a simple integer like MIDI notes.  This is only used when Bindings
 * are inside an OscBindingSet.
 * 
 */
class Binding {
	
  public:
	
	Binding();
	Binding(Binding* src);
	virtual ~Binding();

    // keep these on a linked list for now, convert to vector later
	void setNext(Binding* c);
	Binding* getNext();

    // true if the binding is filled out enough to be useful
	bool isValid();

    // true if this represents a MIDI binding
	bool isMidi();

    // transient field used to hold the name of the BindingSet this
    // binding came from for runtime information
    void setSource(const char* name);
    const char* getSource();

	// trigger

    Trigger *trigger = nullptr;
    TriggerMode* triggerMode = 0;

    // key number, midi note number
    int triggerValue = 0;

    // for TriggerMidi, the MIDI channel
    // todo: eventually want the note/program/control types as seperate
    // values rather than overloading Trigger
    int midiChannel = 0;

    // target
    
    void setSymbolName(const char* s);
	const char* getSymbolName();

	void setArguments(const char* c);
	const char* getArguments();

	// scope
    // this used to be more complicated, parsing the scope string
    // into track and group numbers, not scopes are maintained as strings
    // until it reaches Actionator

    void setScope(const char* s);
    const char* getScope();

    // temporary DisplayButton kludge
    // Action buttons in the UI are sort of like bindings in that
    // they have target, scope, and arguments but don't have a trigger
    // since the button itself is an implicit trigger.  Because of this
    // I've been using some of the same UI machinery used to edit regular
    // Bindings, with the DisplayButton model being converted to a Binding list
    // for editing, then back to a DisplayButton list when saving.
    // DisplayButton has things that Binding doesn't have though, and these
    // differences are going to grow.  Eventually the Button panel needs to be it's
    // own independent editor, but until then we store a few things temporarily
    // on the Binding during editing that won't actually be used at runtime.
    
    // transient id number set during editing to correlate
    // this Binding object with something else, notably a DisplayButton
    int id = 0;

    // transient display name for the DisplayButton
    juce::String displayName;
    
  private:

    void parseScope();

	Binding* mNext = nullptr;
	char* mSymbolName = nullptr;
    char* mArguments = nullptr;
    char* mScope = nullptr;
    char* mSource = nullptr;
    
};

//////////////////////////////////////////////////////////////////////
//
// BindingSet
//
//////////////////////////////////////////////////////////////////////

/**
 * An object managing a named collection of Bindings, with convenience
 * methods for searching them.
 */
class BindingSet : public Structure {

  public:

	BindingSet();
	BindingSet(BindingSet* src);
	~BindingSet();

    // Structure downcast
    BindingSet* getNextBindingSet() {
        return (BindingSet*)getNext();
    }
    
    Structure* clone();

    Binding* getBindings();
	void setBindings(Binding* b);
    Binding* stealBindings();

	void addBinding(Binding* c);
	void removeBinding(Binding* c);

    Binding* findBinding(Binding* b);

    void setActive(bool b) {
        mActive = b;
    }

    bool isActive() {
        return mActive;
    }
  
    void setMerge(bool b) {
        mMerge = b;
    }
    
    bool isMerge() {
        return mMerge;
    }

  private:

	Binding* mBindings;
    bool mActive = false;
    bool mMerge = false;
	
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
