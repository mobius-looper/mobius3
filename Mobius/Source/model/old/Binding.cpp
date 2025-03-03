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

#include "../../util/Util.h"
#include "../../util/Trace.h"

#include "Trigger.h"
#include "ActionType.h"
#include "Structure.h"
#include "../Scope.h"

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
    release = false;
    triggerValue =  0;
    midiChannel = 0;
    
	mNext = nullptr;
    mSymbolName = nullptr;
    mArguments = nullptr;
    mScope = nullptr;
    mSource = nullptr;
}

Binding::~Binding()
{
	Binding *el, *next;

	delete mSymbolName;
    delete mArguments;
    delete mScope;
    delete mSource;
    
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
    mSource = nullptr;
    
    trigger = src->trigger;
    triggerMode = src->triggerMode;
    release = src->release;
    triggerValue = src->triggerValue;
    midiChannel = src->midiChannel;

    setSymbolName(src->getSymbolName());
    setArguments(src->getArguments());
    setScope(src->getScope());

    // temporary transient fields for DisplayButton
    id = src->id;
    displayName = src->displayName;
    
    // mSource is a transient information field for
    // the InfoPanel and does not need to be copied
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

void Binding::setSource(const char *name) 
{
	delete mSource;
	mSource = CopyString(name);
}

const char* Binding::getSource()
{
	return mSource;
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
}

const char* Binding::getScope() 
{
    return mScope;
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

    setName(src->getName());

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

    // assume that if you're starting with an overlay, the new one is also one
    mOverlay = src->isOverlay();

    // hmm, when cloning to create a new one, activation shouldn't be assumed
    // but when cloning to edit an existing one, activation is expected
    // to be retained
    // update: activation is no longer in here
    //mActive = src->isActive();

    
}

Structure* BindingSet::clone()
{
    return new BindingSet(this);
}

Binding* BindingSet::getBindings()
{
	return mBindings;
}

Binding* BindingSet::stealBindings()
{
    Binding* list = mBindings;
    mBindings = nullptr;
    return list;
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
                b->release == src->release &&
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
