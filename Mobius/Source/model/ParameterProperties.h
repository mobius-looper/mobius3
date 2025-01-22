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
     * Don't need a name since we're always attached to a symbol, but
     * we can allow an alternate display name.
     */
    juce::String displayName;
    

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

    /**
     * When true, this may be added to Focus Lock Parameters in the GroupDefinition.
     * Not used for general binding focus yet but could be.
     */
    bool mayFocus = false;

    /**
     * When true, this is selected for group focus.
     */
    bool focus = false;

    /**
     * When true, this parameter may retain it's current value after track reset.
     */
    bool mayResetRetain = false;

    /**
     * When true, this parameter is selected to retain it's value after reset.
     */
    bool resetRetain = false;

    //////////////////////////////////////////////////////////////////////
    //
    // Rendering
    //
    // Properties in this section are only used by the form generator
    // to show things nicer than the raw values.
    //
    //////////////////////////////////////////////////////////////////////

    /**
     * Can be used to force the selection of one of the parameter dislay types
     * Still in flux, but "combo" and "slider" are the two initial suggestions
     */
    juce::String displayType;

    /**
     * For number fields, adds a numeric offset to the value when displayed.
     * This is almost always 1 so internal 0 is presented as 1.
     */
    int displayBase = 0;

    /**
     * Mostly for combo box fields, insert logic to calculate the allowed values
     * and adjust the values.
     */
    juce::String displayHelper;

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

    // For similar utilities on Structures, see ParameterHelper
    // Maybe put the Enum utils in there too
    
};


