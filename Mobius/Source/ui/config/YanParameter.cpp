
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/Symbol.h"
#include "../../model/ParameterProperties.h"
#include "../../script/MslValue.h"

#include "../common/YanField.h"

#include "YanParameter.h"

YanParameter::YanParameter(juce::String label) : YanField(label)
{
}

YanParameter::~YanParameter()
{
}

void YanParameter::init(Symbol* s)
{
    symbol = s;
    isText = false;
    isCombo = false;
    isCheckbox = false;
    
    if (s == nullptr) {
        Trace(1, "YanParameter: Missing symbol");
    }
    else {
        ParameterProperties* props = s->parameterProperties.get();
        if (props == nullptr) {
            Trace(1, "YanParameter: Symbol is not associated with a parameter %s", s->getName());
        }

        // TypeEnum doesn't seem to be set reliably, look for a value list
        //nelse if (props->type == TypeEnum) {
        else if (props->values.size() > 0) {
            isCombo = true;
            if (props->valueLabels.size() > 0)
              combo.setItems(props->valueLabels);
            else
              combo.setItems(props->values);
            
            addAndMakeVisible(&combo);
        }
        else if (props->type == TypeBool) {
            isCheckbox = true;
            addAndMakeVisible(&checkbox);
        }
        else {
            isText = true;
            addAndMakeVisible(&input);
        }
    }
}

int YanParameter::getPreferredComponentWidth()
{
    int width = 0;
    if (isCombo)
      width = combo.getPreferredComponentWidth();
    else if (isCheckbox)
      width = checkbox.getPreferredComponentWidth();
    else
      width = input.getPreferredComponentWidth();
    return width;
}

void YanParameter::resized()
{
    juce::Rectangle<int> remainder = resizeLabel();
    if (isCombo)
      combo.setBounds(remainder);
    else if (isCheckbox)
      checkbox.setBounds(remainder);
    else
      input.setBounds(remainder);
}

void YanParameter::load(MslValue* v)
{
    if (v == nullptr) {
        // no current value, initialize fields to suitable defaults
        if (isCombo)
          combo.setSelection(0);
        else if (isCheckbox)
          checkbox.setValue(false);
        else
          input.setValue("");
    }
    else {
        ParameterProperties* props = symbol->parameterProperties.get();
        if (isCombo) {
            if (v->type == MslValue::Enum) {
                int ordinal = v->getInt();
                if (ordinal < props->values.size()) {
                    combo.setSelection(ordinal);
                }
                else {
                    Trace(1, "YanParameter: Ordinal value did not match enumerated value list %s %d",
                          symbol->getName(), ordinal);
                }
            }
            else {
                juce::String current(v->getString());
                int ordinal = -1;
                for (int i = 0 ; i < props->values.size() ; i++) {
                    juce::String allowed = props->values[i];
                    if (allowed == current) {
                        ordinal = i;
                        break;
                    }
                }
                if (ordinal < 0) {
                    Trace(1, "YanParameter: Value did not match enumeration %s %s",
                          symbol->getName(), current.toUTF8());
                }
                else {
                    combo.setSelection(ordinal);
                }
            }
        }
        else if (isCheckbox) {
            checkbox.setValue(v->getBool());
        }
        else {
            input.setValue(v->getString());
        }
    }
}

void YanParameter::save(MslValue* v)
{
    v->setNull();
    if (isCombo) {
        ParameterProperties* props = symbol->parameterProperties.get();
        int ordinal = combo.getSelection();
        if (ordinal >= 0) {
            if (ordinal >= props->values.size()) {
                Trace(1, "YanParameter: Combo selection out of range %s %d",
                      symbol->getName(), ordinal);
            }
            else {
                v->setEnum(props->values[ordinal].toUTF8(), ordinal);
            }
        }
    }
    else if (isCheckbox) {
        v->setBool(checkbox.getValue());
    }
    else {
        v->setJString(input.getValue());
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
