// UPDATE: None of these should be necessary, they are almost never
// set by scripts other than old test scripts.

/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Static object definitions for global parameters.
 * 
 * These are accessible from scripts though we most cannot be bound.
 *
 * Like SetupParmeters, there is no private copy of the MobiusConfig that
 * gets modified, we will directly modify the real MobiusConfig so the
 * change may persist.  
 *
 * If the parameter is cached somewhere, we handle the propgation to
 * whatever internal object is caching it.   Where we can we modify
 * both the "external" MobiusConfig and the "interrupt" MobiusConfig.
 *
 * Few of these are flagged "ordinal" so they can be seen in the UI.
 * Most could but I'm trying to reduce clutter and questions.
 */

#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <ctype.h>

#include "../../util/Util.h"
#include "../../util/List.h"
#include "../../model/MobiusConfig.h"
#include "../../model/Setup.h"
#include "../Audio.h"

#include "Action.h"
#include "Export.h"
#include "Function.h"
#include "Mobius.h"
#include "Mode.h"
#include "Project.h"
#include "Track.h"
#include "Script.h"
#include "Synchronizer.h"

#include "Parameter.h"

#define MAX_TRACKS 32

/****************************************************************************
 *                                                                          *
 *   						  GLOBAL PARAMETER                              *
 *                                                                          *
 ****************************************************************************/

class GlobalParameter : public Parameter {
  public:

    GlobalParameter(const char* name) :
        Parameter(name) {
        scope = PARAM_SCOPE_GLOBAL;
        mComplained = false;
    }

    /**
     * Overload the Parameter versions and cast to a MobiusConfig.
     */
	void getObjectValue(void* obj, ExValue* value);
	void setObjectValue(void* obj, ExValue* value);

    /**
     * Overload the Parameter versions and pass a Mobius;
     */
	virtual void getValue(Export* exp, ExValue* value);
	virtual void setValue(Action* action);

    /**
     * Overload the Parameter versions and resolve a MobiusConfig.
     */
	int getOrdinalValue(Export* exp);

    /**
     * These must always be overloaded.
     * update: which is stupid because some now overload Export so it doesn't need to
     */
	virtual void getValue(MobiusConfig* c, ExValue* value) = 0;
	virtual void setValue(MobiusConfig* c, ExValue* value) = 0;

    /**
     * This must be overloaded by anything that supports ordinals.
     */
	virtual int getOrdinalValue(MobiusConfig* c);

  private:
    bool mComplained;

};


void GlobalParameter::getObjectValue(void* obj, ExValue* value)
{
    getValue((MobiusConfig*)obj, value);
}

void GlobalParameter::setObjectValue(void* obj, ExValue* value)
{
    setValue((MobiusConfig*)obj, value);
}

void GlobalParameter::getValue(Export* exp, ExValue* value)
{
	Mobius* m = (Mobius*)exp->getMobius();
    if (m == nullptr) {
        Trace(1, "Mobius not passed in Export!\n");
		value->setNull();
    }
    else {
        // for gets use the external one
        // !! think about this, should we consistently use the interrupt 
        // config, it probably doesn't matter since only scripts
        // deal with most globals 
        MobiusConfig* config = m->getConfiguration();
        getValue(config, value);
    }
}

void GlobalParameter::setValue(Action* action)
{
	Mobius* m = (Mobius*)action->mobius;
    if (m == nullptr)
	  Trace(1, "Mobius not passed in Action!\n");
    else {
        MobiusConfig* config = m->getConfiguration();
        setValue(config, &(action->arg));

        config = m->getConfiguration();
        if (config != nullptr)
          setValue(config, &(action->arg));
    }
}

int GlobalParameter::getOrdinalValue(Export* exp)
{
    int value = -1;
	Mobius* m = (Mobius*)exp->getMobius();
    if (m == nullptr) {
        Trace(1, "Mobius not passed in Export!\n");
    }
    else {
        // for gets use the external one
        // !! think about this, should we consistently use the interrupt 
        // config, it probably doesn't matter since only scripts
        // deal with most globals 
        MobiusConfig* config = m->getConfiguration();
        value = getOrdinalValue(config);
    }
    return value;
}

/**
 * NEW: We used to complain here if the subclass didn't overload it,
 * but in the new world we're ALWAYS asking for ordinals rather than calling
 * getValue like before.  The only classes that need to override this are the ones
 * that don't have int or bool types.  For those simple types we can convert the
 * numeric value to the "ordinal".
 */
int GlobalParameter::getOrdinalValue(MobiusConfig* c)
{
    int value = -1;
    
    if (type == TYPE_INT || type == TYPE_BOOLEAN) {
        ExValue ev;
        getValue(c, &ev);
        value = ev.getInt();
    }
    else {
        // this soaks up so many resources, only do it once!
        if (!mComplained) {
            Trace(1, "Parameter %s: getOrdinalValue(MobiusConfig) not overloaded!\n",
                  getName());
            mComplained = true;
        }
    }
    return value;
}

//////////////////////////////////////////////////////////////////////
//
// SetupName
//
//////////////////////////////////////////////////////////////////////

// This one is important and awkward
//
// The name "setup" is used everywhere in test scripts and probably
// user scripts.  It was a bindable parameter though don't know how often
// that was used.
//
// In the olden code, this both set the active setup and saved it in the
// MobiusConfig which was authoritative.  The notion of what the active setup
// means is different now, it's not really a MobiusConfig parameter, it's more of
// a session parameter that needs to be saved somewhere on exit, but you don't go
// to the global parameters panel and change the Active Setup parameter, you pick
// it from a manu or from a script, or other ways.  Since this is still the
// way we deal with changing setups from the UI and scripts, need to keep it,
// but it won't be edited in MobiusConfig any more.
//
// Now we always go to the "live" model which is Mobius->getSetup
//

class SetupNameParameterType : public GlobalParameter
{
  public:
	SetupNameParameterType();

	void setValue(Action* action) override;
	void getValue(Export* exp, ExValue* value) override;
    int getOrdinalValue(Export* exp) override;

    // have to define these, but they're not used
    void getValue(MobiusConfig* c, ExValue* value) override;
	void setValue(MobiusConfig* c, ExValue* value) override;
    int getOrdinalValue(MobiusConfig* c) override;

	int getHigh(Mobius* m) override;
    void getOrdinalLabel(Mobius* m, int i, ExValue* value) override;
};

SetupNameParameterType::SetupNameParameterType() :
    // this must match the TargetSetup name
    GlobalParameter("setup")
{
	type = TYPE_STRING;
    bindable = true;
	dynamic = true;
}

// These can't be used for editing the MobiusConfig any more
// we shouldn't be calling them

int SetupNameParameterType::getOrdinalValue(MobiusConfig* c)
{
    (void)c;
    Trace(1, "SetupNameParameter::getOrdinalValue Who called this?");
    return 0;
}

void SetupNameParameterType::getValue(MobiusConfig* c, ExValue* value)
{
    (void)c;
    Trace(1, "SetupNameParameter::getValue Who called this?");
	value->setString("???");
}

/**
 * For scripts accept a name or a number.
 * Number is 1 based like SetupNumberParameter.
 *
 * Scripts should use Action now
 */
void SetupNameParameterType::setValue(MobiusConfig* c, ExValue* value)
{
    (void)c;
    (void)value;
    Trace(1, "SetupNameParameter::setValue Who called this?");
}

int SetupNameParameterType::getOrdinalValue(Export* exp)
{
    (void)exp;
    return 0;
}

/**
 * Unusual GlobalParameter override to get the value raw and live from Mobius
 * rather than MobiusConfig.
 */
void SetupNameParameterType::getValue(Export* exp, ExValue* value)
{
    (void)exp;
    value->setString("???");
}

/**
 * Here for scripts and for bindings if we expose them.
 * This is now a transient session parameter that will be persisted
 * on shutdown, possibly in MobiusConfig but maybe elsewhere.
 * Number is 1 based like SetupNumberParameter.
 */
void SetupNameParameterType::setValue(Action* action)
{
    (void)action;
    Trace(1, "SetupNameParameter::setValue(action)  Who called this?");
#if 0    
	Mobius* m = (Mobius*)action->mobius;
	if (m == nullptr)
	  Trace(1, "Mobius not passed in Action!\n");
    else {
        MobiusConfig* config = m->getConfiguration();

        Setup* setup = nullptr;
        if (action->arg.getType() == EX_INT)
          setup = config->getSetup(action->arg.getInt());
        else 
          setup = config->getSetup(action->arg.getString());

        if (setup != nullptr) {
            
            m->setActiveSetup(setup->ordinal);

            // historically we have saved this in MobiusConfig
            // which we still can but it won't be saved in a file
            // Supervisor handles that in the "session" save

            // it MIGHT be important to have this for old code
            // that looked here but since changing the name to StartingSetup
            // I don't think there is any
            config->setStartingSetupName(setup->getName());
        }
    }
#endif
}

/**
 * !! The max can change as setups are added/removed.
 * Need to work out a way to convey that to ParameterEditor.
 */
int SetupNameParameterType::getHigh(Mobius* m)
{
    (void)m;
    return 0;
}

/**
 * Given an ordinal, map it into a display label.
 * Since we set the interrupt setup, that
 */
void SetupNameParameterType::getOrdinalLabel(Mobius* mobius,
                                             int i, ExValue* value)
{
    (void)mobius;
    (void)i;
    value->setString("???");
}

SetupNameParameterType SetupNameParameterTypeObj;
Parameter* SetupNameParameter = &SetupNameParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// Track
//
// !! Not sure I like this.  We already have the track select
// functions but those have TrackCopy semantics so maybe it makes
// sense to have this too (which doesn't).  This also gives us a way
// to switch tracks more easilly through the plugin interface.
//
//////////////////////////////////////////////////////////////////////

class TrackParameterType : public GlobalParameter
{
  public:
	TrackParameterType();
	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
	void getValue(Export* exp, ExValue* value);
    int getOrdinalValue(Export* exp);
	void setValue(Action* action);
};

TrackParameterType::TrackParameterType() :
	// changed from "track" to "selectedTrack" to avoid ambiguity
	// with the read-only variable
    GlobalParameter("selectedTrack")
{
	type = TYPE_INT;
	low = 1;
	high = MAX_TRACKS;
    // not in XML
    transient = true;
    // but a good one for CC bindings
    bindable = true;
}

void TrackParameterType::getValue(MobiusConfig* c, ExValue* value)
{
    (void)c;
    (void)value;
    // transient, shouldn't be here, 
    // !! the selected track from the Setup could be the same as this
    // think!
    Trace(1, "TrackParameterType::getValue!\n");
}

void TrackParameterType::setValue(MobiusConfig* c, ExValue* value)
{
    (void)c;
    (void)value;
    // transient, shouldn't be here, 
    Trace(1, "TrackParameterType::setValue!\n");
}

void TrackParameterType::getValue(Export* exp, ExValue* value)
{
	// let this be 1 based in the script
    Mobius* m = (Mobius*)exp->getMobius();
	Track* t = m->getTrack(m->getActiveTrack());
	if (t != nullptr)
	  value->setInt(t->getDisplayNumber());
	else {
		// assume zero
		value->setInt(1);
	}
}

void TrackParameterType::setValue(Action* action)
{
    Mobius* m = (Mobius*)action->mobius;
	// let this be 1 based in the script
	int ivalue = action->arg.getInt() - 1;
	m->setActiveTrack(ivalue);
}

/**
 * We'll be here since we're bindable and each interrupt
 * may have an Export that will try to export our ordinal value.
 */
int TrackParameterType::getOrdinalValue(Export *exp)
{
    ExValue v;
    getValue(exp, &v);
    return v.getInt();
}

TrackParameterType TrackParameterTypeObj;
Parameter* TrackParameter = &TrackParameterTypeObj;

/****************************************************************************
 *                                                                          *
 *   							   DEVICES                                  *
 *                                                                          *
 ****************************************************************************/

//////////////////////////////////////////////////////////////////////
//
// InputLatency
//
//////////////////////////////////////////////////////////////////////

class InputLatencyParameterType : public GlobalParameter
{
  public:
	InputLatencyParameterType();
	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
	void setValue(Action* action);
};

InputLatencyParameterType::InputLatencyParameterType() :
    GlobalParameter("inputLatency")
{
    // not bindable
	type = TYPE_INT;
}

void InputLatencyParameterType::getValue(MobiusConfig* c, ExValue* value)
{
	value->setInt(c->getInputLatency());
}

void InputLatencyParameterType::setValue(MobiusConfig* c, ExValue* value)
{
	c->setInputLatency(value->getInt());
}

/**
 * Binding this is rare but we do set this in test scripts.
 * For this to have any meaning we have to propagate this to the
 * Streams and the Loops via the Tracks.
 */
void InputLatencyParameterType::setValue(Action* action)
{
    int latency = action->arg.getInt();
    
    Mobius* m = (Mobius*)action->mobius;
	MobiusConfig* config = m->getConfiguration();
	config->setInputLatency(latency);
    
    MobiusConfig* iconfig = m->getConfiguration();
    if (iconfig != nullptr) {
        iconfig->setInputLatency(latency);

        for (int i = 0 ; i < m->getTrackCount() ; i++) {
            Track* t = m->getTrack(i);
            t->updateGlobalParameters(iconfig);
        }
    }
}

InputLatencyParameterType InputLatencyParameterTypeObj;
Parameter* InputLatencyParameter = &InputLatencyParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// OutputLatency
//
//////////////////////////////////////////////////////////////////////

class OutputLatencyParameterType : public GlobalParameter
{
  public:
	OutputLatencyParameterType();
	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
	void setValue(Action* action);
};

OutputLatencyParameterType::OutputLatencyParameterType() :
    GlobalParameter("outputLatency")
{
    // not bindable
	type = TYPE_INT;
}

void OutputLatencyParameterType::getValue(MobiusConfig* c, ExValue* value)
{
	value->setInt(c->getOutputLatency());
}

void OutputLatencyParameterType::setValue(MobiusConfig* c, ExValue* value)
{
	c->setOutputLatency(value->getInt());
}

/**
 * Binding this is rare but we do set this in test scripts.
 * For this to have any meaning we have to propagate this to the
 * Streams and Loops via the Tracks.
 */
void OutputLatencyParameterType::setValue(Action* action)
{
    int latency = action->arg.getInt();

    Mobius* m = (Mobius*)action->mobius;
	MobiusConfig* config = m->getConfiguration();
	config->setOutputLatency(latency);

    MobiusConfig* iconfig = m->getConfiguration();
    if (iconfig != nullptr) {
        iconfig->setOutputLatency(latency);

        for (int i = 0 ; i < m->getTrackCount() ; i++) {
            Track* t = m->getTrack(i);
            t->updateGlobalParameters(iconfig);
        }
    }
}

OutputLatencyParameterType OutputLatencyParameterTypeObj;
Parameter* OutputLatencyParameter = &OutputLatencyParameterTypeObj;

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
