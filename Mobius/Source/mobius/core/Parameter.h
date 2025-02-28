/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Static object definitions for Mobius parameters.
 *
 * This is part of the public interface.
 *
 */

#ifndef MOBIUS_PARAMETER_H
#define MOBIUS_PARAMETER_H

#include "../../model/SystemConstant.h"

/****************************************************************************
 *                                                                          *
 *                                 CONSTANTS                                *
 *                                                                          *
 ****************************************************************************/

#define MAX_PARAMETER_ALIAS 4

typedef enum {

	TYPE_INT,
	TYPE_BOOLEAN,
	TYPE_ENUM,
	TYPE_STRING

} ParameterType;

typedef enum {

    // it is really important that these initialze properly
    // don't defualt and assume it's Preset
    PARAM_SCOPE_NONE,
	PARAM_SCOPE_PRESET,
	PARAM_SCOPE_TRACK,
	PARAM_SCOPE_SETUP,
	PARAM_SCOPE_GLOBAL

} ParameterScope;

/****************************************************************************
 *                                                                          *
 *                                 PARAMETER                                *
 *                                                                          *
 ****************************************************************************/

class Parameter : public SystemConstant {

    friend class Mobius;

  public:

	Parameter();
	Parameter(const char* name);
	virtual ~Parameter();
    //void localize(MessageCatalog* cat);

	const char* aliases[MAX_PARAMETER_ALIAS];

    bool bindable;      // true if this bindable 
	bool dynamic;		// true if labels and max ordinal can change
    bool deprecated;    // true if this is a backward compatible parameter
    bool transient;     // memory only, not stored in config objects
    bool resettable;    // true for Setup parameters that may be reset
    bool scheduled;     // true if setting the value schedules an event
    bool takesAction;   // true if ownership of the Action may be taken
    bool control;       // true if this is displayed as control in the binding UI

    /**
     * When this is set, it is a hint to the UI to display the value
     * of this parameter as a positive and negative range with zero
     * at the center.  This has no effect on the value of the parameter
     * only the way it is displayed.
     */
    bool zeroCenter;

    /**
     * When set, this parameter will retain it's value after track reset.
     */
    bool resetRetain = false;

    /**
     * Control parameters  have a default value, usually either the 
     * upper end of the range or the center.
     */
    int mDefault;

	ParameterType type;
	ParameterScope scope;
    const char** values;
	const char** valueLabels;

    /**
     * Used in rare cases where we need to change the
     * name of a parameter and upgrade the xml.
     */
    const char* xmlAlias;

    //
    // Configurable Parameter property access
    // 

    int getLow();
    virtual int getHigh(class Mobius* m);

    /**
     * The maximum value used for bindings.
     * This is usually the same as getHigh() except for a ew
     * integers that don't have an upper bound.  Since we have 
     * to have some bounds for scaling MIDI CCs, this will default
     * to 127 and can be overridden.
     */
    virtual int getBindingHigh(class Mobius* m);

	virtual void getOrdinalLabel(class Mobius* m, int i, class ExValue* value);

	//
	// Parameter value access
	//

    /**
     * Get or set the value from a configuration object.
     */
    virtual void getObjectValue(void* object, class ExValue* value);
    virtual void setObjectValue(void* object, class ExValue* value);

    //
    // Get or set the value at runtime
    //

    virtual void getValue(class Export* exp, class ExValue* value);
    virtual void setValue(class Action* action);

    // maybe this can be a quality of the Export?
    virtual int getOrdinalValue(class Export* exp);

	//
	// Coercion helpers
	//

	/**
	 * Convert a string value to an enumeration ordinal value.
     * If the value is not in the enum, an error is traced and zero is returned.
	 */
	int getEnum(const char *value);

	/**
	 * Convert a string value to an enumeration ordinal value, returning
     * -1 if the value isn't in the enum.
	 */
	int getEnumValue(const char *value);

	/**
	 * Convert an ExValue with an string or a number into an ordinal.
	 */
	int getEnum(ExValue *value);

    /**
     * Upgrade an enumeration value.
     */
    void fixEnum(ExValue* value, const char* oldValue, const char* newValue);

	/**
	 * Convert a CC number in the range of 0-127 to an enumeration ordinal.
	 * !! This isn't used any more. Scaling needs to be done at the
	 * binding trigger layer appropriate for the trigger (host, key, midi, etc.)
	 */
	int getControllerEnum(int value);

    // internal use only

    virtual void getDisplayValue(class Mobius* m, ExValue* value);

    // formerly protected, made public for MobiusKernel
	static Parameter* getParameter(const char* name);

  protected:

    static void initParameters();
	//static void localizeAll(class MessageCatalog* cat);
    static void dumpFlags();
    static void deleteParameters();
    
	static Parameter* getParameter(Parameter** group, const char* name);

	static Parameter* getParameterWithDisplayName(Parameter** group, const char* name);
	static Parameter* getParameterWithDisplayName(const char* name);

	static void checkAmbiguousNames();
    void addAlias(const char* alias);

	// const char** allocLabelArray(int size);
    int getOrdinalInternal(class ExValue* value, const char** varray);

	int low;
	int high;


  private:

    void init();

};

/****************************************************************************
 *                                                                          *
 *                            PARAMETER CONSTANTS                           *
 *                                                                          *
 ****************************************************************************/

// Preset Parameters

extern Parameter* AltFeedbackEnableParameter;
extern Parameter* BounceQuantizeParameter;
extern Parameter* EmptyLoopActionParameter;
extern Parameter* EmptyTrackActionParameter;
extern Parameter* LoopCountParameter;
extern Parameter* MaxRedoParameter;
extern Parameter* MaxUndoParameter;
extern Parameter* MultiplyModeParameter;
extern Parameter* MuteCancelParameter;
extern Parameter* MuteModeParameter;
extern Parameter* NoFeedbackUndoParameter;
extern Parameter* NoLayerFlatteningParameter;
extern Parameter* OverdubQuantizedParameter;
extern Parameter* OverdubTransferParameter;
extern Parameter* PitchBendRangeParameter;
extern Parameter* PitchSequenceParameter;
extern Parameter* PitchShiftRestartParameter;
extern Parameter* PitchStepRangeParameter;
extern Parameter* PitchTransferParameter;
extern Parameter* QuantizeParameter;
extern Parameter* SpeedBendRangeParameter;
extern Parameter* SpeedRecordParameter;
extern Parameter* SpeedSequenceParameter;
extern Parameter* SpeedShiftRestartParameter;
extern Parameter* SpeedStepRangeParameter;
extern Parameter* SpeedTransferParameter;
extern Parameter* TimeStretchRangeParameter;
extern Parameter* RecordResetsFeedbackParameter;
extern Parameter* RecordTransferParameter;
extern Parameter* ReturnLocationParameter;
extern Parameter* ReverseTransferParameter;
extern Parameter* RoundingOverdubParameter;
extern Parameter* ShuffleModeParameter;
extern Parameter* SlipModeParameter;
extern Parameter* SlipTimeParameter;
extern Parameter* SoundCopyParameter;
extern Parameter* SubCycleParameter;
extern Parameter* SwitchDurationParameter;
extern Parameter* SwitchLocationParameter;
extern Parameter* SwitchQuantizeParameter;
extern Parameter* SwitchVelocityParameter;
extern Parameter* TimeCopyParameter;
extern Parameter* TrackLeaveActionParameter;
extern Parameter* WindowEdgeAmountParameter;
extern Parameter* WindowEdgeUnitParameter;
extern Parameter* WindowSlideAmountParameter;
extern Parameter* WindowSlideUnitParameter;

// Deprecated Parameters

//extern Parameter* AutoRecordParameter;
extern Parameter* InsertModeParameter;
extern Parameter* InterfaceModeParameter;
extern Parameter* LoopCopyParameter;
extern Parameter* OverdubModeParameter;
extern Parameter* RecordModeParameter;
extern Parameter* SamplerStyleParameter;
extern Parameter* TrackCopyParameter;

// Track Parameters

extern Parameter* AltFeedbackLevelParameter;
extern Parameter* AudioInputPortParameter;
extern Parameter* AudioOutputPortParameter;
extern Parameter* FeedbackLevelParameter;
extern Parameter* FocusParameter;
extern Parameter* GroupParameter;
extern Parameter* InputLevelParameter;
extern Parameter* MonoParameter;
extern Parameter* OutputLevelParameter;
extern Parameter* PanParameter;
extern Parameter* PluginInputPortParameter;
extern Parameter* PluginOutputPortParameter;
extern Parameter* SpeedBendParameter;
extern Parameter* SpeedOctaveParameter;
extern Parameter* SpeedStepParameter;
extern Parameter* TrackPresetParameter;

// Global Parameters

extern Parameter* InputLatencyParameter;
extern Parameter* OutputLatencyParameter;
extern Parameter* SetupNameParameter;
extern Parameter* TrackParameter;

// Parameter Groups

extern Parameter* Parameters[];

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
