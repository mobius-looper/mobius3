/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Static object definitions for Mobius parameters.
 *
 * There are four parameter levels:
 *
 *    Global - usually in MobiusConfig
 *    Setup  - in Setup
 *    Track  - in SetupTrack or Track
 *    Preset - in Preset
 *
 */

#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <ctype.h>

#include "../../util/Util.h"
#include "../../util/List.h"

#include "Action.h"
#include "../Audio.h"
#include "Export.h"
#include "Function.h"
#include "Mobius.h"
#include "Mode.h"
#include "Project.h"
#include "Track.h"
#include "Script.h"
#include "Synchronizer.h"

#include "Parameter.h"
#include "Mem.h"

/****************************************************************************
 *                                                                          *
 *   							  PARAMETER                                 *
 *                                                                          *
 ****************************************************************************/

// Shared text for boolean values

const char* BOOLEAN_VALUE_NAMES[] = {
	"off", "on", nullptr
};

const char* BOOLEAN_VALUE_LABELS[] = {
	nullptr, nullptr, nullptr
};

Parameter::Parameter()
{
    init();
}

Parameter::Parameter(const char* name) :
    SystemConstant(name)
{
    init();
}

void Parameter::init()
{
	bindable = false;
	dynamic = false;
    deprecated = false;
    transient = false;
    resettable = false;
    scheduled = false;
    takesAction = false;
    control = false;

	type = TYPE_INT;
	scope = PARAM_SCOPE_NONE;
	low = 0;
	high = 0;
    zeroCenter = false;
    mDefault = 0;

	values = nullptr;
	valueLabels = nullptr;
    xmlAlias = nullptr;

	for (int i = 0 ; i < MAX_PARAMETER_ALIAS ; i++)
	  aliases[i] = nullptr;
}

Parameter::~Parameter()
{
}

void Parameter::addAlias(const char* alias) 
{
    bool added = false;

	for (int i = 0 ; i < MAX_PARAMETER_ALIAS ; i++) {
        if (aliases[i] == nullptr) {
            aliases[i] = alias;
            added = true;
            break;
        }
    }

    if (!added)
      Trace(1, "Alias overflow: %s\n", alias);
}

/**
 * Must be overloaded in the subclass.
 */
void Parameter::getObjectValue(void* object, ExValue* value)
{
    (void)object;
    (void)value;
    Trace(1, "Parameter %s: getObjectValue not overloaded!\n",
          getName());
}

/**
 * Must be overloaded in the subclass.
 */
void Parameter::setObjectValue(void* object, ExValue* value)
{
    (void)object;
    (void)value;
    Trace(1, "Parameter %s: setObjectValue not overloaded!\n",
          getName());
}

void Parameter::getValue(Export* exp, ExValue* value)
{
    (void)exp;
    Trace(1, "Parameter %s: getValue not overloaded!\n",
          getName());
	value->setString("");
}

int Parameter::getOrdinalValue(Export* exp)
{
    (void)exp;
    Trace(1, "Parameter %s: getOrdinalValue not overloaded! \n",
          getName());
    return -1;
}

void Parameter::setValue(Action* action)
{
    (void)action;
    Trace(1, "Parameter %s: setValue not overloaded!\n",
          getName());
}

/**
 * Allocate a label array and fill it with nulls.
 * wtf did this do?
 */
#if 0
const char** Parameter::allocLabelArray(int size)
{
	int fullsize = size + 1; // leave a null terminator
	const char** labels = new const char*[fullsize];
    MemTrack(labels, "Parameter:allocLabelArray", fullsize * sizeof(char*));
	for (int i = 0 ; i < fullsize ; i++)
	  labels[i] = nullptr;

	return labels;
}
#endif

//////////////////////////////////////////////////////////////////////
//
// Default Ordinal mapping for the UI
// A few classes overload these if they don't have a fixed enumeration.
//
//////////////////////////////////////////////////////////////////////

int Parameter::getLow()
{
    return low;
}

int Parameter::getHigh(Mobius* m)
{
    (void)m;
    int max = high;

    if (type == TYPE_BOOLEAN) {
        max = 1;
    }
    else if (values != nullptr) {
        for ( ; values[max] != nullptr ; max++);
        max--;
    }

    return max;
}

int Parameter::getBindingHigh(Mobius* m)
{
    int max = getHigh(m);

    // if an int doesn't have a max, give it something so we can
    // have a reasonable upper bound for CC scaling
    if (type == TYPE_INT && max == 0)
      max = 127;

    return max;
}

/**
 * Given an ordinal, map it into a display label.
 */
void Parameter::getOrdinalLabel(Mobius* m, 
                                       int i, ExValue* value)
{
    (void)m;
	if (valueLabels != nullptr) {
		value->setString(valueLabels[i]);
	}
	else if (type == TYPE_INT) {
		value->setInt(i);
	}
    else if (type == TYPE_BOOLEAN) {
		value->setString(BOOLEAN_VALUE_LABELS[i]);
	}
    else 
	  value->setInt(i);
}

void Parameter::getDisplayValue(Mobius* m, ExValue* value)
{
    (void)m;
    // weird function used in just a few places by
    // things that overload getOrdinalLabel
    value->setNull();
}

//////////////////////////////////////////////////////////////////////
//
// coersion utilities
//
//////////////////////////////////////////////////////////////////////

/**
 * Convert a string value to an enumeration ordinal value.
 * This is the one used by most of the code, if the name doesn't match
 * it traces a warning message and returns the first value.
 */
int Parameter::getEnum(const char *value)
{
	int ivalue = getEnumValue(value);

    // if we couldn't find a match, pick the first one
    // !! instead we should leave it at the current value?
    if (ivalue < 0) {
        Trace(1, "ERROR: Invalid value for parameter %s: %s\n",
              getName(), value);
        ivalue = 0;
    }

	return ivalue;
}

/**
 * Convert a string value to an enumeration ordinal value if possible, 
 * return -1 if invalid.  This is like getEnum() but used in cases
 * where the enum is an optional script arg and we need to know
 * whether it really matched or not.
 */
int Parameter::getEnumValue(const char *value)
{
	int ivalue = -1;

	if (value != nullptr) {

		for (int i = 0 ; values[i] != nullptr ; i++) {
			if (StringEqualNoCase(values[i], value)) {
				ivalue = i;
				break;
			}
		}
        
        if (ivalue < 0) {
            // Try again with prefix matching, it is convenient
            // to allow common abbreviations like "quantize" rather 
            // than "quantized" or "all" rather than "always".  It
            // might be safe to do this all the time but we'd have to 
            // carefully go through all the enums to make sure
            // there are no ambiguities.
            for (int i = 0 ; values[i] != nullptr ; i++) {
                if (StartsWithNoCase(values[i], value)) {
                    ivalue = i;
                    break;
                }
            }
        }
	}

	return ivalue;
}

/**
 * Check for an enumeration value that has been changed and convert
 * the old name from the XML or script into the new name.
 */
void Parameter::fixEnum(ExValue* value, const char* oldName, 
                               const char* newName)
{
	if (value->getType() == EX_STRING) {
        const char* current = value->getString();
        if (StringEqualNoCase(oldName, current))
          value->setString(newName);
    }
}

/**
 * Convert a Continuous Controller number in the range of 0-127
 * to an enumerated value.
 * !! this isn't used any more, if we're going to do scaling
 * it needs to be done in a way appropriate for the binding.
 */
int Parameter::getControllerEnum(int value)
{
	int ivalue = 0;

	if (value >= 0 && value < 128) {
		int max = 0;
		for (max = 0 ; values[max] != nullptr ; max++);

		int unit = 128 / max;
		ivalue = value / unit;
	}

	return ivalue;
}

/**
 * Coerce an ExValue into an enumeration ordinal.
 * This must NOT scale, it is used in parameter setters
 * and must be symetrical with getOrdinalValue.
 */
int Parameter::getEnum(ExValue *value)
{
	int ivalue = 0;

	if (value->getType() == EX_STRING) {
		// map it through the value table
        ivalue = getEnum(value->getString());
	}
	else {
		// assume it is an ordinal value, but check the range
		// clamp it between 0 and max
		int i = value->getInt();
		if (i >= 0) {
			int max = 0;
			if (values != nullptr)
			  for (max = 0 ; values[max] != nullptr ; max++);

			if (i < max)
			  ivalue = i;
			else
			  ivalue = max;
		}
	}
	return ivalue;
}

//////////////////////////////////////////////////////////////////////
//
// Parameter Search
//
//////////////////////////////////////////////////////////////////////

Parameter* Parameter::getParameter(Parameter** group, 
										  const char* name)
{
	Parameter* found = nullptr;
	
	for (int i = 0 ; group[i] != nullptr && found == nullptr ; i++) {
		Parameter* p = group[i];
		if (StringEqualNoCase(p->getName(), name))
		  found = p;
	}

	if (found == nullptr) {
		// not a name match, try aliases
		for (int i = 0 ; group[i] != nullptr && found == nullptr ; i++) {
			Parameter* p = group[i];
			for (int j = 0 ; 
				 j < MAX_PARAMETER_ALIAS && p->aliases[j] != nullptr ; 
				 j++) {
				if (StringEqualNoCase(p->aliases[j], name)) {
					found = p;
					break;
				}
			}
		}
	}

	return found;
}

Parameter* Parameter::getParameterWithDisplayName(Parameter** group,
														 const char* name)
{
	Parameter* found = nullptr;
	
	for (int i = 0 ; group[i] != nullptr ; i++) {
		Parameter* p = group[i];
		if (StringEqualNoCase(p->getDisplayName(), name)) {
			found = p;
			break;	
		}
	}
	return found;
}

Parameter* Parameter::getParameter(const char* name)
{
	return getParameter(Parameters, name);
}

Parameter* Parameter::getParameterWithDisplayName(const char* name)
{
	return getParameterWithDisplayName(Parameters, name);
}

/****************************************************************************
 *                                                                          *
 *                             SCRIPT PARAMETERS                            *
 *                                                                          *
 ****************************************************************************/

/****************************************************************************
 *                                                                          *
 *                               PARAMETER LIST                             *
 *                                                                          *
 ****************************************************************************/
/*
 * Can't use a simple static initializer for the Parameters array any more
 * now that they've been broken up into several files.  Have to build
 * the array at runtime.
 */

#define MAX_STATIC_PARAMETERS 256

Parameter* Parameters[MAX_STATIC_PARAMETERS];
int ParameterIndex = 0;

void add(Parameter* p)
{
	if (ParameterIndex >= MAX_STATIC_PARAMETERS - 1) {
		printf("Parameter array overflow!\n");
		fflush(stdout);
	}
	else {
		Parameters[ParameterIndex++] = p;
		// keep it nullptr terminated
		Parameters[ParameterIndex] = nullptr;
	}
}

/**
 * Called early during Mobius initializations to initialize the 
 * static parameter arrays.  Had to start doing this after splitting
 * the parameters out into several files, they are no longer accessible
 * with static initializers.
 */
void Parameter::initParameters()
{
    // ignore if already initialized
    if (ParameterIndex == 0) {

        // Preset
        add(AltFeedbackEnableParameter);
        add(BounceQuantizeParameter);
        add(EmptyLoopActionParameter);
        add(EmptyTrackActionParameter);
        add(LoopCountParameter);
        add(MaxRedoParameter);
        add(MaxUndoParameter);
        add(MultiplyModeParameter);
        add(MuteCancelParameter);
        add(MuteModeParameter);
        add(NoFeedbackUndoParameter);
        add(NoLayerFlatteningParameter);
        add(OverdubQuantizedParameter);
        add(OverdubTransferParameter);
        add(PitchBendRangeParameter);
        add(PitchSequenceParameter);
        add(PitchShiftRestartParameter);
        add(PitchStepRangeParameter);
        add(PitchTransferParameter);
        add(QuantizeParameter);
        add(SpeedBendRangeParameter);
        add(SpeedRecordParameter);
        add(SpeedSequenceParameter);
        add(SpeedShiftRestartParameter);
        add(SpeedStepRangeParameter);
        add(SpeedTransferParameter);
        add(TimeStretchRangeParameter);
        add(RecordResetsFeedbackParameter);
        add(RecordTransferParameter);
        add(ReturnLocationParameter);
        add(ReverseTransferParameter);
        add(RoundingOverdubParameter);
        add(ShuffleModeParameter);
        add(SlipModeParameter);
        add(SlipTimeParameter);
        add(SoundCopyParameter);
        add(SubCycleParameter);
        add(SwitchDurationParameter);
        add(SwitchLocationParameter);
        add(SwitchQuantizeParameter);
        add(SwitchVelocityParameter);
        add(TimeCopyParameter);
        add(TrackLeaveActionParameter);
        add(WindowEdgeAmountParameter);
        add(WindowEdgeUnitParameter);
        add(WindowSlideAmountParameter);
        add(WindowSlideUnitParameter);

        // Deprecated

        //add(AutoRecordParameter);
        //add(InsertModeParameter);
        //add(InterfaceModeParameter);
        //add(LoopCopyParameter);
        //add(OverdubModeParameter);
        //add(RecordModeParameter);
        //add(SamplerStyleParameter);
        //add(TrackCopyParameter);

        // Track

        add(AltFeedbackLevelParameter);
        add(AudioInputPortParameter);
        add(AudioOutputPortParameter);
        add(FeedbackLevelParameter);
        add(FocusParameter);
        add(GroupParameter);
        add(InputLevelParameter);
        add(InputPortParameter);
        add(MonoParameter);
        add(OutputLevelParameter);
        add(OutputPortParameter);
        add(PanParameter);
        add(PluginInputPortParameter);
        add(PluginOutputPortParameter);

        add(SpeedOctaveParameter);
        add(SpeedBendParameter);
        add(SpeedStepParameter);

        add(PitchOctaveParameter);
        add(PitchBendParameter);
        add(PitchStepParameter);

        add(TimeStretchParameter);

        add(TrackNameParameter);
        add(TrackPresetParameter);
        add(TrackPresetNumberParameter);
        add(TrackSyncUnitParameter);
        add(SyncSourceParameter);

        // Setup

        //add(BeatsPerBarParameter);
        add(DefaultSyncSourceParameter);
        add(DefaultTrackSyncUnitParameter);
        //add(ManualStartParameter);
        //add(MaxTempoParameter);
        //add(MinTempoParameter);
        add(MuteSyncModeParameter);
        add(RealignTimeParameter);
        add(ResizeSyncAdjustParameter);
        add(SlaveSyncUnitParameter);
        add(SpeedSyncAdjustParameter);

        // Global

        add(InputLatencyParameter);
        add(OutputLatencyParameter);
        add(SetupNameParameter);
        add(TrackParameter);

        // sanity check on scopes since they're critical
        for (int i = 0 ; Parameters[i] != nullptr ; i++) {
            if (Parameters[i]->scope == PARAM_SCOPE_NONE) {
                Trace(1, "Parameter %s has no scope!\n", 
                      Parameters[i]->getName());
            }
        }
    }
}

/**
 * Like MobiusMode and Function, release the dynamically
 * allocated Parameter objects on shutdown.
 *
 * update: not no more!
 * these and all other constant objects should now be statically initialized
 * with the stupid object-and-pointer-to-it pair until we can load these
 * dynamically from a file.
 */
void Parameter::deleteParameters()
{
#if 0    
	for (int i = 0 ; Parameters[i] != nullptr ; i++) {
        Parameter* p = Parameters[i];

        Trace(2, "Delete %s\n", p->getName());
        //delete p;
    }
#endif    
    // important to "clear" the array since this can be called
    // more than once during shutdown
    // is this still relevant now that we don't dynamically allocate them?
    Parameters[0] = nullptr;
}

void Parameter::checkAmbiguousNames()
{
	int i;

	for (i = 0 ; Parameters[i] != nullptr ; i++) {
        Parameter* p = Parameters[i];
        const char** values = p->values;
        if (values != nullptr) {
            for (int j = 0 ; values[j] != nullptr ; j++) {
                Parameter* other = getParameter(values[j]);
                if (other != nullptr) {
                    printf("WARNING: Ambiguous parameter name/value %s\n", values[j]);
					fflush(stdout);
                }
            }
        }
    }
}

void Parameter::dumpFlags()
{
    int i;

    printf("*** Bindable ***\n");
	for (i = 0 ; Parameters[i] != nullptr ; i++) {
        Parameter* p = Parameters[i];
        if (p->bindable)
          printf("%s\n", p->getName());
    }

    printf("*** Hidden ***\n");
	for (i = 0 ; Parameters[i] != nullptr ; i++) {
        Parameter* p = Parameters[i];
        if (!p->bindable)
          printf("%s\n", p->getName());
    }

    printf("*** Deprecated ***\n");
	for (i = 0 ; Parameters[i] != nullptr ; i++) {
        Parameter* p = Parameters[i];
        if (p->deprecated)
          printf("%s\n", p->getName());
    }

}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
