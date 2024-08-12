/**
 * ConfigEditor for symbol properties.
 */

#pragma once

#include <JuceHeader.h>

#include "../../Symbolizer.h"

#include "../common/BasicTabs.h"
#include "../common/BasicTable.h"

#include "ConfigEditor.h"

/**
 * Enumeration of column ids for the two tables.  Though one
 * table won't have all of these, it's nice to have a single number
 * space to refer to both of them.
 */
enum FunctionColumns {
    FunctionColumnName = 1,
    FunctionColumnFocus,
    FunctionColumnConfirmation,
    FunctionColumnMuteCancel
};

/**
 * Each table row represents one input or output device.
 * The checks array represents which of the columns are checked.
 */
class FunctionTableRow
{
  public:

    FunctionTableRow() {}
    ~FunctionTableRow() {}

    juce::String name;

    juce::Array<FunctionColumns> checks;
};

class FunctionTable : public BasicTable, public BasicTable::Model
{
  public:

    FunctionTable();
    ~FunctionTable() {}

    void init(class SymbolTable* symbols);
    void load(class SymbolTable* symbols);
    void save(class SymbolTable* symbols);
    
    FunctionTableRow* getRow(int row) {
        return functions[row];;
    }
    
    juce::String getName(int row);

    // BasicTable::Model
    int getNumRows() override;
    juce::String getCellText(int row, int col) override;
    bool getCellCheck(int row, int column) override;
    void setCellCheck(int row, int column, bool state) override;
    
  private:
    bool initialized = false;

    juce::OwnedArray<FunctionTableRow> functions;

    FunctionTableRow* getRow(juce::String name);
    //void loadFunctions(juce::String names, MidiDeviceColumn colid);
    //juce::String getDevices(MidiDeviceColumn colid);

};
    
class PropertiesEditor : public ConfigEditor,
                         public BasicTable::CheckboxListener
{
  public:
    PropertiesEditor(class Supervisor* s);
    ~PropertiesEditor();

    juce::String getTitle() override {return juce::String("Symbol Properties");}
    
    void showing() override;
    void hiding() override;
    void load() override;
    void save() override;
    void cancel() override;

    void resized() override;

    // BasicTable::CheckboxListener
    void tableCheckboxTouched(BasicTable* table, int row, int col, bool state) override;
    
  private:
    BasicTabs tabs;
    FunctionTable functionTable;

};

