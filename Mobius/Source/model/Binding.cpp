/*
 * Model for associationg triggers, actions, and destinations.
 *
 * I'd like to keep as much awareness of this model out of the engine
 * as possible.  The only exceptions may be these old Trigger types
 *
 *   TriggerScript
 *   TriggerEvent
 *   TriggerThread
 *   TriggerUnknown
 *
 * I forget how these were used, try to get rid of them.
 */

#include "../util/Util.h"
#include "../util/Trace.h"

#include "Trigger.h"
#include "ActionType.h"
#include "Structure.h"

#include "Binding.h"

//////////////////////////////////////////////////////////////////////
//
// Binding
//
//////////////////////////////////////////////////////////////////////

Binding::Binding()
{
    trigger = nullptr;
    triggerMode = nullptr;
    triggerValue =  0;
    midiChannel = 0;
    trackNumber = 0;
    groupOrdinal = 0;
    
	mNext = nullptr;
    mSymbolName = nullptr;
    mArguments = nullptr;
    mScope = nullptr;
}

Binding::~Binding()
{
	Binding *el, *next;

	delete mSymbolName;
    delete mArguments;
    delete mScope;

	for (el = mNext ; el != nullptr ; el = next) {
		next = el->getNext();
		el->setNext(nullptr);
		delete el;
	}
}

Binding::Binding(Binding* src)
{
	mNext = nullptr;
    mSymbolName = nullptr;
    mArguments = nullptr;
    mScope = nullptr;
    
    trigger = src->trigger;
    triggerMode = src->triggerMode;
    triggerValue = src->triggerValue;
    midiChannel = src->midiChannel;

    setSymbolName(src->getSymbolName());
    setArguments(src->getArguments());
    setScope(src->getScope());

    // trackNumber, groupOrdinal set as a side
    // effect of setScope if we can, if not
    // should we trust the src?

    // need this for the BindingTable
    id = src->id;
}

void Binding::setNext(Binding* c)
{
	mNext = c;
}

Binding* Binding::getNext()
{
	return mNext;
}

void Binding::setSymbolName(const char *name) 
{
	delete mSymbolName;
	mSymbolName = CopyString(name);
}

const char* Binding::getSymbolName()
{
	return mSymbolName;
}

void Binding::setArguments(const char* args) 
{
	delete mArguments;
	mArguments = CopyString(args);
}

const char* Binding::getArguments() 
{
	return mArguments;
}

void Binding::setScope(const char* s)
{
    delete mScope;
    mScope = CopyString(s);
    parseScope();
}

const char* Binding::getScope() 
{
    return mScope;
}

/**
 * Parse a scope into track an group numbers.
 * Tracks are expected to be identified with integers starting
 * from 1.  Groups are identified with upper case letters A-Z.
 */
void Binding::parseScope()
{
    trackNumber = 0;
    groupOrdinal = 0;

    if (mScope != nullptr) {
        size_t len = strlen(mScope);
        if (len > 1) {
            // must be a number 
            trackNumber = atoi(mScope);
        }
        else if (len == 1) {
            char ch = mScope[0];
            if (ch >= 'A') {
                groupOrdinal = (ch - 'A') + 1;
            }
            else {
                // normally an integer, anything else
                // collapses to zero
                trackNumber = atoi(mScope);
            }
        }
    }
}

/**
 * If you call setTrack or setGroup rather
 * than setScope, it can set the other number
 * as a side effect.
 */
void Binding::setTrack(int t) 
{
    if (t > 0) {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%d", t);
        setScope(buffer);
    }
}

void Binding::setGroup(int t) 
{
    if (t > 0) {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%c", (char)('A' + (t - 1)));
        setScope(buffer);
    }
}

//
// Utilities
//

bool Binding::isMidi()
{
	return (trigger == TriggerNote ||
			trigger == TriggerProgram ||
			trigger == TriggerControl ||
			trigger == TriggerPitch);
}

/**
 * Check to see if this object represents a valid binding.
 * Used during serialization to filter partially constructed bindings
 * that were created by the dialog.
 */
bool Binding::isValid()
{
	bool valid = false;

    if (mSymbolName == nullptr) {
        Trace(1, "Binding: Filtering binding with no name\n");
    }
    else if (trigger == nullptr) {
        Trace(1, "Binding: Filtering binding with no trigger: %s\n", mSymbolName);
    }
    else {
		if (trigger == TriggerKey) {
			// key must have a non-zero value
			valid = (triggerValue > 0);
            if (!valid)
              Trace(1, "Filtering binding with no value %s\n", mSymbolName);
		}
		else if (trigger == TriggerNote ||
				 trigger == TriggerProgram ||
				 trigger == TriggerControl) {

			// hmm, zero is a valid value so no way to detect if
			// they didn't enter anything unless the UI uses negative
			// must have a midi status
			valid = (triggerValue >= 0);
            if (!valid)
              Trace(1, "Filtering binding with no value %s\n", mSymbolName);
		}
        else if (trigger == TriggerPitch) {
            // doesn't need a value
            valid = true;
        }
		else if (trigger == TriggerHost) {
			valid = true;
		}
        else if (trigger == TriggerOsc) {
            // anythign?
            valid = true;
        }
        else if (trigger == TriggerUI) {
            valid = true;
        }
		else {
			// not sure about mouse, wheel yet
		}
	}

	return valid;
}

//////////////////////////////////////////////////////////////////////
//
// BindingSet
//
//////////////////////////////////////////////////////////////////////

BindingSet::BindingSet()
{
	mBindings = nullptr;
}

BindingSet::~BindingSet()
{
	delete mBindings;
}

BindingSet::BindingSet(BindingSet* src)
{
    mBindings = nullptr;

    Binding* last = nullptr;
    Binding* srcBinding = src->getBindings();
    while (srcBinding != nullptr) {
        Binding* copy = new Binding(srcBinding);
        if (last == nullptr)
          mBindings = copy;
        else
          last->setNext(copy);
        last = copy;
        srcBinding = srcBinding->getNext();
    }
}

Structure* BindingSet::clone()
{
    return new BindingSet(this);
}

Binding* BindingSet::getBindings()
{
	return mBindings;
}

/**
 * !! Does not delete the current bindings
 * this is inconsistent
 */
void BindingSet::setBindings(Binding* b)
{
	mBindings = b;
}

void BindingSet::addBinding(Binding* b) 
{
    if (b != nullptr) {
        // keep them ordered
        Binding *prev;
        for (prev = mBindings ; prev != nullptr && prev->getNext() != nullptr ; 
             prev = prev->getNext());
        if (prev == nullptr)
          mBindings = b;
        else
          prev->setNext(b);
    }
}

void BindingSet::removeBinding(Binding* b)
{
    if (b != nullptr) {
        Binding *prev = nullptr;
        Binding* el = mBindings;
    
        for ( ; el != nullptr && el != b ; el = el->getNext())
          prev = el;

        if (el == b) {
            if (prev == nullptr)
              mBindings = b->getNext();
            else
              prev->setNext(b->getNext());
        }
        else {
            // not on the list, should we still nullptr out the next pointer?
            Trace(1, "BindingSet::removeBinding binding not found!\n");
        }

        b->setNext(nullptr);
    }
}

/**
 * Added for UpgradePanel
 * See if a Binding already exists before adding another one
 */
Binding* BindingSet::findBinding(Binding* src)
{
    Binding* found = nullptr;
    
    if (src != nullptr) {
        for (Binding* b = mBindings ; b != nullptr ; b = b->getNext()) {
            // ignoring triggerMode
            if (b->trigger == src->trigger &&
                b->triggerValue == src->triggerValue &&
                b->midiChannel == src->midiChannel &&
                StringEqual(b->getSymbolName(), src->getSymbolName()) &&
                StringEqual(b->getArguments(), src->getArguments()) &&
                StringEqual(b->getScope(), src->getScope())) {
                found = b;
                break;
            }
        }
    }
    return found;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
