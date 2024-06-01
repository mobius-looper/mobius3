/**
 * A ParameterField extends the Field model to provide
 * initialization based on a Mobius Parameter definition.
 *
 * The two models are similar and could be redisigned to share more
 * but Parameter has a lot of old code dependent on it so we
 * need to convert.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/UIParameter.h"
#include "../../model/ExValue.h"

#include "ParameterField.h"

ParameterField::ParameterField(UIParameter* p) :
    Field(p->getName(), p->getDisplayName(), convertParameterType(p->type))
{
    parameter = p;

    setMulti(p->multi);

    // todo: need to figure out how to handle fields with
    // confirable highs
    setMin(p->low);
    setMax(p->high);

    // enums must have allowed values, strings are optional
    if (p->type == TypeEnum || p->type == TypeString) {
        if (p->values != nullptr) {
            setAllowedValues(p->values);
        }
        if (p->valueLabels != nullptr) {
            setAllowedValueLabels(p->valueLabels);
        }
    }

    // hack, these look better if they're all the same size
    // until we can be smarter about deriving this just pick
    // a number that looks right for the current preset/setup panels
    if (p->type == TypeEnum) {
        // ugh, 20 is WAY too wide, sizing on these sucks "emwidth"
        // isn't working well, might be font height problems
        setWidthUnits(10);
    }
}    

ParameterField::~ParameterField()
{
}

Field::Type ParameterField::convertParameterType(UIParameterType intype)
{
    Field::Type ftype = Field::Type::Integer;

    switch (intype) {
        case TypeInt: ftype = Field::Type::Integer; break;
        case TypeBool: ftype = Field::Type::Boolean; break;
        case TypeString: ftype = Field::Type::String; break;
        case TypeEnum: ftype = Field::Type::String; break;
        case TypeStructure: ftype = Field::Type::String; break;
    }

    return ftype;
}

/**
 * Set the field's value by pulling it out of a configuration object.
 */
void ParameterField::loadValue(void *obj)
{
    juce::var newValue;

    // newer complex parameter values use juce::var
    // should be here for all multi valued parameters
    if (parameter->juceValues) {
        parameter->getValue(obj, newValue);
    }
    else {
        // old-school ExValue
        ExValue ev;
        parameter->getValue(obj, &ev);

        if (parameter->multi) {
            // supposed to be using juce::var for these
            Trace(1, "ParameterField: muli-value parameter not supported without Juce accessors\n");
        }
        else {
            switch (parameter->type) {
                case TypeInt: {
                    newValue = ev.getInt();
                }
                    break;
                case TypeBool: {
                    newValue = ev.getBool();
                }
                    break;
                case TypeString: {
                    newValue = ev.getString();
                }
                    break;
                case TypeEnum: {
                    // these should always be ordinals now, but are there
                    // cases where we'd be using strings?
                    if (ev.getType() == EX_INT) {
                        int ordinal = ev.getInt();
                        // Field needs to be heavily rewritten to handle enumerations
                        // as numbers and let the subclass decide how to render them
                        // can't just return the label here, need the base name
                        const char* enumName = parameter->getEnumName(ordinal);
                        newValue = juce::String(enumName);
                    }
                    else {
                        Trace(1, "ParameterField: Unexpected Enum value type %s\n",
                              parameter->getName());
                        newValue = ev.getString();
                    }
                }
                    break;
                case TypeStructure: {
                    newValue = ev.getString();
                }
                    break;
            }
        }
    }

    setValue(newValue);
}

void ParameterField::saveValue(void *obj)
{
    if (parameter->juceValues) {
        // wouldn't compile on Mac
        // parameter->setValue(obj, getValue());
        // had to do an intermediate juce::var assignment for some reason
        juce::var curValue = getValue();
        parameter->setValue(obj, curValue);
    }
    else {
        ExValue ev;

        if (parameter->multi) {
            // todo: will need to handle multi-valued lists properly
            Trace(1, "ParameterField: muli-value parameter not supported without Juce accessors\n");
        }
        else {
            switch (parameter->type) {
                case TypeInt: {
                    ev.setInt(getIntValue());
                }
                    break;
                case TypeBool: {
                    ev.setBool(getBoolValue());
                }
                    break;
                case TypeString: {
                    ev.setString(getCharValue());
                }
                    break;
                case TypeEnum: {
                    // Fields for UIParameters deal with enums exclusively
                    // as ordinals
                    // Field::getIntValue will return the menu index
                    // which is the same as the ordinal, but it can't
                    // go the other way around in loadValue
                    // hmm, this didn't see to work
                    //ev.setInt(getIntValue());
                    const char* enumName = getCharValue();
                    ev.setInt(parameter->getEnumOrdinal(enumName));
                }
                    break;
                case TypeStructure: {
                    ev.setString(getCharValue());
                }
                    break;
            }
        }

        // should do this only if we had a conversion
        if (!ev.isNull())
          parameter->setValue(obj, &ev);
    }
}
