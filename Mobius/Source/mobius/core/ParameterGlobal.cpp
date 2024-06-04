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

class SetupNameParameterType : public GlobalParameter
{
  public:
	SetupNameParameterType();

	void setValue(Action* action);
	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
    int getOrdinalValue(MobiusConfig* c);

	int getHigh(Mobius* m);
    void getOrdinalLabel(Mobius* m, int i, ExValue* value);
};

SetupNameParameterType::SetupNameParameterType() :
    // this must match the TargetSetup name
    GlobalParameter("setup")
{
	type = TYPE_STRING;
    bindable = true;
	dynamic = true;
}

int SetupNameParameterType::getOrdinalValue(MobiusConfig* c)
{
    Setup* setup = c->getStartingSetup();
    return setup->ordinal;
}

void SetupNameParameterType::getValue(MobiusConfig* c, ExValue* value)
{
    Setup* setup = c->getStartingSetup();
	value->setString(setup->getName());
}

/**
 * For scripts accept a name or a number.
 * Number is 1 based like SetupNumberParameter.
 */
void SetupNameParameterType::setValue(MobiusConfig* c, ExValue* value)
{
    Setup* setup = NULL;

    if (value->getType() == EX_INT)
      setup = c->getSetup(value->getInt());
    else 
      setup = c->getSetup(value->getString());

    // !! allocates memory
    if (setup != NULL)
      c->setStartingSetupName(setup->getName());
}

/**
 * For bindings, we not only update the config object, we also propagate
 * the change through the engine.
 * For scripts accept a name or a number.
 * Number is 1 based like SetupNumberParameter.
 *
 * This is one of the rare overloads to get the Action so we
 * can check the trigger.
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
            // Set the external one so that if you open the setup
            // window you'll see the one we're actually using selected.
            // in theory we could be cloning this config at the same time
            // while opening the setup window but worse case it just gets
            // the wrong selection.

            // !! allocates memory
            config->setStartingSetupName(setup->getName());

            // then set the one we're actually using internally
            m->setActiveSetup(setup->ordinal);
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

Parameter* SetupNameParameter = new SetupNameParameterType();

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

	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
	void setValue(Action* action);
};

Parameter* SetupNumberParameter = new SetupNumberParameterType();

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
        // we're always in the interrupt so can set it now
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

Parameter* TrackParameter = new TrackParameterType();

//////////////////////////////////////////////////////////////////////
//
// Bindings
//
//////////////////////////////////////////////////////////////////////

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
    int value = 0;

    Trace(1, "BindingsParameterType touched\n");
#if 0    
	BindingConfig* bindings = c->getOverlayBindingConfig();
    if (bindings != NULL) 
      value = bindings->getNumber();
#endif
    return value;
}

/**
 * This will return null to mean "no overlay".
 */
void BindingsParameterType::getValue(MobiusConfig* c, ExValue* value)
{
    (void)c;
    (void)value;
    Trace(1, "BindingsParameterType touched\n");
//	BindingConfig* bindings = c->getOverlayBindingConfig();
//    if (bindings != NULL) 
//	  value->setString(bindings->getName());
//	else
//      value->setNull();
}

void BindingsParameterType::setValue(MobiusConfig* c, ExValue* value)
{
    (void)c;
    (void)value;
    Trace(1, "BindingsParameterType touched\n");
//    if (value->getType() == EX_INT) {
//        // assume it's an int, these are numbered from zero but zero 
//        // is always the base binding
//        int index = value->getInt();
//        c->setOverlayBindingConfig(index);
//    }
//    else {
//        c->setOverlayBindingConfig(value->getString());
//    }
}

/**
 * Note that we call the setters on Mobius so it will also
 * update the configuration cache.
 *
 * This is one of the rare overloads to get to the Action
 * so we can have side effects on Mobius.
 */
void BindingsParameterType::setValue(Action* action)
{
    (void)action;
    Trace(1, "BindingsParameterType touched\n");
#if 0    
    Mobius* m = (Mobius*)action->mobius;
	MobiusConfig* config = m->getConfiguration();

    if (action->arg.isNull()) {
        // clear the overlay
        m->setOverlayBindings((BindingConfig*)NULL);
    }
    else if (action->arg.getType() == EX_STRING) {
        BindingConfig* b = config->getBindingConfig(action->arg.getString());
        // may be null to clear the overlay
        m->setOverlayBindings(b);
    }
    else {
        // assume it's an int, these are numbered from zero but zero 
        // is always the base binding
        BindingConfig* b = config->getBindingConfig(action->arg.getInt());
        m->setOverlayBindings(b);
    }
#endif    
}

/**
 * !! The max can change as bindings are added/removed.
 * Need to work out a way to convey that to ParameterEditor.
 */
int BindingsParameterType::getHigh(Mobius* m)
{
    (void)m;
    Trace(1, "BindingsParameterType touched\n");
#if 0    
    int max = 0;

	MobiusConfig* config = m->getConfiguration();
    max = config->getBindingConfigCount();
    // this is the number of configs, the max ordinal is zero based
    max--;

    return max;
#endif
    return 0;
}

/**
 * Given an ordinal, map it into a display label.
 */
void BindingsParameterType::getOrdinalLabel(Mobius* m,
                                                   int i, ExValue* value)
{
    (void)m;
    (void)i;
    (void)value;
    Trace(1, "BindingsParameterType touched\n");
#if 0    
	MobiusConfig* config = m->getConfiguration();
	BindingConfig* bindings = config->getBindingConfig(i);
    if (i == 0) {
        // This will be "Common Bindings" but we want to display
        // this as "No Overlay"
        value->setString("No Overlay");
    }
    else if (bindings != NULL)
	  value->setString(bindings->getName());
	else
	  value->setString("???");
#endif    
    value->setString("???");
}

Parameter* BindingsParameter = new BindingsParameterType();

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

Parameter* FadeFramesParameter = new FadeFramesParameterType();

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

Parameter* MaxSyncDriftParameter = new MaxSyncDriftParameterType();

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

Parameter* DriftCheckPointParameter = 
new DriftCheckPointParameterType();

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

Parameter* NoiseFloorParameter = new NoiseFloorParameterType();

//////////////////////////////////////////////////////////////////////
//
// PluginPorts
//
//////////////////////////////////////////////////////////////////////

class PluginPortsParameterType : public GlobalParameter
{
  public:
	PluginPortsParameterType();
	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
};

PluginPortsParameterType::PluginPortsParameterType() :
    GlobalParameter("pluginPorts")
{
    // not worth bindable
	type = TYPE_INT;
    low = 1;
	high = 8;
}

void PluginPortsParameterType::getValue(MobiusConfig* c, ExValue* value)
{
	value->setInt(c->getPluginPorts());
}

void PluginPortsParameterType::setValue(MobiusConfig* c, ExValue* value)
{
	c->setPluginPorts(value->getInt());
}

Parameter* PluginPortsParameter = new PluginPortsParameterType();

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

Parameter* MidiExportParameter = new MidiExportParameterType();

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

Parameter* HostMidiExportParameter = new HostMidiExportParameterType();

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

Parameter* LongPressParameter = new LongPressParameterType();

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

Parameter* SpreadRangeParameter = new SpreadRangeParameterType();

//////////////////////////////////////////////////////////////////////
//
// TraceDebugLevel
//
// Moved up to the UI
//
//////////////////////////////////////////////////////////////////////
#if 0
// Unlike TracePrintLevel this was marginally useful to have as a parameter
// but when would you ever need to set this in a script?
// left as Parameters for MobiusConfig editing, but these will probably
// be moved to UIConfig at some point and edited more directly
// so we won't need these

class TraceDebugLevelParameterType : public GlobalParameter
{
  public:
	TraceDebugLevelParameterType();
	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
	void setValue(Action* action);
};

TraceDebugLevelParameterType::TraceDebugLevelParameterType() :
    GlobalParameter("traceDebugLevel")
{
    // not worth bindable
	type = TYPE_INT;
	high = 4;
}

void TraceDebugLevelParameterType::getValue(MobiusConfig* c, ExValue* value)
{
	value->setInt(c->getTraceDebugLevel());
}
void TraceDebugLevelParameterType::setValue(MobiusConfig* c, ExValue* value)
{
    c->setTraceDebugLevel(value->getInt());
}

/**
 * Binding is rare but we can set this in a test script.
 * For this to have meaning we need to propagate to the Trace 
 * global variables.
 */
void TraceDebugLevelParameterType::setValue(Action* action)
{
    int level = action->arg.getInt();

	MobiusConfig* config = action->mobius->getConfiguration();
	config->setTraceDebugLevel(level);

    // I'm disabling this while we work out how we want to congiure
    // trace, it is being forced on by Supervisor which is enough
    // so don't let old values lingering in MobiusConfig override that
    //TraceDebugLevel = level;
}

Parameter* TraceDebugLevelParameter = new TraceDebugLevelParameterType();

//////////////////////////////////////////////////////////////////////
//
// TracePrintLevel
//
//////////////////////////////////////////////////////////////////////

// this was useless, and adds clutter, take it out

class TracePrintLevelParameterType : public GlobalParameter
{
  public:
	TracePrintLevelParameterType();
	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
	void setValue(Action* action);
};

TracePrintLevelParameterType::TracePrintLevelParameterType() :
    GlobalParameter("tracePrintLevel")
{
    // not worth bindable
	type = TYPE_INT;
	high = 4;
}

void TracePrintLevelParameterType::getValue(MobiusConfig* c, ExValue* value)
{
	value->setInt(c->getTracePrintLevel());
}
void TracePrintLevelParameterType::setValue(MobiusConfig* c, ExValue* value)
{
    c->setTracePrintLevel(value->getInt());
}

/**
 * Binding is rare but we can set this in a test script.
 * For this to have meaning we need to propagate to the Trace 
 * global variables.
 */
void TracePrintLevelParameterType::setValue(Action* action)
{
    int level = action->arg.getInt();

	MobiusConfig* config = action->mobius->getConfiguration();
	config->setTracePrintLevel(level);

    // I'm disabling this while we work out how we want to congiure
    // trace, it is being forced on by Supervisor which is enough
    // so don't let old values lingering in MobiusConfig override that
    //TracePrintLevel = level;
}

Parameter* TracePrintLevelParameter = new TracePrintLevelParameterType();
#endif

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

Parameter* CustomModeParameter = new CustomModeParameterType();

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

Parameter* AutoFeedbackReductionParameter = new AutoFeedbackReductionParameterType();

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

Parameter* IsolateOverdubsParameter = new IsolateOverdubsParameterType();

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

Parameter* MonitorAudioParameter = new MonitorAudioParameterType();

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

Parameter* SaveLayersParameter = new SaveLayersParameterType();

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

Parameter* QuickSaveParameter = new QuickSaveParameterType();

//////////////////////////////////////////////////////////////////////
//
// UnitTests
//
//////////////////////////////////////////////////////////////////////
#if 0
class UnitTestsParameterType : public GlobalParameter
{
  public:
	UnitTestsParameterType();
	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
};

UnitTestsParameterType::UnitTestsParameterType() :
    GlobalParameter("unitTests")
{
    // not bindable
	type = TYPE_STRING;
}

void UnitTestsParameterType::getValue(MobiusConfig* c, ExValue* value)
{
	value->setString(c->getUnitTests());
}

void UnitTestsParameterType::setValue(MobiusConfig* c, ExValue* value)
{
	c->setUnitTests(value->getString());
}

Parameter* UnitTestsParameter = new UnitTestsParameterType();
#endif

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

Parameter* IntegerWaveFileParameter = new IntegerWaveFileParameterType();

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

Parameter* AltFeedbackDisableParameter = new AltFeedbackDisableParameterType();

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

Parameter* GroupFocusLockParameter = new GroupFocusLockParameterType();

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

Parameter* FocusLockFunctionsParameter = 
new FocusLockFunctionsParameterType();

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
 */
void MuteCancelFunctionsParameterType::setValue(Action* action)
{
    // don't bother propagating to the interrupt
    Mobius* m = (Mobius*)action->mobius;
	MobiusConfig* config = m->getConfiguration();
	if (action->arg.isNull())
	  config->setMuteCancelFunctions(NULL);
	else
	  config->setMuteCancelFunctions(new StringList(action->arg.getString()));

    // this one is unusual because we don't look at the parameter value
    // we look at flags left on the Functions
    m->refreshFunctionPreferences();
}

Parameter* MuteCancelFunctionsParameter = 
new MuteCancelFunctionsParameterType();

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
 */
void ConfirmationFunctionsParameterType::setValue(Action* action)
{
    // don't bother propagating to the interrupt
    Mobius* m = (Mobius*)action->mobius;
	MobiusConfig* config = m->getConfiguration();
	if (action->arg.isNull())
	  config->setConfirmationFunctions(NULL);
	else
	  config->setConfirmationFunctions(new StringList(action->arg.getString()));

    // this one is unusual because we don't look at the parameter value
    // we look at flags left on the Functions
    m->refreshFunctionPreferences();
}

Parameter* ConfirmationFunctionsParameter = 
new ConfirmationFunctionsParameterType();

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

Parameter* MidiRecordModeParameter = new MidiRecordModeParameterType();

//////////////////////////////////////////////////////////////////////
//
// DualPluginWindow
//
//////////////////////////////////////////////////////////////////////
#if 0
class DualPluginWindowParameterType : public GlobalParameter
{
  public:
	DualPluginWindowParameterType();
	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
};

DualPluginWindowParameterType::DualPluginWindowParameterType() :
    GlobalParameter("dualPluginWindow")
{
    // not worth bindable
	type = TYPE_BOOLEAN;
}

void DualPluginWindowParameterType::getValue(MobiusConfig* c, ExValue* value)
{
	value->setBool(c->isDualPluginWindow());
}

void DualPluginWindowParameterType::setValue(MobiusConfig* c, ExValue* value)
{
	c->setDualPluginWindow(value->getBool());
}

Parameter* DualPluginWindowParameter = new DualPluginWindowParameterType();
#endif

//////////////////////////////////////////////////////////////////////
//
// CustomMessageFile
//
//////////////////////////////////////////////////////////////////////

#if 0
class CustomMessageFileParameterType : public GlobalParameter
{
  public:
	CustomMessageFileParameterType();
	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
};

CustomMessageFileParameterType::CustomMessageFileParameterType() :
    GlobalParameter("customMessageFile")
{
    // not bindable
	type = TYPE_STRING;
}

void CustomMessageFileParameterType::getValue(MobiusConfig* c, ExValue* value)
{
	value->setString(c->getCustomMessageFile());
}

void CustomMessageFileParameterType::setValue(MobiusConfig* c, ExValue* value)
{
	c->setCustomMessageFile(value->getString());
}

Parameter* CustomMessageFileParameter = new CustomMessageFileParameterType();
#endif

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
	value->setInt(c->getTracks());
}

void TracksParameterType::setValue(MobiusConfig* c, ExValue* value)
{
	c->setTracks(value->getInt());
}

Parameter* TracksParameter = new TracksParameterType();

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

Parameter* TrackGroupsParameter = new TrackGroupsParameterType();

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

Parameter* MaxLoopsParameter = new MaxLoopsParameterType();

/****************************************************************************
 *                                                                          *
 *                                    OSC                                   *
 *                                                                          *
 ****************************************************************************/
#if 0

//////////////////////////////////////////////////////////////////////
//
// OscInputPort
//
//////////////////////////////////////////////////////////////////////

class OscInputPortParameterType : public GlobalParameter
{
  public:
	OscInputPortParameterType();
	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
};

OscInputPortParameterType::OscInputPortParameterType() :
    GlobalParameter("oscInputPort")
{
    // not bindable
	type = TYPE_INT;
}

void OscInputPortParameterType::getValue(MobiusConfig* c, ExValue* value)
{
	value->setInt(c->getOscInputPort());
}

void OscInputPortParameterType::setValue(MobiusConfig* c, ExValue* value)
{
	c->setOscInputPort(value->getInt());
}

Parameter* OscInputPortParameter = new OscInputPortParameterType();

//////////////////////////////////////////////////////////////////////
//
// OscOutputPort
//
//////////////////////////////////////////////////////////////////////

class OscOutputPortParameterType : public GlobalParameter
{
  public:
	OscOutputPortParameterType();
	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
};

OscOutputPortParameterType::OscOutputPortParameterType() :
    GlobalParameter("oscOutputPort")
{
    // not bindable
	type = TYPE_INT;
}

void OscOutputPortParameterType::getValue(MobiusConfig* c, ExValue* value)
{
	value->setInt(c->getOscOutputPort());
}

void OscOutputPortParameterType::setValue(MobiusConfig* c, ExValue* value)
{
	c->setOscOutputPort(value->getInt());
}

Parameter* OscOutputPortParameter = new OscOutputPortParameterType();

//////////////////////////////////////////////////////////////////////
//
// OscOutputHost
//
//////////////////////////////////////////////////////////////////////

class OscOutputHostParameterType : public GlobalParameter
{
  public:
	OscOutputHostParameterType();
	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
};

OscOutputHostParameterType::OscOutputHostParameterType() :
    GlobalParameter("oscOutputHost")
{
    // not bindable
	type = TYPE_STRING;
}

void OscOutputHostParameterType::getValue(MobiusConfig* c, ExValue* value)
{
	value->setString(c->getOscOutputHost());
}

void OscOutputHostParameterType::setValue(MobiusConfig* c, ExValue* value)
{
	c->setOscOutputHost(value->getString());
}

Parameter* OscOutputHostParameter = new OscOutputHostParameterType();

//////////////////////////////////////////////////////////////////////
//
// OscTrace
//
//////////////////////////////////////////////////////////////////////

class OscTraceParameterType : public GlobalParameter
{
  public:
	OscTraceParameterType();
	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
};

OscTraceParameterType::OscTraceParameterType() :
    GlobalParameter("oscTrace")
{
    // not worth bindable
	type = TYPE_BOOLEAN;
}

void OscTraceParameterType::getValue(MobiusConfig* c, ExValue* value)
{
	value->setBool(c->isOscTrace());
}

void OscTraceParameterType::setValue(MobiusConfig* c, ExValue* value)
{
	c->setOscTrace(value->getBool());
}

Parameter* OscTraceParameter = new OscTraceParameterType();

//////////////////////////////////////////////////////////////////////
//
// OscEnable
//
//////////////////////////////////////////////////////////////////////

class OscEnableParameterType : public GlobalParameter
{
  public:
	OscEnableParameterType();
	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
};

OscEnableParameterType::OscEnableParameterType() :
    GlobalParameter("oscEnable")
{
    // not worth bindable
	type = TYPE_BOOLEAN;
}

void OscEnableParameterType::getValue(MobiusConfig* c, ExValue* value)
{
	value->setBool(c->isOscEnable());
}

void OscEnableParameterType::setValue(MobiusConfig* c, ExValue* value)
{
	c->setOscEnable(value->getBool());
}

Parameter* OscEnableParameter = new OscEnableParameterType();
#endif

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

Parameter* InputLatencyParameter = new InputLatencyParameterType();

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

Parameter* OutputLatencyParameter = new OutputLatencyParameterType();


// **********************************************************************
//
// Devices are no longer defined in MobiusConfig
//
// **********************************************************************

#if 0

//////////////////////////////////////////////////////////////////////
//
// MidiInput
//
//////////////////////////////////////////////////////////////////////

class MidiInputParameterType : public GlobalParameter
{
  public:
	MidiInputParameterType();
	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
};

MidiInputParameterType::MidiInputParameterType() :
    GlobalParameter("midiInput")
{
    // not bindable
	type = TYPE_STRING;
}

void MidiInputParameterType::getValue(MobiusConfig* c, ExValue* value)
{
	value->setString(c->getMidiInput());
}

void MidiInputParameterType::setValue(MobiusConfig* c, ExValue* value)
{
	c->setMidiInput(value->getString());
}

Parameter* MidiInputParameter = new MidiInputParameterType();

//////////////////////////////////////////////////////////////////////
//
// MidiOutput
//
//////////////////////////////////////////////////////////////////////

class MidiOutputParameterType : public GlobalParameter
{
  public:
	MidiOutputParameterType();
	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
};

MidiOutputParameterType::MidiOutputParameterType() :
    GlobalParameter("midiOutput")
{
    // not bindable
	type = TYPE_STRING;
}

void MidiOutputParameterType::getValue(MobiusConfig* c, ExValue* value)
{
	value->setString(c->getMidiOutput());
}

void MidiOutputParameterType::setValue(MobiusConfig* c, ExValue* value)
{
	c->setMidiOutput(value->getString());
}

Parameter* MidiOutputParameter = new MidiOutputParameterType();

//////////////////////////////////////////////////////////////////////
//
// MidiThrough
//
//////////////////////////////////////////////////////////////////////

class MidiThroughParameterType : public GlobalParameter
{
  public:
	MidiThroughParameterType();
	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
};

MidiThroughParameterType::MidiThroughParameterType() :
    GlobalParameter("midiThrough")
{
    // not bindable
	type = TYPE_STRING;
}

void MidiThroughParameterType::getValue(MobiusConfig* c, ExValue* value)
{
	value->setString(c->getMidiThrough());
}

void MidiThroughParameterType::setValue(MobiusConfig* c, ExValue* value)
{
	c->setMidiThrough(value->getString());
}

Parameter* MidiThroughParameter = new MidiThroughParameterType();

//////////////////////////////////////////////////////////////////////
//
// PluginMidiInput
//
//////////////////////////////////////////////////////////////////////

class PluginMidiInputParameterType : public GlobalParameter
{
  public:
	PluginMidiInputParameterType();
	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
};

PluginMidiInputParameterType::PluginMidiInputParameterType() :
    GlobalParameter("pluginMidiInput")
{
    // not bindable
	type = TYPE_STRING;
    addAlias("vstMidiInput");
}

void PluginMidiInputParameterType::getValue(MobiusConfig* c, ExValue* value)
{
	value->setString(c->getPluginMidiInput());
}

void PluginMidiInputParameterType::setValue(MobiusConfig* c, ExValue* value)
{
	c->setPluginMidiInput(value->getString());
}

Parameter* PluginMidiInputParameter = new PluginMidiInputParameterType();

//////////////////////////////////////////////////////////////////////
//
// PluginMidiOutput
//
//////////////////////////////////////////////////////////////////////

class PluginMidiOutputParameterType : public GlobalParameter
{
  public:
	PluginMidiOutputParameterType();
	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
};

PluginMidiOutputParameterType::PluginMidiOutputParameterType() :
    GlobalParameter("pluginMidiOutput")
{
    // not bindable
	type = TYPE_STRING;
	addAlias("vstMidiOutput");
}

void PluginMidiOutputParameterType::getValue(MobiusConfig* c, ExValue* value)
{
	value->setString(c->getPluginMidiOutput());
}

void PluginMidiOutputParameterType::setValue(MobiusConfig* c, ExValue* value)
{
	c->setPluginMidiOutput(value->getString());
}

Parameter* PluginMidiOutputParameter = new PluginMidiOutputParameterType();

//////////////////////////////////////////////////////////////////////
//
// PluginMidiThrough
//
//////////////////////////////////////////////////////////////////////

class PluginMidiThroughParameterType : public GlobalParameter
{
  public:
	PluginMidiThroughParameterType();
	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
};

PluginMidiThroughParameterType::PluginMidiThroughParameterType() :
    GlobalParameter("pluginMidiThrough")
{
    // not bindable
	type = TYPE_STRING;
	addAlias("vstMidiThrough");
}

void PluginMidiThroughParameterType::getValue(MobiusConfig* c, ExValue* value)
{
	value->setString(c->getPluginMidiThrough());
}

void PluginMidiThroughParameterType::setValue(MobiusConfig* c, ExValue* value)
{
	c->setPluginMidiThrough(value->getString());
}

Parameter* PluginMidiThroughParameter = new PluginMidiThroughParameterType();

//////////////////////////////////////////////////////////////////////
//
// AudioInput
//
//////////////////////////////////////////////////////////////////////

class AudioInputParameterType : public GlobalParameter
{
  public:
	AudioInputParameterType();
	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
};

AudioInputParameterType::AudioInputParameterType() :
    GlobalParameter("audioInput")
{
    // not bindable
	type = TYPE_STRING;
}

void AudioInputParameterType::getValue(MobiusConfig* c, ExValue* value)
{
	value->setString(c->getAudioInput());
}

void AudioInputParameterType::setValue(MobiusConfig* c, ExValue* value)
{
	c->setAudioInput(value->getString());
}

Parameter* AudioInputParameter = new AudioInputParameterType();

//////////////////////////////////////////////////////////////////////
//
// AudioOutput
//
//////////////////////////////////////////////////////////////////////

class AudioOutputParameterType : public GlobalParameter
{
  public:
	AudioOutputParameterType();
	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
};

AudioOutputParameterType::AudioOutputParameterType() :
    GlobalParameter("audioOutput")
{
    // not bindable
	type = TYPE_STRING;
}

void AudioOutputParameterType::getValue(MobiusConfig* c, ExValue* value)
{
	value->setString(c->getAudioOutput());
}

void AudioOutputParameterType::setValue(MobiusConfig* c, ExValue* value)
{
	c->setAudioOutput(value->getString());
}

Parameter* AudioOutputParameter = new AudioOutputParameterType();

//////////////////////////////////////////////////////////////////////
//
// SampleRate
//
//////////////////////////////////////////////////////////////////////

class SampleRateParameterType : public GlobalParameter
{
  public:
	SampleRateParameterType();
	void getValue(MobiusConfig* c, ExValue* value);
	void setValue(MobiusConfig* c, ExValue* value);
};

const char* SAMPLE_RATE_NAMES[] = {
	"44100", "48000", NULL
};

/**
 * Could be a int, but an enum helps is constrain the value better.
 */
SampleRateParameterType::SampleRateParameterType() :
    GlobalParameter("sampleRate")
{
    // not worth bindable
	type = TYPE_ENUM;
	values = SAMPLE_RATE_NAMES;
}

void SampleRateParameterType::getValue(MobiusConfig* c, ExValue* value)
{
	value->setString(values[c->getSampleRate()]);
}

void SampleRateParameterType::setValue(MobiusConfig* c, ExValue* value)
{
    AudioSampleRate rate = (AudioSampleRate)getEnum(value);
	c->setSampleRate(rate);
}

Parameter* SampleRateParameter = new SampleRateParameterType();

#endif

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

Parameter* EdpismsParameter = new EdpismsParameterType();

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
