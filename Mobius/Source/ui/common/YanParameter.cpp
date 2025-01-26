
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/Symbol.h"
#include "../../model/ParameterProperties.h"
#include "../../script/MslValue.h"

#include "YanField.h"
#include "YanFieldHelpers.h"
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
        else if (props->values.size() > 0 || props->displayType == "combo") {
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
        else if (props->type == TypeStructure) {
            isCombo = true;
            // this must have displayHelper specified
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

void YanParameter::load(Provider* p, MslValue* v)
{
    ParameterProperties* props = symbol->parameterProperties.get();
    bool hasHelper = (props->displayHelper.length() > 0);
    
    if (isCombo) {
        if (hasHelper) {
            if (p == nullptr) {
                Trace(1, "YanParameter: Parameter has a display helper but Provider not supplied");
            }
            else {
                juce::String svalue;
                if (v != nullptr) svalue = juce::String(v->getString());
                YanFieldHelpers::comboInit(p, &combo, props->displayHelper, svalue);
            }
        }
        else if (v == nullptr) {
            combo.setSelection(0);
        }
        else {
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
    }
    else if (isCheckbox) {
        if (v == nullptr)
          checkbox.setValue(false);
        else
          checkbox.setValue(v->getBool());
    }
    else if (props->type == TypeInt) {
        if (v == nullptr) {
            input.setValue(juce::String(symbol->parameterProperties->defaultValue));
        }
        else {
            // it will be an input field, but allow a value offset
            // todo: for integers within a small range need to allow a slider
            int val = v->getInt();
            val += props->displayBase;
            input.setValue(juce::String(val));
        }
    }
    else {
        if (v == nullptr)
          input.setValue("");
        else
          input.setValue(v->getString());
    }
}

void YanParameter::save(MslValue* v)
{
    ParameterProperties* props = symbol->parameterProperties.get();
    v->setNull();
    
    if (isCombo) {
        juce::String helper = props->displayHelper;
        if (helper.length() > 0) {
            juce::String result = YanFieldHelpers::comboSave(&combo, helper);
            v->setString(result.toUTF8());
        }
        else {
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
    }
    else if (isCheckbox) {
        v->setBool(checkbox.getValue());
    }
    else if (props->type == TypeInt) {
        juce::String svalue = input.getValue();
        int ival = svalue.getIntValue();
        ival -= props->displayBase;
        v->setInt(ival);
    }
    else {
        v->setJString(input.getValue());
    }
    
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
