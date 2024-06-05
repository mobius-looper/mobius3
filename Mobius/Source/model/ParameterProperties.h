/**
 * Structure that is attached to a Symbol associated with a Parameter
 * to describe how it behaves.
 *
 * Eventual replacement for UIParameter.
 */

#pragma once

// UIParameterType and UIParameterScope are defined in here
// until we can move them
#include "UIParameter.h"

class ParameterProperties
{
  public:

    ParameterProperties() {}
    ~ParameterProperties() {}

    /**
     * For configuration parameters, indidicates which structdure this
     * parameter lives in.  Global=MobiusConfig, Preset=Preset, etc.
     */
    UIParameterScope scope = ScopeGlobal;

    /**
     * The value type.
     */
	UIParameterType type = TypeInt;
    
    /**
     * True if it supports multiple values.
     */
    bool multi = false;

    /**
     * For TypeEnum, the  set of allowed values.
     */
    juce::StringArray values;

    /**
     * For TypeEnum, the set of alternate display names.
     */
    juce::StringArray valueLabels;

    /**
     * For TypeInt, the lowest allowed value.
     */
    int low = 0;

    /**
     * For TypeInt, the highest allowed value.
     * If the dynamic flag is set, the high value must be calculated
     * at runtime.
     */
    int high = 0;

    /**
     * For TypeInt, a few parameters may have a default value other
     * than zer.  Typically this will be the upper end of a range
     * or the center.
     */
    int defaultValue = 0;

    /**
     * Indiciates the high value must be calculated at runtime.
     */
    bool dynamic = false;
    
    /**
     * Indicates that the value should be displayed as a positive
     * or negative integer with zero at the center of the low/high range.
     */
    bool zeroCenter = false;

    /**
     * Indicates that this can be highlighted in the UI as a sweepable control.
     * These are the most common parameters used in bindings and can be
     * separated from other parameters to make them easier to find.
     * Explore other presentation categories like "advanced".
     */
    bool control = false;

    /**
     * Indicates that this parameter exists only at runtime and will not
     * be saved in a configuration file.  It can still be used in bindings
     * but will be omitted from configuration file generators.
     */
    bool transient = false;

    /**
     * Indiciates that this parameter may use juce::var for value access.
     * Since we're redesigning this model just for the UI, this can
     * eventually be the default and we can remove ExValue
     */
    bool juceValues = false;

    /**
     * Indicates that this parameter cannot be bound to MIDI or host parameters
     * so keep it out of the operation selection UI.
     */
    bool noBinding;

    /**
     * In a few cases the names were changed to be more consistent
     * or obvious or because I liked them better.  In order to correlate
     * the new parameter definitions with the old ones, this would
     * be the name of the old Parameter.
     *
     * update: this was used when we generated code for UIParameter definitions
     * it may not be necessary any more, except for correlating core parameters.
     */
    juce::String coreName;
    
    //////////////////////////////////////////////////////////////////////
    //
    // Enumeration Utilities
    //
    //////////////////////////////////////////////////////////////////////
    
    /**
     * For type=enum, convert a symbolic value into an ordinal value by
     * locating the given value within the allowedValues set.
     *
     * This currently only supports internal names, not display names.
     * Do we have a reason to support display names?
     *
     * Original model getEnun would trace a warning if the value was
     * not in the set of allowed values and returned zero.  This now
     * does not trace and returns -1.
     */
    int getEnumOrdinal(const char* value);

    /**
     * For type=enum, convert an ordinal value into the symbolic value
     * defined by the values array.
     */
    const char* getEnumName(int ordinal);

    /**
     * For type=enum, convert an ordinal value into the symbolic value
     * defined by the valueLabels array.
     */
    const char* getEnumLabel(int ordinal);

    //////////////////////////////////////////////////////////////////////
    //
    // Model Queries
    //
    // For parameters with type='structure' or options='dynamic' some
    // characteristics of the parameter cannot be statically defined and
    // must be calculated at runtime.  These functions provide a way to do that
    // until the Query model is fleshed out.
    //
    // Currently it is assumed that the calculation can be satisfied using
    // only the model contained within MobiusConfig.  Eventually some of
    // these will shift to UIConfig or another object at which point Query
    // must be used in a larger context.
    //
    //////////////////////////////////////////////////////////////////////
    
    /**
     * Calculate the maximum ordinal value of a dynamic or structure parameter.
     */
    int getDynamicHigh(class MobiusConfig* container);

    /**
     * For type=structure, calculate the set of structure names.
     * This is in effect the symbolic values of a dynamic enumeration.
     * If this becomes necessary for types other than type=structure
     * change the name.
     * 
     * The result is returned as a StringList and must be deleted.
     */
    class StringList* getStructureNames(class MobiusConfig* container);

    /**
     * For type=structure convert a symbolic structure name into an ordinal
     * within the value as that would be returned by getDynamicValues
     */
    int getStructureOrdinal(class MobiusConfig* container, const char* name);

    /**
     * For type=structure convert an ordinal into a symbolic name.
     * Note that the string returned will be found within the container
     * model and will become invalid if the container is deleted.
     * It should be considered temporary and copied if it needs to live
     * for an indefinite time.
     */
    const char* getStructureName(class MobiusConfig* container, int ordinal);

  private:

    class Structure* getStructureList(class MobiusConfig* container);


};


