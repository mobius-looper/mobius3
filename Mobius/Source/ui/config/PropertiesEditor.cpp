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
#include "../../model/FunctionDefinition.h"
#include "../../model/FunctionProperties.h"

#include "../../Supervisor.h"
#include "../../Symbolizer.h"

#include "../JuceUtil.h"

#include "PropertiesEditor.h"
 
PropertiesEditor::PropertiesEditor(Supervisor* s) : ConfigEditor(s)
{
    setName("PropertiesEditor");
    
    addAndMakeVisible(tabs);

    functionTable.setCheckboxListener(this);

    tabs.add("Functions", &functionTable);
    // tabs.add("Parameters", &parameterTable);
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
    functionTable.init(supervisor->getSymbols());
    functionTable.load(supervisor->getSymbols());
}

/**
 * Called by the Save button in the footer.
 * Tell the ConfigEditor we are done.
 */
void PropertiesEditor::save()
{
    functionTable.save(supervisor->getSymbols());
        
    // update the file
    // need to add access to Symbolizer
    //supervisor->updateDeviceConfig();
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
// FunctionTable
//
//////////////////////////////////////////////////////////////////////

FunctionTable::FunctionTable()
{
    // are our own model
    setBasicModel(this);
}

/**
 * Load the function properties into the table
 */
void FunctionTable::init(SymbolTable* symbols)
{
    if (!initialized) {
        addColumn("Name", FunctionColumnName);
        addColumnCheckbox("Focus", FunctionColumnFocus);
        addColumnCheckbox("Confirmation", FunctionColumnConfirmation);
        addColumnCheckbox("Mute Cancel", FunctionColumnMuteCancel);


        juce::StringArray functionNames;
        for (auto symbol : symbols->getSymbols()) {
            if (symbol->functionProperties != nullptr) {

                // weed out the ones that can't have any of the three checkboxes
                // todo: need to suppress each checkbox

                if (symbol->function->mayFocus ||
                    symbol->function->mayConfirm ||
                    symbol->function->mayCancelMute) {

                    functionNames.add(symbol->name);
                }
            }
        }

        functionNames.sort(false);
        
        for (auto name : functionNames) {
            FunctionTableRow* func = new FunctionTableRow();
            func->name = name;
            functions.add(func);
        }

        initialized = true;
    }
}

/**
 * Look up a device in the table by name.
 */
FunctionTableRow* FunctionTable::getRow(juce::String name)
{
    FunctionTableRow* found = nullptr;
    for (auto row : functions) {
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
void FunctionTable::load(SymbolTable* symbols)
{
    for (auto symbol : symbols->getSymbols()) {
        if (symbol->functionProperties != nullptr) {

            // not every function will have a row, some were suppressed
            FunctionTableRow* row = getRow(symbol->name);
            if (row != nullptr) {
                if (symbol->functionProperties->focus)
                  row->checks.add(FunctionColumnFocus);

                if (symbol->functionProperties->confirmation)
                  row->checks.add(FunctionColumnConfirmation);

                if (symbol->functionProperties->muteCancel)
                  row->checks.add(FunctionColumnMuteCancel);
            }
        }
    }
    
    updateContent();
}


/**
 * Convert the table model back into the MachineConfig
 */
void FunctionTable::save(SymbolTable* symbols)
{
    for (auto func : functions) {

        Symbol* s = symbols->find(func->name);
        if (s != nullptr) {
            FunctionProperties* props = s->functionProperties.get();
            if (props != nullptr) {

                props->focus = func->checks.contains(FunctionColumnFocus);
                props->confirmation = func->checks.contains(FunctionColumnConfirmation);
                props->muteCancel = func->checks.contains(FunctionColumnMuteCancel);
            }
        }
    }
    
}

//
// BasicTable::Model
//


int FunctionTable::getNumRows()
{
    return functions.size();
}

juce::String FunctionTable::getCellText(int row, int columnId)
{
    juce::String value;
    
    FunctionTableRow* func = functions[row];
    if (func == nullptr) {
        Trace(1, "FunctionTable::getCellText row out of bounds %d\n", row);
    }
    else if (columnId == FunctionColumnName) {
        value = func->name;
    }
    else {
        // these are all checkboxes, shouldn't be here
        Trace(1, "FunctionTable::getCellText not supposed to be here\n");
    }
    return value;
}

bool FunctionTable::getCellCheck(int row, int columnId)
{
    bool state = false;
    
    FunctionTableRow* func = functions[row];
    if (func == nullptr) {
        Trace(1, "FunctionTable::getCellCheck row out of bounds %d\n", row);
    }
    else {
        state = func->checks.contains((FunctionColumns)columnId);
    }
    return state;
}

void FunctionTable::setCellCheck(int row, int columnId, bool state)
{
    FunctionTableRow* func = functions[row];
    if (func == nullptr) {
        Trace(1, "FunctionTable::setCellCheck row out of bounds %d\n", row);
    }
    else if (state) {
        func->checks.add((FunctionColumns)columnId);
    }
    else {
        func->checks.removeAllInstancesOf((FunctionColumns)columnId);
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
