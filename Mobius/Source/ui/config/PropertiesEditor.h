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
 * Enumeration of column ids for the two tables.  Though both tables
 * won't have all of these, it's nice to have a single number
 * space to refer to both of them.
 */
enum PropertyColumns {
    PropertyColumnName = 1,
    PropertyColumnFocus,
    PropertyColumnMuteCancel,
    PropertyColumnConfirmation,
    PropertyColumnResetRetain,
    PropertyColumnQuantize
};

/**
 * Each table row represents one function or parameter.
 * The checks array represents which of the columns are checked.
 */
class PropertyTableRow
{
  public:

    PropertyTableRow() {}
    ~PropertyTableRow() {}

    juce::String name;
    class Symbol* symbol = nullptr;
    
    juce::Array<PropertyColumns> checks;
};

class PropertyTable : public BasicTable, public BasicTable::Model
{
  public:

    PropertyTable();
    ~PropertyTable() {}

    void init(class SymbolTable* symbols, bool parameter);
    void load(class SymbolTable* symbols);
    void save(class SymbolTable* symbols);
    
    PropertyTableRow* getRow(int row) {
        return objects[row];;
    }
    
    juce::String getName(int row);

    // BasicTable override
    // this arguably should be in the Model but I don't want to rewritie all of those
    bool needsCheckbox(int row, int column) override;

    // BasicTable::Model
    int getNumRows() override;
    juce::String getCellText(int row, int col) override;
    bool getCellCheck(int row, int column) override;
    void setCellCheck(int row, int column, bool state) override;
    
  private:
    bool isParameter = false;
    bool initialized = false;

    juce::OwnedArray<PropertyTableRow> objects;

    PropertyTableRow* getRow(juce::String name);
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
    PropertyTable functionTable;
    PropertyTable parameterTable;

};

