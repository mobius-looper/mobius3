// commented out things related to BindingConfig

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
    if (m == NULL) {
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
    if (m == NULL)
	  Trace(1, "Mobius not passed in Action!\n");
    else {
        MobiusConfig* config = m->getConfiguration();
        setValue(config, &(action->arg));

        config = m->getConfiguration();
        if (config != NULL)
          setValue(config, &(action->arg));
    }
}

int GlobalParameter::getOrdinalValue(Export* exp)
{
    int value = -1;
	Mobius* m = (Mobius*)exp->getMobius();
    if (m == NULL) {
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
    Trace(1, "SetupNameParameter::getOrdinalValue Who called this?");
    Setup* setup = c->getStartingSetup();
    return setup->ordinal;
}

void SetupNameParameterType::getValue(MobiusConfig* c, ExValue* value)
{
    Trace(1, "SetupNameParameter::getValue Who called this?");
    Setup* setup = c->getStartingSetup();
	value->setString(setup->getName());
}

/**
 * For scripts accept a name or a number.
 * Number is 1 based like SetupNumberParameter.
 *
 * Scripts should use Action now
 */
void SetupNameParameterType::setValue(MobiusConfig* c, ExValue* value)
{
    Trace(1, "SetupNameParameter::setValue Who called this?");
    
    Setup* setup = NULL;

    if (value->getType() == EX_INT)
      setup = c->getSetup(value->getInt());
    else 
      setup = c->getSetup(value->getString());

    // !! allocates memory
    if (setup != NULL)
      c->setStartingSetupName(setup->getName());
}

int SetupNameParameterType::getOrdinalValue(Export* exp)
{
    Mobius* m = (Mobius*)exp->getMobius();
    Setup* setup = m->getSetup();

    // sigh, setup->ordinal is unreliable, have to refresh
    // fugly dependencies on Setup being inside MobiusConfig,
    // why was ordinal broken?
    MobiusConfig* config = m->getConfiguration();
    Structure::ordinate(config->getSetups());
    return setup->ordinal;
}

/**
 * Unusual GlobalParameter override to get the value raw and live from Mobius
 * rather than MobiusConfig.
 */
void SetupNameParameterType::getValue(Export* exp, ExValue* value)
{
    Mobius* m = (Mobius*)exp->getMobius();
    Setup* setup = m->getSetup();
    value->setString(setup->getName());
}

/**
 * Here for scripts and for bindings if we expose them.
 * This is now a transient session parameter that will be persisted
 * on shutdown, possibly in MobiusConfig but maybe elsewhere.
 * Number is 1 based like SetupNumberParameter.
 */
void SetupNameParameterType::setValue(Action* action)
{
	Mobius* m = (Mobius*)action->mobius;
	if (m == NULL)
	  Trace(1, "Mobius not passed in Action!\n");
    else {
        MobiusConfig* config = m->getConfiguration();

        Setup* setup = NULL;
        if (action->arg.getType() == EX_INT)
          setup = config->getSetup(action->arg.getInt());
        else 
          setup = config->getSetup(action->arg.getString());

        if (setup != NULL) {
            
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
}

/**
 * !! The max can change as setups are added/removed.
 * Need to work out a way to convey that to ParameterEditor.
 */
int SetupNameParameterType::getHigh(Mobius* m)
{
	MobiusConfig* config = m->getConfiguration();
    int max = Structure::count(config->getSetups());
    // this is the number of configs, the max ordinal is zero based
    max--;

    return max;
}

/**
 * Given an ordinal, map it into a display label.
 * Since we set the interrupt setup, that
 */
void SetupNameParameterType::getOrdinalLabel(Mobius* mobius,
                                                    int i, ExValue* value)
{
    // use the interrupt config since that's the one we're really using
    Mobius* m = (Mobius*)mobius;
	MobiusConfig* config = m->getConfiguration();
	Setup* setup = config->getSetup(i);
	if (setup != NULL)
	  value->setString(setup->getName());
	else
      value->setString("???");
}

SetupNameParameterType SetupNameParameterTypeObj;
Parameter* SetupNameParameter = &SetupNameParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// SetupNumber
//
//////////////////////////////////////////////////////////////////////

/**
 * Provided so scripts can deal with setups as numbers if necessary
 * though I would expect usually they will be referenced using names.
 * 
 * !! NOTE: For consistency with TrackPresetNumber the setup numbers
 * are the zero based intenral numbers.  This is unlike how
 * tracks and loops are numbered from 1.
 */
class SetupNumberParameterType : public GlobalParameter
{
  public:
	SetupNumberParameterType();

	void getValue(MobiusConfig* c, ExValue* value) override;
	void setValue(MobiusConfig* c, ExValue* value) override;
	void setValue(Action* action) override;
};

SetupNumberParameterType SetupNumberParameterTypeObj;
Parameter* SetupNumberParameter = &SetupNumberParameterTypeObj;

SetupNumberParameterType::SetupNumberParameterType() :
    GlobalParameter("setupNumber")
{
	type = TYPE_INT;
    // not displayed in the UI, don't include it in the XML
    transient = true;
    // dynmic means it can change after the UI is initialized
    // I don't think we need to say this if it isn't bindable
	dynamic = true;
}

void SetupNumberParameterType::getValue(MobiusConfig* c, ExValue* value)
{
    Setup* setup = c->getStartingSetup();
    value->setInt(setup->ordinal);
}

/**
 * This is a fake parameter so we won't edit them in the config.
 */
void SetupNumberParameterType::setValue(MobiusConfig* c, ExValue* value)
{
    (void)c;
    (void)value;
}

void SetupNumberParameterType::setValue(Action* action)
{
    Mobius* m = action->mobius;
    // validate using the external config
    MobiusConfig* config = m->getConfiguration();
    int index = action->arg.getInt();
    Setup* setup = config->getSetup(index);

    if (setup != NULL) {
        m->setActiveSetup(index);
    }
}

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
	if (t != NULL)
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

//////////////////////////////////////////////////////////////////////
//
// Bindings
//
//////////////////////////////////////////////////////////////////////

// new: this is now a UI concept, with no implementation
// down here, except that it needs to be a core parameter
// so old scripts can still to "set bindings foo"

/**
 * This is a funny one because ordinal value 0 means "no overlay"
 * and we want to show that and treat it as a valid value.
 */
class BindingsParameterType : public GlobalParameter
{
  public:
	BindingsParameterType();

    int getOrdinalValue(MobiusConfig* c);
	int getHigh(Mobius* m);
    void getOrdinalLabel(Mobius* m, int i, ExValue* value);

	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);

	void setValue(Action* action);
};

BindingsParameterType::BindingsParameterType() :
    // formerly "midiConfig" but don't bother with an alias
    // this must match the TargetBindings name
    GlobalParameter("bindings")
{
	type = TYPE_STRING;
    bindable = true;
    dynamic = true;
}

int BindingsParameterType::getOrdinalValue(MobiusConfig* c)
{
    (void)c;
    Trace(1, "BindingsParameterType::getOrdinalValue touched\n");
    return 0;
}

void BindingsParameterType::getValue(MobiusConfig* c, ExValue* value)
{
    (void)c;
    Trace(1, "BindingsParameterType::getValue touched\n");
    value->setNull();
}

void BindingsParameterType::setValue(MobiusConfig* c, ExValue* value)
{
    (void)c;
    (void)value;
    Trace(1, "BindingsParameterType::setValue touched\n");
}

/**
 * new: Changing this in a script is complex because it has to be
 * fowarded all the way back up to Supervisor.
 */
void BindingsParameterType::setValue(Action* action)
{
    Mobius* m = (Mobius*)action->mobius;
    m->activateBindings(action);
}

int BindingsParameterType::getHigh(Mobius* m)
{
    (void)m;
    Trace(1, "BindingsParameterType::getHigh touched\n");
    return 0;
}

void BindingsParameterType::getOrdinalLabel(Mobius* m,
                                            int i, ExValue* value)
{
    (void)m;
    (void)i;
    (void)value;
    Trace(1, "BindingsParameterType::getOrdinalLabel touched\n");
    value->setString("???");
}

BindingsParameterType BindingsParameterTypeObj;
Parameter* BindingsParameter = &BindingsParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// FadeFrames
//
//////////////////////////////////////////////////////////////////////

class FadeFramesParameterType : public GlobalParameter
{
  public:
	FadeFramesParameterType();
	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
	void setValue(Action* action);
};

FadeFramesParameterType::FadeFramesParameterType() :
    GlobalParameter("fadeFrames")
{
    // not bindable
	type = TYPE_INT;
	high = 1024;
}

void FadeFramesParameterType::getValue(MobiusConfig* c, ExValue* value)
{
	value->setInt(c->getFadeFrames());
}
void FadeFramesParameterType::setValue(MobiusConfig* c, ExValue* value)
{
    c->setFadeFrames(value->getInt());
}

/**
 * Binding this is rare but we do set it in test scripts.
 * For this to have any meaning we have to propagate it to the
 * AudioFade class.  
 */
void FadeFramesParameterType::setValue(Action* action)
{
    int frames = action->arg.getInt();

    // don't bother propagating to the interrupt config, we only
    // need it in AudioFade
	MobiusConfig* config = action->mobius->getConfiguration();
	config->setFadeFrames(frames);

    AudioFade::setRange(frames);
}

FadeFramesParameterType FadeFramesParameterTypeObj;
Parameter* FadeFramesParameter = &FadeFramesParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// MaxSyncDrift
//
//////////////////////////////////////////////////////////////////////

class MaxSyncDriftParameterType : public GlobalParameter
{
  public:
	MaxSyncDriftParameterType();
	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
	void setValue(Action* action);
};

MaxSyncDriftParameterType::MaxSyncDriftParameterType() :
    GlobalParameter("maxSyncDrift")
{
    // not worth bindable
	type = TYPE_INT;
	high = 1024 * 16;
    // The low end is dependent on the sync source, for
    // Host sync you could set this to zero and get good results,   
    // for MIDI sync the effective minimum has to be around 512 due
    // to jitter.  Unfortunately we can't know that context here so
    // leave the low at zero.
}

void MaxSyncDriftParameterType::getValue(MobiusConfig* c, ExValue* value)
{
	value->setInt(c->getMaxSyncDrift());
}
void MaxSyncDriftParameterType::setValue(MobiusConfig* c, ExValue* value)
{
    c->setMaxSyncDrift(value->getInt());
}

/**
 * Binding this is rare but we occasionally set this in test scripts.
 * For this to have any meaning, we also need to propagate it to the
 * Synchronizer which keeps a cached copy.  Also copy it to the interrupt
 * config just so they stay in sync though that isn't used.
 */
void MaxSyncDriftParameterType::setValue(Action* action)
{
    int drift = action->arg.getInt();

    Mobius* m = (Mobius*)action->mobius;
	MobiusConfig* config = m->getConfiguration();
	config->setMaxSyncDrift(drift);

    MobiusConfig* iconfig = m->getConfiguration();
    if (iconfig != NULL) {
        iconfig->setMaxSyncDrift(drift);
        Synchronizer* sync = m->getSynchronizer();
        sync->updateConfiguration(iconfig);
    }
}

MaxSyncDriftParameterType MaxSyncDriftParameterTypeObj;
Parameter* MaxSyncDriftParameter = &MaxSyncDriftParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// DriftCheckPoint
//
//////////////////////////////////////////////////////////////////////

/**
 * This is not currently exposed int he UI, though it can be set in scripts.
 */
class DriftCheckPointParameterType : public GlobalParameter
{
  public:
	DriftCheckPointParameterType();
	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
	void setValue(Action* action);
};

const char* DRIFT_CHECK_POINT_NAMES[] = {
	"loop", "external", NULL
};

DriftCheckPointParameterType::DriftCheckPointParameterType() :
    GlobalParameter("driftCheckPoint")
{
    // don't bother making this bindable
	type = TYPE_ENUM;
	values = DRIFT_CHECK_POINT_NAMES;
}

void DriftCheckPointParameterType::getValue(MobiusConfig* c, ExValue* value)
{
	value->setString(values[c->getDriftCheckPoint()]);
}

void DriftCheckPointParameterType::setValue(MobiusConfig* c, ExValue* value)
{
    DriftCheckPoint dcp = (DriftCheckPoint)getEnum(value);
    c->setDriftCheckPoint(dcp);
}

/**
 * Binding this is rare but we occasionally set this in test scripts.
 * For this to have any meaning, we also need to propagate it to the
 * Synchronizer which keeps a cached copy.  Also copy it to the interrupt
 * config just so they stay in sync though that isn't used.
 */
void DriftCheckPointParameterType::setValue(Action* action)
{
    DriftCheckPoint dcp = (DriftCheckPoint)getEnum(&(action->arg));

    Mobius* m = (Mobius*)action->mobius;
	MobiusConfig* config = m->getConfiguration();
	config->setDriftCheckPoint(dcp);

    MobiusConfig* iconfig = m->getConfiguration();
    if (iconfig != NULL) {
        iconfig->setDriftCheckPoint(dcp);
        Synchronizer* sync = m->getSynchronizer();
        sync->updateConfiguration(iconfig);
    }
}

DriftCheckPointParameterType DriftCheckPointParameterTypeObj;
Parameter* DriftCheckPointParameter = &DriftCheckPointParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// NoiseFloor
//
//////////////////////////////////////////////////////////////////////

class NoiseFloorParameterType : public GlobalParameter
{
  public:
	NoiseFloorParameterType();
	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
};

NoiseFloorParameterType::NoiseFloorParameterType() :
    GlobalParameter("noiseFloor")
{
    // not bindable
	type = TYPE_INT;
    // where the hell did this value come from?
	high = 15359;
}

void NoiseFloorParameterType::getValue(MobiusConfig* c, ExValue* value)
{
	value->setInt(c->getNoiseFloor());
}

void NoiseFloorParameterType::setValue(MobiusConfig* c, ExValue* value)
{
    c->setNoiseFloor(value->getInt());
}

NoiseFloorParameterType NoiseFloorParameterTypeObj;
Parameter* NoiseFloorParameter = &NoiseFloorParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// MidiExport, HostMidiExport
//
//////////////////////////////////////////////////////////////////////

class MidiExportParameterType : public GlobalParameter
{
  public:
	MidiExportParameterType();
	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
};

MidiExportParameterType::MidiExportParameterType() : 
    GlobalParameter("midiExport")
{
    // not worth bindable
	type = TYPE_BOOLEAN;
    // original name
    addAlias("midiFeedback");
}

void MidiExportParameterType::getValue(MobiusConfig* c, ExValue* value)
{
	value->setBool(c->isMidiExport());
}

void MidiExportParameterType::setValue(MobiusConfig* c, ExValue* value)
{
	c->setMidiExport(value->getBool());
}

MidiExportParameterType MidiExportParameterTypeObj;
Parameter* MidiExportParameter = &MidiExportParameterTypeObj;

/////////////////////////////////////////

class HostMidiExportParameterType : public GlobalParameter
{
  public:
	HostMidiExportParameterType();
	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
};

HostMidiExportParameterType::HostMidiExportParameterType() :
    GlobalParameter("hostMidiExport")
{
    // not worth bindable
	type = TYPE_BOOLEAN;
    addAlias("hostMidiFeedback");
}

void HostMidiExportParameterType::getValue(MobiusConfig* c, ExValue* value)
{
	value->setBool(c->isHostMidiExport());
}

void HostMidiExportParameterType::setValue(MobiusConfig* c, ExValue* value)
{
	c->setHostMidiExport(value->getBool());
}

HostMidiExportParameterType HostMidiExportParameterTypeObj;
Parameter* HostMidiExportParameter = &HostMidiExportParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// LongPress
//
//////////////////////////////////////////////////////////////////////

class LongPressParameterType : public GlobalParameter
{
  public:
	LongPressParameterType();
	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
};

LongPressParameterType::LongPressParameterType() :
    GlobalParameter("longPress")
{
    // not bindable
	type = TYPE_INT;
	low = 250;
	high = 10000;
}

void LongPressParameterType::getValue(MobiusConfig* c, ExValue* value)
{
	value->setInt(c->getLongPress());
}

void LongPressParameterType::setValue(MobiusConfig* c, ExValue* value)
{
	c->setLongPress(value->getInt());
}

LongPressParameterType LongPressParameterTypeObj;
Parameter* LongPressParameter = &LongPressParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// SpreadRange
//
//////////////////////////////////////////////////////////////////////

class SpreadRangeParameterType : public GlobalParameter
{
  public:
	SpreadRangeParameterType();
	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
};

SpreadRangeParameterType::SpreadRangeParameterType() :
    GlobalParameter("spreadRange")
{
    // not worth bindable
	type = TYPE_INT;
	low = 1;
	high = 128;
    addAlias("shiftRange");
}

void SpreadRangeParameterType::getValue(MobiusConfig* c, ExValue* value)
{
	value->setInt(c->getSpreadRange());
}

void SpreadRangeParameterType::setValue(MobiusConfig* c, ExValue* value)
{
	c->setSpreadRange(value->getInt());
}

SpreadRangeParameterType SpreadRangeParameterTypeObj;
Parameter* SpreadRangeParameter = &SpreadRangeParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// CustomMode
//
//////////////////////////////////////////////////////////////////////

class CustomModeParameterType : public GlobalParameter
{
  public:
	CustomModeParameterType();
	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
	void getValue(Export* exp, ExValue* value);
	void setValue(Action* action);
};

CustomModeParameterType::CustomModeParameterType() :
    GlobalParameter("customMode")
{
    // not bindable
	type = TYPE_STRING;
    // sould this be in the Setup?
    transient = true;
}

void CustomModeParameterType::getValue(MobiusConfig* c, ExValue* value)
{
    (void)c;
    (void)value;
    Trace(1, "CustomModeParameterType::getValue!\n");
}

void CustomModeParameterType::setValue(MobiusConfig* c, ExValue* value)
{
    (void)c;
    (void)value;
    Trace(1, "CustomModeParameterType::setValue!\n");
}

void CustomModeParameterType::getValue(Export* exp, ExValue* value)
{
    Mobius* m = (Mobius*)exp->getMobius();
	value->setString(m->getCustomMode());
}

void CustomModeParameterType::setValue(Action* action)
{
    Mobius* m = (Mobius*)action->mobius;
	m->setCustomMode(action->arg.getString());
}

CustomModeParameterType CustomModeParameterTypeObj;
Parameter* CustomModeParameter = &CustomModeParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// AutoFeedbackReduction
//
//////////////////////////////////////////////////////////////////////

class AutoFeedbackReductionParameterType : public GlobalParameter
{
  public:
    AutoFeedbackReductionParameterType();
	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
    void setValue(Action* action);
};

AutoFeedbackReductionParameterType::AutoFeedbackReductionParameterType() :
    GlobalParameter("autoFeedbackReduction")
{
    // not worth bindable
    type = TYPE_BOOLEAN;
}

void AutoFeedbackReductionParameterType::getValue(MobiusConfig* c, 
                                                  ExValue* value)
{
	value->setBool(c->isAutoFeedbackReduction());
}
void AutoFeedbackReductionParameterType::setValue(MobiusConfig* c, 
                                                  ExValue* value)
{
    c->setAutoFeedbackReduction(value->getBool());
}

/**
 * Binding this is rare but we do set this in test scripts.
 * For this to have any meaning we have to propagate this to the
 * Loops via the Tracks.
 */
void AutoFeedbackReductionParameterType::setValue(Action* action)
{
    bool afr = action->arg.getBool();

    Mobius* m = (Mobius*)action->mobius;
    MobiusConfig* config = m->getConfiguration();
    config->setAutoFeedbackReduction(afr);

    MobiusConfig* iconfig = m->getConfiguration();
    if (iconfig != NULL) {
        iconfig->setAutoFeedbackReduction(afr);

        for (int i = 0 ; i < m->getTrackCount() ; i++) {
            Track* t = m->getTrack(i);
            t->updateGlobalParameters(iconfig);
        }
    }
}

AutoFeedbackReductionParameterType AutoFeedbackReductionParameterTypeObj;
Parameter* AutoFeedbackReductionParameter = &AutoFeedbackReductionParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// IsolateOverdubs
//
//////////////////////////////////////////////////////////////////////

class IsolateOverdubsParameterType : public GlobalParameter
{
  public:
	IsolateOverdubsParameterType();
	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
};

IsolateOverdubsParameterType::IsolateOverdubsParameterType() :
    GlobalParameter("isolateOverdubs")
{
    // not worth bindable
	type = TYPE_BOOLEAN;
}

void IsolateOverdubsParameterType::getValue(MobiusConfig* c, ExValue* value)
{
	value->setBool(c->isIsolateOverdubs());
}

void IsolateOverdubsParameterType::setValue(MobiusConfig* c, ExValue* value)
{
	c->setIsolateOverdubs(value->getBool());
}

IsolateOverdubsParameterType IsolateOverdubsParameterTypeObj;
Parameter* IsolateOverdubsParameter = &IsolateOverdubsParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// MonitorAudio
//
//////////////////////////////////////////////////////////////////////

class MonitorAudioParameterType : public GlobalParameter
{
  public:
	MonitorAudioParameterType();
	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
    void setValue(Action* action);
};

MonitorAudioParameterType::MonitorAudioParameterType() :
    GlobalParameter("monitorAudio")
{
    // not worth bindable
	type = TYPE_BOOLEAN;
}

void MonitorAudioParameterType::getValue(MobiusConfig* c, ExValue* value)
{
	value->setBool(c->isMonitorAudio());
}
void MonitorAudioParameterType::setValue(MobiusConfig* c, ExValue* value)
{
    c->setMonitorAudio(value->getBool());
}

/**
 * Binding this is rare, but we do set it in test scripts.
 * For this to have any meaning we need to propagate it to the
 * interrupt config where Track will look at it, and also 
 * to the Recorder.
 *
 * UPDATE: Recorder is gone and we lost the abillity to do this.
 * Redesign if we think it is still useful.
 */
void MonitorAudioParameterType::setValue(Action* action)
{
    bool monitor = action->arg.getBool();

    Mobius* m = (Mobius*)action->mobius;
    MobiusConfig* config = m->getConfiguration();
	config->setMonitorAudio(monitor);
#if 0
    Recorder* rec = m->getRecorder();
    if (rec != NULL)
      rec->setEcho(monitor);
#endif
}

MonitorAudioParameterType MonitorAudioParameterTypeObj;
Parameter* MonitorAudioParameter = &MonitorAudioParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// SaveLayers
//
//////////////////////////////////////////////////////////////////////

class SaveLayersParameterType : public GlobalParameter
{
  public:
	SaveLayersParameterType();
	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
};

SaveLayersParameterType::SaveLayersParameterType() :
    GlobalParameter("saveLayers")
{
    // not worth bindable
	type = TYPE_BOOLEAN;
}

void SaveLayersParameterType::getValue(MobiusConfig* c, ExValue* value)
{
	value->setBool(c->isSaveLayers());
}

void SaveLayersParameterType::setValue(MobiusConfig* c, ExValue* value)
{
	c->setSaveLayers(value->getBool());
}

SaveLayersParameterType SaveLayersParameterTypeObj;
Parameter* SaveLayersParameter = &SaveLayersParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// QuickSave
//
//////////////////////////////////////////////////////////////////////

class QuickSaveParameterType : public GlobalParameter
{
  public:
	QuickSaveParameterType();
	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
};

QuickSaveParameterType::QuickSaveParameterType() :
    GlobalParameter("quickSave")
{
    // not bindable
	type = TYPE_STRING;
}

void QuickSaveParameterType::getValue(MobiusConfig* c, ExValue* value)
{
	value->setString(c->getQuickSave());
}

void QuickSaveParameterType::setValue(MobiusConfig* c, ExValue* value)
{
	c->setQuickSave(value->getString());
}

QuickSaveParameterType QuickSaveParameterTypeObj;
Parameter* QuickSaveParameter = &QuickSaveParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// IntegerWaveFile
//
//////////////////////////////////////////////////////////////////////

class IntegerWaveFileParameterType : public GlobalParameter
{
  public:
	IntegerWaveFileParameterType();
	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
	void setValue(Action* action);
};

IntegerWaveFileParameterType::IntegerWaveFileParameterType() :
    GlobalParameter("16BitWaveFile")
{
    // not worth bindable
	type = TYPE_BOOLEAN;
}

void IntegerWaveFileParameterType::getValue(MobiusConfig* c, ExValue* value)
{
	value->setBool(c->isIntegerWaveFile());
}
void IntegerWaveFileParameterType::setValue(MobiusConfig* c, ExValue* value)
{
    c->setIntegerWaveFile(value->getBool());
}

/**
 * Binding this is rare but we do set it in test scripts.
 * For this to have any meaning we have to propagate it to the
 * Audio class.  
 */
void IntegerWaveFileParameterType::setValue(Action* action)
{
    bool isInt = action->arg.getBool();
    // don't bother propagating this to the interrupt config
    Mobius* m = (Mobius*)action->mobius;
    MobiusConfig* config = m->getConfiguration();
	config->setIntegerWaveFile(isInt);

    // this shouldn't be a parameter and we should be dealing with
    // files at a higher level
    Trace(1, "IntegerWaveFileParameterType: setWriteFormat!\n");
    //Audio::setWriteFormatPCM(isInt);
}

IntegerWaveFileParameterType IntegerWaveFileParameterTypeObj;
Parameter* IntegerWaveFileParameter = &IntegerWaveFileParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// AltFeedbackDisable
//
//////////////////////////////////////////////////////////////////////

class AltFeedbackDisableParameterType : public GlobalParameter
{
  public:
	AltFeedbackDisableParameterType();
	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
	void getValue(Mobius* m, ExValue* value);
	void setValue(Mobius* m, ExValue* value);
};

AltFeedbackDisableParameterType::AltFeedbackDisableParameterType() :
    GlobalParameter("altFeedbackDisable")
{
    // not bindable
	type = TYPE_STRING;
}

void AltFeedbackDisableParameterType::getValue(MobiusConfig* c, ExValue* value)
{
	StringList* l = c->getAltFeedbackDisables();
	if (l == NULL)
	  value->setString(NULL);
	else {
		char* str = l->toCsv();
		value->setString(str);
		delete str;
	}
}

void AltFeedbackDisableParameterType::setValue(MobiusConfig* c, ExValue* value)
{
	if (value->isNull())
	  c->setAltFeedbackDisables(NULL);
	else
	  c->setAltFeedbackDisables(new StringList(value->getString()));
}

AltFeedbackDisableParameterType AltFeedbackDisableParameterTypeObj;
Parameter* AltFeedbackDisableParameter = &AltFeedbackDisableParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// GroupFocusLock
//
//////////////////////////////////////////////////////////////////////

class GroupFocusLockParameterType : public GlobalParameter
{
  public:
	GroupFocusLockParameterType();
	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
};

GroupFocusLockParameterType::GroupFocusLockParameterType() :
    GlobalParameter("groupFocusLock")
{
    // not worth bindable?
	type = TYPE_BOOLEAN;
}

void GroupFocusLockParameterType::getValue(MobiusConfig* c, ExValue* value)
{
	value->setBool(c->isGroupFocusLock());
}

void GroupFocusLockParameterType::setValue(MobiusConfig* c, ExValue* value)
{
	c->setGroupFocusLock(value->getBool());
}

GroupFocusLockParameterType GroupFocusLockParameterTypeObj;
Parameter* GroupFocusLockParameter = &GroupFocusLockParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// FocusLockFunctions
//
//////////////////////////////////////////////////////////////////////

class FocusLockFunctionsParameterType : public GlobalParameter
{
  public:
	FocusLockFunctionsParameterType();
	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
};

FocusLockFunctionsParameterType::FocusLockFunctionsParameterType() :
    GlobalParameter("focusLockFunctions")
{
    // not bindable
	type = TYPE_STRING;
    // the old name
    addAlias("groupFunctions");
}

void FocusLockFunctionsParameterType::getValue(MobiusConfig* c, ExValue* value)
{
	StringList* l = c->getFocusLockFunctions();
	if (l == NULL)
	  value->setString(NULL);
	else {
		char* str = l->toCsv();
		value->setString(str);
		delete str;
	}
}

void FocusLockFunctionsParameterType::setValue(MobiusConfig* c, ExValue* value)
{
	if (value->isNull())
	  c->setFocusLockFunctions(NULL);
	else
	  c->setFocusLockFunctions(new StringList(value->getString()));
}

FocusLockFunctionsParameterType FocusLockFunctionsParameterTypeObj;
Parameter* FocusLockFunctionsParameter = &FocusLockFunctionsParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// MuteCancelFunctions
//
//////////////////////////////////////////////////////////////////////

class MuteCancelFunctionsParameterType : public GlobalParameter
{
  public:
	MuteCancelFunctionsParameterType();
	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
	void setValue(Action* action);
};

MuteCancelFunctionsParameterType::MuteCancelFunctionsParameterType() :
    GlobalParameter("muteCancelFunctions")
{
    // not bindable
	type = TYPE_STRING;
}

void MuteCancelFunctionsParameterType::getValue(MobiusConfig* c, ExValue* value)
{
	StringList* l = c->getMuteCancelFunctions();
	if (l == NULL)
	  value->setString(NULL);
	else {
		char* str = l->toCsv();
		value->setString(str);
		delete str;
	}
}

void MuteCancelFunctionsParameterType::setValue(MobiusConfig* c, ExValue* value)
{
	if (value->isNull())
	  c->setMuteCancelFunctions(NULL);
	else
	  c->setMuteCancelFunctions(new StringList(value->getString()));
}

/**
 * Binding this is impossible but we might set it in a test script.
 * For this to have any meaning we have to propagate it to the
 * Funtion class.
 *
 * Changing this in an action was removed see also
 * ConfirmationFunctionsParameterType
 *
 * update: This is no longer used, old values will be upgraded to the
 * to be properties on the Symbol associated with each function.
 */
void MuteCancelFunctionsParameterType::setValue(Action* action)
{
    (void)action;
    Trace(1, "MuteCancelFunctionsParameterType::setValue Who called this?");
}

MuteCancelFunctionsParameterType MuteCancelFunctionsParameterTypeObj;
Parameter* MuteCancelFunctionsParameter = &MuteCancelFunctionsParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// ConfirmationFunctions
//
//////////////////////////////////////////////////////////////////////

class ConfirmationFunctionsParameterType : public GlobalParameter
{
  public:
	ConfirmationFunctionsParameterType();
	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
	void setValue(Action* action);
};

ConfirmationFunctionsParameterType::ConfirmationFunctionsParameterType() :
    GlobalParameter("confirmationFunctions")
{
    // not bindable
	type = TYPE_STRING;
}

void ConfirmationFunctionsParameterType::getValue(MobiusConfig* c, ExValue* value)
{
	StringList* l = c->getConfirmationFunctions();
	if (l == NULL)
	  value->setString(NULL);
	else {
		char* str = l->toCsv();
		value->setString(str);
		delete str;
	}
}

void ConfirmationFunctionsParameterType::setValue(MobiusConfig* c, ExValue* value)
{
	if (value->isNull())
	  c->setConfirmationFunctions(NULL);
	else
	  c->setConfirmationFunctions(new StringList(value->getString()));
}

/**
 * Binding this is impossible but we might set it in a test script.
 * For this to have any meaning we have to propagate it to the
 * Funtion class.
 * 
 * update: This is no longer used, old values will be upgraded to the
 * to be properties on the Symbol associated with each function.
 */
void ConfirmationFunctionsParameterType::setValue(Action* action)
{
    (void)action;
    Trace(1, "ConfirmationFunctionsParameterType::setValue Who called this?");
}

ConfirmationFunctionsParameterType ConfirmationFunctionsParameterTypeObj;
Parameter* ConfirmationFunctionsParameter = &ConfirmationFunctionsParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// MidiRecordMode
//
//////////////////////////////////////////////////////////////////////

class MidiRecordModeParameterType : public GlobalParameter
{
  public:
	MidiRecordModeParameterType();
	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
	void setValue(Action* action);
};

const char* MIDI_RECORD_MODE_NAMES[] = {
	"average", "smooth", "pulse", NULL
};

MidiRecordModeParameterType::MidiRecordModeParameterType() :
    GlobalParameter("midiRecordMode")
{
    // not worth bindable
	type = TYPE_ENUM;
	values = MIDI_RECORD_MODE_NAMES;
}

void MidiRecordModeParameterType::getValue(MobiusConfig* c, ExValue* value)
{
	value->setString(values[c->getMidiRecordMode()]);
}

void MidiRecordModeParameterType::setValue(MobiusConfig* c, ExValue* value)
{
    MidiRecordMode mode = (MidiRecordMode)getEnum(value);
    c->setMidiRecordMode(mode);
}

/**
 * Binding this is rare but we occasionally set this in test scripts.
 * For this to have any meaning, we also need to propagate it to the
 * Synchronizer which keeps a cached copy.  Also copy it to the interrupt
 * config just so they stay in sync though that isn't used.
 */
void MidiRecordModeParameterType::setValue(Action* action)
{
    MidiRecordMode mode = (MidiRecordMode)getEnum(&(action->arg));

    Mobius* m = (Mobius*)action->mobius;
    MobiusConfig* config = m->getConfiguration();
	config->setMidiRecordMode(mode);

    MobiusConfig* iconfig = m->getConfiguration();
    if (iconfig != NULL) {
        iconfig->setMidiRecordMode(mode);
        Synchronizer* sync = m->getSynchronizer();
        sync->updateConfiguration(iconfig);
    }
}

MidiRecordModeParameterType MidiRecordModeParameterTypeObj;
Parameter* MidiRecordModeParameter = &MidiRecordModeParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// Tracks
//
//////////////////////////////////////////////////////////////////////

class TracksParameterType : public GlobalParameter
{
  public:
	TracksParameterType();
	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
};

TracksParameterType::TracksParameterType() :
    GlobalParameter("tracks")
{
    // not bindable
	type = TYPE_INT;
    low = 1;
	high = MAX_TRACKS;
}

void TracksParameterType::getValue(MobiusConfig* c, ExValue* value)
{
    // note that this won't be accurate with the introduction of MIDI tracks,
    // but it would only be used by old scripts that can't deal with
    // midi tracks anyway
	value->setInt(c->getCoreTracks());
}

void TracksParameterType::setValue(MobiusConfig* c, ExValue* value)
{
    // can't set this at runtime, and scripts shouldn't try
    // not used for structure editing any more
    Trace(1, "TracksParameterType::setValue You should not be here");
	//c->setTracks(value->getInt());
}

TracksParameterType TracksParameterTypeObj;
Parameter* TracksParameter = &TracksParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// TrackGroups
//
//////////////////////////////////////////////////////////////////////

class TrackGroupsParameterType : public GlobalParameter
{
  public:
	TrackGroupsParameterType();
	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
};

TrackGroupsParameterType::TrackGroupsParameterType() :
    GlobalParameter("trackGroups")
{
    // not bindable
	type = TYPE_INT;
    high = 8;
}

void TrackGroupsParameterType::getValue(MobiusConfig* c, ExValue* value)
{
	value->setInt(c->getTrackGroups());
}

void TrackGroupsParameterType::setValue(MobiusConfig* c, ExValue* value)
{
	c->setTrackGroups(value->getInt());
}

TrackGroupsParameterType TrackGroupsParameterTypeObj;
Parameter* TrackGroupsParameter = &TrackGroupsParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// MaxLoops
//
//////////////////////////////////////////////////////////////////////

class MaxLoopsParameterType : public GlobalParameter
{
  public:
	MaxLoopsParameterType();
	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
};

MaxLoopsParameterType::MaxLoopsParameterType() :
    GlobalParameter("maxLoops")
{
    // not bindable
	type = TYPE_INT;
    high = 16;
}

void MaxLoopsParameterType::getValue(MobiusConfig* c, ExValue* value)
{
	value->setInt(c->getMaxLoops());
}

void MaxLoopsParameterType::setValue(MobiusConfig* c, ExValue* value)
{
	c->setMaxLoops(value->getInt());
}

MaxLoopsParameterType MaxLoopsParameterTypeObj;
Parameter* MaxLoopsParameter = &MaxLoopsParameterTypeObj;

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
    if (iconfig != NULL) {
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
    if (iconfig != NULL) {
        iconfig->setOutputLatency(latency);

        for (int i = 0 ; i < m->getTrackCount() ; i++) {
            Track* t = m->getTrack(i);
            t->updateGlobalParameters(iconfig);
        }
    }
}

OutputLatencyParameterType OutputLatencyParameterTypeObj;
Parameter* OutputLatencyParameter = &OutputLatencyParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// Edpisms
//
// This was not a Parameter in the most recent old code, but it still
// existed in MobiusConfig and was tested by InsertFunction and a few
// others.  Added a Parameter just to get some old tests in mutetests.mos
// working.  I doubt there are any users that need this so consider removing
// it again, but will need to adjust the test captures.
//
//////////////////////////////////////////////////////////////////////

class EdpismsParameterType : public GlobalParameter
{
  public:
	EdpismsParameterType();
	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
    void setValue(Action* action);
};

EdpismsParameterType::EdpismsParameterType() : 
    GlobalParameter("edpisms")
{
    // not worth bindable
	type = TYPE_BOOLEAN;
}

void EdpismsParameterType::getValue(MobiusConfig* c, ExValue* value)
{
	value->setBool(c->isEdpisms());
}

void EdpismsParameterType::setValue(MobiusConfig* c, ExValue* value)
{
	c->setEdpisms(value->getBool());
}

void EdpismsParameterType::setValue(Action* action)
{
    bool enable = action->arg.getBool();

    Mobius* m = (Mobius*)action->mobius;
    MobiusConfig* config = m->getConfiguration();
    config->setEdpisms(enable);
}

EdpismsParameterType EdpismsParameterTypeObj;
Parameter* EdpismsParameter = &EdpismsParameterTypeObj;

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
