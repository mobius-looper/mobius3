
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

void YanParameter::setListener(Listener* l)
{
    listener = l;
}

void YanParameter::init(Provider* p, Symbol* s)
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
            initCombo(p, s);
        }
        else if (props->type == TypeStructure) {
            initCombo(p, s);
        }
        else if (props->type == TypeBool) {
            isCheckbox = true;
            addAndMakeVisible(&checkbox);
            // yuno have one?
            // checkbox.setListener(this);
        }
        else {
            isText = true;
            addAndMakeVisible(&input);
            input.setListener(this);
        }
    }
}

void YanParameter::initCombo(Provider* p, Symbol* s)
{
    isCombo = true;
    addAndMakeVisible(&combo);
    combo.setListener(this);

    ParameterProperties* props = s->parameterProperties.get();
            
    if (props->displayHelper.length() > 0) {
        YanFieldHelpers::comboInit(p, &combo, props->displayHelper, structureNames);
    }
    else {
        // Structure fields are supposed to have helpers, I can't think
        // of a reason to let them specifiy a fixed set of names
        if (props->type == TypeStructure)
          Trace(1, "YanParameter: Structure symbol without a parameterHelper");
        
        if (props->valueLabels.size() > 0)
          combo.setItems(props->valueLabels);
        else
          combo.setItems(props->values);
    }
}

/**
 * When a field is marked defaulted it means that there is no editable value
 * and that the parameter will have an effective value that comes from somewhere else.
 * This happens in the SessionTrackEditor when forms are displayed for parameters that
 * do not have track overrides.  The field shows the shared value from the session
 * but it cannot be changed without manual intervention.  Also called "unlocking"
 * the parameter.
 *
 * When a field is defaulted/locked the internal component is disabled, and the label
 * color turns grey.
 */
void YanParameter::setDefaulted(bool b)
{
    defaulted = b;

    // this can always turn on, but it only
    // turns off if the field is not also occluded
    if (b || !occluded)
      setDisabled(b);
}

bool YanParameter::isDefaulted()
{
    return defaulted;
}

/**
 * When a field is marked occluded it means that there is an overlay in place that
 * overrides the value of this parameter.  Like being defaulted, the parameter may
 * not be edited, but the color of the label is different.
 *
 * The parameter may ALSO be defaulted, they are independent states.
 */
void YanParameter::setOccluded(bool b)
{
    occluded = b;

    if (occluded) {
        setDisabled(true);
        // straignt up yellow looks too bright, tone it down
        // beige looks too much like defaulted grey
        setLabelColor(juce::Colours::lightpink);
    }
    else {
        // don't enable if it is also defaulted
        if (!defaulted)
          setDisabled(false);
        unsetLabelColor();
    }
}

bool YanParameter::isOccluded()
{
    return occluded;
}

/**
 * Inner handler for disabling editing for defaulted and occluded.
 * Can also be called directly though in current use it won't be.
 */
void YanParameter::setDisabled(bool b)
{
    if (isCombo)
      combo.setDisabled(b);
    else if (isCheckbox)
      checkbox.setDisabled(b);
    else
      input.setDisabled(b);
    YanField::setDisabled(b);
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
    ParameterProperties* props = symbol->parameterProperties.get();
    
    if (isCombo) {
        if (structureNames.size() > 0) {
            // we had a parameterHelper that found the allowed values
            if (v == nullptr) {
                // this is usually "None" or other placeholder at the beginning
                combo.setSelection(0);
            }
            else {
                int ordinal = structureNames.indexOf(juce::String(v->getString()));
                if (ordinal >= 0)
                  combo.setSelection(ordinal);
                else {
                    // this is relatively common for things like MIDI devices
                    // when you move between machines
                    Trace(1, "YanParameter: Desired combo value not in range %s",
                          v->getString());
                    combo.setSelection(0);
                }
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
        // note that defaultValue still has displayBase applied to it
        int value = props->defaultValue;
        if (v != nullptr)
          value = v->getInt();
        value += props->displayBase;
        input.setValue(juce::String(value));
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

//////////////////////////////////////////////////////////////////////
//
// Change Notification
//
//////////////////////////////////////////////////////////////////////

void YanParameter::yanComboSelected(YanCombo* c, int selection)
{
    (void)c;
    (void)selection;
    if (listener != nullptr)
      listener->yanParameterChanged(this);
}

void YanParameter::yanInputChanged(YanInput* i)
{
    (void)i;
    if (listener != nullptr)
      listener->yanParameterChanged(this);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
