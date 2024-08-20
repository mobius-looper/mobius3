/**
 * Eventual replacement for UIParameter
 */

#include "ParameterProperties.h"

/**
 * Convert a symbolic parameter value into an ordinal.
 * This could support both internal names and display names
 * but it's only using internal names at the moment.
 *
 * This cannot be used for type=string.
 * For type=structure you must use getDynamicEnumOrdinal
 */
int ParameterProperties::getEnumOrdinal(const char* value)
{
	int enumOrdinal = -1;

	if (value != nullptr) {
		for (int i = 0 ; i < values.size() ; i++) {
            if (values[i] == value) {
				enumOrdinal = i;
				break;
			}
		}
    }
    return enumOrdinal;
}

/**
 * Convert an ordinal into the symbolic enumeration name.
 * This almost always comes from an (int) cast of
 * a zero based enum value, but check the range.
 *
 * Not liking the mismash of const char* and juce::String.
 * Standardize on one or the other.  Since symbolc names are
 * only used at the shell level, could just use juce::String and
 * be done with it.
 * 
 */ 
const char* ParameterProperties::getEnumName(int enumOrdinal)
{
    const char* enumName = nullptr;

    if (enumOrdinal >= 0 && enumOrdinal < values.size()) {
        // hmm, here we have a problem with the lifespan of
        // the UTF8 representations in StringArray
        // I think it's okay as long as you don't put this in an
        // intermediate juce::String
        enumName = values[enumOrdinal].toUTF8();
    }
    return enumName;
}

/**
 * Convert an ordinal into the symbolic enumeration label.
 * Labels are usually what is displayed in the UI.
 * getEnumName is what would be in an XML file.
 */ 
const char* ParameterProperties::getEnumLabel(int enumOrdinal)
{
    const char* label = nullptr;

    if (enumOrdinal >= 0 && enumOrdinal < valueLabels.size()) {
        // same potential issues with UTF8 lifespan
        label = valueLabels[enumOrdinal].toUTF8();
        // don't think this can be nullptr, but it can be empty
        if (label == nullptr || strlen(label) == 0) {
            // missing or abbreviated label array, fall back to the name
            label = getEnumName(enumOrdinal);
        }
    }
    return label;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
