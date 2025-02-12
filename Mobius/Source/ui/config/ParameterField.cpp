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
#include "../../model/Symbol.h"
#include "../../model/ParameterProperties.h"
#include "../../model/ParameterHelper.h"
#include "../../model/ExValue.h"
#include "../../Supervisor.h"
#include "../../Provider.h"

#include "ParameterField.h"

static const char* ParameterFieldNone = "[None]";

ParameterField::ParameterField(Supervisor* s, UIParameter* p) :
    Field(p->getName(), p->getDisplayName(), convertParameterType(p->type))
{
    supervisor = s;
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
    else if (p->type == TypeStructure) {
        refreshAllowedValuesInternal(false);
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

ParameterField::ParameterField(Provider* p, SymbolId id)
{
    provider = p;
    symbol = p->getSymbols()->getSymbol(id);
    ParameterProperties* props = symbol->parameterProperties.get();

    init(symbol->name, props->displayName, convertParameterType(props->type));

    setMulti(props->multi);

    // todo: need to figure out how to handle fields with
    // confirable highs
    setMin(props->low);
    setMax(props->high);

    // enums must have allowed values, strings are optional
    if (props->type == TypeEnum || props->type == TypeString) {
        if (props->values.size() > 0) {
            setAllowedValues(props->values);
        }
        if (props->valueLabels.size() > 0) {
            setAllowedValueLabels(props->valueLabels);
        }
    }
    else if (props->type == TypeStructure) {
        refreshAllowedValuesInternal(false);
    }

    // hack, these look better if they're all the same size
    // until we can be smarter about deriving this just pick
    // a number that looks right for the current preset/setup panels
    if (props->type == TypeEnum) {
        // ugh, 20 is WAY too wide, sizing on these sucks "emwidth"
        // isn't working well, might be font height problems
        setWidthUnits(10);
    }
}

/**
 * Structure parameters need to refresh their allowed values to track object renames.
 * Gag, this can be called in two contexts: during initialization before rendering where we
 * just save the allowedValues and after rendering when we update them.
 */
void ParameterField::refreshAllowedValuesInternal(bool rendered)
{
    if (parameter != nullptr) {
        if (parameter->type == TypeStructure) {
            // these are combos (string + multi) but must have allowed values

            // returns an old-school StringList for some reason, we never want that
            // why not just return juce::StringArray and be done with it?
            
            // one of the few uses of this, only works for Presets
            StringList* list = parameter->getStructureNames(supervisor->getOldMobiusConfig());
            juce::StringArray values;

            // always start with this?  for the first usage of selecting Preset names
            // in the Setup these are always optional so need to allow empty
            values.add(ParameterFieldNone);
        
            if (list != nullptr) {
                for (int i = 0 ; i < list->size() ; i++) {
                    values.add(juce::String(list->getString(i)));
                }
                delete list;
            }

            // actually don't need this shit, just call updateAllowedValues for both?
            if (rendered)
              updateAllowedValues(values);
            else
              setAllowedValues(values);
        }
    }
    else if (provider != nullptr && symbol != nullptr) {
        // new way of doing this
        ParameterProperties* props = symbol->parameterProperties.get();
        if (props != nullptr) {
            if (props->type == TypeStructure) {

                juce::StringArray names;
                ParameterHelper::getStructureNames(provider, symbol, names);
                
                // always start with this?  for the first usage of selecting Preset names
                // in the Setup these are always optional so need to allow empty
                names.insert(0, ParameterFieldNone);
        
                // actually don't need this shit, just call updateAllowedValues for both?
                if (rendered)
                  updateAllowedValues(names);
                else
                  setAllowedValues(names);
            }
        }
    }
}

void ParameterField::refreshAllowedValues()
{
    refreshAllowedValuesInternal(true);
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
                    int ival = ev.getInt();
                    if (parameter->displayBase > 0)
                      ival += parameter->displayBase;
                    newValue = ival;
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
                    if (ev.isNull()) {
                        newValue = ParameterFieldNone;
                    }
                    else {
                        const char* str = ev.getString();
                        if (str == nullptr || strlen(str) == 0)
                          newValue = ParameterFieldNone;
                        else
                          newValue = ev.getString();
                    }
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
        bool invalid = false;
        
        if (parameter->multi) {
            // todo: will need to handle multi-valued lists properly
            Trace(1, "ParameterField: muli-value parameter not supported without Juce accessors\n");
            invalid = true;
        }
        else {
            switch (parameter->type) {
                case TypeInt: {
                    int curValue = getIntValue();
                    if (parameter->displayBase > 0) {
                        curValue -= (parameter->displayBase);
                        // if they force edited it out of range, limit it
                        if (curValue < 0) curValue = 0;
                    }
                    ev.setInt(curValue);
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
                    juce::String s = getStringValue();
                    if (s == juce::String(ParameterFieldNone))
                      ev.setString("");
                    else
                      ev.setString(s.toUTF8());
                }
                    break;
                    
                default: {
                    Trace(1, "ParamaeterField: Invalid parameter type %d", parameter->type);
                    invalid = true;
                }
                    break;
            }
        }

        // should do this only if we had a conversion
        if (!invalid)
          parameter->setValue(obj, &ev);
    }
}
