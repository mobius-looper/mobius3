/**
 * ConfigEditor for configuring symbol properties.
 *
 * This was adapted from MidiDeviceEditor and had a lot of extra
 * support for checkbox side effects which we don't need here.
 *
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/Symbol.h"
#include "../../model/FunctionProperties.h"
#include "../../model/ParameterProperties.h"

#include "../../Supervisor.h"
#include "../../Symbolizer.h"

#include "../JuceUtil.h"

#include "PropertiesEditor.h"
 
PropertiesEditor::PropertiesEditor(Supervisor* s) : ConfigEditor(s)
{
    setName("PropertiesEditor");
    
    addAndMakeVisible(tabs);

    functionTable.setCheckboxListener(this);
    parameterTable.setCheckboxListener(this);
    
    tabs.add("Functions", &functionTable);
    tabs.add("Parameters", &parameterTable);
}

PropertiesEditor::~PropertiesEditor()
{
    // members will delete themselves
    // remove the MidiManager log if we were still showing
    hiding();
}

//////////////////////////////////////////////////////////////////////
//
// ConfigEditor overloads
//
//////////////////////////////////////////////////////////////////////

/**
 * Called by ConfigEditor when we're about to be made visible.
 */
void PropertiesEditor::showing()
{
}

/**
 * Called by ConfigEditor when we're about to be made invisible.
 */
void PropertiesEditor::hiding()
{
}

/**
 * Called by ConfigEditor when asked to edit properties.
 */
void PropertiesEditor::load()
{
    // merge these!
    functionTable.init(supervisor->getSymbols(), false);
    functionTable.load(supervisor->getSymbols());
    
    parameterTable.init(supervisor->getSymbols(), true);
    parameterTable.load(supervisor->getSymbols());
}

/**
 * Called by the Save button in the footer.
 * Tell the ConfigEditor we are done.
 */
void PropertiesEditor::save()
{
    functionTable.save(supervisor->getSymbols());
    parameterTable.save(supervisor->getSymbols());
    supervisor->updateSymbolProperties();
}

/**
 * Throw away all editing state.
 */
void PropertiesEditor::cancel()
{
}

void PropertiesEditor::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    tabs.setBounds(area);
}

//////////////////////////////////////////////////////////////////////
//
// Internal Methods
// 
//////////////////////////////////////////////////////////////////////

/**
 * Called by either the input or output device table when a checkbox
 * is clicked on or off.
 *
 * The FunctionTableRow checks array will already have been updated
 * by setCellCheck to have the change, here we can add side effects like
 * unckecking other boxes if only one may be selected in the column.
 * note: this was from MidiDeviceEditor, we don't need it here
 */
void PropertiesEditor::tableCheckboxTouched(BasicTable* table, int row, int colid, bool state)
{
    (void)table;
    (void)row;
    (void)colid;
    (void)state;
}

//////////////////////////////////////////////////////////////////////
//
// PropertyTable
//
//////////////////////////////////////////////////////////////////////

PropertyTable::PropertyTable()
{
    // are our own model
    setBasicModel(this);
}

/**
 * Load the function properties into the table
 */
void PropertyTable::init(SymbolTable* symbols, bool parameter)
{
    if (!initialized) {
        isParameter = parameter;
        
        addColumn("Name", PropertyColumnName);

        if (parameter) {
            //addColumnCheckbox("Focus Lock", PropertyColumnFocus);
            addColumnCheckbox("Reset Retain", PropertyColumnResetRetain);
        }
        else {
            addColumnCheckbox("Focus Lock", PropertyColumnFocus);
            addColumnCheckbox("Mute Cancel", PropertyColumnMuteCancel);
            addColumnCheckbox("Confirmation", PropertyColumnConfirmation);
            addColumnCheckbox("Quantize", PropertyColumnQuantize);
        }
        
        juce::StringArray objectNames;
        for (auto symbol : symbols->getSymbols()) {
            if (symbol->functionProperties != nullptr && !isParameter) {

                // weed out the ones that can't have any of the three checkboxes
                // force the may flag on if the option is set by other means

                if (!symbol->functionProperties->mayFocus &&
                    symbol->functionProperties->focus) {
                    Trace(1, "PropertiesEditor: Forcing mayFocus on for %s", symbol->getName());
                    symbol->functionProperties->mayFocus = true;
                }
                
                if (!symbol->functionProperties->mayConfirm &&
                    symbol->functionProperties->confirmation) {
                    Trace(1, "PropertiesEditor: Forcing mayConfirm on for %s", symbol->getName());
                    symbol->functionProperties->mayConfirm = true;
                }
                
                if (!symbol->functionProperties->mayCancelMute &&
                    symbol->functionProperties->muteCancel) {
                    Trace(1, "PropertiesEditor: Forcing mayCancelMute on for %s", symbol->getName());
                    symbol->functionProperties->mayCancelMute = true;
                }

                if (!symbol->functionProperties->mayQuantize &&
                    symbol->functionProperties->quantized) {
                    Trace(1, "PropertiesEditor: Forcing mayQuantize on for %s", symbol->getName());
                    symbol->functionProperties->mayQuantize = true;
                }

                if (symbol->functionProperties->mayFocus ||
                    symbol->functionProperties->mayConfirm ||
                    symbol->functionProperties->mayCancelMute ||
                    symbol->functionProperties->mayQuantize) {

                    objectNames.add(symbol->name);
                }
            }
            else if (symbol->parameterProperties != nullptr && isParameter) {

                if (!symbol->parameterProperties->mayFocus &&
                    symbol->parameterProperties->focus) {
                    Trace(1, "PropertiesEditor: Forcing mayFocus on for %s", symbol->getName());
                    symbol->parameterProperties->mayFocus = true;
                }
                
                if (!symbol->parameterProperties->mayResetRetain &&
                    symbol->parameterProperties->resetRetain) {
                    Trace(1, "PropertiesEditor: Forcing mayResetRetain on for %s", symbol->getName());
                    symbol->parameterProperties->mayResetRetain = true;
                }

                if (symbol->parameterProperties->mayFocus ||
                    symbol->parameterProperties->mayResetRetain) {
                    
                    objectNames.add(symbol->name);
                }
            }
        }

        objectNames.sort(false);
        
        for (auto name : objectNames) {
            PropertyTableRow* obj = new PropertyTableRow();
            obj->name = name;
            // get back to the symbol after sorting
            obj->symbol = symbols->find(name);
            objects.add(obj);
        }

        initialized = true;
    }
}

/**
 * BasicTable overload to determine whether this cell needs a checkbox.
 * Should be in the model.
 */
bool PropertyTable::needsCheckbox(int row, int column)
{
    bool needs = false;
    
    PropertyTableRow* obj = objects[row];
    if (obj != nullptr && obj->symbol != nullptr) {

        if (obj->symbol->functionProperties != nullptr) {
            if (column == PropertyColumnFocus) {
                needs = obj->symbol->functionProperties->mayFocus;
            }
            else if (column == PropertyColumnConfirmation) {
                needs = obj->symbol->functionProperties->mayConfirm;
            }
            else if (column == PropertyColumnMuteCancel) {
                needs = obj->symbol->functionProperties->mayCancelMute;
            }
            else if (column == PropertyColumnQuantize) {
                needs = obj->symbol->functionProperties->mayQuantize;
            }
        }
        else if (obj->symbol->parameterProperties != nullptr) {
            if (column == PropertyColumnFocus) {
                needs = obj->symbol->parameterProperties->mayFocus;
            }
            else if (column == PropertyColumnResetRetain) {
                needs = obj->symbol->parameterProperties->mayResetRetain;
            }
        }
    }
    return needs;
}

/**
 * Look up a device in the table by name.
 */
PropertyTableRow* PropertyTable::getRow(juce::String name)
{
    PropertyTableRow* found = nullptr;
    for (auto row : objects) {
        if (row->name == name) {
            found = row;
            break;
        }
    }
    return found;
}

/**
 * Load the current symbol properties into the table.
 */
void PropertyTable::load(SymbolTable* symbols)
{
    for (auto symbol : symbols->getSymbols()) {

        if (isParameter) {
            if (symbol->parameterProperties != nullptr) {
                PropertyTableRow* row = getRow(symbol->name);
                if (row != nullptr) {
                    if (symbol->parameterProperties->focus)
                      row->checks.add(PropertyColumnFocus);

                    if (symbol->parameterProperties->resetRetain)
                      row->checks.add(PropertyColumnResetRetain);
                }
            }
        }
        else {
            if (symbol->functionProperties != nullptr) {
                // not every function will have a row, some were suppressed
                PropertyTableRow* row = getRow(symbol->name);
                if (row != nullptr) {
                    if (symbol->functionProperties->focus)
                      row->checks.add(PropertyColumnFocus);

                    if (symbol->functionProperties->confirmation)
                      row->checks.add(PropertyColumnConfirmation);

                    if (symbol->functionProperties->muteCancel)
                      row->checks.add(PropertyColumnMuteCancel);

                    if (symbol->functionProperties->quantized)
                      row->checks.add(PropertyColumnQuantize);
                }
            }
        }
    }
    
    updateContent();
}


/**
 * Convert the table model back into the MachineConfig
 */
void PropertyTable::save(SymbolTable* symbols)
{
    for (auto obj : objects) {

        Symbol* s = symbols->find(obj->name);
        if (s != nullptr) {
            if (s->functionProperties != nullptr) {
                FunctionProperties* props = s->functionProperties.get();
                if (props != nullptr) {
                    props->focus = obj->checks.contains(PropertyColumnFocus);
                    props->confirmation = obj->checks.contains(PropertyColumnConfirmation);
                    props->muteCancel = obj->checks.contains(PropertyColumnMuteCancel);
                    props->quantized = obj->checks.contains(PropertyColumnQuantize);
                }
            }
            else if (s->parameterProperties != nullptr) {
                ParameterProperties* props = s->parameterProperties.get();
                if (props != nullptr) {
                    props->focus = obj->checks.contains(PropertyColumnFocus);
                    props->resetRetain = obj->checks.contains(PropertyColumnResetRetain);
                }
            }
        }
    }
}

//
// BasicTable::Model
//


int PropertyTable::getNumRows()
{
    return objects.size();
}

juce::String PropertyTable::getCellText(int row, int columnId)
{
    juce::String value;
    
    PropertyTableRow* obj = objects[row];
    if (obj == nullptr) {
        Trace(1, "PropertyTable::getCellText row out of bounds %d\n", row);
    }
    else if (columnId == PropertyColumnName) {
        value = obj->name;
    }
    return value;
}

bool PropertyTable::getCellCheck(int row, int columnId)
{
    bool state = false;
    
    PropertyTableRow* obj = objects[row];
    if (obj == nullptr) {
        Trace(1, "PropertyTable::getCellCheck row out of bounds %d\n", row);
    }
    else {
        state = obj->checks.contains((PropertyColumns)columnId);
    }
    return state;
}

void PropertyTable::setCellCheck(int row, int columnId, bool state)
{
    PropertyTableRow* obj = objects[row];
    if (obj == nullptr) {
        Trace(1, "PropertyTable::setCellCheck row out of bounds %d\n", row);
    }
    else if (state) {
        obj->checks.add((PropertyColumns)columnId);
    }
    else {
        obj->checks.removeAllInstancesOf((PropertyColumns)columnId);
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
