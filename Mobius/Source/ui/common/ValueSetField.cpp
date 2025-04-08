
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/Form.h"
#include "../../script/MslValue.h"
#include "../../Services.h"
#include "../../Provider.h"

#include "YanField.h"
#include "YanFieldHelpers.h"
#include "ValueSetField.h"

ValueSetField::ValueSetField(juce::String label) : YanField(label)
{
}

ValueSetField::~ValueSetField()
{
}

void ValueSetField::init(Provider* p, Field* def)
{
    definition = def;
    type = TypeText;
    
    if (def == nullptr) {
        Trace(1, "ValueSetField: Missing definition");
    }
    else {
        if (def->values.size() > 0 || def->displayType == "combo") {
            initCombo(p, def);
        }
        else if (def->type == TypeStructure) {
            initCombo(p, def);
        }
        else if (def->type == TypeBool) {
            type = TypeCheckbox;
            addAndMakeVisible(&checkbox);
        }
        else if (def->type == TypeString && def->file) {
            type = TypeFile;
            addAndMakeVisible(&file);
            // this is the magic that connects it all together
            file.initialize(def->name, p);
        }
        else {
            type = TypeText;
            addAndMakeVisible(&input);
            input.setListener(this);
        }
    }
}

void ValueSetField::initCombo(Provider* p, Field* def)
{
    type = TypeCombo;
    addAndMakeVisible(&combo);
    combo.setListener(this);
            
    if (def->displayHelper.length() > 0) {
        YanFieldHelpers::comboInit(p, &combo, def->displayHelper, structureNames);
    }
    else {
        // Structure fields are supposed to have helpers, I can't think
        // of a reason to let them specifiy a fixed set of names
        if (def->type == TypeStructure)
          Trace(1, "ValueSetField: Structure field without a parameterHelper");
        
        if (def->valueLabels.size() > 0)
          combo.setItems(def->valueLabels);
        else
          combo.setItems(def->values);
    }
}

int ValueSetField::getPreferredComponentWidth()
{
    int width = 0;
    if (type == TypeCombo)
      width = combo.getPreferredComponentWidth();
    else if (type == TypeCheckbox)
      width = checkbox.getPreferredComponentWidth();
    else if (type == TypeText)
      width = input.getPreferredComponentWidth();
    else if (type == TypeFile)
     width = file.getPreferredComponentWidth();
    
    return width;
}

void ValueSetField::resized()
{
    juce::Rectangle<int> remainder = resizeLabel();
    if (type == TypeCombo)
      combo.setBounds(remainder);
    else if (type == TypeCheckbox)
      checkbox.setBounds(remainder);
    else if (type == TypeText)
      input.setBounds(remainder);
    else if (type == TypeFile)
      file.setBounds(remainder);
}

void ValueSetField::load(MslValue* v)
{
    if (type == TypeCombo) {
        if (structureNames.size() > 0) {
            // we had a parameterHelper that found the allowed values
            if (v == nullptr) {
                // this is usually "None" or other placeholder at the beginning
                combo.setSelection(0);
            }
            else {
                int index = structureNames.indexOf(juce::String(v->getString()));
                if (index >= 0)
                  combo.setSelection(index);
                else {
                    // this is relatively common for things like MIDI devices
                    // when you move between machines
                    Trace(1, "ValueSetField: Desired combo value not in range %s",
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
                int index = v->getInt();
                if (index < definition->values.size()) {
                    combo.setSelection(index);
                }
                else {
                    Trace(1, "ValueSetField: Ordinal value did not match enumerated value list %s %d",
                          definition->name.toUTF8(), index);
                }
            }
            else {
                juce::String current(v->getString());
                int index = -1;
                for (int i = 0 ; i < definition->values.size() ; i++) {
                    juce::String allowed = definition->values[i];
                    if (allowed == current) {
                        index = i;
                        break;
                    }
                }
                if (index < 0) {
                    Trace(1, "ValueSetField: Value did not match enumeration %s %s",
                          definition->name.toUTF8(), current.toUTF8());
                }
                else {
                    combo.setSelection(index);
                }
            }
        }
    }
    else if (type ==  TypeCheckbox) {
        if (v == nullptr)
          checkbox.setValue(false);
        else
          checkbox.setValue(v->getBool());
    }
    else if (type == TypeFile) {

          
    }
    else if (definition->type == TypeInt) {
        // note that defaultValue still has displayBase applied to it
        int value = definition->defaultValue;
        if (v != nullptr)
          value = v->getInt();
        value += definition->displayBase;
        input.setValue(juce::String(value));
    }
    else {
        if (v == nullptr)
          input.setValue("");
        else
          input.setValue(v->getString());
    }
}

void ValueSetField::save(MslValue* v)
{
    v->setNull();
    
    if (type == TypeCombo) {
        juce::String helper = definition->displayHelper;
        if (helper.length() > 0) {
            juce::String result = YanFieldHelpers::comboSave(&combo, helper);
            v->setString(result.toUTF8());
        }
        else {
            int index = combo.getSelection();
            if (index >= 0) {
                if (index >= definition->values.size()) {
                    Trace(1, "ValueSetField: Combo selection out of range %s %d",
                          definition->name.toUTF8(), index);
                }
                else {
                    v->setEnum(definition->values[index].toUTF8(), index);
                }
            }
        }
    }
    else if (type == TypeCheckbox) {
        v->setBool(checkbox.getValue());
    }
    else if (type == TypeFile) {
    }
    else if (definition->type == TypeInt) {
        juce::String svalue = input.getValue();
        int ival = svalue.getIntValue();
        ival -= definition->displayBase;
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

void ValueSetField::yanComboSelected(YanCombo* c, int selection)
{
    (void)c;
    (void)selection;
    //if (listener != nullptr)
    //listener->yanParameterChanged(this);
}

void ValueSetField::yanInputChanged(YanInput* i)
{
    (void)i;
    //if (listener != nullptr)
    //listener->yanParameterChanged(this);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
