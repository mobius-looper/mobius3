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

#include "OldBinding.h"

//////////////////////////////////////////////////////////////////////
//
// Binding
//
//////////////////////////////////////////////////////////////////////

OldBinding::OldBinding()
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

OldBinding::~OldBinding()
{
	OldBinding *el, *next;

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

OldBinding::OldBinding(OldBinding* src)
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

void OldBinding::setNext(OldBinding* c)
{
	mNext = c;
}

OldBinding* OldBinding::getNext()
{
	return mNext;
}

void OldBinding::setSymbolName(const char *name) 
{
	delete mSymbolName;
	mSymbolName = CopyString(name);
}

const char* OldBinding::getSymbolName()
{
	return mSymbolName;
}

void OldBinding::setSource(const char *name) 
{
	delete mSource;
	mSource = CopyString(name);
}

const char* OldBinding::getSource()
{
	return mSource;
}

void OldBinding::setArguments(const char* args) 
{
	delete mArguments;
	mArguments = CopyString(args);
}

const char* OldBinding::getArguments() 
{
	return mArguments;
}

void OldBinding::setScope(const char* s)
{
    delete mScope;
    mScope = CopyString(s);
}

const char* OldBinding::getScope() 
{
    return mScope;
}

//
// Utilities
//

bool OldBinding::isMidi()
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
bool OldBinding::isValid()
{
	bool valid = false;

    if (mSymbolName == nullptr) {
        Trace(1, "OldBinding: Filtering binding with no name\n");
    }
    else if (trigger == nullptr) {
        Trace(1, "OldBinding: Filtering binding with no trigger: %s\n", mSymbolName);
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

OldBindingSet::OldBindingSet()
{
	mBindings = nullptr;
}

OldBindingSet::~OldBindingSet()
{
	delete mBindings;
}

OldBindingSet::OldBindingSet(OldBindingSet* src)
{
    mBindings = nullptr;

    setName(src->getName());

    OldBinding* last = nullptr;
    OldBinding* srcBinding = src->getBindings();
    while (srcBinding != nullptr) {
        OldBinding* copy = new OldBinding(srcBinding);
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

Structure* OldBindingSet::clone()
{
    return new OldBindingSet(this);
}

OldBinding* OldBindingSet::getBindings()
{
	return mBindings;
}

OldBinding* OldBindingSet::stealBindings()
{
    OldBinding* list = mBindings;
    mBindings = nullptr;
    return list;
}

/**
 * !! Does not delete the current bindings
 * this is inconsistent
 */
void OldBindingSet::setBindings(OldBinding* b)
{
	mBindings = b;
}

void OldBindingSet::addBinding(OldBinding* b) 
{
    if (b != nullptr) {
        // keep them ordered
        OldBinding *prev;
        for (prev = mBindings ; prev != nullptr && prev->getNext() != nullptr ; 
             prev = prev->getNext());
        if (prev == nullptr)
          mBindings = b;
        else
          prev->setNext(b);
    }
}

void OldBindingSet::removeBinding(OldBinding* b)
{
    if (b != nullptr) {
        OldBinding *prev = nullptr;
        OldBinding* el = mBindings;
    
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
            Trace(1, "OldBindingSet::removeBinding binding not found!\n");
        }

        b->setNext(nullptr);
    }
}

/**
 * Added for UpgradePanel
 * See if a Binding already exists before adding another one
 */
OldBinding* OldBindingSet::findBinding(OldBinding* src)
{
    OldBinding* found = nullptr;
    
    if (src != nullptr) {
        for (OldBinding* b = mBindings ; b != nullptr ; b = b->getNext()) {
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
